/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_dcache.c
 **
 ** DESCRIPTION:
 **	Directory cache functions for use by the Overlay Filesystem.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-02-22	ARTHUR N.	Initial Release
 ** 2003-02-24	ARTHUR N.	Added ovlfs_inode2parent().
 ** 2003-02-28	ARTHUR N.	Corrected cleanup on failed path_walk().
 ** 2003-03-14	ARTHUR N.	Replaced ovlfs_inode_get_child_dentry() and
 **				 ovlfs_inode_lookup_child_dentry() with a
 **				 unified version.
 ** 2003-06-09	ARTHUR N.	Corrected module version support.
 ** 2003-07-19	ARTHUR N.	Removed unused, and incorrect
 **				 inode_get_named_dent().
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_dcache.c	1.6	[03/07/27 22:20:34]"

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "ovlfs.ver"
# endif
#endif

#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include "ovl_debug.h"
#include "ovl_fs.h"
#include "ovl_inlines.h"

#if defined(KERNEL_VERSION) && ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0) )

/**
 ** FUNCTION: ovlfs_inode2dentry
 **
 **  PURPOSE: Given an inode, find an existing dentry for the inode.
 **
 ** NOTES:
 **	- This is essentially the same as d_find_alias() with the added check
 **	  for root inodes of filesystems.
 **
 ** RULES:
 **	- Produces the dentry.
 **/

struct dentry	*ovlfs_inode2dentry (struct inode *inode)
{
	struct dentry	*result;

	DPRINTC2("ino %lu\n", inode->i_ino);

	if ( ( inode->i_sb != NULL ) && ( inode->i_sb->s_root != NULL ) &&
	     ( inode->i_sb->s_root->d_inode == inode ) )
	{
		result = dget(inode->i_sb->s_root);
	}
	else
	{
		result = d_find_alias(inode);
	}

	DPRINTR2("ino %lu; ret = 0x%08x\n", inode->i_ino, (int) result);

	return	result;
}



/**
 ** FUNCTION: ovlfs_inode2parent
 **
 **  PURPOSE: Given an inode, get the parent dentry for the inode.
 **/

struct dentry	*ovlfs_inode2parent (struct inode *inode)
{
	struct dentry	*this_dent;
	struct dentry	*result;

	DPRINTC2("ino %lu\n", inode->i_ino);

		/* Get the dentry for the given inode and read the parent */
		/*  from there...                                         */

	this_dent = ovlfs_inode2dentry(inode);

	if ( this_dent == NULL )
	{
		result = NULL;
		goto	ovlfs_inode2parent__out;
	}

	ASSERT(this_dent->d_parent != NULL);

	result = dget(this_dent->d_parent);
	dput(this_dent);

ovlfs_inode2parent__out:

	DPRINTR2("ino %lu; result = 0x%08x\n", inode->i_ino, (int) result);

	return	result;
}



/**
 ** FUNCTION: ovlfs_dentry_follow_down
 **
 **  PURPOSE: Given a dentry that is a mount-point, find the dentry mounted on
 **           top of it.
 **
 ** NOTES:
 **	- If no mount root is found on top of the given dentry, the dentry
 **	  itself is returned.
 **
 ** RULES:
 **	- This function produces the dentry it returns; the caller must make
 **	  sure to dput() it when done with it.  This is only true on success.
 **
 **	- If the ovlfs_sb is specified, mounts will NOT be followed through
 **	  inodes belonging to that super block.  This prevents problems, such
 **	  as infinite recursions, when the ovlfs mount point is a sub-directory
 **	  of one of the base or storage roots.
 **/

int	ovlfs_dentry_follow_down (struct dentry *dent,
	                          struct dentry **r_mounted,
	                          struct super_block *ovlfs_sb)
{
	struct dentry	*result;
	struct vfsmount	*start_mnt;
	struct vfsmount	*cur_mnt;

	DPRINTC2("dent 0x%08x, r_mounted 0x%08d, ovlfs_sb 0x%08x\n",
	         (int) dent, (int) r_mounted, (int) ovlfs_sb);

		/* Scan the vfsmount list until no match is found for the   */
		/*  current dentry, or a mount root is found which does not */
		/*  have another filesystem mounted on it.                  */

	spin_lock(&dcache_lock);

	start_mnt = current->fs->rootmnt;
	cur_mnt   = start_mnt;
	result    = dent;

	do
	{
			/* If the mount point of the current vfsmount is  */
			/*  the current dentry, update.  But, only if the */
			/*  mounted root does not belong to the specified */
			/*  super block.                                  */

		if ( ( cur_mnt->mnt_mountpoint == result ) &&
		     ( cur_mnt->mnt_root->d_sb != ovlfs_sb) )
		{
				/* Continue with the root of this mount.    */
				/*  Reset the "start mount" so that further */
				/*  mounts on this dentry, earlier in the   */
				/*  list are not missed.                    */

			result    = cur_mnt->mnt_root;
			start_mnt = cur_mnt;

			ASSERT(result != NULL);
		}

			/* Go to the next mount. */

		cur_mnt = list_entry(cur_mnt->mnt_list.next,
				     struct vfsmount, mnt_list);
	} while ( ( cur_mnt != start_mnt ) && ( result->d_mounted ) );

	spin_unlock(&dcache_lock);	/* See dget() */

	r_mounted[0] = dget(result);

	DPRINTR2("dent 0x%08x, r_mounted 0x%08d, ovlfs_sb 0x%08x; ret = 0\n",
	         (int) dent, (int) r_mounted, (int) ovlfs_sb);

	return	0;
}



/**
 ** FUNCTION: ovlfs_inode_get_child_dentry
 **
 **  PURPOSE: Given a directory inode and the name of an entry in the
 **           directory, return a positive dentry for the directory entry.
 **
 ** NOTES:
 **	- A negative dentry will only be returned when OVLFS_DENT_GET_NEGATIVE
 **	  is specified in flags.
 **
 **	- A postivie dentry will only be returned when OVLFS_DENT_GET_POSITIVE
 **	  is specified in flags.
 **
 **	- All information pertains to a single filesystem.
 **/

int	ovlfs_inode_get_child_dentry (struct inode *inode, const char *name,
	                              int name_len, struct dentry **r_dent,
	                              int flags)
{
	struct dentry	*d_ent;
	struct dentry	*this_dent;
	int		ret_code;

	DPRINTC2("ino %lu, name %.*s, len %d, r_dent 0x%08x\n", inode->i_ino,
	         name_len, name, name_len, (int) r_dent);

		/* Find a DENT for the inode. */

	this_dent = ovlfs_inode2dentry(inode);

	if ( this_dent == NULL )
		return	-ENOENT;


		/* Lookup the dentry. */

	d_ent = lookup_one_len(name, this_dent, name_len);

	if ( d_ent == NULL )
		ret_code = -ENOENT;
	else if ( IS_ERR(d_ent) )
		ret_code = PTR_ERR(d_ent);
	else if ( ( ! ( flags & OVLFS_DENT_GET_POSITIVE ) ) &&
		  ( d_ent->d_inode != NULL ) )
	{
			/* Positive dentry which is not wanted. */

		dput(d_ent);
		ret_code = -EEXIST;
	}
	else if ( ( ! ( flags & OVLFS_DENT_GET_NEGATIVE ) ) &&
		  ( d_ent->d_inode == NULL ) )
	{
			/* Negative dentry which is not wanted. */

		dput(d_ent);
		ret_code = -ENOENT;
	}
	else
	{
		r_dent[0] = d_ent;
		ret_code  = 0;
	}

		/* "Unget" the DENT for the ovlfs inode. */

	dput(this_dent);

	DPRINTR2("ino %lu, name %.*s, len %d, r_dent 0x%08x; ret = %d\n",
	         inode->i_ino, name_len, name, name_len, (int) r_dent,
	         ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: get_dentry_mode
 ** FUNCTION: get_inode_mode
 **/

int	get_dentry_mode (const char *name, int mode, struct dentry **r_dent)
{
	struct nameidata	nd;
	int			rc;
	int			ret_code;

	DPRINTC2("name %s, mode 0%o, r_dent 0x%08x\n", name, mode,
	         (int) r_dent);

		/* path_init() returns 1 on "normal" and 0 to mean "done" */
		/* path_walk() calls path_release() on failure (ugh).     */

	rc = path_init(name, LOOKUP_FOLLOW | LOOKUP_POSITIVE, &nd);

        if ( rc == 0 )
		ret_code = 0;
	else
		ret_code = path_walk(name, &nd);

	if ( ret_code == 0 )
	{
			/* Keep the dentry and mark it, then release the   */
			/*  path information structure, which will release */
			/*  the dentry.                                    */

		r_dent[0] = dget(nd.dentry);
		path_release(&nd);
	}

	DPRINTR2("name %s, mode 0%o, r_dent 0x%08x; ret = %d\n", name, mode,
	         (int) r_dent, ret_code);

	return	ret_code;
}

int	get_inode_mode (const char *name, int mode, struct inode **r_inode)
{
	struct nameidata	nd;
	int			rc;
	int			ret_code;

	DPRINTC2("name %s, mode 0%o, r_inode 0x%08x\n", name, mode,
	         (int) r_inode);

		/* path_init() returns 1 on "normal" and 0 to mean "done" */
		/* path_walk() calls path_release() on failure (ugh).     */

	rc = path_init(name, LOOKUP_FOLLOW | LOOKUP_POSITIVE, &nd);

        if ( rc == 0 )
		ret_code = 0;
	else
		ret_code = path_walk(name, &nd);

	if ( ret_code == 0 )
	{
			/* Keep the inode and mark it, then release the    */
			/*  path information structure, which will release */
			/*  the dentry, and the inode in-turn.             */

		r_inode[0] = nd.dentry->d_inode;
		IMARK(r_inode[0]);
		path_release(&nd);
	}

	DPRINTR2("name %s, mode 0%o, r_inode 0x%08x; ret = %d\n", name, mode,
	         (int) r_inode, ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: get_inode_vfsmount
 **
 **  PURPOSE: Given an inode, find the vfsmount for it.
 **/

int	get_inode_vfsmount (struct inode *inode, struct vfsmount **r_mnt)
{
	struct vfsmount		*start_mnt;
	struct vfsmount		*cur_mnt;
	struct vfsmount		*result;
	int			rc;

	DPRINTC2("inode at 0x%08x, i_dev %d, i_ino %lu, r_mnt at 0x%08x\n",
	         (int) inode, inode->i_dev, inode->i_ino, (int) r_mnt);

		/* Start with the root directory's vfsmount, then walk */
		/*  through the mnt_list.                              */

	spin_lock(&dcache_lock);

	start_mnt = current->fs->rootmnt;
	cur_mnt   = start_mnt;
	result    = NULL;

	do
	{
			/* If the device of the current mount points's super */
			/*  block matches the inode's device, keep it.       */

		if ( ( cur_mnt->mnt_sb != NULL ) &&
		     ( cur_mnt->mnt_sb->s_dev == inode->i_dev ) )
		{
			result = cur_mnt;
		}
		else
		{
			cur_mnt = list_entry(cur_mnt->mnt_list.next,
			                     struct vfsmount, mnt_list);
		}
	} while ( ( cur_mnt != start_mnt ) && ( result == NULL ) );

	rc = 0;
	if ( result != NULL )
		r_mnt[0] = mntget(result);
	else
		rc = -ENOENT;

	spin_unlock(&dcache_lock);

	DPRINTR2("inode at 0x%08x, i_dev %d, i_ino %lu, r_mnt at 0x%08x; "
	         "result = 0x%08x, rc = %d\n", (int) inode, inode->i_dev,
	         inode->i_ino, (int) r_mnt, (int) result, rc);

	return	rc;
}

#endif
