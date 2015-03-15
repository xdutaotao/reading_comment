/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_debug.c
 **
 ** DESCRIPTION: This file contains debugging functions that log debugging
 **              information in a file.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 09/26/1997	ARTHUR N.	Added to source code control.
 ** 09/26/1997	ARTHUR N.	Added comments.
 ** 03/09/1998	ARTHUR N.	Added the copyright notice.
 ** 02/06/1999	ARTHUR N.	Added the ovlfs_debugf() function.
 ** 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
 ** 02/24/2003	ARTHUR N.	Added busy filesystem debug functions.
 ** 03/06/2003	ARTHUR N.	Added stack use measurement functions.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_debug.c	1.7	[03/07/27 22:20:34]"

#include <stdarg.h>

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "ovlfs.ver"
# endif
#endif

#include <linux/version.h>

#include <linux/stddef.h>

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <asm/statfs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>

#include "file_io.h"
#include "ovl_fs.h"

#ifndef OVLFS_DEBUG_LEVEL
# ifdef KDEBUG
#  define OVLFS_DEBUG_LEVEL	KDEBUG
# else
#  define OVLFS_DEBUG_LEVEL	0
# endif
#endif


/**
 ** GLOBALS:
 **/

static int	ovlfs_debug_fd = -1;
int		ovlfs_debug_level = OVLFS_DEBUG_LEVEL;

#if DEBUG_STACK_USAGE
static int	Max_stack_usage = 0;
static int	Last_stack_usage = 0;
static uint8_t	*Start_stack = NULL;

# define OVLFS_STACK_PAD	32
static uint8_t	PREP_IND[4] = { 0x01, 0xab, 0x34, 0xed };
#endif



/**
 ** FUNCTION: ovlfs_open_debug_file
 **
 **  PURPOSE: Open the debugging output file within the current process.
 **
 ** NOTES:
 **	- Since file descriptors are allocated per process, opening the debug
 **	  file in this manner will only allow for access within the current
 **	  process.
 **/

static int	ovlfs_open_debug_file ()
{
#if KDEBUG > 2
	printk("OVLFS: open_debug_file called: ovlfs_debug_fd = %d\n",
	       ovlfs_debug_fd);
#endif

	if ( ovlfs_debug_fd == -1 )
	{
		ovlfs_debug_fd = file_open(OVLFS_DEBUG_FILE,
		                           O_WRONLY | O_APPEND | O_CREAT,
		                           0644);

	}

	return	ovlfs_debug_fd;
}



/**
 ** FUNCTION: ovlfs_close_debug_file
 **
 **  PURPOSE: Close the debug file opened in the current process.
 **
 ** NOTES:
 **	- If this function is called in a different process than the file was
 **	  opened in, it may close a file in use by the process and will not
 **	  close the debug file itself.
 **/

static void	ovlfs_close_debug_file ()
{
	if ( ovlfs_debug_fd != -1 )
	{
		file_close(ovlfs_debug_fd);
		ovlfs_debug_fd = -1;
	}
}


/**
 ** FUNCTION: ovlfs_dprintf
 **
 **  PURPOSE: Print a message into the debug file using printf() format, if
 **           the level of the message is less than or equal to the current
 **           debug level.
 **/

#define LOCK_DEBUG_BUF(f)	spin_lock_irqsave(&Debug_buf_lock, (f))
#define UNLOCK_DEBUG_BUF(f)	spin_unlock_irqrestore(&Debug_buf_lock, (f))

static spinlock_t	Debug_buf_lock = SPIN_LOCK_UNLOCKED;
static char		Debug_buffer[4096];

void	ovlfs_dprintf (int level, char *fmt, ...)
{
	int		flags;
	va_list		arg_list;

	if ( level > ovlfs_debug_level )
		return;

	if ( ovlfs_open_debug_file() < 0 )
	{
#if KDEBUG > 2
		printk("OVLFS: ovlfs_dprintf: ovlfs_open_debug_file failed"
		       " (%d)\n", ovlfs_debug_fd);
#endif

		return;
	}

	LOCK_DEBUG_BUF(flags);

	va_start(arg_list, fmt);

		/* Pretend that the kernel space is user space */

	if ( vsprintf(Debug_buffer, fmt, arg_list) > 0 )
	{
		int	ret;

		ret = file_write(ovlfs_debug_fd, Debug_buffer,
		                 strlen(Debug_buffer));

# if KDEBUG > 2
		printk("OVLFS: ovlfs_dprintf: write returned %d\n", ret);
# endif
	}

	va_end(arg_list);

	UNLOCK_DEBUG_BUF(flags);

	ovlfs_close_debug_file();
}


/**
 ** FUNCTION: ovlfs_debugf
 **
 **  PURPOSE: Print a message into the debug file using printf() format,
 **           regardless of the current debug level.
 **/

void	ovlfs_debugf (char *fmt, ...)
{
	int		flags;
	va_list		arg_list;

	if ( ovlfs_open_debug_file() < 0 )
	{
#if KDEBUG > 2
		printk("OVLFS: ovlfs_dprintf: ovlfs_open_debug_file failed"
		       " (%d)\n", ovlfs_debug_fd);
#endif

		return;
	}

	LOCK_DEBUG_BUF(flags);

	va_start(arg_list, fmt);

		/* Pretend that the kernel space is user space */

	if ( vsprintf(Debug_buffer, fmt, arg_list) > 0 )
	{
		int	ret;

		ret = file_write(ovlfs_debug_fd, Debug_buffer,
		                 strlen(Debug_buffer));

#if KDEBUG > 2
		printk("OVLFS: ovlfs_dprintf: write returned %d\n", ret);
#endif
	}

	va_end(arg_list);

	UNLOCK_DEBUG_BUF(flags);

	ovlfs_close_debug_file();
}



/**
 ** FUNCTION: ovlfs_end_debug
 **
 **  PURPOSE: Stop any debugging.  This currently is not needed.
 **/

void	ovlfs_end_debug()
{
	ovlfs_close_debug_file();
}



#if DEBUG_BUSY_OVLFS

/**
 ** FUNCTION: ovlfs_debug_inodes
 **
 **  PURPOSE: Write the list of inodes currently cached in the kernel to the
 **           debug file.
 **/

void	ovlfs_debug_inodes (struct inode *first_inode)
{
	struct inode	*one_inode;

	one_inode = first_inode;

	dprintf(0, "USED INODE LIST:\n");

	if ( one_inode == INODE_NULL )
		dprintf(0, "first inode is null!\n");
	else
		dprintf(0, "\tSUP BLK   \tINUM     \tADDRESS   \tUSE COUNT\n");

	do
	{
#if POST_22_KERNEL_F
		if ( atomic_read(&(one_inode->i_count)) != 0 )
#else
		if ( one_inode->i_count != 0 )
#endif
		{
			dprintf(0, "\t0x%08x\t%09d\t0x%08x\t%d\n",
			        one_inode->i_sb, one_inode->i_ino,
			        one_inode, one_inode->i_count);
		}

#if POST_20_KERNEL_F
		one_inode = list_entry(&(one_inode->i_list.next), struct inode,
		                       i_list);
#else
		one_inode = one_inode->i_next;
#endif
	} while ( ( one_inode != first_inode ) && ( one_inode != INODE_NULL ) );
}



/**
 ** FUNCTION: debug_show_busy_inode
 **/

static void	debug_show_busy_inode (struct inode *inode,
		                       const char *list_name)
{
	printk(KERN_EMERG "OVLFS: busy inode, ino %lu, on %s; inode addr "
	       "0x%08x\n", inode->i_ino, list_name, (int) inode);
}



/**
 ** FUNCTION: debug_busy_inodes
 **/

int	debug_busy_inodes_on_list (struct list_head *head, kdev_t dev,
	                           const char *list_name)
{
	struct list_head	*cur;
	struct inode		*one_inode;
	int			count;

	if ( list_empty(head) )
		return	0;

	count = 0;
	cur = head;

	do
	{
		one_inode = list_entry(cur, struct inode, i_list);

		if ( ( one_inode != NULL ) && ( one_inode->i_dev == dev ) )
		{
			if ( atomic_read(&(one_inode->i_count)) != 0 )
			{
				count++;
				debug_show_busy_inode(one_inode, list_name);
			}
		}

		cur = cur->next;
	} while ( cur != head );

	return	count;
}



/**
 ** FUNCTION: debug_busy_inodes
 **/

int	debug_busy_inodes (kdev_t dev, struct list_head *inode_in_use,
	                   struct list_head *inode_unused)
{
	int	count;

	count  = debug_busy_inodes_on_list(inode_in_use, dev, "inode_in_use");
	count += debug_busy_inodes_on_list(inode_unused, dev, "inode_unused");

	return	count;
}


/**
 ** FUNCTION: debug_busy_inodes_sb
 **/

int	debug_busy_inodes_sb (struct super_block *sb,
	                      struct list_head *inode_in_use,
	                      struct list_head *inode_unused)
{
	int	count;

	count  = debug_busy_inodes_on_list(inode_in_use, sb->s_dev,
	                                   "inode_in_use");
	count += debug_busy_inodes_on_list(inode_unused, sb->s_dev,
	                                   "inode_unused");
	count += debug_busy_inodes_on_list(&(sb->s_dirty), sb->s_dev,
	                                   "s_dirty");
	count += debug_busy_inodes_on_list(&(sb->s_locked_inodes), sb->s_dev,
	                                   "s_locked_inodes");

	return	count;
}



/**
 **  FUNCTION: debug_show_busy_dentry
 **
 **   PURPOSE: Generate a message indicating a busy dentry.
 **/


void	debug_show_busy_dentry (struct dentry *dent)
{
	printk(KERN_EMERG "OVLFS: busy dentry, name %.*s; dentry addr "
	       "0x%08x\n", (int) dent->d_name.len, dent->d_name.name,
	       (int) dent);
}



/**
 ** FUNCTION: debug_busy_dentry_list
 **
 **  PURPOSE: Find dentries on the given list for the specified device.
 **/

int	debug_busy_dentry_list (struct list_head *d_list, kdev_t dev)
{
	struct list_head	*cur;
	struct dentry		*dent;
	int			count;

	if ( list_empty(d_list) )
		return	0;

	count = 0;
	cur = d_list;

	do
	{
		dent = list_entry(cur, struct dentry, d_hash);

		if ( ( dent != NULL ) && ( dent->d_sb != NULL ) &&
		     ( dent->d_sb->s_dev == dev ) )
		{
			debug_show_busy_dentry(dent);
			count++;
		}

		cur = cur->next;
	} while ( cur != d_list );

	return	count;
}



/**
 ** FUNCTION: debug_busy_dentry
 **
 **  PURPOSE: Find busy dentries for the device given.
 **
 ** NOTES:
 **	- The dentry hash and its shift value must be given as arguments since
 **	  they are static variables in the kernel.
 **/

int	debug_busy_dentry (struct list_head *hashtable, uint32_t hash_shift,
	                   kdev_t dev)
{
	int			cur_list;
	uint32_t		num_list;
	int			count;

	num_list = ( 1 << hash_shift );
	cur_list = 0;
	count    = 0;

	while ( cur_list < num_list )
	{
		count += debug_busy_dentry_list(hashtable + cur_list, dev);
		cur_list++;
	}

	return	count;
}

#endif



#if DEBUG_STACK_USAGE

/**
 ** FUNCTION: debug_prep_stack_usage
 **
 **  PURPOSE: Call at the entry of each kernel thread for which stack usage is
 **           to be calculated.
 **/

static void	__debug_prep_stack_usage ()
{
	union task_union	*tasku;
	uint8_t			*stack_low;
	uint8_t			*stack_high;

		/* Only prepare once per thread. */

	if ( Start_stack != NULL )
		return;

	tasku = (union task_union *) current;

		/* Calculate the high and low ends of the stack; just use  */
		/*  one of this function' stack variables to find the high */
		/*  point (this is the unused section only).               */

	stack_low  = ( (uint8_t *) tasku ) + sizeof ( struct task_struct );
	stack_high = ( (uint8_t *) &stack_high );

	Start_stack = stack_low;

	stack_low  += OVLFS_STACK_PAD;
	stack_high -= OVLFS_STACK_PAD;

	if ( stack_low < stack_high )
	{
		if ( ! memcmp(stack_low, PREP_IND, 4) )
		{
			DPRINT5((KERN_WARNING "OVLFS: %s already prep'ed?\n",
			         __FUNCTION__));
			return;
		}


			/* Place a tag at the start to indicate prep'ed */

		memcpy(stack_low, PREP_IND, 4);


			/* Fill with a fixed pattern in the unused section. */

		memset(stack_low + 4, 0x3e, (stack_high - stack_low));
	}
	else
	{
		printk(KERN_EMERG "OVLFS: %s: stack usage too low at 0x%08x, "
		       "stack high 0x%08x!\n", __FUNCTION__,
		        (int) stack_low, (int) stack_high);
	}
}

	/* Stupid debugging problems: */
void	debug_prep_stack_usage ()
{
	__debug_prep_stack_usage();
}



/**
 ** FUNCTION: debug_calc_stack_usage
 **
 **  PURPOSE: Calculate the amount of stack used.
 **/

static void	__debug_calc_stack_usage ()
{
	union task_union	*tasku;
	int			found_f;
	int			last_f;
	uint32_t		usage;
	uint32_t		avail;
	uint8_t			*stack_low;
	uint8_t			*stack_high;

	tasku = (union task_union *) current;

		/* Calculate the high and low ends of the stack; just use  */
		/*  one of this function' stack variables to find the high */
		/*  point (this is the unused section only).               */

	stack_low  = ( (uint8_t *) tasku ) + sizeof ( struct task_struct );
	stack_high = ( (uint8_t *) &stack_high );

	if ( Start_stack == stack_low )
		last_f = TRUE;
	else
		last_f = FALSE;

	stack_low  += OVLFS_STACK_PAD;
	stack_high -= OVLFS_STACK_PAD;

	if ( stack_low < stack_high )
	{
		if ( ! last_f )
			return;

		if ( memcmp(stack_low, PREP_IND, 4) )
		{
			printk(KERN_WARNING "OVLFS: %s called before prep?\n",
			       __FUNCTION__);
			return;
		}

		stack_low += 4;

			/* Search for the end of the fill pattern. */

		found_f = FALSE;

		while ( ( stack_low < stack_high ) && ( ! found_f ) )
		{
			if ( stack_low[0] != 0x3e )
				found_f = TRUE;
			else
				stack_low++;
		}

			/* If no used portion was found, the total stack */
			/*  usage was negligable.                        */

		if ( found_f )
		{
			usage = stack_high - stack_low;
			Last_stack_usage = usage;

			if ( usage > Max_stack_usage )
			{
				avail = (sizeof (union task_union)) -
				        ((sizeof (struct task_struct)) +
				         OVLFS_STACK_PAD * 2);

				Max_stack_usage = usage;
				printk("OVLFS: %s: new max, %u (of %u)\n",
				       __FUNCTION__, usage, avail);
			}
		}
	}
	else
	{
		printk(KERN_EMERG "OVLFS: %s: stack usage too low at 0x%08x!\n",
		       __FUNCTION__, (int) stack_low);
	}

	if ( last_f )
		Start_stack = NULL;
}

void	debug_calc_stack_usage ()
{
	__debug_calc_stack_usage();
}

#endif
