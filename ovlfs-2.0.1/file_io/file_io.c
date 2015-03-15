/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: file_io.c
 **
 ** DESCRIPTION:
 **	File I/O operations for use in the Linux kernel.  These functions
 **	support the file descriptor concept in addition to the open, read,
 **	write, lseek, and dup operations.
 **
 ** NOTES:
 **	- An internal table is used to track the "file descriptors".  Because
 **	  of this:
 **
 **		1. These file descriptors are usable by all kernel tasks.  In
 **		   other words, one descriptor accesses the same file from
 **		   all tasks.
 **
 **		2. Files managed here are not attached to any process.  Good,
 **		   bad, or otherwise!
 **
 **		3. There is no coorelation between user-mode process file
 **		   descriptors and those maintained here.
 **
 **	- There is no such thing as "starndard output", "standard input", etc.
 **
 **	- Only the operations needed by the Overlay Filesystem have been
 **	  implemented.  Please do not hesitate to add more as-needed.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-03-11	ARTHUR N.	Initial Release.
 ** 2003-04-24	ARTHUR N.	Increment module use count in file_dup_filp.
 ** 2003-06-09	ARTHUR N.	Corrected module version support.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)file_io.c	1.3	[03/07/27 22:20:33]"

#include "kernel_vers_spec.h"

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "file_io.ver"
# endif
#endif

#ifdef MODULE
# include <linux/module.h>
#endif

#include <asm/uaccess.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>

#include "file_io.h"
#include "file_io_priv.h"



/***
 *** FILE-LOCAL VARIABLES
 ***/

static fio_file_info_t	FIO_file_tbl[FIO_MAX_FILE] = { { NULL, 0 }, };
static spinlock_t	FIO_lock = SPIN_LOCK_UNLOCKED;



#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)

/**
 ** FUNCTION: default_llseek
 **
 **  PURPOSE: Perform the default lseek behavior when a file/inode does not
 **           implement the lseek entry point.
 **/

static inline int   default_llseek (struct file *file, off_t offset,
                                    int from_where)
{
    off_t   tmp = 0;
    int     ret = 0;

    switch (from_where) {
        case 0:
            tmp = offset;
            break;
        case 1:
            tmp = file->f_pos + offset;
            break;
        case 2:
            tmp = file->f_dentry->d_inode->i_size + offset;
            break;
        default:
            ret = -EINVAL;
            break;
    }

    if ( (tmp < 0) && ( ret >= 0 ) )
        ret = -EINVAL;
    else if (tmp != file->f_pos) {
        file->f_pos = tmp;
        file->f_reada = 0;
        file->f_version = ++global_event;
    }

    return  ret;
}

#endif



/**
 ** FUNCITON: file_write
 **
 **  PURPOSE: Given a file pointer, a data buffer, and the number of bytes to
 **           write to the file, write the data.
 **/

size_t	file_write (int fd, const char *buf, size_t count)
{
	struct file	*filp;
	size_t		written;
	mm_segment_t	orig_fs;
	int		rc;
	int		ret_code;

	FIO_LOCK();

	if ( ( fd < 0 ) || ( fd >= FIO_MAX_FILE ) ||
	     ( ! FIO_file_tbl[fd].in_use ) )
	{
		ret_code = -EBADF;
		goto file_write__out_unlock;
	}

	filp = FIO_file_tbl[fd].filp;


		/* Make sure the file pointer is valid and has a write */
		/*  operation associated with it.                      */

	if ( ( filp == (struct file *) NULL ) || ( filp->f_op == NULL ) ||
	     ( filp->f_op->write == NULL ) )
	{
		ret_code = -EBADF;
		goto	file_write__out_unlock;
	}

	FIO_UNLOCK();

		/* Perform the write. */

	orig_fs = get_fs();
	set_fs(KERNEL_DS);

		/* Call the write operation. */

	ret_code = filp->f_op->write(filp, buf, count, &filp->f_pos);

		/* Restore the FS. */
	set_fs(orig_fs);

	return	ret_code;

file_write__out_unlock:
	FIO_UNLOCK();

	return	ret_code;
}



/**
 ** FUNCITON: file_read
 **
 **  PURPOSE: Given a file descriptor, a data buffer, and the number of bytes
 **           to read, read from the file.
 **/

size_t	file_read (int fd, char *buf, size_t count)
{
	size_t		written;
	mm_segment_t	orig_fs;
	int		ret_code;
	struct file	*filp;

	FIO_LOCK();

	if ( ( fd < 0 ) || ( fd >= FIO_MAX_FILE ) ||
	     ( ! FIO_file_tbl[fd].in_use ) )
	{
		ret_code = -EBADF;
		goto file_read__out_unlock;
	}

	filp = FIO_file_tbl[fd].filp;


		/* Make sure the file pointer is valid and has a read */
		/*  operation associated with it.                     */

	if ( ( filp == (struct file *) NULL ) || ( filp->f_op == NULL ) ||
	     ( filp->f_op->read == NULL ) )
	{
		ret_code = -EINVAL;
		goto file_read__out_unlock;
	}

	FIO_UNLOCK();

		/* Perform a single read and return the result. */

	written  = 0;

	orig_fs = get_fs();
	set_fs(KERNEL_DS);

		/* Call the read operation. */

	ret_code = filp->f_op->read(filp, buf, count, &filp->f_pos);

		/* Restore the FS. */
	set_fs(orig_fs);

	return	ret_code;

file_read__out_unlock:
	FIO_UNLOCK();

	return	ret_code;
}



/**
 ** FUNCITON: file_lseek
 **
 **  PURPOSE: Given a file descriptor, an offset, and a starting indicator,
 **           seek to the requested position in the file.
 **/

loff_t	file_lseek (int fd, loff_t offset, int from_where)
{
	loff_t		ret_code;
	struct file	*filp;

	FIO_LOCK();

	if ( ( fd < 0 ) || ( fd >= FIO_MAX_FILE ) ||
	     ( ! FIO_file_tbl[fd].in_use ) )
	{
		ret_code = -EBADF;
		goto file_lseek__out_unlock;
	}

	filp = FIO_file_tbl[fd].filp;


		/* Make sure the file pointer is valid and has a read */
		/*  operation associated with it.                     */

	if ( filp == (struct file *) NULL )
	{
		ret_code = -EINVAL;
		goto file_lseek__out_unlock;
	}

	FIO_UNLOCK();

		/* Perform a single lseek and return the result. */

		/* Call the llseek operation, if defined. */

	if ( ( filp->f_op != NULL ) && ( filp->f_op->llseek != NULL ) )
		ret_code = filp->f_op->llseek(filp, offset, from_where);
	else
		ret_code = default_llseek(filp, offset, from_where);

	return	ret_code;

file_lseek__out_unlock:
	FIO_UNLOCK();

	return	ret_code;
}



/**
 ** FUNCTION: file_open
 **
 **  PURPOSE: Open the specified file with the requested mode and return a file
 **           descriptor.
 **
 ** ARGUMENTS:
 **	path		- Path to the file; can be relative - the current
 **			  directory of the calling task is used as the starting
 **			  point.
 **	open_flags	- Mode in which to open the file:
 **				0	- Read-only
 **				1	- Write-only
 **				2	- Read-Write
 **				3	- Special
 **			  Can use O_CREAT and friends with this as well.
 **	f_mode		- Access permissions for files that are created.
 **
 ** RULES:
 **	- The path string must be in kernel memory.
 **/

int	file_open (const char *path, int open_flags, int f_mode)
{
	struct file	*result;
	int		slot;
	int		ret_code;

	if ( path == NULL )
		return	-EINVAL;

		/* Find an empty slot. */

	FIO_LOCK();

	slot = 0;

	while ( ( FIO_file_tbl[slot].in_use ) && ( slot < FIO_MAX_FILE ) )
		slot++;

	if ( slot >= FIO_MAX_FILE )
	{
		ret_code = -ENFILE;
		goto file_open__out_unlock;
	}

	FIO_file_tbl[slot].in_use = TRUE;

	FIO_UNLOCK();

#if MODULE
	MOD_INC_USE_COUNT;
#endif

		/* Just use the filp_open() function. */

	result = filp_open(path, open_flags, f_mode);

	if ( ! IS_ERR(result) )
	{
		FIO_file_tbl[slot].filp = result;
		ret_code = slot;
	}
	else
	{
		ret_code = PTR_ERR(result);

#if MODULE
		MOD_DEC_USE_COUNT;
#endif
	}

	return	ret_code;

file_open__out_unlock:
	FIO_UNLOCK();

	return	ret_code;
}



/**
 ** FUNCTION: file_dup_filp
 **
 **  PURPOSE: Attach the given file pointer to a file descriptor for access.
 **
 ** RULES:
 **	- The caller must produce the file structure.
 **	- file_close() will consume it.
 **/

int	file_dup_filp (struct file *filp)
{
	int		slot;
	int		ret_code;

	if ( filp == NULL )
		return	-EINVAL;

		/* Find an empty slot. */

	FIO_LOCK();

	slot = 0;

	while ( ( FIO_file_tbl[slot].in_use ) && ( slot < FIO_MAX_FILE ) )
		slot++;


		/* If an empty slot was found, attach it. */

	if ( slot < FIO_MAX_FILE )
	{
			/* Just store the given file pointer. */

		FIO_file_tbl[slot].filp   = filp;
		FIO_file_tbl[slot].in_use = TRUE;
		ret_code = slot;

#if MODULE
		MOD_INC_USE_COUNT;
#endif
	}
	else
	{
		ret_code = -ENFILE;
	}

	FIO_UNLOCK();

	return	ret_code;
}



/**
 ** FUNCTION: file_close
 **
 **  PURPOSE: Close the file descriptor specified.
 **/

void	file_close (int fd)
{
	struct file	*filp;

	FIO_LOCK();

		/* Validate the descriptor given. */

	if ( ( fd < 0 ) || ( fd >= FIO_MAX_FILE ) ||
	     ( ! FIO_file_tbl[fd].in_use ) )
	{
		goto file_close__out_unlock;
	}

		/* Remember the file structure for the descriptor and    */
		/*  clear out the table entry before releasing the lock. */

	filp = FIO_file_tbl[fd].filp;

	FIO_file_tbl[fd].in_use = FALSE;
	FIO_file_tbl[fd].filp   = NULL;

	FIO_UNLOCK();


		/* Make sure the file pointer is valid and close it. */

	if ( filp != (struct file *) NULL )
	{
		filp_close(filp, current->files);
#if MODULE
		MOD_DEC_USE_COUNT;
#endif
	}


	return;

file_close__out_unlock:
	FIO_UNLOCK();
	return;
}



#if MODULE

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arthur Naseef");
MODULE_DESCRIPTION("Kernel-level File I/O");

EXPORT_SYMBOL(file_open);
EXPORT_SYMBOL(file_close);
EXPORT_SYMBOL(file_read);
EXPORT_SYMBOL(file_write);
EXPORT_SYMBOL(file_dup_filp);
EXPORT_SYMBOL(file_lseek);

/**
 ** FUNCTION: init_module
 **
 **  PURPOSE: Startup the module.
 **/

int	init_module ()
{
	return	0;
}



/**
 ** FUNCTION: cleanup_module
 **
 **  PURPOSE: Cleanup the module.
 **/

void	cleanup_module ()
{
}

#endif
