/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_dir.c
 **
 ** DESCRIPTION: This file contains the source code for the directory
 **              operations of the overlay filesystem.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 09/27/1997	art            	Added to source code control.
 ** 02/24/1998	ARTHUR N.	Added support for hiding magic directories and
 **				 added locking around call to lookup on the
 **				 storage filesystem's inode.
 ** 02/26/1998	ARTHUR N.	Added the blksize, blocks, version, and
 **				 nrpages to the inode information retained.
 ** 02/28/1998	ARTHUR N.	Fixed bug in read_into_dir() which would cause
 **				 directory entries to be missing if the readdir
 **				 operation of the real directory did not return
 **				 all entries in a single call.
 ** 03/01/1998	ARTHUR N.	Improved comments; added use of the term pseudo
 **				 filesystem.
 ** 03/01/1998	ARTHUR N.	Added support for the magic directory naming
 **				 option for the ovlfs (i.e. the pseudo fs).
 ** 03/01/1998	ARTHUR N.	Allocate GFP_KERNEL priority memory only.
 ** 03/03/1998	ARTHUR N.	Added support for the noxmnt option and fixed
 **				 crossing-mount points.
 ** 03/05/1998	ARTHUR N.	Added support for malloc-checking.
 ** 03/09/1998	ARTHUR N.	Added the copyright notice.
 ** 03/09/1998	ARTHUR N.	Update the access time of the directory after
 **				 the readdir() call completes.
 ** 03/22/1998	ARTHUR N.	Modified to use the storage operations.
 ** 03/28/1998	ARTHUR N.	Modified comments, fixed mapping of inodes,
 **				 and use s.parent_ino in ovlfs_dir_readdir.
 ** 06/27/1998	ARTHUR N.	Mark inode as dirty after setting the reference
 **				 name in read_into_dir_cb so that the reference
 **				 name will get written back to storage.
 ** 02/06/1999	ARTHUR N.	Many changes to support storage methods.  This
 **				 version is not complete, but is being saved
 **				 because large changes are coming.
 ** 02/06/1999	ARTHUR N.	Reworked the update of directories and inodes
 **				 so that updating a directory's contents does
 **				 not attempt to update the entries' inodes.
 ** 02/06/1999	ARTHUR N.	Do not add a new inode if the directory entry
 **				 already exists in read_into_dir_cb().
 ** 02/06/1999	ARTHUR N.	Corrected the test for existence of an entry.
 ** 02/07/1999	ARTHUR N.	Corrected return value from ovlfs_dir_readdir()
 **				 when the callback returns an error.
 ** 02/17/1999	ARTHUR N.	Set the inode number to match by name when
 **				 scanning real directories.  NOTE: still need
 **				 a "better" (i.e. more well defined) mapping
 **				 mechanism.
 ** 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
 ** 02/24/2003	ARTHUR N.	Added do_lookup3() and removed unused code.
 ** 02/27/2003	ARTHUR N.	Don't revert directory entries to base-mapped
 **				 inodes when different stg-mapped inodes exist.
 ** 03/10/2003	ARTHUR N.	Disable automatic update of existing directory
 **				 entries.
 ** 03/11/2003	ARTHUR N.	Add flags argument to add_inode storage op.
 ** 03/26/2003	ARTHUR N.	Changed the storage system's Read_dir operation
 **				 to use an loff_t pointer to allow the storage
 **				 system to define the value as it needs.
 ** 07/05/2003	ARTHUR N.	[SF BUG 766406] List all directory entries,
 **				 including the very first one and those on
 **				 which the filldir() callback returns "full".
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_dir.c	1.28	[03/07/27 22:20:34]"

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "ovlfs.ver"
# endif
#endif

#include <linux/version.h>

#include <linux/stddef.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/string.h>

#if defined(KERNEL_VERSION) && ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0) )
#undef MODULE
#include <linux/module.h>
#endif

#include "kernel_lists.h"
#include "ovl_fs.h"
#include "ovl_inlines.h"



/**
 ** FUNCTION: inode_mapped_to_fs_p
 **
 **  PURPOSE: Determine whether the inode, whose number and super block are
 **           given, is mapped to a reference inode in the specified fs.
 **/

#if TBD    /* see upd_inode_to_dir */

static int	inode_mapped_to_fs_p (struct super_block *sb, ino_t inum,
		                      char which)
{
	kdev_t	o_dev;
	_ulong	o_ino;
	int	mapped_f;
	int	rc;

		/* TBD: consider stale mappings. */

	rc = do_get_mapping_op(sb, inum, which, &o_dev, &o_ino);

	if ( ( rc == 0 ) && ( o_dev != NODEV ) )
		mapped_f = TRUE;
	else
		mapped_f = FALSE;

	return	mapped_f;
}

#endif    /* TBD */



/**
 ** FUNCTION: upd_inode_to_dir
 **
 **  PURPOSE: Add or update the directory entry for the inode given.
 **
 ** RULES:
 **	- The given directory must be from the ovlfs.
 **	- The given inode must be from the base or storage filesystem.
 **	- cand_inum must be set with the inode number of the existing ovlfs
 **	  inode when new_ent_f is false.  When new_ent_f is true, the existing
 **	  value is ignored.
 **	- On success, cand_inum is set with the ovlfs inode number.
 **	- The root of the base or storage filesystem will NOT be allowed; the
 **	  error, -EDEADLK, will be returned.
 **/

/* TBD: take flags */
static int	upd_inode_to_dir (struct inode *dir, kdev_t dev, _ulong o_inum,
		                  char which, _ulong *cand_inum, int new_ent_f,
		                  const char *name, int name_len)
{
	_ulong	mapped_inum;
	_ulong	ent_inum;
	int	rc;
	int	ret_code;

	DPRINTC2("dir ino %lu, dev 0x%x, inum %lu, which %c, new_ent_f %d\n",
	         dir->i_ino, dev, o_inum, which, new_ent_f);

	ret_code = 0;
	ent_inum = cand_inum[0];


		/* Make sure the real inode is not one of this filesystem's */
		/*  root references.                                        */

	if ( is_base_or_stg_root_p(dir->i_sb, dev, o_inum) )
	{
		DPRINT2(("OVLFS: %s: found a root inode; dev %d, inum %lu\n",
		         __FUNCTION__, dev, o_inum));

		ret_code = -EDEADLK;
		goto	upd_inode_to_dir__out;
	}


		/* See if the inode mapping has an ovlfs inode number for */
		/*  the other fs' device and inode number pair.           */

	rc = do_map_lookup(dir->i_sb, dev, o_inum, &mapped_inum, which);

	if ( rc < 0 )
	{
			/* Not found.  If an entry exists, just use it;    */
			/*  otherwise, try to add a new inode in the ovlfs */
			/*  storage.                                       */

		if ( new_ent_f )
		{
			DPRINT2(("OVLFS: unable to find pseudo fs inode for "
			         " dev 0x%x, inum %ld\n", dev, o_inum));


			ret_code = do_add_inode_op(dir->i_sb, dir->i_ino,
			                           name, name_len, &ent_inum,
			                           0);

			if ( ret_code < 0 )
				goto	upd_inode_to_dir__out;


				/* Add a mapping for the inode. */

			ret_code = do_map_inode(dir->i_sb, ent_inum,
			                        dev, o_inum, which);

			if ( ret_code < 0 )
				goto	upd_inode_to_dir__out;
		}
	}
	else if ( new_ent_f )
	{
			/* Used the mapped inode number. */
		ent_inum = mapped_inum;
	}
	else
	{
/* TBD: perhaps no logic here is good.  We should force the user to update. */
/*      And, if logic is added to detect "lost" inodes, this problem could  */
/*      be resolved to satisfaction.                                        */

/*      Once a solution is implemented to detect and remove directory       */
/*      entries that have lost their reference inodes, this should be good. */

#if TBD
			/* Found an inode in the mapping and an entry    */
			/*  already exists in the directory.  Check that */
			/*  they are the same inode.                     */

			/* The inode numbers will not match in two cases: */
			/*  the base or storage filesystem is changed     */
			/*  directly; or an ovlfs entry mapped to a base  */
			/*  fs inode is removed and recreated.            */

			/*  There is only so much that can be done.  The     */
			/*   trick is that the name is mightier than the     */
			/*   inode number so that backing up and restoring   */
			/*   files does not cause a problem.  Also, inodes   */
			/*   come, go, and get reused - we would not want to */
			/*   attempt to access an inode that was released or */
			/*   refers to an entirely different file.           */

			/* If the inode numbers do not match and either     */
			/*	- the inode given is in the storage fs, or  */
			/*	- it is not mapped to a storage fs inode    */
			/*  then update it.                                 */

		if ( ( ent_inum != mapped_inum ) &&
		     ( ( which != 'b' ) ||
		       ( ! inode_mapped_to_fs_p(dir->i_sb, ent_inum, 's') ) ) )
		{
				/* Just remove the entry and mark it as */
				/*  missing so it will get added below. */

			ret_code = do_unlink_op(dir, name, name_len);
			if ( ret_code < 0 )
				goto	upd_inode_to_dir__out;
			    
			new_ent_f = TRUE;
			ent_inum  = mapped_inum;
		}
#endif
	}



		/* If the entry does not yet exist in the directory, add it */
		/*  now.  Leave the entry as-is if it exists, even if it is */
		/*  marked as being unlinked.                               */

	if ( new_ent_f )
		ret_code = do_add_dirent(dir, name, name_len, ent_inum);

	if ( ret_code == 0 )
		cand_inum[0] = ent_inum;

upd_inode_to_dir__out:

	DPRINTR2("dir ino %lu, dev 0x%x, inum %lu, which %c, new_ent_f %d; "
	         "ret = %d\n", dir->i_ino, dev, o_inum, which, new_ent_f,
	         ret_code);

	return  ret_code;
}



/**
 ** STRUCTURE: dir_info_struct
 **
 ** NOTES:
 **	- Don't confuse this "temporary" structure with the ovlfs_dir_info_t
 **	  data structure.
 **/

struct dir_info_struct {
    void        *dirent;
    int         ent_num;
    filldir_t   callback;
    list_t      dlist;
} ;

typedef struct dir_info_struct  dir_info_t;




/**
 ** FUNCTION: read_into_dir_cb
 **
 **  PURPOSE: Handle the callback from the readdir() call for the read_into_dir
 **           function.
 **
 ** NOTES:
 **     - This callback assumes that all entries in a directory reside on the
 **       same device as the directory itself.  This should be safe since one
 **       filesystem must reside on one device, and the filesystem's readdir()
 **       is the one passing the entries here.
 **
 **	- If the directory entry exists, it could be mapped to a different
 **	  inode in the real (i.e. base or storage) fs.  This situation is not
 **	  handled well.  For now, the entry remains mapped to the original
 **	  inode. 
 **
 **	- This function has always been the biggest issue with the ovlfs.
 **	  That is due to the responsibilty this function has to take a
 **	  directory entry found in a real filesystem and create or update the
 **	  ovlfs directory and inode pool.  Without this, there would only be
 **	  an empty root directory (pretty boring).
 **/

struct read_into_cb_info_struct {
    struct super_block  *o_sb;
    struct inode        *o_dir;
    struct inode        *dir;
    kdev_t              dev;
    char                is_stg;
} ;

typedef struct read_into_cb_info_struct read_into_cb_info_t;

#if defined(KERNEL_VERSION) && ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0) )
static int  read_into_dir_cb (void *d_ent, const char *name, int name_len,
                              loff_t ent_no, ino_t real_inum, unsigned dt_type)
#else
static int  read_into_dir_cb (void *d_ent, const char *name, int name_len,
                              off_t ent_no, ino_t real_inum)
#endif
{
	read_into_cb_info_t	*cb_info;
	_ulong			ent_inum;
	int			flags;
	char			which;
	int			new_ent_f;
	int			rc;
	int			ret_code;

	DPRINTC2("name %.*s, len %d, ent_no %d, real_inum %u\n", name_len,
	         name, name_len, (int) ent_no, (int) real_inum);

	cb_info  = (read_into_cb_info_t *) d_ent;


#if ! FASTER_AND_MORE_DANGEROUS

	if ( ( d_ent == NULL ) || ( cb_info->dir == NULL ) ||
	     ( name == NULL ) || ( name_len <= 0 ) ||
	     ( cb_info->dir->i_sb == NULL ) ||
	     ( cb_info->dir->i_sb->s_magic != OVLFS_SUPER_MAGIC ) )
	{
		DPRINT1(("OVLFS: read_into_dir_cb: invalid argument given\n"));

		ret_code = -ENOTDIR;
		goto	read_into_dir_cb__out;
	}

#endif


		/* Check for "." and ".."; do not add either one */

	ret_code = 0;
	if ( ( name_len < 3 ) && ( name[0] == '.' ) )
		if ( ( name_len == 1 ) || ( name[1] == '.' ) )
			goto	read_into_dir_cb__out;

		/* Check for the base or storage root; if this entry is */
		/*  either, do not add it.  However, this is not an     */
		/*  error.                                              */

	rc = is_base_or_stg_root_p(cb_info->dir->i_sb, cb_info->dev, real_inum);
	if ( rc )
	{
		DPRINT2(("OVLFS: %s: found a root inode; dev %d, inum %lu\n",
		         __FUNCTION__, cb_info->dev, real_inum));

		goto	read_into_dir_cb__out;
	}


		/* See if the named directory entry already exists, even if */
		/*  it is marked as unlinked.                               */

	new_ent_f = FALSE;
	if ( do_lookup_op(cb_info->dir, name, name_len, &ent_inum,
	                  &flags) < 0 )
	{
			/* Not found, this is a new directory entry. */
		new_ent_f = TRUE;
	}

	if ( cb_info->is_stg )
		which = 's';
	else
		which = 'b';

	ret_code = upd_inode_to_dir(cb_info->dir, cb_info->dev, real_inum,
	                            which, &ent_inum, new_ent_f, name,
	                            name_len);

read_into_dir_cb__out:

	DPRINTR2("name %.*s, len %d, ent_no %d, real_inum %u; ret = %d\n",
	         name_len, name, name_len, (int) ent_no, (int) real_inum,
	         ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: read_into_dir
 **
 **  PURPOSE: Read the contents of the given "real" directory, o_dir, and add
 **           the contents to the given pseudo fs directory.
 **
 ** TBD:
 **	- How is do_open_inode() different than ovlfs_open_inode()?
 **/

#if POST_20_KERNEL_F

static int  do_open_inode (struct inode *inode, int flags,
                           struct file **r_filp)
{
	struct file		*result;
	struct vfsmount		*mnt;
	struct dentry		*d_ent;
	int			ret;

	DPRINTC2("ino %lu, flags 0%o, r_filp 0x%08x\n", inode->i_ino, flags,
	         (int) r_filp);

	ret = 0;

		/* As a special case, if the inode given is the root inode,  */
		/*  grab its mnt and dentry from the current task structure; */
		/*  the dentry doesn't reside in the dentry cache...         */

	if ( inode == current->fs->root->d_inode )
	{
		mnt   = mntget(current->fs->rootmnt);
		d_ent = dget(current->fs->root);
	}
	else
	{
		ret = get_inode_vfsmount(inode, &mnt);

		if ( ret < 0 )
			goto	do_open_inode__out;

		d_ent = ovlfs_inode2dentry(inode);

		if ( d_ent == NULL )
		{
			mntput(mnt);
			ret = -ENOENT;
			goto	do_open_inode__out;
		}
	}

	DPRINT9(("OVLFS: opening directory named %.*s with mnt 0x%08x\n",
	         (int) d_ent->d_name.len, d_ent->d_name.name, (int) mnt));

		/* Note: dentry_open puts the vfsmount or attaches to the */
		/*       file structure for release at fput() time.       */

	result = dentry_open(d_ent, mnt, flags);

	if ( IS_ERR(result) )
		ret = PTR_ERR(result);
	else
		r_filp[0] = result;

do_open_inode__out:

	DPRINTR2("ino %lu, flags 0%o, r_filp 0x%08x; ret = %d\n", inode->i_ino,
             flags, (int) r_filp, ret);

	return	ret;
}

static int  read_into_dir (struct inode *dir, struct inode *o_dir, char is_stg)
{
    struct file         *filp;		/* MUST be a stack variable */
    read_into_cb_info_t cb_info;	/* MUST be a stack variable */
    int                 last_pos;
    int                 ret;

#if ! FASTER_AND_MORE_DANGEROUS

    if ( ( dir == INODE_NULL ) || ( o_dir == INODE_NULL ) )
    {
        DPRINT1(("OVLFS: read_into_dir: invalid argument\n"));

        return  -ENOTDIR;
    }

#endif

    DPRINTC((KDEBUG_CALL_PREFIX "read_into_dir(inode %lu, dir inode %lu, "
             "is-stg %d)\n", dir->i_ino, o_dir->i_ino, is_stg));


        /* Make sure the real inode is a directory (that we can use) */

    if ( ( o_dir->i_fop == NULL ) || ( o_dir->i_fop->readdir == NULL ) )
    {
        ret = -ENOTDIR;
        goto    read_into_dir_out;
    }

        /* Open the real directory for reading. */

    ret = do_open_inode(o_dir, FMODE_READ, &filp);

    if ( ret != 0 )
        goto    read_into_dir_out;

    last_pos       = -1;
    cb_info.dir    = dir;
    cb_info.o_sb   = o_dir->i_sb;
    cb_info.o_dir  = o_dir;
    cb_info.dev    = o_dir->i_dev;
    cb_info.is_stg = is_stg;


        /* Keep calling the directory's readdir() function until an  */
        /*  error is encountered, or no more entries are returned.   */

    while ( ( ret >= 0 ) && ( filp->f_pos > last_pos ) )
    {
        last_pos = filp->f_pos;
        ret = filp->f_op->readdir(filp, &cb_info, read_into_dir_cb);
    }

        /* The lock id passed here will never be used since we are not using */
        /*  POSIX locks.  The standard setting is used for readability.      */

    filp_close(filp, current->files);


read_into_dir_out:

    DPRINTC((KDEBUG_RET_PREFIX "read_into_dir(inode %lu, dir inode %lu, "
             "is-stg %d) = %d\n", dir->i_ino, o_dir->i_ino, is_stg, ret));

    return  ret;
}

#else

	/* Pre-2.2 version: */

static int  read_into_dir (struct inode *dir, struct inode *o_dir, char is_stg)
{
    struct file         file;		/* MUST be a stack variable */
    read_into_cb_info_t cb_info;	/* MUST be a stack variable */
    int                 last_pos;
    int                 ret;

#if ! FASTER_AND_MORE_DANGEROUS

    if ( ( dir == INODE_NULL ) || ( o_dir == INODE_NULL ) )
    {
        DPRINT1(("OVLFS: read_into_dir: invalid argument\n"));

        return  -ENOTDIR;
    }

#endif

        /* Make sure the real inode is a directory (that we can use) */

    if ( ( o_dir->i_op == NULL ) ||
         ( o_dir->i_op->default_file_ops == NULL ) ||
         ( o_dir->i_op->default_file_ops->readdir == NULL ) )
        return  -ENOTDIR;


        /* Setup a file to use to read the real directory */

    memset(&file, '\0', sizeof(struct file));

    if ( o_dir->i_op != NULL )
        file.f_op = o_dir->i_op->default_file_ops;

    file.f_count = 1;   /* Just in case anyone looks */
    file.f_op = o_dir->i_op->default_file_ops;

	/* TBD: is it safe to continue with this file structure without */
	/*      calling the original filesystem's open on it?           */

        /*  While it may be for some filesystems, don't do it. */

    ret = 0;
    last_pos       = -1;
    cb_info.dir    = dir;
    cb_info.o_sb   = o_dir->i_sb;
    cb_info.o_dir  = o_dir;
    cb_info.dev    = o_dir->i_dev;
    cb_info.is_stg = is_stg;


        /* Keep calling the directory's readdir() function until an  */
        /*  error is encountered, or no more entries are returned    */

    while ( ( ret >= 0 ) && ( file.f_pos > last_pos ) )
    {
        last_pos = file.f_pos;
        ret = file.f_op->readdir(o_dir, &file, &cb_info, read_into_dir_cb);
    }

    return  ret;
}

#endif



/**
 ** FUNCTION: ovlfs_read_directory
 **
 **  PURPOSE: Given a pseudo fs directory inode, determine the contents for the
 **           directory and fill in the directory's information.
 **
 ** NOTES:
 **	- The pseudo fs' storage must already contain the inode information
 **	  for the given directory.
 **
 **     - Unlike real physical directories, if the pseudo fs' directory does
 **       not exist in its storage, it must be created.
 **
 **	- *** WARNING *** It is imperative that this function does NOT attempt
 **	  to iget() or, in any other way, obtain the inode for one of the given
 **	  directory's entries.  If it does so, a recursion will occur that will
 **	  end in a kernel stack overflow and corruption.
 **/

int ovlfs_read_directory (struct inode *dir)
{
    struct inode    *o_inode;
    int             ret;

#if ! FASTER_AND_MORE_DANGEROUS

    if ( ( dir == INODE_NULL ) || ( dir->u.generic_ip == NULL ) ||
         ( ! is_ovlfs_inode(dir) ) )
    {
        DPRINT1(("OVLFS: ovlfs_read_directory: invalid inode given\n"));

        return  -ENOTDIR;
    }

#endif

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_read_directory(inode %ld)\n",
             dir->i_ino));


        /***
         *** Read the entries to place in the directory's list of entries.
         ***  First look for the inode in the storage fs so that any
         ***  pseudo fs inodes created by read_into_dir() will take the
         ***  attributes of the storage fs' inode instead of the base fs'
         ***  inode.
         ***/

    ret = ovlfs_resolve_stg_inode(dir, &o_inode);

    if ( ret >= 0 )
    {
            /* Read the storage fs dir's contents into the pseudo fs */

        read_into_dir(dir, o_inode, TRUE);
        IPUT(o_inode);
    }
    else if ( ret == -ENOENT )	/* It's ok if the stg dir doesn't exist */
        ret = 0;


        /* Now process the base filesystem's directory contents. */

    ret = ovlfs_resolve_base_inode(dir, &o_inode);

    if ( ret >= 0 )
    {
            /* Read the base fs dir's contents into the pseudo fs */

        read_into_dir(dir, o_inode, FALSE);
        IPUT(o_inode);
    }
    else if ( ret == -ENOENT )	/* It's ok if the base dir doesn't exist */
        ret = 0;


        /* If successful, return the number of entries in the directory */

    if ( ret >= 0 )
        ret = do_num_dirent_op(dir, 0);

    return  ret;
}



/**
 ** FUNCTION: update_dir_entry
 **
 **  PURPOSE: Given a directory inode and the name of an entry, check whether
 **           the entry exists in the storage and/or base filesystem and add it
 **           to the ovlfs if so.
 **
 ** RULES:
 **	- Returns the inode number of the ovlfs inode for the directory entry
 **	  on success.
 **	- Does not consume the directory given.
 **/

int	update_dir_entry (struct inode *dir, const char *fname, int len,
	                  _ulong *r_inum)
{
	struct dentry	*o_dir;
	struct dentry	*o_dent;
	_ulong		ent_inum;
	char		which;
	int		ret_code;

	DPRINTC2("dir ino %lu, name %.*s, len %d, r_inum 0x%08x\n",
	         dir->i_ino, len, fname, len, (int) r_inum);


		/* Try the storage filesystem.  If it is not found there, */
		/*  continue on to try the base filesystem.               */

	which = 's';
	ret_code = ovlfs_resolve_stg_dentry(dir, &o_dir);
	if ( ret_code < 0 )
		goto	update_dir_entry__try_base;

		/* Lookup the entry in the directory. */

	ret_code = do_lookup3(o_dir, fname, len, &o_dent, TRUE);
	dput(o_dir);

	if ( ret_code == 0 )
		goto	update_dir_entry__found;


update_dir_entry__try_base:

		/* It wasn't in the storage fs; try the base fs.  */

	which = 'b';
	ret_code = ovlfs_resolve_base_dentry(dir, &o_dir);
	if ( ret_code < 0 )
		goto	update_dir_entry__out;


		/* Lookup the entry in the other directory. */

	ret_code = do_lookup3(o_dir, fname, len, &o_dent, TRUE);
	dput(o_dir);

	if ( ret_code < 0 )
		goto	update_dir_entry__out;


update_dir_entry__found:
		/* Found an entry with this name in one of the other file- */
		/*  systems.  Add the entry to the ovlfs directory.        */

	ent_inum = 0;
	ret_code = upd_inode_to_dir(dir, o_dent->d_inode->i_dev,
	                            o_dent->d_inode->i_ino, which, &ent_inum,
	                            TRUE, fname, len);
	dput(o_dent);
	if ( ret_code < 0 )
		goto	update_dir_entry__out;

		/* Return the ovlfs inode's number. */
	r_inum[0] = ent_inum;

update_dir_entry__out:
	DPRINTR2("dir ino %lu, name %.*s, len %d, r_inum 0x%08x; ent_inum = "
	         "%lu, ret = %d\n", dir->i_ino, len, fname, len, (int) r_inum,
	         ent_inum, ret_code);

	return	ret_code;
}



                           /****                ****/
                           /****  ENTRY POINTS  ****/
                           /****                ****/


/**
 ** FUNCTION: ovlfs_dir_readdir
 **
 **  PURPOSE: Read the overlay filesystem directory whose inode is given.
 **
 ** NOTES:
 **     - This is the second version of this method; the ovlfs storage is
 **       used here to determine the contents of the directory given.
 **
 **     - (CONSIDER) It might be a good idea to update the contents of the
 **       directory's storage since the actual base and/or storage filesystems
 **       may have changed.
 **
 **	- The entire contents of the directory are returned unless the filldir
 **	  callback returns a negative value.
 **/

static inline void  update_time (struct inode *inode)
{
    if ( ! IS_RDONLY(inode) )
    {
#if POST_20_KERNEL_F
            /* TBD: can we synchronize these two? */

        UPDATE_ATIME(inode);
        inode->i_ctime = CURRENT_TIME;
#else
        inode->i_atime = ( inode->i_ctime = CURRENT_TIME );
#endif
        INODE_MARK_DIRTY(inode);
    }
}

static inline _ulong	det_ovlfs_inode_parent (ovlfs_inode_info_t *i_info,
                                                struct inode *inode)
{
#if STORE_REF_INODES
# if !REF_DENTRY_F
    if ( i_info->ovlfs_dir != INODE_NULL )
        return  i_info->ovlfs_dir->i_ino;
# endif
#endif

    if ( i_info->s.parent_ino > 0 )
        return  i_info->s.parent_ino;

    return  inode->i_ino;	/* must be the root of the fs */
}

#if POST_20_KERNEL_F
static int  ovlfs_dir_readdir (struct file *filp, void *dirent,
                               filldir_t filldir)
#else
static int  ovlfs_dir_readdir (struct inode *inode, struct file *filp,
                               void *dirent, filldir_t filldir)
#endif
{
    int                 cur_ent_no;
    int                 error;
    ovlfs_inode_info_t  *i_info;
    ovlfs_dir_info_t    *d_info;
    struct inode        *o_inode;
    _ulong              inum;
    int                 rc;
#if POST_20_KERNEL_F
    struct inode        *inode;

    DPRINTC2("filp 0x%08x dirent 0x%08x", (int) filp, (int) dirent);

        /* Grab the inode from the file structure. */

    if ( ( filp != NULL ) && ( filp->f_dentry != NULL ) &&
         ( filp->f_dentry->d_inode != NULL ) )
    {
        inode = filp->f_dentry->d_inode;
    }
    else
    {
        return  -ENOTDIR;
    }
#else
    DPRINTC2("inode 0x%08x filp 0x%08x dirent 0x%08x", (int) inode, (int) filp,
             (int) dirent);
#endif


#if ! FASTER_AND_MORE_DANGEROUS

    if ( ( inode == (struct inode *) NULL ) || ( !S_ISDIR(inode->i_mode) ) ||
         ( ! is_ovlfs_inode(inode) ) )
    {
        DPRINT1(("OVLFS: ovlfs_dir_readdir called with invalid inode at 0x%x\n",
                 (int) inode));

        return  -ENOTDIR;
    }

#endif

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_dir_readdir(inode %ld, f_pos %d)\n",
             inode->i_ino, (int) filp->f_pos));

    cur_ent_no = filp->f_pos;
    i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;


        /* Update the directory's contents, but only when the first entry is */
        /*  read, so as to prevent strange results in the middle of a read.  */
        /*  TBD: consider the locking here -- if it is possible for two      */
        /*  readdir() calls to run at once, this still could be a problem.   */

    if ( cur_ent_no == 0 )
    {
        rc = ovlfs_read_directory(inode);

        if ( rc < 0 )
            return    rc;
    }

        /* NOTE: All cases in the switch fall through (except default)! */

    switch ( cur_ent_no )
    {
        case 0:
            rc = filldir(dirent, ".", 1, cur_ent_no, inode->i_ino, DT_UNKNOWN);
            if ( rc < 0 )
            {
                update_time(inode);

                DPRINTC((KDEBUG_RET_PREFIX
                         "ovlfs_dir_readdir(inode %ld) = 0 on '.'\n",
                         inode->i_ino));

                return  0;
            }
            cur_ent_no++;
            filp->f_pos++;

        case 1:
                /* Try to determine the parent directory of this one */

            inum = det_ovlfs_inode_parent(i_info, inode);

            rc = filldir(dirent, "..", 2, cur_ent_no, inum, DT_UNKNOWN);
            if ( rc < 0 )
            {
                update_time(inode);
 
                DPRINTC((KDEBUG_RET_PREFIX
                         "ovlfs_dir_readdir(inode %ld) = 0 on '..'\n",
                         inode->i_ino));

                return  0;
            }

            cur_ent_no++;
            filp->f_pos++;

        case 2:
#if USE_MAGIC
            if ( ( ovlfs_magic_opt_p(inode->i_sb, OVLFS_BASE_MAGIC) ) &&
                 ( ! ovlfs_magic_opt_p(inode->i_sb, OVLFS_HIDE_MAGIC) ) )
            {
                    /* Make sure there is a base inode for this inode; */
                    /*  if not, just skip it.                          */

                error = ovlfs_resolve_base_inode(inode, &o_inode);

                if ( error >= 0 )
                {
                    char    *name;
                    int     len;

                        /* Determine the directory's name from the ovlfs' */
                        /*  options, but use the defaults if not found.   */

                    name = ovlfs_sb_opt(inode->i_sb, Bmagic_name);
                    len = ovlfs_sb_opt(inode->i_sb, Bmagic_len);

                    if ( ( name == (char *) NULL ) || ( len <= 0 ) )
                    {
                        name = OVLFS_MAGICR_NAME;
                        len = OVLFS_MAGICR_NAME_LEN;
                    }

                    rc = filldir(dirent, name, len, cur_ent_no, o_inode->i_ino,
                                 DT_UNKNOWN);

                    if ( rc < 0 )
                    {
                        update_time(inode);

                        DPRINTC((KDEBUG_RET_PREFIX
                                 "ovlfs_dir_readdir(inode %ld) = 0 on magicr\n",
                                 inode->i_ino));

                        return  0;
                    }

                    cur_ent_no++;
                    IPUT(o_inode);
                }
            }
            filp->f_pos++;
#endif


        case 3:
#if USE_MAGIC
            if ( ( ovlfs_magic_opt_p(inode->i_sb, OVLFS_OVL_MAGIC) ) &&
                 ( ! ovlfs_magic_opt_p(inode->i_sb, OVLFS_HIDE_MAGIC) ) )
            {
                    /* Make sure there is a storage inode for this inode; */
                    /*  if not, just skip it.                             */

                error = ovlfs_resolve_stg_inode(inode, &o_inode);

                if ( error >= 0 )
                {
                    char    *name;
                    int     len;

                        /* Determine the directory's name from the ovlfs' */
                        /*  options, but use the defaults if not found.   */

                    name = ovlfs_sb_opt(inode->i_sb, Smagic_name);
                    len = ovlfs_sb_opt(inode->i_sb, Smagic_len);

                    if ( ( name == (char *) NULL ) || ( len <= 0 ) )
                    {
                        name = OVLFS_MAGIC_NAME;
                        len = OVLFS_MAGIC_NAME_LEN;
                    }

                    rc = filldir(dirent, name, len, cur_ent_no, o_inode->i_ino,
                                 DT_UNKNOWN);

                    if ( rc < 0 )
                    {
                        update_time(inode);

                        DPRINTC((KDEBUG_RET_PREFIX
                                 "ovlfs_dir_readdir(inode %ld) = 0 on magic\n",
                                 inode->i_ino));

                        return  0;
                    }

                    cur_ent_no++;
                    IPUT(o_inode);
                }
            }
            filp->f_pos++;
#endif
    
        case 4:
                /***
                 *** Skip to file position 5 here whether or not magic is
                 ***  compiled in; this way, the default action always
                 ***  knows which file position is the first entry in its
                 ***  storage (that is 5).
                 ***/

            filp->f_pos = 5;
    
        default:
                /* Now loop through any and all entries in this directory's */
                /*  stored list of entries.                                 */

            error = 0;

            while ( error == 0 )
            {
                loff_t  save_pos;
                loff_t  tmp_pos;

                    /* Get the nth entry (n >= 1) */

                save_pos = filp->f_pos;	/* Save in case filldir() fails... */

                tmp_pos = filp->f_pos - 5;
                error = do_read_dir_op(inode, &tmp_pos, &d_info);
                filp->f_pos = tmp_pos + 5;    /* Always save resulting pos */

                    /* Make sure the entry was obtained and that the entry */
                    /*  is not marked as being deleted.                    */

                if ( error < 0 )
                {
                    DPRINT1(("OVLFS: do_read_dir_op(%lu, %Lu) returned error "
                             "%d\n", inode->i_ino, filp->f_pos - 5, error));
                }
                else if ( d_info == (ovlfs_dir_info_t *) NULL )
                {
                        /* There must be no more entries. (not an error :)) */
                    error = 1;
                }
                else if ( ! ( d_info->flags & OVLFS_DIRENT_UNLINKED ) )
                {
                    error = filldir(dirent, d_info->name, d_info->len,
                                    cur_ent_no, d_info->ino, DT_UNKNOWN);

                        /* If successful, continue to the next entry */

                    if ( error >= 0 )
                    {
                        error = 0;
                        cur_ent_no++;
                    }
                    else
                    {
			error = 1;	/* This can and will happen normally */

                            /* Restore the file position so that the current */
                            /*  entry will be retrieved next time.           */

                        filp->f_pos = save_pos;

                        DPRINT2(("OVLFS: ovlfs_dir_readdir: filldir returned "
                                 "error %d on ent no %d.\n", error,
                                 cur_ent_no));
                    }
                }
            }
            break;
    }

    update_time(inode);

    DPRINTR2("inode %ld = %d; pos %d\n", inode->i_ino, error,
             (int) filp->f_pos);

    return  error;
}


struct file_operations  ovlfs_dir_fops = {
    readdir:	ovlfs_dir_readdir,
		NULL,
} ;
