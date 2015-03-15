/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_ref.c
 **
 ** DESCRIPTION:
 **	Code used to access inode/dentry references for the Overlay
 **	Filesystem's inodes.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-03-11	ARTHUR N.	Created from ovl_ino.c.
 ** 2003-03-11	ARTHUR N.	Moved inode_refs_valid_p() here.
 ** 2003-03-13	ARTHUR N.	Corrected successful return value for
 **				 do_get_ref_inode().
 ** 2003-03-13	ARTHUR N.	Allow any user to make a directory hierarchy
 **				 regardless of permissions.
 ** 2003-07-25	ARTHUR N.	Use the storage and base filesystems' own
 **				 dcache hash methods, when defined.
 ** 2003-07-25	ARTHUR N.	Force removal of bad inodes from the inode
 **				 cache.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_ref.c	1.7	[03/07/27 22:20:35]"

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
#include <linux/slab.h>
#include <linux/fs.h>

#include "ovl_debug.h"
#include "ovl_fs.h"
#include "ovl_inlines.h"

#if PROC_DONT_IGET_F
# include <linux/proc_fs.h>
#endif



/**
 ** FUNCTION: do_iget
 **
 **  PURPOSE: Given an inode number and super block, retrieve the VFS inode.
 **
 ** NOTES:
 **	- The cross_mnt_f argument is only used for kernel version 2.0.
 **/

struct inode	*do_iget (struct super_block *sb, _ulong inum, int cross_mnt_f) 
{
	struct inode	*result;

		/* As a special case (thanks to devfs), check whether the */
		/*  inode specified is the super block's root, and just   */
		/*  return it if so.                                      */

#if POST_20_KERNEL_F
		/* Check for NULL s_root and d_inode since this happens */
		/*  at filesystem mount time.                           */

	if ( ( sb->s_root != NULL ) && ( sb->s_root->d_inode != NULL ) &&
	     ( sb->s_root->d_inode->i_ino == inum ) )
	{
		result = sb->s_root->d_inode;
		IMARK(result);
		goto	do_iget__out;
	}
#else
	if ( sb->s_mounted->i_ino == inum )
	{
		result = sb->s_mounted;
		IMARK(result);
		goto	do_iget__out;
	}
#endif

	result = NULL;

#if PROC_DONT_IGET_F
		/* As another special case (thanks to procfs), always return */
		/*  NULL at this point for proc filesystems.  The procfs     */
		/*  does nothing of real value in its read_inode() entry pt, */
		/*  and instead does all of the real work in lookup().       */

	if ( sb->s_magic == PROC_SUPER_MAGIC )
		goto	do_iget__out;
#endif

		/* Um, talk about interfaces that are not clean... */

		/* Check that the super block's read inode entry point is */
		/*  set and return an error if not.  This is needed under */
		/*  2.4 kernels with devfs (yuck).  It may also help with */
		/*  reiserfs.                                             */

	if ( sb->s_op->read_inode == NULL )
		goto	do_iget__out;

#if POST_20_KERNEL_F
# warning "this will not support reiserfs (read_inode2 argument missing)"

	result = iget4(sb, inum, NULL, NULL);
#else
	result = __iget(sb, inum, ovlfs_sb_xmnt(inode->i_sb));
#endif

		/* Check for bad inodes and don't return them. */

	if ( is_bad_inode(result) )
	{
			/* Force the inode to be removed from the cache;  */
			/*  do this before iput - otherwise, it won't     */
			/*  work.  Also, the inode will remain when it is */
			/*  still in use elsewhere. (how, I don't know..) */

		force_delete(result);
		IPUT(result);
		result = NULL;
	}

do_iget__out:

	return	result;
}



#if POST_20_KERNEL_F

/**
 ** FUNCTION: ovlfs_attach_reference
 **
 **  PURPOSE: Attach the given reference to the ovlfs inode.
 **/

int	ovlfs_attach_reference (struct inode *inode, struct dentry *ref_dent,
	                        char which, int flags)
{
	ovlfs_inode_info_t	*i_info;
	struct inode		*ref_inode;
	int			map_opt;
	int			stg_opts;
	int			ret_code;

	ref_inode = ref_dent->d_inode;
	ASSERT(ref_inode != NULL);

	DPRINTC2("ino %lu, dev 0x%02x, ino %lu, which %c, flags 0x%x\n",
	         inode->i_ino, ref_inode->i_dev, ref_inode->i_ino, which,
	         flags);

	ret_code = 0;

	i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;

	ASSERT(i_info != NULL);

	stg_opts = ovlfs_sb_opt(inode->i_sb, storage_opts);

	switch ( which )
	{
	    case 'b':
		if ( ( ! (flags & OVLFS_NO_REF_ID_UPD_F) ) &&
		     ( ! (stg_opts & OVLFS_NO_REF_UPD) ) )
		{
			if ( ( i_info->s.base_dev != ref_inode->i_dev ) ||
			     ( i_info->s.base_ino != ref_inode->i_ino ) )
			{
				i_info->s.base_dev = ref_inode->i_dev;
				i_info->s.base_ino = ref_inode->i_ino;

				/* TBD: only mark the inode itself dirty, */
				/*      and not its data.                 */
				INODE_MARK_DIRTY(inode);
			}
		}

		if ( ( ! (flags & OVLFS_NO_REF_ATTACH_F) ) &&
		     ( ! (stg_opts & OVLFS_NO_STORE_REFS) ) )
		{
			if ( i_info->base_dent != ref_dent )
			{
					/* TBD: consider locking issues */
				if ( i_info->base_dent != NULL )
					dput(i_info->base_dent);
				i_info->base_dent = dget(ref_dent);
			}
		}

		map_opt = OVLFS_STORE_BASE_MAP;
		break;

	    case 'o':
	    case 's':
		if ( ( ! (flags & OVLFS_NO_REF_ID_UPD_F) ) &&
		     ( ! (stg_opts & OVLFS_NO_REF_UPD) ) )
		{
			if ( ( i_info->s.stg_dev != ref_inode->i_dev ) ||
			     ( i_info->s.stg_ino != ref_inode->i_ino ) )
			{
				i_info->s.stg_dev = ref_inode->i_dev;
				i_info->s.stg_ino = ref_inode->i_ino;

				/* TBD: only mark the inode itself dirty, */
				/*      and not its data.                 */
				INODE_MARK_DIRTY(inode);
			}
		}

		if ( ( ! (flags & OVLFS_NO_REF_ATTACH_F) ) &&
		     ( ! (stg_opts & OVLFS_NO_STORE_REFS) ) )
		{
			if ( i_info->overlay_dent != ref_dent )
			{
					/* TBD: consider locking issues */
				if ( i_info->overlay_dent != NULL )
					dput(i_info->overlay_dent);
				i_info->overlay_dent = dget(ref_dent);
			}
		}

		map_opt = OVLFS_STORE_STG_MAP;
		break;

	    default:
		ret_code = -EINVAL;
		goto	ovlfs_attach_reference__out;
		break;
	}


		/* Add the reference inode mapping back to the ovlfs inode. */

	if ( ( ! ( flags & OVLFS_NO_REF_MAP_F ) ) && ( stg_opts & map_opt ) )
	{
		ret_code = do_map_inode(inode->i_sb, inode->i_ino,
		                        ref_inode->i_dev, ref_inode->i_ino,
		                        which);
	}

ovlfs_attach_reference__out:
	DPRINTR2("ino %lu, dev 0x%02x, ino %lu, which %c, flags 0x%x; "
	         "ret = %d\n", inode->i_ino, ref_inode->i_dev,
	         ref_inode->i_ino, which, flags, ret_code);

	return	ret_code;
}

#endif



/**
 ** FUNCTION: set_attrib
 **
 **  PURPOSE: Given an inode that is in the storage filesystem, update the
 **           inode's attributes to match the pseudo filesystem inode's
 **           attributes.
 **
 ** NOTES:
 **	- This method is inteded for the operation of the overlay filesystem;
 **	  it should not be used as a general purpose attribute update method.
 **/

static int set_attrib (struct inode *inode, struct inode *ref)
{
    struct iattr    changes;
#if POST_20_KERNEL_F
    struct dentry   *d_ent;
    int             rc;
#endif

    if ( ( inode == INODE_NULL ) || ( ref == INODE_NULL ) )
        return  -EINVAL;

#if POST_20_KERNEL_F

    d_ent = ovlfs_inode2dentry(inode);

    if ( d_ent == NULL )
        return  -ENOENT;

    if ( ( inode->i_op != NULL ) && ( inode->i_op->setattr != NULL ) )
#else
        /* If the inode's super block contains the notify_change()    */
        /*  function, that will be called to make the actual changes; */
        /*  otherwise, just make the changes directly to the inode.   */

    if ( ( inode->i_sb != NULL ) && ( inode->i_sb->s_op != NULL ) &&
         ( inode->i_sb->s_op->notify_change != NULL ) )
#endif
    {
            /* Only update those attributes that do not match */

        memset(&changes, '\0', sizeof(changes));

        if ( inode->i_mode != ref->i_mode )
        {
            changes.ia_mode = ref->i_mode;
            changes.ia_valid |= ATTR_MODE;
        }

        if ( inode->i_uid != ref->i_uid )
        {
            changes.ia_uid = ref->i_uid;
            changes.ia_valid |= ATTR_UID;
        }
    
        if ( inode->i_gid != ref->i_gid )
        {
            changes.ia_gid = ref->i_gid;
            changes.ia_valid |= ATTR_GID;
        }
    
        if ( inode->i_atime != ref->i_atime )
        {
            changes.ia_atime = ref->i_atime;
            changes.ia_valid |= ATTR_ATIME;
        }
    
        if ( inode->i_ctime != ref->i_ctime )
        {
            changes.ia_ctime = ref->i_ctime;
            changes.ia_valid |= ATTR_CTIME;
        }
    
        if ( inode->i_mtime != ref->i_mtime )
        {
            changes.ia_mtime = ref->i_mtime;
            changes.ia_valid |= ATTR_MTIME;
        }
    
        if ( changes.ia_valid != 0 )
        {
#if POST_20_KERNEL_F
            rc = inode->i_op->setattr(d_ent, &changes);

            dput(d_ent);

            return  rc;
#else
            return  inode->i_sb->s_op->notify_change(inode, &changes);
#endif
        }
    }
    else
    {
        inode->i_mode  = ref->i_mode;
        inode->i_uid   = ref->i_uid;
        inode->i_gid   = ref->i_gid;
        inode->i_atime = ref->i_atime;
        inode->i_ctime = ref->i_ctime;
        inode->i_mtime = ref->i_mtime;

            /* Mark the inode as dirty so that it will be written */

        INODE_MARK_DIRTY(inode);
    }

    return  0;
}



#if PRE_22_KERNEL_F

/**
 ** FUNCTION: ovlfs_get_ref_inode
 **
 **  PURPOSE: Get the reference inode, in the requested filesystem for the
 **           given ovlfs inode, from its device and inode number.
 **
 ** NOTES:
 **	- This function ONLY calls iget() to retrieve the inode; it does NOT
 **	  lookup the inode from the parent.
 **/

static struct inode *ovlfs_get_ref_inode (struct inode *inode, char which)
{
    ovlfs_inode_info_t  *i_info;
    struct inode        *result;
    kdev_t              s_dev = NODEV;
    _ulong              inum = 0;
    struct super_block  *sb;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_get_ref_inode: looking for inode in fs "
             "'%c'\n", which));

#if ! FASTER_AND_MORE_DANGEROUS

    if ( ( inode == INODE_NULL ) || ( ! is_ovlfs_inode(inode) ) )
    {
        DPRINT1(("OVLFS: ovlfs_get_ref_inode: invalid inode given at 0x%x\n",
                 (int) inode));

        return  INODE_NULL;
    }

    if ( inode->u.generic_ip == NULL )
    {
        DPRINT1(("OVLFS: ovlfs_get_ref_inode: inode #%ld, dev 0x%x has null "
                 "generic ip pointer!\n", inode->i_ino, inode->i_dev));

        return  INODE_NULL;
    }

#endif

    i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;

    switch ( which )
    {
        case 'b':
            s_dev = i_info->s.base_dev;
            inum  = i_info->s.base_ino;
            break;

        case 'o':
        case 's':
            s_dev = i_info->s.stg_dev;
            inum  = i_info->s.stg_ino;
            break;

        default:
            DPRINT1(("OVLFS: ovlfs_get_ref_inode: invalid value for which:"
                     " '%c'\n", which));
            s_dev = NODEV;
            break;
    }

        /* Catch inode references to our own filesystem to prevent */
        /*  infinite recursions.                                   */

    if ( s_dev == inode->i_dev )
    {
        DPRINT2(("OVLFS: ovlfs_get_ref_inode: referenced inode is in our "
                 "filesystem!\n"));

        return INODE_NULL;
    }


        /* If a device and inode number were found, retrieve the inode */

    if ( s_dev != NODEV )
    {
            /* Obtain the super block for the device */

            /* Produce the super block. */

        sb = get_super(s_dev);

        if ( sb != (struct super_block *) NULL )
        {
            result = do_iget(sb, inum, ovlfs_sb_xmnt(inode->i_sb));

                /* As a sanity check, make sure the inode is not an */
                /*  unlinked directory.                             */

            if ( ( result != INODE_NULL ) && ( S_ISDIR(result->i_mode) ) &&
                 ( result->i_nlink == 0 ) )
            {
                IPUT(result);    /* This may cause meaningless err msgs */
                result = INODE_NULL;
            }

               /* Consume the super block now. */
            drop_super(sb);
        }
        else
        {
            DPRINT1(("OVLFS: ovlfs_get_ref_inode: unable to obtain super "
                     "block for device %d\n", (int) s_dev));

            result = INODE_NULL;
        }
    }
    else
        result = INODE_NULL;

    return  result;
}

#endif




/**
 ** FUNCTION: attach_ref_hist
 **
 **  PURPOSE: Attach a new inode to the reference history given.
 **
 ** RULES:
 **	- The inode given must be the ancestor of the last entry added.
 **	- This function "produces" the inode given.
 **/

static int	attach_ref_hist (ref_hist_t **history, struct inode *inode)
{
	ref_hist_t	*new_hist;
	int		ret_code;

	DPRINTC2("history 0x%08x\n", (int) history[0]);

	ret_code = 0;

		/* Allocate memory for the new history element. */

	new_hist = MALLOC(sizeof new_hist[0]);

	if ( new_hist == NULL )
	{
		ret_code = -ENOMEM;
		goto	attach_ref_hist__out;
	}

		/* Remember the inode */
	new_hist->inode = inode;
	IMARK(inode);

		/* Link this in at the start of the list. */
	new_hist->descendent = history[0];
	history[0] = new_hist;

attach_ref_hist__out:

	DPRINTR2("history 0x%08x; ret = %d\n", (int) history[0], ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: release_history
 **/

static void	release_history (ref_hist_t *history)
{
	ref_hist_t	*cur;

		/* Loop until the end of the history is found, releasing */
		/*  each list entry as we go.                            */

	while ( history != NULL )
	{
		cur = history;
		history = history->descendent;

		if ( cur->inode != NULL )
		{
			IPUT(cur->inode);
			cur->inode = NULL;
		}

		FREE(cur);
	}
}



/**
 ** FUNCTION: init_walk_info
 **
 **  PURPOSE: Initialize the walk_info structure given for use.
 **/

static void	init_walk_info (walk_info_t *w_info, struct inode *inode,
		                char which, int flags)
{
	w_info->inode        = inode;
	w_info->which        = which;
	w_info->cur_ancestor = NULL;
	w_info->history      = NULL;
	w_info->flags        = flags;
}



/**
 ** FUNCTION: release_walk_info
 **
 **  PURPOSE: Release the reference walk information structure given.
 **/

static void	release_walk_info (walk_info_t *w_info)
{
	w_info->inode = NULL;
	w_info->which = ' ';

	if ( w_info->cur_ancestor != NULL )
	{
		dput(w_info->cur_ancestor);
		w_info->cur_ancestor = NULL;
	}

	release_history(w_info->history);

	w_info->history = NULL;
}



/**
 ** FUNCTION: resolve_mounts
 **
 **  PURPOSE: Follow mount points and validate the result.
 **
 ** RULES:
 **	- The inode given must be an ovlfs inode.
 **	- my_sb must be the ovlfs super block.
 **	- dent must be inode's reference.
 **	- Checks the ovlfs xmnt option and only follows mounts when set.
 **	- Never returns a reference to the ovlfs filesystem.
 **	- Only returns the storage or base root inode when the ovlfs inode is
 **	  the filesystem's root.
 **	- Does not consume the dentry given.
 **	- Produces the dentry returned iff successful.
 **/

static int	resolve_mounts (struct super_block *my_sb, struct inode *inode,
		                struct dentry *dent, struct dentry **r_mnt)
{
	struct dentry	*d_mnt;
	int		ret_code;

	DPRINTC2("my dev %u, ino %lu, dent 0x%08x, r_mnt 0x%08x\n",
	         my_sb->s_dev, inode->i_ino, (int) dent, (int) r_mnt);


		/* Check for a mount-point and the cross-mount-point option. */

	if ( ( ovlfs_sb_xmnt(my_sb) ) && ( dent->d_mounted ) )
	{
			/* Follow down the mounts now. */

		ret_code = ovlfs_dentry_follow_down(dent, &d_mnt, my_sb);

		if ( ret_code < 0 )
			goto	resolve_mounts__out;

			/* Make sure we did not follow the mounts   */
			/*  down to the storage or base fs root     */
			/*  unless we are resolving the ovlfs root. */

		if ( ( inode->i_ino != OVLFS_ROOT_INO ) &&
		     ( is_base_or_stg_root_p(my_sb, d_mnt->d_inode->i_dev,
					     d_mnt->d_inode->i_ino) ) )
		{
			dput(d_mnt);
			ret_code = -EDEADLK;
			goto	resolve_mounts__out;
		}
	}
	else
	{
		d_mnt = dget(dent);
	}

	ret_code = 0;
	r_mnt[0] = d_mnt;

resolve_mounts__out:

	DPRINTR2("my dev %u, ino %lu, dent 0x%08x, r_mnt 0x%08x; ret = %d\n",
	         my_sb->s_dev, inode->i_ino, (int) dent, (int) r_mnt,
	         ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: do_get_ref_dent
 ** FUNCTION: do_get_ref_inode
 **
 **  PURPOSE: Retrieve the reference dentry for the given inode directly from
 **           it's information structure or by its device and inode numbers.
 **
 ** NOTES:
 **	- Does NOT retrieve the inode's parent.
 **
 ** RULES:
 **	- Only returns the storage or base filesystem root inodes for the ovlfs
 **	  root inode.
 **	- Never returns inodes in the ovlfs filesystem.
 **	- Does not cross mount-points.
 **/

static int	do_get_ref_inode (struct inode *inode, struct inode **r_inode,
		                  char which, int flags)
{
	ovlfs_inode_info_t	*i_info;
	struct super_block	*o_sb;
	struct inode		*result;
	struct dentry		*dent;
	kdev_t			dev;
	ino_t			inum;
	int			xmnt;
	int			ret_code;

	DPRINTC2("ino %lu, r_inode 0x%08x, ind %c\n", inode->i_ino,
	         (int) r_inode, which);

	ret_code = -ENOENT;
	i_info   = (ovlfs_inode_info_t *) inode->u.generic_ip;

		/* Check for a disabled reference and just return */
		/*  "no such entry" in this case.                 */

	if ( ( i_info->s.flags & OVL_IFLAG__NO_BASE_REF ) && ( which == 'b' ) )
		goto	do_get_ref_inode__out;

	switch ( which )
	{
	    case 'b':
			/* If the ovlfs inode is the fs root, grab the      */
			/*  result from the super block's info, if possible */

		if ( inode->i_ino == OVLFS_ROOT_INO )
		{
			dent = ovlfs_sb_opt(inode->i_sb, root_ent);

			if ( ( dent != NULL ) && ( dent->d_inode != NULL ) )
			{
				r_inode[0] = dent->d_inode;
				IMARK(r_inode[0]);
				goto	do_get_ref_inode__out;
			}
		}

		if ( ( i_info->base_dent != NULL ) &&
		     ( i_info->base_dent->d_inode != NULL ) )
		{
			r_inode[0] = i_info->base_dent->d_inode;
			IMARK(r_inode[0]);
			goto	do_get_ref_inode__out;
		}

		dev  = i_info->s.base_dev;
		inum = i_info->s.base_ino;
		break;

	    case 'o':
	    case 's':
			/* If the ovlfs inode is the fs root, grab the      */
			/*  result from the super block's info, if possible */

		if ( inode->i_ino == OVLFS_ROOT_INO )
		{
			dent = ovlfs_sb_opt(inode->i_sb, storage_ent);

			if ( ( dent != NULL ) && ( dent->d_inode != NULL ) )
			{
				r_inode[0] = dent->d_inode;
				IMARK(r_inode[0]);
				goto	do_get_ref_inode__out;
			}
		}

		if ( ( i_info->overlay_dent != NULL ) &&
		     ( i_info->overlay_dent->d_inode != NULL ) )
		{
			r_inode[0] = i_info->overlay_dent->d_inode;
			IMARK(r_inode[0]);
			goto	do_get_ref_inode__out;
		}

		dev  = i_info->s.stg_dev;
		inum = i_info->s.stg_ino;
		break;

	    default:
		ret_code = -EINVAL;
		goto	do_get_ref_inode__out;
		break;
	}

		/* Stop if no reference is defined, or it refers to this */
		/*  ovlfs filesystem.                                    */

	ret_code = -ENOENT;
	if ( ( dev == NODEV ) || ( dev == inode->i_dev ) )
		goto	do_get_ref_inode__out;


		/* Don't grab the root inode of the storage or base fs */
		/*  unless the given inode is the ovlfs root.          */

	if ( ( inode->i_ino != OVLFS_ROOT_INO ) &&
	     ( is_base_or_stg_root_p(inode->i_sb, dev, inum) ) )
		goto	do_get_ref_inode__out;


		/* Grab the device's super block */

	o_sb = get_super(dev);
	if ( o_sb == NULL )
		goto	do_get_ref_inode__out;


		/* Grab the inode; note that the xmnt flag only affects */
		/*  pre-2.2 kernels.                                    */

	xmnt = ( flags & OVLFS_FOLLOW_MOUNTS );
	result = do_iget(o_sb, inum, xmnt);
	if ( result == NULL )
		goto	do_get_ref_inode__out_super;

	ret_code = 0;
	r_inode[0] = result;

do_get_ref_inode__out_super:
	drop_super(o_sb);

do_get_ref_inode__out:

	DPRINTR2("ino %lu, r_inode 0x%08x, ind %c; ret = %d\n", inode->i_ino,
	         (int) r_inode, which, ret_code);

	return	ret_code;
}

static int	do_get_ref_dent (struct inode *inode, struct dentry **r_dent,
		                 char which, int flags)
{
	ovlfs_inode_info_t	*i_info;
	struct inode		*o_inode;
	struct dentry		*result;
	struct dentry		*d_mnt;
	int			ret_code;

	DPRINTC2("ino %lu, r_dent 0x%08x, ind %c\n", inode->i_ino,
	         (int) r_dent, which);

	ret_code = -ENOENT;

	i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;

		/* Check for a disabled reference and just return */
		/*  "no such entry" in this case.                 */

	if ( ( i_info->s.flags & OVL_IFLAG__NO_BASE_REF ) && ( which == 'b' ) )
		goto	do_get_ref_dent__out;


		/* First look in the inode's info structure for the */
		/*  reference.                                      */

	switch ( which )
	{
	    case 'b':
		if ( i_info->base_dent != NULL )
		{
			result = dget(i_info->base_dent);
			goto	do_get_ref_dent__mounts;
		}
		break;

	    case 'o':
	    case 's':
		if ( i_info->overlay_dent != NULL )
		{
			result = dget(i_info->overlay_dent);
			goto	do_get_ref_dent__mounts;
		}
		break;
	}

		/* Not there. */

		/* If the ovlfs inode is the fs root, grab the result from   */
		/*  the super block's info, if possible.  Do so here in      */
		/*  addition to the logic in do_get_ref_inode() to avoid the */
		/*  need to go back to the dentry from the inode.            */

	if ( inode->i_ino == OVLFS_ROOT_INO )
	{
		if ( which == 'b' )
			result = ovlfs_sb_opt(inode->i_sb, root_ent);
		else if ( ( which == 'o' ) || ( which == 's' ) )
			result = ovlfs_sb_opt(inode->i_sb, storage_ent);
		else
			result = NULL;


			/* This may be null, in which case do_get_ref_inode */
			/*  will get the result.  But, it shouldn't...      */

		if ( result != NULL )
		{
				/* Produce the dentry. */
			dget(result);
			goto	do_get_ref_dent__mounts;
		}
	}


		/* Either this is not the root, or the reference was not */
		/*  available above.                                     */

		/* Retrieve the reference inode. */

	ret_code = do_get_ref_inode(inode, &o_inode, which, flags);
	if ( ret_code != 0 )
		goto	do_get_ref_dent__out;


		/* Then, obtain the inode's dentry. */

	ret_code = -ENOENT;
	result = ovlfs_inode2dentry(o_inode);
	IPUT(o_inode);
	if ( result == NULL )
		goto	do_get_ref_dent__out;

do_get_ref_dent__mounts:
	if ( flags & OVLFS_FOLLOW_MOUNTS )
	{
			/* Follow mount points; on success, d_mnt has the   */
			/*  end-point.  We always have to consume result.   */
			/*  Also note that resolve_mounts() will not return */
			/*  the base or storage roots, unless inode is the  */
			/*  root, and will not return an ovlfs dentry.      */

		ret_code = resolve_mounts(inode->i_sb, inode, result, &d_mnt);
		dput(result);
		if ( ret_code != 0 )
			goto	do_get_ref_dent__out;

		result = d_mnt;
	}

		/* Success: got the result.  Indicate this and return it. */
	ret_code  = 0;
	r_dent[0] = result;


do_get_ref_dent__out:

	DPRINTR2("ino %lu, r_dent 0x%08x, ind %c; ret = %d\n", inode->i_ino,
	         (int) r_dent, which, ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: find_ancestor
 **
 **  PURPOSE: Find an ancestor, of the given inode in the specified filesystem.
 **
 ** NOTES:
 **	- On failure, the walk structure may contain partial history.
 **
 ** RULES:
 **	- The inode is an ovlfs inode.
 **	- The ancestor refers to the base or storage filesystem.
 **	- The caller must release the walk information in all cases.
 **	- The starting inode must not be the ovlfs root.
 **	- Follows down mount points on the resulting ancestor.
 **/

static int	find_ancestor (walk_info_t *w_info)
{
	struct inode	*cand;
	struct inode	*parent;
	int		rc;
	int		ret_code;

	DPRINTC2("ino %lu\n", w_info->inode->i_ino);

		/* Grab the inode of the parent directory for the current */
		/*  ovlfs inode.  This must be retrieved.                 */

	ret_code = -ENOENT;
	cand = ovlfs_get_parent_dir(w_info->inode);
	if ( cand == NULL )
		goto	find_ancestor__out;


		/* Loop until an ancestor is found, or the root is reached */
		/*  and no ancestor is available.                          */

	ret_code = 0;

	while ( ( w_info->cur_ancestor == NULL ) && ( ret_code == 0 ) )
	{
		parent = NULL;

			/* Try to get the dentry for the reference to the */
			/*  current candidate ancestor.                   */

		ret_code = do_get_ref_dent(cand, &(w_info->cur_ancestor),
		                           w_info->which, OVLFS_FOLLOW_MOUNTS);


			/* If the current candidate is the root of the  */
			/*  ovlfs, and no reference was retrieved, then */
			/*  no ancestor is available.                   */

		if ( ( cand->i_ino != OVLFS_ROOT_INO ) && ( ret_code != 0 ) )
		{
			rc = attach_ref_hist(&(w_info->history), cand);

			if ( rc == 0 )
			{
					/* Continue with the parent of the */
					/*  last candidate.                */

				parent = ovlfs_get_parent_dir(cand);

				if ( parent != NULL )
					ret_code = 0;
			}
			else
			{
				ret_code = rc;
			}
		}

			/* Done with this candidate; if it's in history, */
			/*  the history will have "produced" it, so this */
			/*  is safe and the correct thing to do.         */

		IPUT(cand);
		cand = parent;
	}

	ASSERT(cand == NULL);

find_ancestor__out:

	DPRINTR2("ino %lu; ret = %d\n", w_info->inode->i_ino, ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: get_ref_from_parent
 **
 **  PURPOSE: Obtain the reference dentry for the given inode given its
 **           parent's dentry.
 **
 ** NOTES:
 **	- Only creates directories.
 **
 ** RULES:
 **	- inode is the ovlfs inode whose references is needed.
 **	- parent is the ovlfs dentry of the parent directory of inode.
 **	- Produces the resulting dentry on success.
 **	- Creates missing entries (as directories) as needed, if requested.
 **	- Only returns the storage or base filesystem root inodes for the ovlfs
 **	  root inode.
 **	- Never returns inodes in the ovlfs filesystem.
 **	- Does not cross mount-points. ??? This doesn't seem correct...
 **/

static int	get_ref_from_parent (struct inode *inode,
		                     struct dentry *parent,
		                     struct dentry **r_dent, int which,
		                     int flags)
{
	struct dentry		*result;
	struct dentry		*d_mnt;
	ovlfs_inode_info_t	*i_info;
	struct inode		*o_inode;
	struct qstr		name_str;
	int			mode;
	int			create_f;
	int			rc;
	int			ret_code;

	DPRINTC2("ino %lu, parent 0x%08x, r_dent 0x%08x, flags 0x%x\n",
	         inode->i_ino, (int) parent, (int) r_dent, flags);

	ret_code = -ENOENT;

	i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;

		/* Check for a disabled reference and just return */
		/*  "no such entry" in this case.                 */

	if ( ( i_info->s.flags & OVL_IFLAG__NO_BASE_REF ) && ( which == 'b' ) )
		goto	get_ref_from_parent__out;


		/* Use the following logic only when adding inodes to */
		/*  the dcache manually is "acceptible".              */

#if ! OVLFS_DONT_ADD_TO_DCACHE

		/* Grab the reference inode, if it is defined, then attach  */
		/*  a dentry to it.  Don't try to grab the dentry here;     */
		/*  it may not be available even when the inode is, and     */
		/*  an attempt to grab the dentry will have already failed. */

	ret_code = do_get_ref_inode(inode, &o_inode, which, flags);

	if ( ret_code == 0 )
	{
			/* There is only one way to land here:         */
			/*	- The inode can be retrieved from the  */
			/*	  inode cache, and                     */ 
			/*	- The dentry can not be retrieved from */
			/*	  the dcache nor the inode's alias.    */

			/* Got a reference inode, attach it to a dentry. */

		name_str.name = i_info->s.name;
		name_str.len  = i_info->s.len;
		name_str.hash = full_name_hash(name_str.name, name_str.len);

			/* If the filesystem for the dentry has its own */
			/*  hash, use it now.                           */

		if ( ( parent->d_op != NULL ) &&
		     ( parent->d_op->d_hash != NULL ) )
		{
			ret_code = parent->d_op->d_hash(parent, &name_str);
			if ( ret_code < 0 )
			{
				IPUT(o_inode);
				goto	get_ref_from_parent__out;
			}
		}

		result = d_alloc(parent, &name_str);

		if ( result == NULL )
		{
			ret_code = -ENOMEM;
			IPUT(o_inode);
			goto	get_ref_from_parent__out;
		}

			/* Note that d_add() consumes the inode. */
		d_add(result, o_inode);
		goto	get_ref_from_parent__mounts;
	}

#endif

		/* No reference inode exists, so lookup the inode from its */
		/*  ancestor by name.                                      */

	result = lookup_one_len(i_info->s.name, parent, i_info->s.len);

	ASSERT(result != NULL);

	if ( IS_ERR(result) )
	{
		ret_code = PTR_ERR(result);
		goto	get_ref_from_parent__out;
	}


		/* If the result is a negative dentry, create it if */
		/*  requested.                                      */

	if ( result->d_inode == NULL )
	{
		if ( flags & ( OVLFS_MAKE_HIER | OVLFS_MAKE_LAST ) )
			create_f = TRUE;
		else
			create_f = FALSE;

		if ( create_f && S_ISDIR(inode->i_mode) )
		{
				/* Use the inode's permissions for the */
				/*  creation of its reference.         */

			mode = inode->i_mode & S_IALLUGO;

			ret_code = vfs_mkdir_noperm(parent->d_inode, result,
			                            mode);

			if ( ret_code != 0 )
			{
				dput(result);
				goto	get_ref_from_parent__out;
			}

				/* Set the new directories attributes to */
				/*  match the ovlfs inode's.             */

			set_attrib(result->d_inode, inode);
		}
		else
		{
				/* No creation, just return the error. */

			dput(result);
			ret_code = -ENOENT;
			goto	get_ref_from_parent__out;
		}
	}

		/* Now we have a positive dentry for the reference. */

		/* Do not return entries in the ovlfs filesystem. */

	if ( result->d_inode->i_dev == inode->i_dev )
	{
		dput(result);
		ret_code = -EDEADLK;
		goto	get_ref_from_parent__out;
	}


		/* Do not return references to the base or storage filesytem */
		/*  roots unless the inode given is the ovlfs root.          */

	if ( ( inode->i_ino != OVLFS_ROOT_INO ) &&
	     ( is_base_or_stg_root_p(inode->i_sb, result->d_inode->i_dev,
	                             result->d_inode->i_ino) ) )
	{
		dput(result);
		ret_code = -EDEADLK;
		goto	get_ref_from_parent__out;
	}

		/* Attach the reference to the inode. */

	rc = ovlfs_attach_reference(inode, result, which, 0);
	if ( rc != 0 )
		DPRINT1(("OVLFS: failed to attach ref dev 0x%02x, ino %lu to "
		         "ovlfs inode %lu; rc = %d\n", result->d_inode->i_dev,
		         result->d_inode->i_ino, inode->i_ino, rc));

get_ref_from_parent__mounts:

	if ( flags & OVLFS_FOLLOW_MOUNTS )
	{
			/* Follow mount points; on success, d_mnt has the */
			/*  end-point.  We always have to consume result. */

		ret_code = resolve_mounts(inode->i_sb, inode, result, &d_mnt);
		dput(result);
		if ( ret_code != 0 )
			goto	get_ref_from_parent__out;

		result = d_mnt;
	}


		/* Good, got the descendent; return it. */

	ret_code = 0;
	r_dent[0] = result;


get_ref_from_parent__out:

	DPRINTR2("ino %lu, parent 0x%08x, r_dent 0x%08x, flags 0x%x; ret = "
	         "%d\n", inode->i_ino, (int) parent, (int) r_dent, flags,
	         ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: reference_walk
 **
 **  PURPOSE: Given that a valid reference ancestor was found for an inode,
 **           whose reference is needed, walk back through the ancestry of
 **           the inode until its reference is retrieved.
 **
 ** NOTES:
 **	- The history is not cleared as it is walked.
 **
 ** RULES:
 **	- The inode is an ovlfs inode.
 **	- The returned dentry refers to the base or storage filesystem.
 **	- The caller must release the walk information in all cases.
 **	- Follows mount points, as appropriate.
 **/

static int	reference_walk (walk_info_t *w_info, struct dentry **r_dent)
{
	ref_hist_t	*cur_anc;
	struct dentry	*descendent;
	int		flags;
	int		ret_code;

	DPRINTC2("ino %lu, r_dent 0x%08x\n", w_info->inode->i_ino,
	          (int) r_dent);

		/* Loop until no more history remains.  Note that errors  */
		/*  caught in the loop result in a jump to the end of the */
		/*  function.                                             */

	ret_code = 0;
	cur_anc  = w_info->history;

	flags = w_info->flags | OVLFS_FOLLOW_MOUNTS;

	while ( cur_anc != NULL )
	{
			/* Retrieve the current ancestor's dentry, which is  */
			/*  an immediate descendent of w_info->cur_ancestor. */

		ret_code = get_ref_from_parent(cur_anc->inode,
		                               w_info->cur_ancestor,
		                               &descendent, w_info->which,
		                               flags);

		if ( ret_code != 0 )
			goto	reference_walk__out;


			/* Got it; update the ancestor to the */
			/*  descendent just received.         */

		dput(w_info->cur_ancestor);
		w_info->cur_ancestor = descendent;


			/* Continue with the next descendent in */
			/*  the history.                        */

		cur_anc = cur_anc->descendent;
	}


		/* At this point, we have the parent of the dentry we are */
		/*  trying to retrieve.  Now, get the dentry itself.      */
		/*  Turn off the "make hierarchy" flag since this is the  */
		/*  last entry in the directory tree.                     */

	flags &= ~OVLFS_MAKE_HIER;

	ret_code = get_ref_from_parent(w_info->inode, w_info->cur_ancestor,
		                       r_dent, w_info->which, flags);

reference_walk__out:

	DPRINTR2("ino %lu, r_dent 0x%08x; ret = %d\n", w_info->inode->i_ino,
	          (int) r_dent, ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: ovlfs_resolve_dentry_hier
 **
 **  PUPROSE: Resolve the reference dentry, for the given ovlfs inode and
 **           reference filesystem specified, creating the hierarchy to the
 **           inode, if requested.
 **/

int	ovlfs_resolve_dentry_hier (struct inode *inode, struct dentry **r_dent,
	                           char which, int flags)
{
	walk_info_t		walk_info;
	struct dentry		*result;
	ovlfs_inode_info_t	*i_info;
	int			ret_code;

	DPRINTC2("ino %lu, r_dent 0x%08x, which %c, flags 0x%x\n",
	         inode->i_ino, (int) r_dent, which, flags);

	ret_code = -ENOENT;	/* Multiple cases coming */
	if ( ( inode == INODE_NULL ) || ( inode->u.generic_ip == NULL ) ||
	     ( r_dent == (struct dentry **) NULL ) )
	{
		DPRINT1(("OVLFS: %s: argument given is not valid (0x%x)\n",
		         __FUNCTION__, (int) inode));

		goto	ovlfs_resolve_dentry_hier__out;
	}

		/* First, check for a disabled reference and just return */
		/*  "no such entry" in this case.                        */

	i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;
	if ( ( i_info->s.flags & OVL_IFLAG__NO_BASE_REF ) && ( which == 'b' ) )
		goto	ovlfs_resolve_dentry_hier__out;


		/* Try the direct method. */

	ret_code = do_get_ref_dent(inode, &result, which, OVLFS_FOLLOW_MOUNTS);


		/* Stop when do_get_ref_dent returned with other than "no  */
		/*  entry" or the ovlfs inode is the root inode; there are */
		/*  no parents of the root, so walking makes no sense.     */

	if ( ( ret_code != -ENOENT ) || ( inode->i_ino == OVLFS_ROOT_INO ) )
		goto	ovlfs_resolve_dentry_hier__out;
	

	init_walk_info(&walk_info, inode, which, flags);

		/* Search up the ancestor list. */

	ret_code = find_ancestor(&walk_info);
	if ( ret_code != 0 )
		goto	ovlfs_resolve_dentry_hier__out_walk;

		/* Got an ancestor, now walk back down. */

	ret_code = reference_walk(&walk_info, &result);

ovlfs_resolve_dentry_hier__out_walk:
	release_walk_info(&walk_info);

ovlfs_resolve_dentry_hier__out:
		/* As a sanity check, in case someone modifies the reference */
		/*  filesystem out from under us, make sure directory inodes */
		/*  are not empty and unlinked.                              */
	
	if ( ret_code == 0 )
	{
		if ( ( S_ISDIR(result->d_inode->i_mode) ) &&
		     ( result->d_inode->i_nlink == 0 ) )
		{
			ret_code = -ENOENT;
			dput(result);
		}
	}

	if ( ret_code == 0 )
		r_dent[0] = result;

	DPRINTR2("ino %lu, r_dent 0x%08x, which %c, flags 0x%x; ret = %d\n",
	         inode->i_ino, (int) r_dent, which, flags, ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: ovlfs_resolve_dentry
 **
 **  PUPROSE: Resolve the reference dentry, for the given ovlfs inode and
 **           reference filesystem specified.
 **/

int	ovlfs_resolve_dentry (struct inode *inode, struct dentry **r_dent,
	                      char which)
{
	int	rc;

	DPRINTC2("inode %lu, r_dent 0x%08x, which %c\n", inode->i_ino,
	         (int) r_dent, which);

		/* Just use ovlfs_resolve_dentry_hier. */

	rc = ovlfs_resolve_dentry_hier(inode, r_dent, which, 0);

	if ( rc != 0 )
		goto	ovlfs_resolve_dentry__out;


ovlfs_resolve_dentry__out:

	DPRINTR2("inode %lu, r_dent 0x%08x, which %c; ret = %d\n",
	         inode->i_ino, (int) r_dent, which, rc);

	return	rc;
}



/**
 ** FUNCTION: ovlfs_resolve_inode
 **
 **  PUPROSE: Resolve the reference inode, for the given ovlfs inode and
 **           reference filesystem specified.
 **/

int	ovlfs_resolve_inode (struct inode *inode, struct inode **r_inode,
	                     char which)
{
	struct dentry	*dent;
	int		rc;

	DPRINTC2("ino %lu, r_inode  0x%08x, which %c\n", inode->i_ino,
	         (int) r_inode, which);

	if ( r_inode == NULL )
		return	-ENOENT;


		/* Just use ovlfs_resolve_dentry_hier. */

	rc = ovlfs_resolve_dentry_hier(inode, &dent, which, 0);

	if ( rc == 0 )
	{
			/* Produce the inode and consume the dentry that was */
			/*  produced by the lookup.                          */

		IMARK(dent->d_inode);
		r_inode[0] = dent->d_inode;

		dput(dent);
	}

	DPRINTR2("ino %lu, r_inode  0x%08x, which %c\n", inode->i_ino,
	         (int) r_inode, which);

	return	rc;
}



/**
 ** FUNCTION: inode_refs_valid_p
 **
 **  PURPOSE: Determine whether the given inode has valid references.
 **/

int	inode_refs_valid_p (struct inode *inode)
{
	int		rc;
	struct inode	*tmp_i;
	int		valid_f;

	valid_f = FALSE;

		/* Check for a valid base reference. */
	rc = ovlfs_resolve_inode(inode, &tmp_i, 'b');

	if ( rc != 0 )
	{
			/* None there, look a valid storage reference. */
		rc = ovlfs_resolve_inode(inode, &tmp_i, 's');
	}

	if ( rc == 0 )
	{
		valid_f = TRUE;
		IPUT(tmp_i);
	}

	return	valid_f;
}
