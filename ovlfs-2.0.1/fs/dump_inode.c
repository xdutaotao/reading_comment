/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: dump_inode.c
 **
 ** DESCRIPTION: This file contains the source code for the dump_inode module
 **              which, when loaded, dumps the list of all inodes into a file.
 **              The information is useful for debugging a filesystem
 **              implementation.
 **
 ** NOTES:
 **	- The module always fails to load.  This is due to the fact that all
 **	  of the work is done during the load and there is no use in leaving
 **	  the module in the kernel.
 **
 **	- Only inodes that may be in use (i_count > 0, i_dirt != 0, or
 **	  i_lock != 0) are dumped.  This is done since other inodes' contents
 **	  are generally not useful.
 **
 **	- The file used as output may be changed by setting dumpi_file to the
 **	  path of the file. (e.g. insmod dump_inode dumpi_file="/tmp/i")
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 03/08/1998	ARTHUR N.	Added to source code control.
 ** 03/09/1998	ARTHUR N.	Added the copyright notice.
 ** 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#) dump_inode.c 1.3"

#include <stdarg.h>

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/stddef.h>

#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/unistd.h>
#include <asm/statfs.h>
#include <asm/segment.h>

#include "ovl_fs.h"	/* Use the OVLFS syscalls */
#include "dumpi.h"

/**
 ** LOCAL CONSTANTS:
 **/

#ifndef KDEBUG
# define KDEBUG 0
#endif

#define DUMPI_OPEN_MODE	( O_WRONLY | O_APPEND | O_CREAT )



/**
 ** STATIC GLOBALS:
 **/

static int	dumpi_debug_fd = -1;
static int	dumpi_debug_level = KDEBUG;



/**
 ** EXTERN GLOBALS:
 ** 	Variables that may be overriden on installing this module:
 **/

char		*dumpi_file = DUMPI_DEBUG_FILE;
unsigned int	dumpi_max = 8192;	/* Max number of inodes to dump */

	/* the following is the name of the first file whose inode is used */
	/*  as the front of the list dumped.                               */

char	*dumpi_start_file = "/";	/* Use the root dir by default */



/**
 ** FUNCTION: dumpi_open_debug_file
 **
 **  PURPOSE: Open the debug file to use to write the output to.
 **/

static int	dumpi_open_debug_file ()
{
	unsigned long	orig_fs;

#if KDEBUG > 2
	printk("DUMPI: open_debug_file called: dumpi_debug_fd = %d\n",
	       dumpi_debug_fd);
#endif

	if ( dumpi_debug_fd == -1 )
	{
		orig_fs = get_fs();
		set_fs(KERNEL_DS);

		dumpi_debug_fd = open(dumpi_file, DUMPI_OPEN_MODE, 0644);

		set_fs(orig_fs);
	}

	return	dumpi_debug_fd;
}



/**
 ** FUNCTION: dumpi_close_debug_file
 **
 **  PURPOSE: Close the debug file and release any resources used for the file.
 **/

static void	dumpi_close_debug_file ()
{
	if ( dumpi_debug_fd != -1 )
	{
		close(dumpi_debug_fd);
		dumpi_debug_fd = -1;
	}
}



/**
 ** FUNCTION: dumpi_dprintf
 **
 **  PURPOSE: Write the specified printf() information into the debug file if
 **           the level of the message is at or below the level of debugging
 **           set for the module.
 **/

#define LOCK_DUMPI_BUF(f)	spin_lock_irqsave(&Dumpi_buf_lock, (f))
#define UNLOCK_DUMPI_BUF(f)	spin_unlock_irqrestore(&Dumpi_buf_lock, (f))

static spinlock_t	Dumpi_buf_lock = SPIN_LOCK_UNLOCKED;
static char		Dumpi_buffer[4096];

static void	dumpi_dprintf (int level, char *fmt, ...)
{
	int		flags;
	va_list		arg_list;
	unsigned long	orig_fs;

	if ( level > dumpi_debug_level )
		return;

	if ( dumpi_open_debug_file() < 0 )
	{
#if KDEBUG > 2
		printk("DUMPI: dumpi_dprintf: dumpi_open_debug_file failed"
		       " (%d)\n", dumpi_debug_fd);
#endif

		return;
	}

	LOCK_DUMPI_BUF(flags);

	va_start(arg_list, fmt);

	orig_fs = get_fs();
	set_fs(KERNEL_DS);

	if ( vsprintf(Dumpi_buffer, fmt, arg_list) > 0 )
	{
		int	ret;

		ret = write(dumpi_debug_fd, Dumpi_buffer, strlen(Dumpi_buffer));

#if KDEBUG > 2
		printk("DUMPI: dumpi_dprintf: sys_write returned %d\n", ret);
#endif
	}

	set_fs(orig_fs);

	va_end(arg_list);

	UNLOCK_DUMPI_BUF(flags);

	dumpi_close_debug_file();
}



/**
 ** FUNCTION: dumpi_end_debug
 **
 **  PURPOSE: Cleanup at the end of debugging.
 **/

static void	dumpi_end_debug()
{
	dumpi_close_debug_file();
}



/**
 ** FUNCTION: dumpi_debug_inodes
 **
 **  PURPOSE: Dump all inode information in the inode list which the given
 **           inode is a member of.  The list is generally (if not always)
 **           the global list of all inodes.
 **/

static void	dumpi_debug_inodes (struct inode *first_inode)
{
	struct inode	*one_inode;
	int		count;

	one_inode = first_inode;

	dprintf(0, "USED INODE LIST:\n");

	if ( one_inode == INODE_NULL )
		dprintf(0, "first inode is null!\n");
	else
		dprintf(0, "\tSUP BLK   \tINUM     \tADDRESS   \tUSE COUNT"
		        "\tFLAGS\n");

	count = 0;

	do
	{
			/* Only dump inodes that are in use by at least one */
			/*  process, are dirty, or are locked.              */

		if ( ( one_inode->i_count != 0 ) ||
		     ( one_inode->i_dirt != 0 ) ||
		     ( one_inode->i_lock != 0 ) )
		{
			char	flags[2];

			if ( one_inode->i_dirt != 0 )
				flags[0] = 'D';
			else
				flags[0] = ' ';

			if ( one_inode->i_lock != 0 )
				flags[1] = 'L';
			else
				flags[1] = ' ';

			dprintf(0, "\t0x%08x\t%09d\t0x%08x\t%9d\t%.2s\n",
			        one_inode->i_sb, one_inode->i_ino,
			        one_inode, one_inode->i_count, flags);

			count++;
		}

		one_inode = one_inode->i_next;
	} while ( ( one_inode != first_inode ) &&
	          ( one_inode != INODE_NULL ) &&
	          ( count < dumpi_max ) );

	if ( one_inode == INODE_NULL )
		dprintf(0, "The End: NULL inode reached.\n");
	else if ( one_inode != first_inode )
		dprintf(0, "The End: non-null inode, but not the first one.\n");
	else
		dprintf(0, "The End: found the first inode again.\n");
}



/**
 ** FUNCTION: init_module
 **
 **  PURPOSE: This is the function called on loading the module into the
 **           kernel.  All work is done here, and the load always fails.
 **/

static int	errno;

int	init_module ()
{
	struct inode	*first;
	int		ret;


		/* Setup the system call functions (open, read, etc.) */

	ovlfs_setup_syscalls();


		/* We need an inode to start with */

	ret = get_inode(dumpi_start_file, &first);

	if ( ret < 0 )
	{
		printk("dump inodes: unable to obtain root directory inode:"
		       " error %d\n", ret);
	}
	else
	{
		dumpi_debug_inodes(first);
		iput(first);
	}

	dumpi_end_debug();

		/* Don't allow the module to finish loading since it does */
		/*  not have any persistent meaning.                      */

		/* Perhaps I should make this into a character device */
	return	-1;
}



/**
 ** FUNCTION: cleanup_module
 **
 **  PURPOSE: Cleanup all resources used by the module on removal from the
 **           kernel.  Note that this function would never be called since
 **           the init_module() always fails, but it is included for
 **           completeness.
 **/

void	cleanup_module()
{
		/* This should never end up being called */

	dumpi_end_debug();
	printk("dump inodes: bye\n");
}
