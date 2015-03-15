/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_misc.c
 **
 ** DESCRIPTION:
 **	This file contains miscellaneous functions for the Overlay Filesystem.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-03-11	ARTHUR N.	Created from ovl_ino.c.
 ** 2003-03-11	ARTHUR N.	Added do_create() from ovl_file.c.
 ** 2003-03-11	ARTHUR N.	Change do_link() so that it never consumes
 **				 the inodes given as arguments.
 ** 2003-03-14	ARTHUR N.	Replaced ovlfs_inode_get_child_dentry() and
 **				 ovlfs_inode_lookup_child_dentry() with a
 **				 unified version.  Also, allow negative and
 **				 positive dentries in do_rename.
 ** 2003-06-09	ARTHUR N.	Corrected module version support.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_misc.c	1.5	[03/07/27 22:20:35]"

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "ovlfs.ver"
# endif
#endif

#include <linux/version.h>

#include <linux/stddef.h>

#include <linux/types.h>
#include <linux/fs.h>

#include "ovl_debug.h"
#include "ovl_fs.h"



/**
 ** FUNCTION: do_mkdir
 **
 **  PURPOSE: Call the inode's mkdir operation with the given arguments.
 **/

int	do_mkdir (struct inode *inode, const char *name, int len, int mode)
{
#if USE_DENTRY_F
	struct dentry	*tmp_dent;
#endif
	int		ret_code;

	DOWN(&(inode->i_sem));

#if USE_DENTRY_F
		/* Create a temporary negative dentry for the target. */

	ret_code = ovlfs_inode_get_child_dentry(inode, name, len, &tmp_dent,
	                                        OVLFS_DENT_GET_NEGATIVE);

	if ( ret_code == 0 )
	{
			/* Use the vfs_mkdir function to do the dirty work. */

		ret_code = vfs_mkdir(inode, tmp_dent, mode);

		dput(tmp_dent);
	}
#else
	IMARK(inode);
	ret_code = inode->i_op->mkdir(inode, name, len, mode);
#endif

	UP(&(inode->i_sem));

	return	ret_code;
}



/**
 ** FUNCTION: do_link
 **
 **  PURPOSE: Call the inode's link operation with the given arguments.
 **
 ** RULES:
 **	- The inode and directory given are NOT consumed.
 **/

int	do_link (struct inode *inode, struct inode *dir, const char *name,
	         int len)
{
#if USE_DENTRY_F
	struct dentry	*ref_dent;
	struct dentry	*new_dent;
#endif
	int		ret;

	DOWN(&(dir->i_sem));

#if POST_20_KERNEL_F

		/* Get a dentry for the link source. */

	ref_dent = ovlfs_inode2dentry(inode);

	if ( ref_dent == NULL )
	{
		ret = -ENOENT;
	}
	else
	{
			/* Create a temporary negative dentry for the target. */

		ret = ovlfs_inode_get_child_dentry(dir, name, len, &new_dent,
		                                   OVLFS_DENT_GET_NEGATIVE);

		if ( ret != 0 )
			dput(ref_dent);
	}

	if ( ret == 0 )
	{
			/* Use the vfs_link function to do the dirty work. */

		ret = vfs_link(ref_dent, dir, new_dent);

		dput(ref_dent);
		dput(new_dent);
	}
#else
	IMARK(inode);
	IMARK(dir);
	ret = dir->i_op->link(inode, dir, name, len);
#endif

	UP(&(dir->i_sem));

	return	ret;
}



/**
 ** FUNCTION: do_unlink
 **
 **  PURPOSE: Call the inode's unlink operation with the given arguments.
 **/

int	do_unlink (struct inode *inode, const char *name, int len)
{
#if POST_20_KERNEL_F
	struct dentry	*tmp_dent;
#endif
	int		ret_code;

	DOWN(&(inode->i_sem));

#if POST_20_KERNEL_F
		/* Grab a dentry for the file being unlinked. */

	ret_code = ovlfs_inode_get_child_dentry(inode, name, len, &tmp_dent,
	                                        OVLFS_DENT_GET_POSITIVE);

	if ( ret_code == 0 )
	{
			/* Use the vfs_unlink function to do the dirty work. */

		ret_code = vfs_unlink(inode, tmp_dent);

		dput(tmp_dent);
	}
#else
	IMARK(inode);
	ret = inode->i_op->unlink(inode, fname, len);
#endif

	UP(&(inode->i_sem));

	return	ret_code;
}



/**
 ** FUNCTION: do_rmdir
 **
 **  PURPOSE: Call the inode's rmdir operation with the given arguments.
 **/

int	do_rmdir (struct inode *inode, const char *name, int len)
{
#if POST_20_KERNEL_F
	struct dentry	*tmp_dent;
#endif
	int		ret_code;

	DOWN(&(inode->i_sem));

#if POST_20_KERNEL_F
		/* Grab a dentry for the directory being removed. */

	ret_code = ovlfs_inode_get_child_dentry(inode, name, len, &tmp_dent,
	                                        OVLFS_DENT_GET_POSITIVE);

	if ( ret_code == 0 )
	{
			/* Use the vfs_rmdir function to do the dirty work. */

		ret_code = vfs_rmdir(inode, tmp_dent);

		dput(tmp_dent);
	}
#else
	IMARK(inode);
	ret = inode->i_op->rmdir(inode, name, len);
#endif

	UP(&(inode->i_sem));

	return	ret_code;
}



/**
 ** FUNCTION: do_lookup
 ** FUNCTION: do_lookup2
 ** FUNCTION: do_lookup3
 **
 **  PURPOSE: Call the inode's lookup operation with the given arguments.
 **
 ** RULES:
 **	- Does not consume the inode given.
 **	- Does produce the resulting inode/dentry.
 **/

#if USE_DENTRY_F

int	do_lookup3 (struct dentry *dent, const char *name, int len,
		    struct dentry **result, int lock_f)
{
	struct dentry	*r_dent;
	int		ret_code;

	if ( lock_f )
		DOWN(&(dent->d_inode->i_sem));


		/* Perform the lookup; this will call the filesystem's */
		/*  lookup entry-point, if needed.                     */

	r_dent = lookup_one_len(name, dent, len);

	if ( IS_ERR(r_dent) )
	{
		ret_code = PTR_ERR(r_dent);
	}
	else if ( r_dent == NULL )
	{
		ret_code = -ENOENT;
	}
	else if ( r_dent->d_inode == NULL )
	{
		dput(r_dent);
		ret_code = -ENOENT;
	}
	else
	{
		ret_code = 0;

		result[0] = r_dent;
	}

	if ( lock_f )
		UP(&(dent->d_inode->i_sem));

	return	ret_code;
}

int	do_lookup2 (struct inode *inode, const char *name, int len,
		    struct dentry **result, int lock_f)
{
	struct dentry	*tmp_dent;
	struct dentry	*r_dent;
	int		ret_code;

	if ( lock_f )
		DOWN(&(inode->i_sem));


		/* Grab a dentry for the directory. */

	tmp_dent = ovlfs_inode2dentry(inode);

	if ( tmp_dent == NULL )
	{
		WARN("failed to obtain dentry for dir inode %lu",
		      inode->i_ino);

		return	-ENOENT;
	}

		/* Perform the lookup; this will call the filesystem's */
		/*  lookup entry-point, if needed.                     */

	r_dent = lookup_one_len(name, tmp_dent, len);

	if ( IS_ERR(r_dent) )
	{
		ret_code = PTR_ERR(r_dent);
	}
	else if ( r_dent == NULL )
	{
		ret_code = -ENOENT;
	}
	else if ( r_dent->d_inode == NULL )
	{
		dput(r_dent);
		ret_code = -ENOENT;
	}
	else
	{
		ret_code = 0;

		result[0] = r_dent;
	}

	dput(tmp_dent);

	if ( lock_f )
		UP(&(inode->i_sem));

	return	ret_code;
}

#endif

int	do_lookup (struct inode *inode, const char *name, int len,
		   struct inode **result, int lock_f)
{
#if POST_20_KERNEL_F
	struct dentry	*tmp_dent;
	struct dentry	*r_dent;
#endif
	int	ret_code;

	if ( lock_f )
		DOWN(&(inode->i_sem));

#if POST_20_KERNEL_F

		/* Grab a dentry for the directory. */

	tmp_dent = ovlfs_inode2dentry(inode);

	if ( tmp_dent == NULL )
	{
		WARN("failed to obtain dentry for dir inode %lu",
		      inode->i_ino);

		return	-ENOENT;
	}

		/* Perform the lookup; this will call the filesystem's */
		/*  lookup entry-point, if needed.                     */

	r_dent = lookup_one_len(name, tmp_dent, len);

	if ( IS_ERR(r_dent) )
	{
		ret_code = PTR_ERR(r_dent);
	}
	else if ( r_dent == NULL )
	{
		ret_code = -ENOENT;
	}
	else if ( r_dent->d_inode == NULL )
	{
		dput(r_dent);
		ret_code = -ENOENT;
	}
	else
	{
		ret_code = 0;

			/* Update the inode's use count since the */
			/*  dput() will release it.               */

		IMARK(r_dent->d_inode);

		result[0] = r_dent->d_inode;

		dput(r_dent);
	}

	dput(tmp_dent);

#else

	IMARK(inode);
	ret_code = inode->i_op->lookup(inode, name, len, result);

#endif

	if ( lock_f )
		UP(&(inode->i_sem));

	return	ret_code;
}



/**
 ** FUNCTION: do_rename
 **
 **  PURPOSE: Call the inode's rename operation with the given arguments.
 **/

int	do_rename (struct inode *olddir, const char *oname, int olen,
	           struct inode *newdir, const char *nname, int nlen,
	           int must_be_dir)
{
#if POST_20_KERNEL_F
	struct dentry	*old_dent;
	struct dentry	*new_dent;
#endif
	int	ret;

#if POST_20_KERNEL_F

	ret = ovlfs_inode_get_child_dentry(olddir, oname, olen, &old_dent,
	                                   OVLFS_DENT_GET_POSITIVE);

	if ( ret == 0 )
	{
			/* Get a dentry for the target; note that this may */
			/*  be a positive or negative dentry.              */

		ret = ovlfs_inode_get_child_dentry(newdir, nname, nlen,
		                                   &new_dent,
		                                   OVLFS_DENT_GET_ANY);

		if ( ret != 0 )
			dput(old_dent);
	}

	if ( ret == 0 )
	{
		ret = vfs_rename(olddir, old_dent, newdir, new_dent);

		dput(old_dent);
		dput(new_dent);
	}
#else
	DOWN(&(newdir->i_sem));
	IMARK(olddir);
	IMARK(newdir);
	ret = o_olddir->i_op->rename(o_olddir, oname, olen,
	                             o_newdir, nname, nlen, must_be_dir);
	UP(&(newdir->i_sem));
#endif

	return	ret;
}



/**
 ** FUNCTION: do_symlink
 **
 **  PURPOSE: Call the given inode's symlink function and return the result.
 **
 ** NOTES:
 **	- This function performs kernel-version-specific handling of the create
 **	  entry point.
 **/

int	do_symlink (struct inode *dir_i, const char *name, int len,
	            const char *link_tgt)
{
#if USE_DENTRY_F
	struct dentry	*dent;
#endif
	int	ret;

	ret = 0;

	DOWN(&(dir_i->i_sem));

#if USE_DENTRY_F

	ret = ovlfs_inode_get_child_dentry(dir_i, name, len, &dent,
	                                   OVLFS_DENT_GET_NEGATIVE);

	if ( ret == 0 )
	{
			/* Create the entry using vfs_create to do all the */
			/*  "dirty work".                                  */

		ret = vfs_symlink(dir_i, dent, link_tgt);

		dput(dent);
	}

#else
	IMARK(dir_i);
        ret = dir_i->i_op->symlink(dir_i, name, len, sym_name);
#endif

	UP(&(dir_i->i_sem));

	return	ret;
}



/**
 ** FUNCTION: do_readlink
 **
 **  PURPOSE: Call the given inode's readlink function and return the result.
 **
 ** NOTES:
 **	- This function performs kernel-version-specific handling of the create
 **	  entry point.
 **/

#if POST_20_KERNEL_F

int	do_readlink (struct dentry *dent, char *buf, int size)
{
	int	ret;

	ret = 0;

	DOWN(&(dent->d_inode->i_sem));

	if ( dent == NULL )
		ret = -ENOENT;
	else
		ret = dent->d_inode->i_op->readlink(dent, buf, size);

	UP(&(dent->d_inode->i_sem));

	return	ret;
}

#else

int	do_readlink (struct inode *inode, char *buf, int size)
{
	int	ret;

	ret = 0;

	DOWN(&(inode->i_sem));

	IMARK(inode);
	ret = inode->i_op->readlink(inode, buf, size);

	UP(&(inode->i_sem));

	return	ret;
}

#endif



/**
 ** FUNCTION: do_mknod
 **
 **  PURPOSE: Call the inode's mknod operation with the given arguments.
 **/

int	do_mknod (struct inode *inode, const char *name, int len, int mode,
	          int dev)
{
#if USE_DENTRY_F
	struct dentry	*tmp_dent;
#endif
	int		ret_code;

	DOWN(&(inode->i_sem));

#if USE_DENTRY_F
		/* Create a temporary negative dentry for the target. */

	ret_code = ovlfs_inode_get_child_dentry(inode, name, len, &tmp_dent,
	                                        OVLFS_DENT_GET_NEGATIVE);

	if ( ret_code == 0 )
	{
			/* Use the vfs_mknod function to do the dirty work. */

		ret_code = vfs_mknod(inode, tmp_dent, mode, dev);

		dput(tmp_dent);
	}
#else
	IMARK(inode);
	ret_code = inode->i_op->mknod(inode, name, len, data->mode, data->dev);
#endif

	UP(&(inode->i_sem));

	return	ret_code;
}



/**
 ** FUNCTION: do_create
 **
 **  PURPOSE: Call the given inode's create function and return the result.
 **
 ** NOTES:
 **	- This function performs kernel-version-specific handling of the create
 **	  entry point.
 **
 ** RULES:
 **	- The directory semaphore (i_sem) is locked on entry and unlocked on
 **	  return.
 **/

#if POST_20_KERNEL_F

int	do_create (struct inode *dir_i, const char *name, int len, int mode,
                   struct dentry **r_dent)
{
	struct dentry	*dent;
	int	ret;

	ret = 0;

	DOWN(&(dir_i->i_sem));

	ret = ovlfs_inode_get_child_dentry(dir_i, name, len, &dent,
	                                   OVLFS_DENT_GET_NEGATIVE);

	if ( ret == 0 )
	{
			/* Create the entry using vfs_create to do all the */
			/*  "dirty work".                                  */

		ret = vfs_create(dir_i, dent, mode);

		if ( ret == 0 )
		{
			if ( dent->d_inode == NULL )
				ret = -ENOENT;
			else
				r_dent[0] = dent;
		}
	}

	UP(&(dir_i->i_sem));

	return	ret;
}

#else
	/* Pre-2.2 version */

int	do_create (struct inode *dir_i, const char *name, int len, int mode,
                   struct inode **r_inode)
{
	int	ret;

	ret = 0;

	DOWN(&(dir_i->i_sem));

	IMARK(dir_i);
	ret = dir_i->i_op->create(dir_i, name, len, ref->i_mode, r_inode);

	UP(&(dir_i->i_sem));

	return	ret;
}

#endif
