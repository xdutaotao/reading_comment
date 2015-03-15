/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998-2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_kern.c
 **
 ** DESCRIPTION:
 **	This file contains functions and data that are needed to implement
 **	functions that otherwise exist in the kernel but are not available
 **	for use (since they are declared static).  It also contains slight
 **	variants of functions that otherwise exist in the kernel.
 **
 ** NOTES:
 **	- Why copy?  These functions are defined here because their
 **	  equivalents in the kernel (at least the version I am working with)
 **	  are defined static, and I prefer to create this software easy to
 **	  use (i.e. no kernel patches or rebuilds necessary).
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 02/26/1998	ARTHUR N.	Added to source code control.
 ** 02/27/1998	ARTHUR N.	Added the ovlfs_open_inode() and
 **				 ovlfs_close_inode() functions here.
 ** 03/09/1998	ARTHUR N.	Added the copyright notice.
 ** 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
 ** 03/13/2003	ARTHUR N.	Added vfs_mkdir_noperm to perform directory
 **				 creation without the permission check.
 ** 06/09/2003	ARTHUR N.	Move sched.h to eliminate compiler errors
 **				 due to quotaops.h on x86 dependencies.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_kern.c	1.6	[03/07/27 22:20:34]"

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
#include <linux/quotaops.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/slab.h>
#include <asm/statfs.h>
#include <linux/dcache.h>
#include <linux/dnotify.h>

#include "ovl_fs.h"


/**
 ** FUNCTION: vfs_mkdir_noperm
 **
 **  PURPOSE: Perform a mkdir operation without the permission checking.
 **/

#if POST_20_KERNEL_F

int	vfs_mkdir_noperm (struct inode *dir, struct dentry *dentry, int mode)
{
	int error;

	down(&dir->i_zombie);

#if 0
		/* This is the part we don't want. */

	error = may_create(dir, dentry);
	if (error)
		goto exit_lock;
#endif

	error = -EPERM;
	if (!dir->i_op || !dir->i_op->mkdir)
		goto exit_lock;

	DQUOT_INIT(dir);
	mode &= (S_IRWXUGO|S_ISVTX);
	lock_kernel();
	error = dir->i_op->mkdir(dir, dentry, mode);
	unlock_kernel();

exit_lock:
	up(&dir->i_zombie);
#if 0
		/* Not available for module - at least not in 2.4.19 */
	if (!error)
		inode_dir_notify(dir, DN_CREATE);
#endif
	return error;
}

#endif



#if OVLFS_NEED_OPEN_INODE

# if POST_20_KERNEL_F

/**
 ** FUNCTION: ovlfs_open_dentry
 **
 ** NOTES:
 **	- DOES NOT consume the dentry.
 **	- PRODUCES the file.
 **	- Use ovlfs_close_file() on the file when done.
 **/

int	ovlfs_open_dentry (struct dentry *d_ent, int flags,
	                   struct file **r_file)
{
	struct file	*filp;
	struct vfsmount	*mnt;
	int		ret;

	if ( ( d_ent == NULL ) || ( r_file == NULL ) )
		return	-EINVAL;

		/* Obtain the VFS mount for the inode. */

	ret = get_inode_vfsmount(d_ent->d_inode, &mnt);

	if ( ret == 0 )
	{
			/* dentry_open() puts both the dentry and the     */
			/*  vfsmount.  This function does not consume the */
			/*  dentry, so use dget() on it here.             */

		dget(d_ent);

		filp = dentry_open(d_ent, mnt, flags);

		if ( IS_ERR(filp) )
		{
			ret = PTR_ERR(filp);
		}
		else if ( filp == NULL )
		{
			ret = -ENOENT;
		}
		else
		{
				/* Copy the file information. */

			r_file[0] = filp;
			ret = 0;
		}
	}

	return	ret;
}



/**
 ** FUNCTION: ovlfs_open_inode
 **
 ** NOTES:
 **	- DOES NOT consume the inode.
 **	- PRODUCES the file.
 **	- Use ovlfs_close_file() on the file when done.
 **/

int	ovlfs_open_inode (struct inode *inode, int flags, struct file **r_file)
{
	struct dentry	*d_ent;
	int		ret;

		/* Grab a dentry for the given inode. */

	d_ent = ovlfs_inode2dentry(inode);

	if ( d_ent == NULL )
	{
		ret = -ENOENT;
	}
	else
	{
			/* Now just open the dentry. */

		ret = ovlfs_open_dentry(d_ent, flags, r_file);
	}

	dput(d_ent);

	return	ret;
}



/**
 ** FUNCTION: ovlfs_close_file
 **
 **  PURPOSE: Close a file opened with either the ovlfs_open_inode(), or the
 **           ovlfs_open_dentry() function.
 **/

void    ovlfs_close_file (struct file *file)
{
	if ( file == (struct file *) NULL )
		return;

	filp_close(file, current->files);
}



# else
	/* Kernels < 2.2.0 */



/**
 ** FUNCTION: ovlfs_open_inode
 **
 ** NOTES:
 **     - Copied and modified from linux/fs/open.c do_open(). (in linux source
 **	  tree for kernel source version 2.0.30).
 **
 **     - The inode must already exist.
 **/

int ovlfs_open_inode (struct inode *inode, int flags, struct file **r_file)
{
    int         error;
    struct file *filp;

    DPRINTC2("i_ino %ld, flags %d\n", inode->i_ino, flags);

    filp = MALLOC(sizeof filp[0]);

    if (! filp)
        return -ENOMEM;

    filp->f_count = 1;
    filp->f_flags = flags;
    filp->f_mode = (flags + 1) & O_ACCMODE;

    if (filp->f_mode & FMODE_WRITE)
    {
        error = get_write_access(inode);

        if (error)
        {
            filp->f_count--;
            return error;
        }
    }

    filp->f_inode = inode;
    filp->f_pos = 0;
    filp->f_reada = 0;
    filp->f_op = NULL;

    if (inode->i_op)
        filp->f_op = inode->i_op->default_file_ops;

    if (filp->f_op && filp->f_op->open)
    {
        error = filp->f_op->open(inode, filp);

        if (error < 0)
        {
            if (filp->f_mode & FMODE_WRITE)
                put_write_access(inode);

            filp->f_count--;
            return error;
        }

        IMARK(inode);
    }
    else
        IMARK(inode);

    filp->f_flags &= ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC);

    r_file[0] = filp;

    return 0;
}



/**
 ** FUNCTION: ovlfs_close_file
 **
 **  PURPOSE: Close a file opened with the ovlfs_open_inode() function.
 **/

void	ovlfs_close_file (struct file *file)
{
	if ( file == (struct file *) NULL )
		return;

	if ( ( file->f_op != NULL ) && ( file->f_op->release != NULL ) )
		file->f_op->release(file->f_inode, file);

	if ( file->f_mode & FMODE_WRITE )
		put_write_access(file->f_inode);

	IPUT(file->f_inode);

	FREE(file);
}

# endif

#endif



#if OVLFS_NEED_WAIT_ON_INODE

/**
 ** FUNCTION: ovlfs_wait_on_inode
 **
 **  PURPOSE: Wait for the inode to become free.
 **
 ** NOTES:
 **	- Copied from fs/inode.c (in linux source tree for kernel source
 **	  version 2.0.30).
 **/

int	ovlfs_wait_on_inode (struct inode *inode)
{
#if POST_20_KERNEL_F
	DECLARE_WAITQUEUE(wait, current);
#else
	struct wait_queue	wait = { current, NULL };
#endif
	int			ret;

	ret = 0;

#ifdef I_LOCK
	if ( inode->i_state & I_LOCK )
#else
	if ( inode->i_lock )
#endif
	{
		add_wait_queue(&inode->i_wait, &wait);

			/* Don't call schedule() if the inode is not locked */
			/*  since there may be noone to call wake_up()...   */

#ifdef I_LOCK
		while ( inode->i_state & I_LOCK )
#else
		while ( inode->i_lock )
#endif
		{
			current->state = TASK_INTERRUPTIBLE;
			schedule();

#if POST_20_KERNEL_F
			if ( signal_pending(current) )
#else
			if ( current->signal & (~current->blocked) )
#endif
			{
				ret = -EINTR;
				break;
			}
		}

		remove_wait_queue(&inode->i_wait, &wait);
		current->state = TASK_RUNNING;
	}

	return	ret;
}

#endif



#if OVLFS_NEED_LOCK_INODE

/**
 ** FUNCTION: ovlfs_lock_inode
 **
 **  PURPOSE: Lock the given inode.
 **
 ** NOTES:
 **	- This is a replacement for the kernel function lock_inode() within
 **	  fs/inode.c
 **	- After calling this function, the program MUST call the
 **	  ovlfs_unlock_inode function, or do its work (set i_lock = 0 and call
 **	  wake_up()).
 **/

int	ovlfs_lock_inode (struct inode *inode)
{
	int	ret;

		/* Only wait if the inode is already locked */

#ifdef I_LOCK
	if ( inode->i_state & I_LOCK )
#else
	if ( inode->i_lock )
#endif
	{
		ret = ovlfs_wait_on_inode(inode);

		if ( ret == 0 )
#ifdef I_LOCK
			inode->i_state |= I_LOCK;
#else
			inode->i_lock = 1;
#endif
	}
	else
	{
#ifdef I_LOCK
		inode->i_state |= I_LOCK;
#else
		inode->i_lock = 1;
#endif

		ret = 0;
	}

	return	ret;
}

#endif



#if OVLFS_NEED_UNLOCK_INODE

/**
 ** FUNCTION: ovlfs_unlock_inode
 **
 **  PURPOSE: Unlock the given inode, which was locked by the current process
 **           by the ovlfs_lock_inode function (or equivalent).
 **
 ** NOTES:
 **	- This is a replacement for the kernel function unlock_inode() within
 **	  fs/inode.c
 **/

void	ovlfs_unlock_inode (struct inode *inode)
{
#ifdef I_LOCK
	inode->i_state &= ~I_LOCK;
#else
	inode->i_lock = 0;
#endif
	wake_up(&inode->i_wait);
}

#endif



#if OVLFS_NEED_GET_SUPER

/**
 ** FUNCTION: ovlfs_get_super
 **
 **  PURPOSE: Obtain the super block for the specified device.
 **
 ** NOTES:
 **     - Copied from the kernel source file, fs/super.c. (in linux source
 **	  tree for kernel source version 2.0.30).
 **/

extern struct super_block super_blocks[NR_SUPER];

struct super_block  *ovlfs_get_super (kdev_t dev)
{
    struct super_block * s;

    if (!dev)
        return NULL;
    s = 0+super_blocks;
    while (s < NR_SUPER+super_blocks)
        if (s->s_dev == dev) {
            wait_on_super(s);
            if (s->s_dev == dev)
                return s;
            s = 0+super_blocks;  /* something changed while waiting, restart */
        } else
            s++;
    return NULL;
}

#endif



#if OVLFS_NEED_DEF_PERM

/**
 ** FUNCTION: ovlfs_ck_def_perm
 **
 **  PURPOSE: Perform the default permission checking for the specified inode.
 **
 ** NOTES:
 **	- Copied from fs/namei.c (in linux source tree for kernel source
 **	  version 2.0.30).
 **/

int	ovlfs_ck_def_perm (struct inode *inode, int mask)
{
	int	mode;

	mode = inode->i_mode;

	if ((mask & S_IWOTH) && IS_RDONLY(inode) &&
	    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
		return -EROFS; /* Nobody gets write access to a read-only fs */
	else if ((mask & S_IWOTH) && IS_IMMUTABLE(inode))
		return -EACCES; /* Nobody gets write access to an immutable file */
	else if (current->fsuid == inode->i_uid)
		mode >>= 6;
	else if (in_group_p(inode->i_gid))
		mode >>= 3;
	if (((mode & mask & 0007) == mask) || fsuser())
		return 0;
	return -EACCES;
}

#endif
