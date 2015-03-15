/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998-2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_ino.c
 **
 ** DESCRIPTION: This file contains the inode operations for the overlay
 **              (i.e. psuedo) filesystem.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 09/27/1997	art            	Added to source code control.
 ** 02/19/1998	ARTHUR N.	Moved ovlfs_get_super() into ovl_kern.c.
 ** 02/21/1998	ARTHUR N.	Added locking of inodes that are retrieved
 **				 and used here.
 ** 02/24/1998	ARTHUR N.	Do not always lock the inodes; only wait
 **				 until they are no longer locked.
 ** 02/24/1998	ARTHUR N.	Added support for hiding the magic directory
 **				 names from the directory listing.
 ** 02/26/1998	ARTHUR N.	Removed old unused code.
 ** 02/26/1998	ARTHUR N.	Updated four more fields in the retained inode
 **				 information.
 ** 02/26/1998	ARTHUR N.	Lock inodes when their contents may change,
 **				 as done in the VFS.
 ** 02/26/1998	ARTHUR N.	Added the inode operations, link(), bmap(),
 **				 permission(), follow_link(), and readpage().
 ** 02/26/1998	ARTHUR N.	Update ovlfs_make_hierarchy() so that new
 **				 storage directories are referenced by their
 **				 associated storage information.
 ** 02/28/1998	ARTHUR N.	Mark the superblock as dirty after any changes
 **				 that affect the storage.
 ** 02/28/1998	ARTHUR N.	Do not return an error from unlink() if the
 **				 file does not exist in the storage fs.
 ** 03/01/1998	ARTHUR N.	Added support for the magic directory name
 **				 options.
 ** 03/01/1998	ARTHUR N.	Only define ck_magic_name if USE_MAGIC is set.
 ** 03/01/1998	ARTHUR N.	Reworked ovlfs_rename() slightly.  Set the
 **				 mtime and ctime in ovlfs_rename().
 ** 03/01/1998	ARTHUR N.	Don't return empty, unlinked directories from
 **				 resolve_inode().
 ** 03/01/1998	ARTHUR N.	Update the link counts of directories and
 **				 files affected by rmdir() and unlink() calls.
 **				 Also, lock the storage dir before calling
 **				 its rmdir() operation.
 ** 03/01/1998	ARTHUR N.	In ovlfs_rmdir(), perform the lookup of the
 **				 pseudo fs' inode for the directory to be
 **				 removed before calling the stg fs' rmdir().
 ** 03/02/1998	ARTHUR N.	Return -EXDEV from rename() if the file in
 **				 the storage fs is crossing a mount point. 
 ** 03/03/1998	ARTHUR N.	If OVLFS_ALLOW_XDEV_RENAME is set, remember
 **				 the rename of a file, that would cross a mount
 **				 point in the storage fs, by marking its new
 **				 location within the overlay fs' storage.
 **				 (note that the storage fs will not look as
 **				 expected; the file will stay where it was).
 ** 03/03/1998	ARTHUR N.	Added support for the noxmnt option.
 ** 03/09/1998	ARTHUR N.	Added the copyright notice.
 ** 03/09/1998	ARTHUR N.	Update the ctime and mtime attributes of
 **				 directories on the rmdir() and unlink() ops.
 ** 03/10/1998	ARTHUR N.	Move the update of the inode times in the
 **				 unlink() op so that they are updated even if
 **				 the file does not exist in the storage fs.
 ** 03/10/1998	ARTHUR N.	Improved the comments.
 ** 03/29/1998	ARTHUR N.	Updated to support storage operations.
 ** 08/23/1998	ARTHUR N.	Moved the update of inode attributes into the
 **				 storage filesystem and added the reference
 **				 inode to the storage "add inode" operation.
 ** 02/02/1999	ARTHUR N.	Changed follow_link() so that symbolic links
 **				 in one filesystem will work with files in the
 **				 other filesystem.
 ** 02/04/1999	ARTHUR N.	Removed half-baked code in ovlfs_mk_hierarchy
 **				 that retrieved the base fs inode but did not
 **				 even use it (it is not needed anyway).
 ** 02/06/1999	ARTHUR N.	Added use of the check_dir_update() function.
 **				 Also, added many debugging statements.
 ** 02/06/1999	ARTHUR N.	Removed use of check_dir_update().  Reworked
 **				 the Storage System interface.
 ** 02/07/1999	ARTHUR N.	Update the inode information structure's
 **				 storage mapping.
 ** 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
 ** 02/24/2003	ARTHUR N.	Various bug fixes and code restructuring.
 ** 02/24/2003	ARTHUR N.	Fixed ovlfs_lookup() when no entry is found.
 **				 Also, fixed rmdir() on non-empty dirs.
 ** 02/27/2003	ARTHUR N.	Prevent return of reference roots for non-root
 **				 ovlfs inodes.
 ** 03/06/2003	ARTHUR N.	Replaced recursive inode resolution with
 **				 iterative one.  Also, removed incorrect code
 **				 in ovlfs_create which saved the created
 **				 reference dentry in the directory's info.
 **				 Created ovlfs_attach_reference and use when
 **				 references are retrieved by name.
 ** 03/10/2003	ARTHUR N.	Added disabled base reference flag.  Also,
 **				 added detection of bad inodes retrieved with
 **				 iget().
 ** 03/11/2003	ARTHUR N.	Detect inodes with lost references and delete
 **				 their directory entries.
 ** 03/11/2003	ARTHUR N.	Add flags argument to add_inode storage op and
 **				 create new files with base references
 **				 disabled.
 ** 03/11/2003	ARTHUR N.	Split code into ovl_misc.c and ovl_ref.c.
 ** 03/11/2003	ARTHUR N.	Unlink relinked entries when references are
 **				 lost, and delete others.
 ** 03/13/2003	ARTHUR N.	Added use of d_instantiate() to ovlfs_link().
 ** 03/13/2003	ARTHUR N.	Corrected iput use in ovlfs_link when no
 **				 reference inode exists in the storage fs.
 ** 03/13/2003	ARTHUR N.	Allow rename to replace existing target when
 **				 crossing directories.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_ino.c	1.41	[03/07/27 22:20:34]"

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
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/slab.h>
#include <asm/statfs.h>
#include <linux/dcache.h>

#include "ovl_debug.h"
#include "ovl_fs.h"
#include "ovl_stg.h"
#include "ovl_inlines.h"

#if POST_20_KERNEL_F
# include <asm/uaccess.h>
#endif

#if PROC_DONT_IGET_F
# include <linux/proc_fs.h>
#endif



#if POST_20_KERNEL_F

/**
 ** FUNCTION: release_page_io_file
 **
 **  PURPOSE: Release the reference page I/O file for the given inode.
 **
 ** RULES:
 **	- The page I/O file must be unlocked.
 **	- If a file is specified, the reference will only be released if it
 **	  matches the file given.
 **/

void	release_page_io_file (ovlfs_inode_info_t *i_info)
{

		/* Lock page_io_file access. */

	down(&(i_info->page_io_file_sem));

		/* Put the page I/O file. */

	if ( i_info->page_io_file != NULL )
	{
		fput(i_info->page_io_file);
		i_info->page_io_file = NULL;
	}

	up(&(i_info->page_io_file_sem));
}



/**
 ** FUNCTION: ovlfs_open_ref_file
 **
 **  PURPOSE: Given an ovlfs inode, and flags and mode for opening the file,
 **           open the base or storage file and return the reference.
 **
 ** NOTES:
 **	- Even if the flags include writing, the file may reference the base
 **	  filesystem's inode.  This allows the file to be copied only when it
 **	  really must.
 **
 **	- The file will not be created in the storage filesystem if it does
 **	  not already exist.
 **/

static int	ovlfs_open_ref_file (struct inode *inode, int flags,
		                     struct file **r_file, char *r_which)
{
	struct dentry	*o_dent;
	struct file	*result;
	char		which;
	int		ret;

		/* Get the dentry from the storage fs, if it exists. */

	which = 'o';
	ret = ovlfs_resolve_stg_dentry(inode, &o_dent);

		/* If not, go to the base filesystem. */

	if ( ret < 0 )
	{
		which = 'b';
		ret = ovlfs_resolve_base_dentry(inode, &o_dent);

		if ( ret < 0 )
			goto	ovlfs_open_ref_file__out;
	}

		/* Open the dentry now.  Note that ovlfs_open_dentry does */
		/*  not consume the dentry.                               */

	ret = ovlfs_open_dentry(o_dent, flags, &result);

	if ( ret == 0 )
	{
		r_file[0]  = result;
		r_which[0] = which;
	}

	dput(o_dent);

ovlfs_open_ref_file__out:

	return	ret;
}



/**
 ** FUNCTION: get_page_io_file
 **
 ** RULES:
 **	- Caller must call put_page_io_file() with the file returned.
 **/

static int	get_page_io_file (struct inode *inode, int write_f,
		                  struct file **r_file)
{
	struct inode		*base_i;
	struct dentry		*new_ent;
	ovlfs_inode_info_t	*i_info;
	struct file		*new_file;
	int			flags;
	int			rc;
	int			ret;

	DPRINTC2("ino %lu, write_f %d, r_file 0x%08x\n", inode->i_ino, write_f,
	         (int) r_file);

	rc = 0;
	i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;

		/* Lock page_io_file access.  This may block! */

	down(&(i_info->page_io_file_sem));

		/* There are two sets of cases here:                        */
		/*      A. No page I/O file has yet been defined.           */
		/*      B. A page I/O file has already been defined.        */
		/*      1. Write access is needed, but the reference file   */
		/*         is in the base filesystem.                       */
		/*      2. Write access is not needed or the reference file */
		/*         is not in the base filesystem.                   */

		/* Get a page file now if none is set, then check for the */
		/*  need to copy-on-write.                                */

	if ( i_info->page_io_file == NULL )
	{
		ret = -ENOENT;

			/* Always request read access. */

		flags = FMODE_READ;

		if ( write_f )
			flags |= FMODE_WRITE;

		ret = ovlfs_open_ref_file(inode, flags,
		                          &(i_info->page_io_file),
		                          &(i_info->page_file_which));

		if ( ret < 0 )
			goto	get_page_io_file__unlock_out;
	}

		/* OK, we have a file set.  Check whether copy-on-write is */
		/*  needed.                                                */

		/* TBD: add support for the storage system's write...      */

	if ( ( i_info->page_file_which == 'b' ) && ( write_f ) )
	{
		ret = -EIO;

			/* Grab the base filesystem's inode. */

		rc = ovlfs_resolve_base_inode(inode, &base_i);

		if ( rc != 0 )
			goto	get_page_io_file__unlock_out;


			/* Copy the contents to the storage filesystem. */

		rc = ovlfs_inode_copy_file(inode, base_i, &new_ent);

		IPUT(base_i);

		if ( rc < 0 )
			goto	get_page_io_file__unlock_out;


			/* Open the new file using the same flags. */

		rc = ovlfs_open_dentry(new_ent, i_info->page_io_file->f_flags,
		                       &new_file);

		dput(new_ent);

		if ( rc < 0 )
			goto	get_page_io_file__unlock_out;


			/* Release the existing file and attach the new one. */

		fput(i_info->page_io_file);

		i_info->page_io_file    = new_file;
		i_info->page_file_which = 'o';
	}

		/* Now just take the file we have. */

	ret = 0;
	get_file(i_info->page_io_file);
	r_file[0] = i_info->page_io_file;

get_page_io_file__unlock_out:

		/* Unlock; from this point forward, the caller will use the  */
		/*  reference given.  If another thread comes here and uses  */
		/*  a different file, the data for the threads could be out- */
		/*  of-sync, but I expect the kernel to handle the locking   */
		/*  for this (e.g. page is locked on reads and writes).      */

	up(&(i_info->page_io_file_sem));

	DPRINTR2("ino %lu, write_f %d, r_file 0x%08x; func ret = %d, rc = %d\n",
	         inode->i_ino, write_f, (int) r_file, ret, rc);

	return	ret;
}



/**
 ** FUNCTION: put_page_io_file
 **
 ** RULES:
 **	- Only use as cleanup on a file returned from get_page_io_file().
 **/

static void	put_page_io_file (struct file *file)
{
	fput(file);
}

#endif



/**
 ** FUNCTION: ovlfs_make_hierarchy
 **
 **  PURPOSE: Create all of the ancestor directories of the given directory
 **           in the storage filesystem and return a reference to the storage
 **           inode referring to the given directory inode.
 **
 ** RULES:
 **	- Only creates directories in the storage filesystem.
 **/

int	ovlfs_make_hierarchy (struct inode *dir, struct inode **r_inode,
	                      int tbd)
{
	ovlfs_inode_info_t	*i_info;
	struct dentry		*o_dent;
	int			flags;
	int			ret_code;

	DPRINTC2("dir %lu, r_inode 0x%08x\n", dir->i_ino, (int) r_inode);

		/* Make certain the inode given is an ovlfs inode and that */
		/*  it is a directory inode also.                          */

	ret_code = -EINVAL;
	if ( ( dir == INODE_NULL ) || ( ! is_ovlfs_inode(dir) ) )
		goto	ovlfs_make_hierarchy__out;

	ret_code = -ENOTDIR;
	if ( ( ! S_ISDIR(dir->i_mode) ) || ( ! dir->i_op->mkdir ) )
		goto	ovlfs_make_hierarchy__out;


	ret_code = -ENOENT;
	i_info = (ovlfs_inode_info_t *) dir->u.generic_ip;
	if ( ( i_info == NULL ) || ( i_info->s.name == (char *) NULL ) ||
	     ( i_info->s.len <= 0 ) ||  ( i_info->s.parent_ino == dir->i_ino ) )
		goto	ovlfs_make_hierarchy__out;


		/* Resolve this inode, creating its ancestors and itself, */
		/*  as-needed.                                            */

	flags = OVLFS_MAKE_HIER | OVLFS_MAKE_LAST;
	ret_code = ovlfs_resolve_dentry_hier(dir, &o_dent, 's', flags);
	if ( ret_code != 0 )
		goto	ovlfs_make_hierarchy__out;


		/* Produce and return the inode. */

	IMARK(o_dent->d_inode);
	r_inode[0] = o_dent->d_inode;
	dput(o_dent);

ovlfs_make_hierarchy__out:
	DPRINTR2("dir %lu, r_inode 0x%08x, ret_code = %d\n", dir->i_ino,
	         (int) r_inode, ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: ovlfs_add_dirent_op
 **
 **  PURPOSE: Given the inode of a directory, the name of a node to create in
 **           the directory, and a callback function to perform the actual
 **           creation in the storage filesystem, do the following:
 **
 **               1. Make certain that the directory hierarchy needed in the
 **                  storage filesystem exists,
 **
 **               2. Add the directory entry to the ovlfs directory inode using
 **                  the inode created in the storage fs to determine the file
 **                  mode and other information.
 **
 ** NOTES:
 **    - This method is dependent on using the storage filesystem.  This will
 **      need to be changed to allow for the NOSTORAGE option.
 **/

typedef int (*dirop_cb_t)(struct inode *, void *, const char *, int);

#if POST_20_KERNEL_F
static int  ovlfs_add_dirent_op (struct inode *inode, const char *fname,
                                 int len, struct dentry *d_new_ent,
                                 dirop_cb_t dirop_cb, void *cb_data)
#else
static int  ovlfs_add_dirent_op (struct inode *inode, const char *fname,
                                 int len, dirop_cb_t dirop_cb, void *cb_data)
#endif
{
#if POST_20_KERNEL_F
	struct inode		*new_inode;
#endif
	struct inode		*o_inode;
	struct inode		*new_r_ent;
	_ulong			new_inum;
	int			ret;

	DPRINTC2("ino %ld, name %.*s, len %d, func 0x%p, data 0x%p\n",
                 inode->i_ino, len, fname, len, dirop_cb, cb_data);

#if ! FASTER_AND_MORE_DANGEROUS

	if ( ( inode == INODE_NULL ) || ( dirop_cb == NULL ) )
	{
		DPRINT1(("OVLFS: ovlfs_add_dirent_op: called with null "
		         "argument\n"));

		ret = -ENOTDIR;
		goto ovlfs_add_dirent_op__out;
	}

#endif

		/* Inode is the directory; obtain the storage fs's inode */

	ret = ovlfs_resolve_stg_inode(inode, &o_inode);

	if ( ret < 0 )
	{
			/* The parent does not appear to have an inode in */
			/*  the storage fs; try to create it.             */

		ret = ovlfs_make_hierarchy(inode, &o_inode, 0);
	}

	if ( ret < 0 )
		goto	ovlfs_add_dirent_op__out_inode;


		/* Mark the storage fs' inode as in-use so we can update it. */

	ret = ovlfs_use_inode(o_inode);
	if ( ret < 0 )
		goto	ovlfs_add_dirent_op__out_o_inode;


		/* Make sure the storage fs' inode has the lookup operation */

	if ( ( o_inode->i_op == NULL ) || ( o_inode->i_op->lookup == NULL ) )
	{
		ret = -ENOTDIR;
		goto	ovlfs_add_dirent_op__out_o_inode;
	}


		/* Now use the callback to let the caller perform the      */
		/*  specific operation on the storage filesystem directory */

	ret = dirop_cb(o_inode, cb_data, fname, len);
	if ( ret < 0 )
		goto	ovlfs_add_dirent_op__out_o_inode;


		/* Success; retrieve the new inode now. */

	ret = do_lookup(o_inode, fname, len, &new_r_ent, TRUE);
	if ( ret < 0 )
		goto	ovlfs_add_dirent_op__out_o_inode;


		/* The entry was created successfully; add it to storage. */

	ret = do_add_inode_op(inode->i_sb, inode->i_ino, fname, len, &new_inum,
	                      OVL_IFLAG__NO_BASE_REF);
	if ( ret < 0 )
		goto	ovlfs_add_dirent_op__out_new_r_ent;

		/* Good, we added the new storage fs inode and the new ovlfs */
		/*  inode.  Do not worry here about updating the ovlfs       */
		/*  inode's attributes; that will get done as needed later.  */

		/* Set the mapping from the new ovlfs inode to the new */
		/*  storage filesystem inode.                          */

	ret = do_map_inode(inode->i_sb, new_inum, new_r_ent->i_dev,
	                   new_r_ent->i_ino, 's');
	if ( ret < 0 )
		goto	ovlfs_add_dirent_op__out_new_r_ent;


		/* Finally, add the directory entry in the pseudo fs */
		/*  for the new inode.                               */

	ret = do_add_dirent(inode, fname, len, new_inum);
	if ( ret < 0 )
		goto	ovlfs_add_dirent_op__out_new_r_ent;

	inode->i_mtime = ( inode->i_ctime = CURRENT_TIME );
	INODE_MARK_DIRTY(inode);

#if POST_20_KERNEL_F
		/* Finally, attach the new inode to the dentry. */

	new_inode = do_iget(inode->i_sb, new_inum, 0);

	if ( new_inode == NULL )
	{
		DPRINT1(("OVLFS: %s - iget(0x%08x, %lu) failed to return "
		         "inode just created!\n", __FUNCTION__,
		         (int) inode->i_sb, new_inum));
	}
	else
	{
		d_instantiate(d_new_ent, new_inode);
	}
#endif

ovlfs_add_dirent_op__out_new_r_ent:
	IPUT(new_r_ent);

ovlfs_add_dirent_op__out_o_inode:
	IPUT(o_inode);

ovlfs_add_dirent_op__out_inode:

	IPUT20(inode);

ovlfs_add_dirent_op__out:

	DPRINTR2("ino %ld, name %.*s, len %d, func 0x%p, data 0x%p; ret = %d\n",
                 inode->i_ino, len, fname, len, dirop_cb, cb_data, ret);

	return	ret;
}



/**
 ** FUNCTION: ovlfs_get_parent_dir
 **
 **  PURPOSE: Given an ovlfs inode, obtain the ovlfs inode of the inode's
 **           parent directory.  Note that this will always be correct for
 **           directories, but not for files since files can reside in many
 **           directories.
 **
 ** NOTES:
 **     - If the parent directory can not be determined, NULL is returned.
 **/

struct inode    *ovlfs_get_parent_dir (struct inode *inode)
{
    ovlfs_inode_info_t  *i_info;
    struct inode        *result;

#if ! FASTER_AND_MORE_DANGEROUS
    if ( ( inode == INODE_NULL ) || ( ! is_ovlfs_inode(inode) ) )
    {
        DPRINT1(("OVLFS: ovlfs_get_parent_dir: invalid ovlfs inode given at "
                 "0x%x\n", (int) inode));

        return  INODE_NULL;
    }
#endif

        /* First see if the parent directory's inode is in the inode */
        /*  information structure of this inode.                     */

    result = INODE_NULL;

    if ( inode->u.generic_ip != NULL )
    {
        i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;

#if REF_DENTRY_F
        result = NULL;
#else
        result = i_info->ovlfs_dir
#endif

        if ( result == INODE_NULL )
        {
            if ( i_info->s.parent_ino != 0 )
                result = do_iget(inode->i_sb, i_info->s.parent_ino,
                                 ovlfs_sb_xmnt(inode->i_sb));

            if ( result != INODE_NULL )
            {
#if ! REF_DENTRY_F
                    /* Update the count and store in the inode's information */
                    /*  structure.                                           */
                IMARK(result);
                i_info->ovlfs_dir = result;
#endif
            }
        }
    }

    return  result;
}



#if USE_MAGIC

/**
 ** FUNCTION: ck_magic_name
 **
 **  PURPOSE: Determine if the name given is the name of the specified magic
 **           directory.
 **
 ** NOTES:
 **     - This function must only be called with a valid ovlfs super block.
 **/

static inline int   ck_magic_name (struct super_block *sb, const char *name,
                                   int len, char which)
{
    char    *m_name = NULL;
    int     m_len = 0;

    switch ( which )
    {
        case 'o':
        case 's':
            m_name = ovlfs_sb_opt(sb, Smagic_name);
            m_len = ovlfs_sb_opt(sb, Smagic_len);

            if ( ( m_name == (char *) NULL ) || ( m_len <= 0 ) )
            {
                m_name = OVLFS_MAGIC_NAME;
                m_len = OVLFS_MAGIC_NAME_LEN;
            }
            break;

        default:
            m_name = ovlfs_sb_opt(sb, Bmagic_name);
            m_len = ovlfs_sb_opt(sb, Bmagic_len);

            if ( ( m_name == (char *) NULL ) || ( m_len <= 0 ) )
            {
                m_name = OVLFS_MAGICR_NAME;
                m_len = OVLFS_MAGICR_NAME_LEN;
            }
            break;
    }

        /* Check if the length and name match and return the result */

    return  ( ( len == m_len ) && ( strncmp(name, m_name, len) == 0 ) );
}

#endif



/**
 ** FUNCTION: ck_inode_info
 **
 ** RULES:
 **	- The parent inode number stored in the information structure must
 **	  already be set and should match the parent used.
 **/

static void	ck_inode_info (struct inode *inode, struct inode *parent,
		               const char *fname, int len)
{
	ovlfs_inode_info_t	*info;

	info = (ovlfs_inode_info_t *) inode->u.generic_ip;

#if STORE_REF_INODES
# if ! REF_DENTRY_F
	if ( info->ovlfs_dir == NULL )
		info->ovlfs_dir = parent;
# endif
#endif

	if ( ( info->s.name == NULL ) && ( fname != NULL ) && ( len > 0 ) )
	{
		info->s.name = MALLOC(len + 1);

		if ( info->s.name != NULL )
		{
			memcpy(info->s.name, fname, len);
			info->s.name[len] = '\0';

			info->s.len = len;
		}
	}
}



#if POST_20_KERNEL_F

/**
 ** FUNCTION: ovlfs_lookup
 **
 **  PURPOSE: Perform the lookup inode directory operation.
 **
 ** NOTES:
 **	- This function MUST return 0 on success.
 **/

static struct dentry    *ovlfs_lookup (struct inode *inode,
                                       struct dentry *d_ent)
{
	const char		*fname;
	int			len;
	struct inode		*result;
	ovlfs_inode_info_t	*i_info;
	_ulong			inum;
	int			flags;
	int			unlink_ind;
	int			error;
	int			rc;
	struct dentry		*ret_code;

	result = NULL;

	fname = d_ent->d_name.name;
	len   = d_ent->d_name.len;

	DPRINTC2("ino %ld, name %.*s, len %d\n", inode->i_ino, len, fname,
	         len);

#if ! FASTER_AND_MORE_DANGEROUS
	if ( ! is_ovlfs_inode(inode) )
	{
		DPRINT1(("OVLFS: ovlfs_lookup called with non-ovlfs inode!\n"));

		error = -EINVAL;
		goto ovlfs_lookup__err_out;
	}
#endif

	error = -ENOTDIR;
	if ( inode == (struct inode *) NULL )
		goto ovlfs_lookup__err_out;
	if ( ! S_ISDIR(inode->i_mode) )
		goto ovlfs_lookup__err_out;


		/* Determine the correct inode from the name. */

	if ( len == 0 )
	{
			/* Null names mean "me"; this should never happen... */

		result = inode;
		IMARK(result);
		goto	ovlfs_lookup__out_inst;
	}

#if USE_MAGIC
	if ( ( ovlfs_magic_opt_p(inode->i_sb, OVLFS_OVL_MAGIC) ) &&
	     ( ck_magic_name(inode->i_sb, fname, len, 'o') ) )
	{
		error = ovlfs_resolve_stg_dentry(inode, &ret_code);

		if ( error < 0 )
			goto	ovlfs_lookup__err_out;

		goto	ovlfs_lookup__out;
	}

	if ( ( ovlfs_magic_opt_p(inode->i_sb, OVLFS_BASE_MAGIC) ) &&
	     ( ck_magic_name(inode->i_sb, fname, len, 'b') ) )
	{
		error = ovlfs_resolve_base_dentry(inode, &ret_code);

		if ( error < 0 )
			goto	ovlfs_lookup__err_out;

		goto	ovlfs_lookup__out;
	}
#endif

		/* Lookup the directory entry from storage */

	flags = 0;
	error = do_lookup_op(inode, fname, len, &inum, &flags);
	unlink_ind = ( flags & OVLFS_DIRENT_UNLINKED );

	if ( error == -ENOENT)
	{
			/* It is not there; look for the entry now in case */
			/*  it was created since the last look.            */

		unlink_ind = FALSE;
		error = update_dir_entry(inode, fname, len, &inum);


			/* If ENOENT continues, jump to the d_add() logic; */
			/*  this is necessary for the VFS to handle file   */
			/*  and directory creation properly.               */

		if ( error == -ENOENT )
			goto	ovlfs_lookup__out_add;
	}
	if ( error < 0 )
		goto	ovlfs_lookup__err_out;


		/* If the entry is marked as unlinked, take the ENOENT path */

	if ( unlink_ind )
		goto	ovlfs_lookup__out_add;


		/* Retrieve the inode now.  Ignore the "cross mount point" */
		/*  option since this is an ovlfs inode we want.           */

	result = do_iget(inode->i_sb, inum, 0);
	ASSERT(result != NULL);


		/* Check that the inode has valid references. */

	if ( ! inode_refs_valid_p(result) )
	{
			/* Inode lost its references, delete the directory  */
			/*  entry; note that relinked entries must change   */
			/*  back to being unlinked, and all others must be  */
			/*  deleted for real.                               */

		if ( flags & OVLFS_DIRENT_RELINKED )
			rc = do_unlink_op(inode, fname, len);
		else
			rc = do_delete_dirent_op(inode, fname, len);

		if ( rc != 0 )
		{
			DPRINT1(("OVLFS: failed to remove dirent %.*s len %d "
			         "in dir ino %lu; rc = %d\n", len, fname, len,
			         inode->i_ino, rc));
		}

		IPUT(result);
		result = NULL;

			/* Now take the ENOENT path */

		goto	ovlfs_lookup__out_add;
	}


		/* Set the parent reference and saved name for the returned */
		/*  inode, if they are not already set.                     */

	ck_inode_info(result, inode, fname, len);


		/* If the directory entry is a relinked entry, don't use */
		/*  a base filesystem reference for this inode.          */

		/* TBD: how about the time between the call to iget and */
		/*      here?  Could another task use the inode without */
		/*      benefit of this setting?                        */

	if ( flags & OVLFS_DIRENT_RELINKED )
	{
		i_info = (ovlfs_inode_info_t *) result->u.generic_ip;
		i_info->s.flags |= OVL_IFLAG__NO_BASE_REF;
	}


		/* NOTE: d_instantiate() can be used here to keep the dentry */
		/*  out of the hash table.                                   */

ovlfs_lookup__out_add:

	d_ent->d_op = &ovlfs_dentry_ops;
	d_add(d_ent, result);

	ret_code = NULL;
	goto	ovlfs_lookup__out;

ovlfs_lookup__out_inst:
	DPRINTR2("ino %ld, name %.*s, len %d; success\n", inode->i_ino, len,
	         fname, len);

	d_instantiate(d_ent, result);
	ret_code = NULL;
	goto	ovlfs_lookup__out;

ovlfs_lookup__err_out:

		/* If the error is "not found", just return NULL and leave  */
		/*  the dentry unattached; this is necessary for the VFS to */
		/*  handle creation properly.                               */

	if ( error == -ENOENT )
		ret_code = NULL;
	else
		ret_code = ERR_PTR(error);

ovlfs_lookup__out:
	DPRINTR2("ino %ld, name %.*s, len %d; ret %d = 0x%08x\n", inode->i_ino,
	         len, fname, len, (int) ret_code, (int) ret_code);

	return	ret_code;
}

#else

	/*                 */
	/* Pre-2.2 version */
	/*                 */

static int  ovlfs_lookup (struct inode *inode, const char *fname, int len,
                          struct inode **r_inode)
{
	struct inode	*result;
	_ulong		inum;
	int		flags;
	int		unlink_ind;
	int		error;

	DPRINTC2("ino %ld, name %.*s, len %d\n", inode->i_ino, len, fname,
	         len);

#if ! FASTER_AND_MORE_DANGEROUS

	if ( ! is_ovlfs_inode(inode) )
	{
		DPRINT1(("OVLFS: ovlfs_lookup called with non-ovlfs inode!\n"));

		error = -EINVAL;
		goto ovlfs_lookup__err_out;
	}

#endif

	r_inode[0] = (struct inode *) NULL;

	error = -ENOTDIR;

	if ( inode == (struct inode *) NULL )
		goto ovlfs_lookup__err_out;

	if ( ! S_ISDIR(inode->i_mode) )
		goto ovlfs_lookup__err_out;


		/* Determine the correct inode from the name. */

	if ( len == 0 )
	{
			/* Null names mean "me" */

		result = inode;
		IMARK(result);
		goto	ovlfs_lookup__out;
	}

	if ( ( fname[0] == '.' ) &&
	     ( (len == 1) || (( len == 2 ) && ( fname[1] == '.' )) ) )
	{
			/* The name is either "." or ".." if we get here. */

		if ( len == 1 )
		{
			result = inode;
			IMARK(result);
		}
		else
		{
				/* Get the parent directory of this one. */

			result = ovlfs_get_parent_dir(inode);

			if ( result == INODE_NULL )
			{
					/* No parent, just return me again. */
				result = inode;
				IMARK(result);
			}
		}

		goto	ovlfs_lookup__out;
	}

#if USE_MAGIC
	if ( ( ovlfs_magic_opt_p(inode->i_sb, OVLFS_OVL_MAGIC) ) &&
	     ( ck_magic_name(inode->i_sb, fname, len, 'o') ) )
	{
		error = ovlfs_resolve_stg_inode(inode, &result);

		if ( error < 0 )
			goto	ovlfs_lookup__err_out;

		goto	ovlfs_lookup__out;
	}

	if ( ( ovlfs_magic_opt_p(inode->i_sb, OVLFS_BASE_MAGIC) ) &&
	     ( ck_magic_name(inode->i_sb, fname, len, 'b') ) )
	{
		error = ovlfs_resolve_base_inode(inode, &result);

		if ( error < 0 )
			goto	ovlfs_lookup__err_out;

		goto	ovlfs_lookup__out;
	}
#endif

		/* Lookup the directory entry from storage */

	flags = 0;
	error = do_lookup_op(inode, fname, len, &inum, &flags);
	unlink_ind = ( flags & OVLFS_DIRENT_UNLINKED );

	if ( error == -ENOENT)
	{
			/* It is not there; look for the entry now in case */
			/*  it was created since the last look.            */

		error = update_dir_entry(inode, fname, len, &result);
		if ( error == 0 )
			goto	ovlfs_lookup__out;
	}
	if ( error < 0 )
		goto	ovlfs_lookup__err_out;


		/* If the entry is marked as unlinked, return the error. */

	if ( unlink_ind )
	{
		error = -ENOENT;
		goto	ovlfs_lookup__err_out;
	}

		/* Retrieve the inode now.  Ignore the "cross mount point" */
		/*  option since this is an ovlfs inode we want.           */

	error = -ENOENT;
	result = do_iget(inode->i_sb, inum, 0);
	if ( result == INODE_NULL )
		goto	ovlfs_lookup__err_out;


		/* Set the parent reference and saved name for the returned */
		/*  inode, if they are not already set.                     */

	ck_inode_info(result, inode, fname, len);

ovlfs_lookup__out:
	DPRINTR2("ino %ld, name %.*s, len %d; success\n", inode->i_ino, len,
	         fname, len);

	IPUT(inode);
	r_inode[0] = result;
	return	0;

ovlfs_lookup__err_out:
	DPRINTR2("ino %ld, name %.*s, len %d; error %d\n", inode->i_ino, len,
	         fname, len, error);

	IPUT(inode);
	return	error;
}

#endif



/**
 ** FUNCTION: ovlfs_link
 **
 **  PURPOSE: This is the inode link() method for the overlay filesystem.
 **/

#if POST_20_KERNEL_F
static int	ovlfs_link (struct dentry *src_ent, struct inode *dir,
		            struct dentry *dest_ent)
#else
static int	ovlfs_link (struct inode *orig_inode, struct inode *dir,
		            const char *name, int len)
#endif
{
#if POST_20_KERNEL_F
	const char	*name;
	int		len;
	struct inode	*orig_inode;
#endif
	int		ret;
	struct inode	*o_inode;
	struct inode	*o_dir;

#if POST_20_KERNEL_F
	orig_inode = src_ent->d_inode;
	name       = dest_ent->d_name.name;
	len        = dest_ent->d_name.len;
#endif

		/* Make sure the two inodes given are OVLFS inodes */

	if ( ( ! is_ovlfs_inode(orig_inode) ) || ( ! is_ovlfs_inode(dir) ) )
	{
		ret = -ENOTDIR;
		goto ovlfs_link__out;
	}


		/* Add the new entry into the overlay filesystem directory */

	ret = do_add_dirent(dir, name, len, orig_inode->i_ino);

	if ( ret < 0 )
		goto ovlfs_link__out;


		/* Update the link count for the original inode */

	orig_inode->i_nlink++;
	INODE_MARK_DIRTY(orig_inode);


		/* Perform the link in the storage fs, if possible.  Start   */
		/*  by obtaining the original file inode from storage fs and */
		/*  then obtain the directory inode if the original file's   */
		/*  inode is found.                                          */

	ret = ovlfs_resolve_stg_inode(orig_inode, &o_inode);

	if ( ret < 0 )
	{
		ret = 0;	/* It is ok not to have the stg inode */
		goto ovlfs_link__out_inst;
	}

	ret = ovlfs_resolve_stg_inode(dir, &o_dir);

		/* Did not find the directory, try to create it */

	if ( ret < 0 )
	{
		ret = ovlfs_make_hierarchy(dir, &o_dir, 0);

		if ( ret < 0 )
		{
			IPUT(o_inode);
			goto ovlfs_link__out;
		}
	}

	if ( ( o_dir->i_op == NULL ) || ( o_dir->i_op->link == NULL ) )
	{
		ret = -ENOTDIR;
		goto ovlfs_link__out_iput;
	}

	ret = do_link(o_inode, o_dir, name, len);

		/* TBD: consider flag to allow...  At least, should */
		/*      not return error and leave link!            */

	if ( ret < 0 )
		goto ovlfs_link__out_iput;

ovlfs_link__out_iput:
	IPUT(o_dir);
	IPUT(o_inode);

ovlfs_link__out_inst:

#if POST_20_KERNEL_F
	if ( ret >= 0 )
	{
		IMARK(orig_inode);
		d_instantiate(dest_ent, orig_inode);
	}
#endif


ovlfs_link__out:

	IPUT20(orig_inode);
	IPUT20(dir);

	return	ret;
}



/**
 ** FUNCTION: ovlfs_unlink
 **
 **  PURPOSE: This is the inode unlink method for the overlay filesystem.  The
 **           named file is removed from the specified directory inode.
 **/

#if POST_20_KERNEL_F
static int ovlfs_unlink (struct inode *d_inode, struct dentry *d_ent)
#else
static int ovlfs_unlink (struct inode *d_inode, const char *fname, int len)
#endif
{
#if POST_20_KERNEL_F
    const char      *fname;
    int             len;
#endif
    struct inode    *o_inode;
    struct inode    *f_inode;
    int             ret;

#if POST_20_KERNEL_F
    fname = d_ent->d_name.name;
    len   = d_ent->d_name.len;
#endif

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_unlink(%ld, %.*s, %d)\n",
             d_inode->i_ino, len, fname, len));

#if ! FASTER_AND_MORE_DANGEROUS
    if ( ! is_ovlfs_inode(d_inode) )
    {
        DPRINT1(("OVLFS: ovlfs_unlink called with non-ovlfs inode!\n"));

        IPUT20(d_inode);

        return  -EINVAL;
    }

    if ( ( d_inode->i_op == NULL ) || ( d_inode->i_op->lookup == NULL ) )
    {
        DPRINT1(("OVLFS: ovlfs_unlink called with non-directory inode?\n"));

        IPUT20(d_inode);

        return  -ENOTDIR;
    }
#endif


            /***
             *** Obtain the inode for the file being unlinked so that its link
             ***  count may be updated.  It is important to obtain the inode
             ***  from the VFS in case it is in use somewhere, and therefore is
             ***  in the inode cache.
             ***/

#if POST_20_KERNEL_F
    ret     = 0;
    f_inode = d_ent->d_inode;

    if ( f_inode == NULL )
        ret = -ENOENT;
#else
    ret = do_lookup(d_inode, fname, len, &f_inode, FALSE);
#endif

    if ( ret >= 0 )
    {
            /* Don't unlink directories; rmdir() must be used */

        if ( S_ISDIR(f_inode->i_mode) )
            ret = -EPERM;
        else
        {
                /* Don't set the link count below zero: */

            if ( f_inode->i_nlink > 0 )
                f_inode->i_nlink--;

            INODE_MARK_DIRTY(f_inode);

            ret = do_unlink_op(d_inode, fname, len);
        }

        IPUT20(f_inode);
    }


        /* Now perform the operation in the storage filesystem, if all is ok */

    if ( ret >= 0 )
    {
        ret = ovlfs_resolve_stg_inode(d_inode, &o_inode);

        if ( ret >= 0 )
        {
            ret = ovlfs_use_inode(o_inode);
    
            if ( ret >= 0 )
            {
                if ( ( o_inode->i_op != NULL ) &&
                     ( o_inode->i_op->unlink != NULL ) )
                {
                        /* do_unlink does not put the directory inode it is */
                        /*  given.                                          */
        
                    ret = do_unlink(o_inode, fname, len);
        
                        /* It is not an error if the file does not exist in */
                        /*  the storage filesystem.                         */
        
                    if ( ret == -ENOENT )
                        ret = 0;
                }
                else
                {
                    ret = -ENOTDIR;
                }
        
                ovlfs_release_inode(o_inode);
            }

            IPUT(o_inode);
        }
        else
            if ( ret == -ENOENT )
                ret = 0;    /* The storage fs DOESN'T NEED to have the dir */


        if ( ret >= 0 )
        {
            d_inode->i_ctime = ( d_inode->i_mtime = CURRENT_TIME );

            INODE_MARK_DIRTY(d_inode);
        }
    }

    IPUT20(d_inode);

    return  ret;
}



/**
 ** FUNCTION: mkdir_cb
 **
 **  PURPOSE: Given the directory inode in the storage filesystem, perform
 **           the actual mkdir on that inode.
 **
 ** NOTES:
 **     - This is a callback function for use with the ovlfs_add_dirent_op
 **       function.
 **/

static int  mkdir_cb (struct inode *inode, void *data, const char *name,
                      int len)
{
    int ret;
    int mode;

    mode = (int) data;

    if ( ( inode != INODE_NULL ) && ( inode->i_op != NULL ) &&
         ( inode->i_op->mkdir != NULL ) )
    {
        ret = do_mkdir(inode, name, len, mode);
    }
    else
        ret = -ENOTDIR;

    return  ret;
}



/**
 ** FUNCTION: ovlfs_mkdir
 **
 **  PURPOSE: This is the overlay filesystem's mkdir() inode operation.
 **/

#if POST_20_KERNEL_F

static int  ovlfs_mkdir (struct inode *inode, struct dentry *n_dent, int mode)
{
    return  ovlfs_add_dirent_op(inode, n_dent->d_name.name, n_dent->d_name.len,
                                n_dent, mkdir_cb, (void *) mode);
}

#else

static int  ovlfs_mkdir (struct inode *inode, const char *fname, int len,
                         int mode)
{
    return  ovlfs_add_dirent_op(inode, fname, len, mkdir_cb, (void *) mode);
}

#endif



/**
 ** FUNCTION: symlink_cb
 **
 **  PURPOSE: Given the directory inode in the storage filesystem, perform
 **           the actual symlink on that inode.
 **
 ** NOTES:
 **     - This is a callback function for use with the ovlfs_add_dirent_op
 **       function.
 **/

static int  symlink_cb (struct inode *inode, void *data, const char *name,
                        int len)
{
    int         ret;
    const char  *sym_name;

    sym_name = (const char *) data;

    if ( ( inode != INODE_NULL ) && ( inode->i_op != NULL ) &&
         ( inode->i_op->symlink != NULL ) )
    {
        ret = do_symlink(inode, name, len, sym_name);
    }
    else
        ret = -ENOTDIR;

    return  ret;
}



/**
 ** FUNCTION: ovlfs_symlink
 **
 **  PURPOSE: This is the overlay filesystem's symlink() inode operation.
 **/

#if POST_20_KERNEL_F

static int  ovlfs_symlink (struct inode *inode, struct dentry *new_dent,
                           const char *sym_name)
{
    int ret_code;

    ret_code = ovlfs_add_dirent_op(inode, new_dent->d_name.name,
                                   new_dent->d_name.len, new_dent, symlink_cb,
                                   (void *) sym_name);

    return  ret_code;
}

#else

static int  ovlfs_symlink (struct inode *inode, const char *fname, int len,
                           const char *sym_name)
{
    return  ovlfs_add_dirent_op(inode, fname, len, symlink_cb,
                                (void *) sym_name);
}

#endif



/**
 ** FUNCTION: create_cb
 **
 **  PURPOSE: Create a new directory entry in the given directory inode with
 **           the specified filename and status information.
 **
 ** NOTES:
 **     - This is a callback function for use with the ovlfs_add_dirent_op
 **       function.
 **/

#if POST_20_KERNEL_F

struct create_dat_struct {
    int             mode;
    struct dentry   *result;
} ;

int	do_create (struct inode *, const char *, int, int, struct dentry **);

static int  create_cb (struct inode *inode, void *ptr, const char *fname,
                       int len)
{
    int                         ret;
    struct create_dat_struct    *data;
    struct dentry               *new_dent;

    if ( ( inode == INODE_NULL ) || ( inode->i_op == NULL ) ||
         ( inode->i_op->create == NULL ) || ( ptr == NULL ) )
        return  -ENOTDIR;

    data = (struct create_dat_struct *) ptr;

    ret = do_create(inode, fname, len, data->mode, &new_dent);

    if ( ret >= 0 )
        data->result = new_dent;

    return  ret;
}

#else

struct create_dat_struct {
    int             mode;
    struct inode    *result;
} ;


	/* Pre-2.2 Version. */

int	do_create (struct inode *, const char *, int, int, struct inode **);

static int  create_cb (struct inode *inode, void *ptr, const char *fname,
                       int len)
{
    int                         ret;
    struct create_dat_struct    *data;
    struct inode                *new_inode;

    if ( ( inode == INODE_NULL ) || ( inode->i_op == NULL ) ||
         ( inode->i_op->create == NULL ) || ( ptr == NULL ) )
        return  -ENOTDIR;

    data = (struct create_dat_struct *) ptr;

    ret = do_create(inode, fname, len, data->mode, &new_inode);

    if ( ret >= 0 )
        data->result = new_inode;

    return  ret;
}

#endif



/**
 ** FUNCTION: ovlfs_create
 **
 **  PURPOSE: This is the overlay filesystem's create() inode operation.
 **/

#if POST_20_KERNEL_F

static int	ovlfs_create (struct inode *dir_inode, struct dentry *d_ent,
		              int mode)
{
	struct create_dat_struct	data;
	int				ret;

	data.mode   = mode;
	data.result = DENTRY_NULL;

		/* Let ovlfs_add_dirent_op() do most of the work. */

	ret = ovlfs_add_dirent_op(dir_inode, d_ent->d_name.name,
	                          d_ent->d_name.len, d_ent, create_cb,
	                          (void *) &data);

		/* Even if an error occurred, it may have been after the */
		/*  storage inode's create was called, in which case the */
		/*  the new dentry must be dput().                       */

	if ( data.result != DENTRY_NULL )
		dput(data.result);

	return	ret;
}

#else

	/* Pre-2.2 version */

static int  ovlfs_create (struct inode *inode, const char *fname, int len,
                          int mode, struct inode **result)
{
    struct create_dat_struct    data;
    int                         ret;

    data.mode = mode;
    data.result = INODE_NULL;

        /* Mark the inode since we still need it after this call */

    IMARK(inode);
    ret = ovlfs_add_dirent_op(inode, fname, len, create_cb, (void *) &data);


        /* If successful, obtain the new overlay fs inode */

    if ( ret >= 0 )
    {
        if ( ( inode->i_op != NULL ) && ( inode->i_op->lookup != NULL ) )
        {
            ret = do_lookup(inode, fname, len, result, FALSE);

    
                /* If we got the overlay fs inode, see if its storage inode */
                /*  reference exists; if not, set it to the new inode.      */
    
            if ( ( ret >= 0 ) && ( result[0]->u.generic_ip != NULL ) )
            {
                ovlfs_inode_info_t  *i_info;
    
                i_info = (ovlfs_inode_info_t *) result[0]->u.generic_ip;
    
                if ( i_info->overlay_inode == INODE_NULL )
                    i_info->overlay_inode = data.result;
                else
                    IPUT(data.result);
            }
            else
                IPUT(data.result);
        }
        else
        {
            IPUT(data.result);
            ret = -ENOTDIR;
        }
    }
    else
    {
            /* An error occurred, but it may have been after the storage */
            /*  inode's create was called, in which case the new inode   */
            /*  must be iput().                                          */

        if ( data.result != INODE_NULL )
            IPUT(data.result);
    }

    IPUT20(inode);

    return  ret;
}

#endif



/**
 ** FUNCTION: ovlfs_empty_dir_p
 **
 **  PURPOSE: Indicate whether the given directory is empty.
 **/

static int	ovlfs_empty_dir_p (struct inode *rm_dir)
{
	int	empty_f;
	int	rc;

	DPRINTC2("ino %lu\n", rm_dir->i_ino);

		/* Assume it is not empty and prove otherwise. */

	empty_f = FALSE;

		/* Get the number of entries in the directory, not including */
		/*  those that are marked as unlinked.                       */

	rc = do_num_dirent_op(rm_dir, FALSE);

	if ( rc == 0 )
		empty_f = TRUE;
	else if ( rc < 0 )
		DPRINT1(("OVLFS: %s: error %d on num_dirent_op for ino %lu\n",
		         __FUNCTION__, rc, rm_dir->i_ino));

	DPRINTR2("ino %lu\n", rm_dir->i_ino);

	return	empty_f;
}



/**
 ** FUNCTION: ovlfs_rmdir
 **
 **  PURPOSE: This is the overlay filesystem's rmdir() inode operation.
 **/

#if POST_20_KERNEL_F
static int  ovlfs_rmdir (struct inode *inode, struct dentry *d_ent)
#else
static int  ovlfs_rmdir (struct inode *inode, const char *dname, int len)
#endif
{
#if POST_20_KERNEL_F
    const char      *dname;
    int             len;
#endif
    struct inode    *o_inode;
    struct inode    *rm_dir;
    int             ret;

#if POST_20_KERNEL_F
    dname = d_ent->d_name.name;
    len   = d_ent->d_name.len;
#endif

    DPRINTC2("ino %ld, name %.*s, len %d\n", inode->i_ino, len, dname, len);

        /***
         *** Obtain the inode for the directory being unlinked so that its link
         ***  count may be updated.  It is important to obtain the inode from
         ***  the VFS in case it is in use somewhere, and therefore is in the
         ***  inode cache.
         ***
         *** The lookup is done before doing the storage fs' rmdir() call
         ***  because of a timing issue with the ext2 filesystem that causes
         ***  harmless error messages to be displayed indicating that a cleared
         ***  block and inode are being cleared again.
         ***/

#if POST_20_KERNEL_F
    ret    = 0;
    rm_dir = d_ent->d_inode;

    if ( rm_dir == NULL )
        ret = -ENOENT;
#else
    ret = do_lookup(inode, dname, len, &rm_dir, FALSE);
#endif

            /* Make sure it was found. */
    if ( ret < 0 )
        goto    ovlfs_rmdir__out;

        /* Check whether the directory is empty. */
    if ( ! ovlfs_empty_dir_p(rm_dir) )
    {
        ret = -ENOTEMPTY;
        goto    ovlfs_rmdir__out_rmdir;
    }


        /* Remove the directory from the storage filesystem; if the storage */
        /*  filesystem refuses to remove the directory (and it exists       */
        /*  there), then don't remove the directory from the pseudo fs.     */

        /*  note: inode is the directory */

    ret = ovlfs_resolve_stg_inode(inode, &o_inode);

    if ( ret >= 0 )
    {
        ret = ovlfs_use_inode(o_inode);

        if ( ret < 0 )
            IPUT(o_inode);
    }

    if ( ret >= 0 )
    {
        if ( ( o_inode->i_op != NULL ) && ( o_inode->i_op->rmdir != NULL ) )
        {
            ret = do_rmdir(o_inode, dname, len);
        }
        else
            ret = -ENOTDIR;

        ovlfs_release_inode(o_inode);
        IPUT(o_inode);
    }


        /* Remove the directory entry from the overlay fs inode information */
        /*  if the stg fs' rmdir() was successful, or the dir did not exist */

    if ( ( ret >= 0 ) || ( ret == -ENOENT ) )
    {
        ret = do_unlink_op(inode, dname, len);

            /* If successful, update the unlinked dir's link count */

        if ( ret >= 0 )
        {
                /* Don't set the link count below zero: */

            if ( rm_dir->i_nlink > 0 )
                rm_dir->i_nlink--;

            INODE_MARK_DIRTY(rm_dir);
            INODE_MARK_DIRTY(inode);

            inode->i_ctime = ( inode->i_mtime = CURRENT_TIME );
        }
    }


ovlfs_rmdir__out_rmdir:
    IPUT20(rm_dir);

ovlfs_rmdir__out:
    IPUT20(inode);

    DPRINTR2("ino %ld, name %.*s, len %d; ret = %d\n", inode->i_ino, len,
             dname, len, ret);

    return  ret;
}



/**
 ** FUNCTION: ovlfs_rename
 **
 **  PURPOSE: This is the overlay filesystem's rename() inode operation.
 **
 ** NOTES:
 **     - Must iput both directories (NOT >= 2.2.0)
 **     - The rename() operation for the storage filesystem MUST NOT be called
 **	  if the two directories are not on the same device.
 **
 ** ALGORITHM:
 **     - Determine if the old directory and the file exist in the storage
 **       filesystem.
 **     - If both exist there, obtain the new directory's storage inode, or
 **       create it if it does not exist.  Then, use the old directory's
 **       rename() operation to move the file in the storage filesystem.
 **     - If the rename() in the storage filesystem is successful, or the
 **       file doesn't exist in the storage filesystem, just rename the file
 **       in the pseudo filesystem's storage.
 **/

#if POST_20_KERNEL_F

static int  ovlfs_rename (struct inode *olddir, struct dentry *old_dent,
                          struct inode *newdir, struct dentry *new_dent)

#else

static int  ovlfs_rename (struct inode *olddir, const char *oname, int olen,
                          struct inode *newdir, const char *nname, int nlen,
                          int must_be_dir)

#endif

{
#if POST_20_KERNEL_F
    const char          *oname;
    int                 olen;
    const char          *nname;
    int                 nlen;
    int                 must_be_dir = 0;
#endif
    struct inode        *o_olddir;
    struct inode        *o_newdir;
    struct inode        *new_inode;
    struct inode        *ent_inode;
    ovlfs_inode_info_t  *i_info;
    int                 old_in_ovl;
    int                 ret;

#if POST_20_KERNEL_F
    oname = old_dent->d_name.name;
    olen  = old_dent->d_name.len;
    nname = new_dent->d_name.name;
    nlen  = new_dent->d_name.len;
#endif

    new_inode = INODE_NULL;

    DPRINTC2("old ino %ld, name %.*s, len %d, new ino %ld, name %.*s, len %d, "
             "%d\n", olddir->i_ino, olen, oname, olen, newdir->i_ino, nlen,
             nname, nlen, must_be_dir);


        /* Perform the rename in the storage filesystem if the entry  */
        /*  exists in the storage filesystem.                         */

    ret = ovlfs_resolve_stg_inode(olddir, &o_olddir);

    if ( ret >= 0 )
    {
        ret = ovlfs_use_inode(o_olddir);

        if ( ret < 0 )
            IPUT(o_olddir);
    }

    if ( ret >= 0 )
    {
            /* OK, the old directory exists in the storage fs, see if */
            /*  the file is there also.                               */

        if ( ( o_olddir->i_op != NULL ) && ( o_olddir->i_op->lookup != NULL ) )
        {
            struct inode    *tmp = NULL;

            ret = do_lookup(o_olddir, oname, olen, &tmp, TRUE);

                /* Just looking; don't "keep" this inode */

            if ( ret >= 0 )
                IPUT(tmp);
        }
        else
            ret = -ENOENT;


        if ( ret >= 0 )
        {
                /***
                 *** The file and the old directory are both in the storage
                 ***  filesystem, so it is necessary to rename the file in
                 ***  that filesystem.  Get the new directory's storage fs
                 ***  inode, or create it if is doesn't exist.
                 ***/

            ret = ovlfs_resolve_stg_inode(newdir, &o_newdir);
    
            if ( ret < 0 )
                ret = ovlfs_make_hierarchy(newdir, &o_newdir, 0);
    
            if ( ret >= 0 )
            {
                    /* Make sure the dir's are on the same device */

                if ( o_newdir->i_dev != o_olddir->i_dev )
                {
                    DPRINT1(("OVLFS: rename: old dir and new dir on different "
                             "devs; old 0x%x, new 0x%x\n", o_olddir->i_dev,
                             o_newdir->i_dev));

                    IPUT(o_newdir);
                    ret = -EXDEV;
                }
                else
                {
                        /* Prepare to use the inode */

                    ret = ovlfs_use_inode(o_newdir);
    
                    if ( ret < 0 )
                        IPUT(o_newdir);
                }
            }
            else
                DPRINT1(("OVLFS: ovlfs_rename: error %d obtaining storage dir "
                         "for new directory\n", ret));

            if ( ret >= 0 )
            {
                    /* Both directories exist in the storage filesystem and */
                    /*  the original file resides in the storage fs also.   */
    
                old_in_ovl = 1;
    
                if ( ( o_olddir->i_op != NULL ) &&
                     ( o_olddir->i_op->rename != NULL ) )
                {
                    ret = do_rename(o_olddir, oname, olen, o_newdir, nname,
                                    nlen, must_be_dir);
                }
                else
                    ret = -ENOTDIR;
    
                ovlfs_release_inode(o_newdir);
                IPUT(o_newdir);
            }
        }
        else
        {
            ret = 0;    /* file isn't in the storage fs, that's OK */
        }

        ovlfs_release_inode(o_olddir);
        IPUT(o_olddir);
    }
    else
    {
        ret = 0;    /* file isn't in the storage fs, that's OK */
    }



#if OVLFS_ALLOW_XDEV_RENAME

        /* If the problem is that the two directories in the storage fs   */
        /*  are actually in two different filesystems (i.e. a mount point */
        /*  is being crossed), allow the rename to be maintained by the   */
        /*  pseudo-filesystem anyway.  Just don't lose that storage info. */

    if ( ret == -EXDEV )
        ret = 0;

#endif


        /* An error here means the file was in the storage fs, but some */
        /*  error occurred trying to rename it; this is a real problem. */

    if ( ret >= 0 )
    {
            /* If the entry is moving from one directory to another, */
            /*  obtain the inode information for the new directory.  */

        if ( ( olddir == newdir ) && ( has_rename_op(newdir) ) )
            ret = do_rename_op(newdir, oname, olen, nname, nlen);
        else
        {
                /* Get the old entry's information */

#if POST_20_KERNEL_F
            ret      = 0;
            ent_inode = old_dent->d_inode;

            if ( ent_inode == NULL )
                ret = -ENOENT;
#else
            ret = do_lookup(olddir, oname, olen, &ent_inode, FALSE);
#endif

            if ( ret >= 0 )
            {
                ASSERT(ent_inode != NULL);

                    /* Make sure the old file resides on the same device */
                    /*  as the new directory.  Both should be in "this"  */
                    /*  overlay filesystem.                              */

                if ( ent_inode->i_dev != newdir->i_dev )
                {
                    ret = -EXDEV;
                }
                else
                {
			/* TBD: is this sufficient, or do we really need */
			/*      a storage operation equivalent to ensure */
			/*      the atomic nature of rename?             */

                        /* Unlink the target, in case it already exists. */

                    ret = do_unlink_op(newdir, nname, nlen);

                    if ( ret == -ENOENT )
                        ret = 0;


                        /* Add the new directory entry */
    
                    ret = do_add_dirent(newdir, nname, nlen, ent_inode->i_ino);
    
                    if ( ret >= 0 )
                    {
                            /* If the old dir was the inode's reference */
                            /*  parent, change it to the new dir.       */

                        i_info = (ovlfs_inode_info_t *)
                                 ent_inode->u.generic_ip;

                        if ( i_info->s.parent_ino == olddir->i_ino )
                        {
                            i_info->s.parent_ino = newdir->i_ino;
                            INODE_MARK_DIRTY(ent_inode);
                        }

                            /* Update the new dir's times and link count */
    
                        newdir->i_nlink++;
                        newdir->i_mtime = CURRENT_TIME;
                        newdir->i_ctime = newdir->i_mtime;
                        INODE_MARK_DIRTY(newdir);


                            /* Unlink the old directory entry and set the */
                            /*  storage for the inode to point to the new */
                            /*  file.                                     */
    
                        ret = do_unlink_op(olddir, oname, olen);

                            /* Update the old dir's times and link count */

                        if ( ret >= 0 )
                        {
                            olddir->i_nlink--;
                            olddir->i_mtime = CURRENT_TIME;
                            olddir->i_ctime = olddir->i_mtime;

                            INODE_MARK_DIRTY(olddir);
                        }
                    }
                }

                IPUT20(ent_inode);
            }
            else
            {
                DPRINT1(("OVLFS: %s - did not get old directory entry, %.*s\n",
                         __FUNCTION__,  olen, oname));
            }
        }
    }

    IPUT20(olddir);
    IPUT20(newdir);

    return  ret;
}



/**
 ** FUNCTION: ovlfs_readlink
 **
 **  PURPOSE: This is the overlay filesystem's readlink() inode operation.
 **/

#define SYMLINK_MAX_LEN	4096

#if POST_20_KERNEL_F

int ovlfs_readlink (struct dentry *dent, char *buf, int size)
{
	struct inode	*inode;
	struct dentry	*o_dent;
	int		ret;

	inode = dent->d_inode;

	DPRINTC2("ino %ld, buf 0x%x, size %d\n", dent->d_inode->i_ino,
	         (int) buf, size);

	if ( ! is_ovlfs_inode(inode) )
	{
		DPRINT1(("ovlfs_readlink called with non-ovlfs inode.\n"));

		ret = -EINVAL;
		goto	ovlfs_readlink__out;
	}

	ret = ovlfs_resolve_stg_dentry(inode, &o_dent);

	if ( ret < 0 )
		ret = ovlfs_resolve_base_dentry(inode, &o_dent);

	if ( ret < 0 )
		goto	ovlfs_readlink__out;

	ret = -EINVAL;
	if ( ( o_dent->d_inode->i_op != NULL ) &&
	     ( o_dent->d_inode->i_op->readlink != NULL ) )
	{
		ret = do_readlink(o_dent, buf, size);
	}

	dput(o_dent);

ovlfs_readlink__out:

	DPRINTR2("ino %ld, buf 0x%x, size %d; ret = %d\n",
	         dent->d_inode->i_ino, (int) buf, size, ret);

	return	ret;
}

#else

	/* Pre-2.2 version */

int ovlfs_readlink (struct inode *inode, char *buf, int size)
{
    struct inode    *o_inode;
    int             ret;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_readlink(%ld, 0x%x, %d)\n",
             inode->i_ino, (int) buf, size));

    if ( ! is_ovlfs_inode(inode) )
    {
        DPRINT1(("ovlfs_readlink called with non-ovlfs inode.\n"));

        IPUT20(inode);

        return  -EINVAL;
    }

    ret = ovlfs_resolve_stg_inode(inode, &o_inode);

    if ( ret < 0 )
        ret = ovlfs_resolve_base_inode(inode, &o_inode);

    if ( ret >= 0 )
    {
        ret = ovlfs_use_inode(o_inode);

        if ( ret < 0 )
            IPUT(o_inode);
    }

    if ( ret >= 0 )
    {
        if ( ( o_inode->i_op != NULL ) && ( o_inode->i_op->readlink != NULL ) )
        {
            ret = do_readlink(o_inode, buf, size);
        }
        else
            ret = -EINVAL;

        ovlfs_release_inode(o_inode);
        IPUT(o_inode);
    }

    IPUT20(inode);

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_readlink(%ld, 0x%x, %d) = %d\n",
             inode->i_ino, (int) buf, size, ret));

    return  ret;
}

#endif



/**
 ** FUNCTION: ovlfs_follow_link
 **
 **  PURPOSE: This is the overlay fs' follow_link() inode operation.
 **
 ** NOTES:
 **    - This function MUST return 0 on success.
 **    - Version >= 2.2, the dentry is that of the (potential) symlink file
 **      and nameidata specifies the directory and other important info.
 **/

#if POST_20_KERNEL_F

/* TBD: does it make sense to define this?  Won't the VFS do the same thing */
/*      without it?                                                         */

static int	ovlfs_follow_link (struct dentry *d_ent, struct nameidata *nd)
{
	struct inode	*inode;
	mm_segment_t	orig_fs;
	struct dentry	*o_dent;
	struct inode	*result;
	char		*buf;
	int		ret;

	result = INODE_NULL;

	DPRINTC2("dir 0x%08x, nd 0x%08x\n", (int) d_ent, (int) nd);

	inode = d_ent->d_inode;

		/* Make sure the file inode given is not NULL */

	ret = -ENOENT;
	if ( inode == (struct inode *) NULL )
		goto	ovlfs_follow_link__out;


		/* In case this method is called on a non-symbolic link, or  */
		/*  one of the inodes given is not in the ovlfs, just return */
		/*  the file.                                                */

	if ( ( ! is_ovlfs_inode(inode) ) || ( ! S_ISLNK(inode->i_mode) ) )
	{
		dput(nd->dentry);
		nd->dentry = dget(d_ent);
		nd->last_type = LAST_BIND;    /* Tell the VFS it is done. */

		ret = 0;
		goto	ovlfs_follow_link__out;
	}


		/* Allocate memory to hold the symlink's path */

	ret = -ENOMEM;
	buf = MALLOC(SYMLINK_MAX_LEN);
	if ( buf == NULL )
		goto	ovlfs_follow_link__out;


		/* Find the symbolic link's real fs inode */

	ret = ovlfs_resolve_stg_dentry(inode, &o_dent);

	if ( ret < 0 )
		ret = ovlfs_resolve_base_dentry(inode, &o_dent);

	if ( ret < 0 )
		goto	ovlfs_follow_link__buf_out;


		/* Read the link from the real fs' inode and lookup the */
		/*  inode to which it refers.				*/

	if ( ( o_dent->d_inode->i_op != NULL ) &&
	     ( o_dent->d_inode->i_op->readlink != NULL ) )
	{
		orig_fs = get_fs();
		set_fs(KERNEL_DS);

		ret = do_readlink(o_dent, buf, SYMLINK_MAX_LEN);

		set_fs(orig_fs);

		if ( ret < 0 )
			goto	ovlfs_follow_link__dput_out;


			/* NULL terminate the name buffer */

		if ( ret > SYMLINK_MAX_LEN )
			buf[SYMLINK_MAX_LEN - 1] = '\0'; /* just to be safe */
		else
			buf[ret] = '\0';


			/* Use the VFS method to finish the work. */

		ret = vfs_follow_link(nd, buf);

		if ( ret < 0 )
			DPRINT1(("OVLFS: follow_link: vfs_follow_link on path "
			         "%.*s returned %d\n", SYMLINK_MAX_LEN, buf,
			         ret));
	}
	else
	{
		DPRINT1(("OVLFS: follow_link: resolved symlink to non-symlink "
		         "inode; mode = 0x%x\n", o_dent->d_inode->i_mode));

			/* Just store the given dentry as the result. */

		dput(nd->dentry);
		nd->dentry = dget(d_ent);
		nd->last_type = LAST_BIND;	/* Tell the VFS it is done. */
	}

ovlfs_follow_link__dput_out:
	dput(o_dent);

	if ( ret > 0 )
		ret = 0;

ovlfs_follow_link__buf_out:

	FREE(buf);

ovlfs_follow_link__out:

	return	ret;
}

#else
	/* Pre-2.2 version */

static int  ovlfs_follow_link (struct inode *dir, struct inode *inode,
                               int flag, int mode, struct inode **res_inode)
{
    unsigned long   orig_fs;
    struct inode    *o_inode;
    struct inode    *result;
    char            *buf;
    int             ret;

    result = INODE_NULL;

    DPRINTC2("dir ino %ld, inode->i_ino %ld\n", dir->i_ino, inode->i_ino);

        /* Make sure the file inode given is not NULL */

    if ( inode == (struct inode *) NULL )
    {
        res_inode[0] = (struct inode *) NULL;

        ret = -ENOENT;
        goto    ovlfs_follow_link__out;
    }


        /* In case this method is called on a non-symbolic link, or one    */
        /*  of the inodes given is not in the ovlfs, just return the file. */

    if ( ( ! is_ovlfs_inode(dir) ) || ( ! is_ovlfs_inode(inode) ) ||
         ( ! S_ISLNK(inode->i_mode) ) )
    {
        res_inode[0] = inode;

        ret = 0;
        goto    ovlfs_follow_link__out;
    }


        /* Allocate memory to hold the symlink's path */

    buf = MALLOC(SYMLINK_MAX_LEN);

    if ( buf == NULL )
    {
        ret = -ENOMEM;
        goto    ovlfs_follow_link__inode_out;
    }


        /* Find the symbolic link's real fs inode */

    ret = ovlfs_resolve_stg_inode(inode, &o_inode);

    if ( ret < 0 )
    {
            /* Did not find the inode in the storage fs, check the base */
            /*  filesystem.                                             */

        ret = ovlfs_resolve_base_inode(inode, &o_inode);
    }

    if ( ret < 0 )
        goto    ovlfs_follow_link__buf_out;

    ret = ovlfs_use_inode(o_inode);

    if ( ret < 0 )
        goto    ovlfs_follow_link__buf_out;


        /* Read the link from the real fs' inode and lookup the */
        /*  inode to which it refers.                           */

    if ( ( o_inode->i_op != NULL ) && ( o_inode->i_op->readlink != NULL ) )
    {
        orig_fs = get_fs();
        set_fs(KERNEL_DS);

        ret = do_readlink(o_inode, buf, SYMLINK_MAX_LEN);

        set_fs(orig_fs);

        if ( ret >= 0 )
        {
                /* NULL terminate the name buffer */
            if ( ret > SYMLINK_MAX_LEN )
                buf[SYMLINK_MAX_LEN - 1] = '\0'; /* just to be safe */
            else
                buf[ret] = '\0';

                /* open_namei consumes the directory, even though the */
                /*  source does not metion this; the kernel source    */
                /*  could really use some simple, but very useful     */
                /*  comments!                                         */

            IMARK(dir);
            ret = open_namei(buf, flag, mode, &result, dir);

            if ( ret < 0 )
                DPRINT1(("OVLFS: follow_link: open_namei on path %.*s "
                              "returned %d\n", SYMLINK_MAX_LEN, buf, ret));
        }
        else
            DPRINT1(("OVLFS: follow_link: real inode's readlink() returned "
                          "%d\n", ret));
    }
    else
    {
        DPRINT1(("OVLFS: follow_link: resolved symlink to non-symlink inode; "
	         "mode = 0x%x\n", o_inode->i_mode));

        IMARK(inode);
        result = inode;
    }

    ovlfs_release_inode(o_inode);

    IPUT(o_inode);

    if ( ret > 0 )
        ret = 0;

ovlfs_follow_link__buf_out:

    FREE(buf);

ovlfs_follow_link__inode_out:

    IPUT(inode);

ovlfs_follow_link__out:

    res_inode[0] = result;

    IPUT(dir);

    return  ret;
}

#endif



#if POST_20_KERNEL_F

/**
 ** FUNCTION: ovlfs_read_block
 **
 ** NOTES:
 **	- The file given may refer to "any" filesystem.
 **
 ** RULES:
 **	- The ovlfs_i must be the overlay filesystem inode.
 **	- The caller must zero-fill the buffer as-needed to handle file
 **	  boundaries properly. (Consider file sizes that are not multiples of
 **	  the block size)
 **	- The file pointer given must refer to the actual inode that will
 **	  return the data.  It must not be the inode from this mounted ovlfs.
 **/

static int	ovlfs_read_block (struct file *filp, uint32_t iblock,
		                  struct buffer_head *bh_result,
		                  const struct inode *ovlfs_i)
{
	mm_segment_t	orig_fs;
	struct inode	*inode;
	loff_t		offset;
	loff_t		req_amount;
	int		ret_code;

	DPRINTC2("filp 0x%08x, iblock %u, bh 0x%08x, b_data 0x%08x, b_size "
	         "%u\n", (int) filp, iblock, (int) bh_result,
	         (int) bh_result->b_data, bh_result->b_size);

	inode = filp->f_dentry->d_inode;

	bh_result->b_dev     = ovlfs_i->i_dev;	/* This won't cause problems? */
	bh_result->b_blocknr = iblock;


		/* Read the specified block (in units of i_blocksize) */

	offset = iblock * ovlfs_i->i_blksize;

		/* Determine the amount of data to request in the read; */
		/*  this is necessary for two reasons - (1) to prevent  */
		/*  attempts to read past the end of an inode, and (2)  */
		/*  to handle the case when the ovlfs inode's size does */
		/*  not match the other inode's size.                   */

		/* If the ovlfs inode size is shorter, limit the read to  */
		/*  that point; otherwise, limit it to the actual amount  */
		/*  of data in the other inode.  Don't worry about zero-  */
		/*  filling any remainder; that is handled by the caller. */

	if ( ovlfs_i->i_size <= inode->i_size )
		req_amount = ( ovlfs_i->i_size - offset );
	else
		req_amount = ( inode->i_size - offset );


		/* If the request is too large, trim it.  If empty, just */
		/*  return without data.                                 */

	ret_code = 0;

	if ( req_amount > bh_result->b_size )
		req_amount = bh_result->b_size;
	else if ( req_amount < 0 )
		goto	ovlfs_read_block__out;


		/* Just call the file's read routine to do the work. */

	ret_code = -ENOSYS;

	if ( ( filp->f_op != NULL ) && ( filp->f_op->read != NULL ) )
	{
		orig_fs = get_fs();
		set_fs(KERNEL_DS);

		ret_code = filp->f_op->read(filp, bh_result->b_data,
		                            req_amount, &offset);

		set_fs(orig_fs);
	}

	if ( ( ret_code > 0 ) && ( ret_code != req_amount ) )
		DPRINT1(("OVLFS: %s: warning - only read %u of %u bytes\n",
		         __FUNCTION__, ret_code, bh_result->b_size));

	DPRINTR2("filp 0x%08x, ilbock %u, bh 0x%08x, b_data 0x%08x, b_size "
	         "%u; ret = %d\n", (int) filp, iblock, (int) bh_result,
	         (int) bh_result->b_data, bh_result->b_size, ret_code);

ovlfs_read_block__out:
	return	ret_code;
}



/**
 ** ENTRY POINT: ovlfs_readpage
 **
 **     PURPOSE: This is the overlay fs' readpage() entry point.
 **
 ** NOTES:
 **	- The readpage() method is generally called by the file read of most
 **	  filesystems [see generic_file_read()].
 **
 **	- The OVLFS does not have a real block device and does not call
 **	  readpage out of its file read.
 **
 **	- Page content is simply read directly using ovlfs_read().  It will
 **	  handle all of the references into the alternate filesystem.
 **
 **/

static int  ovlfs_readpage (struct file *file, struct page *page)
{
	struct inode		*inode;
	struct buffer_head	*head;
	struct buffer_head	*one_bh;
	uint32_t		iblock;
	int			bits;
	int			need_put_f;
	int			ret;
	int			rc;

	inode = page->mapping->host;

	DPRINTC2("ino %ld, page 0x%08x\n", inode->i_ino, (int) page);

	need_put_f = FALSE;

#if ! FASTER_AND_MORE_DANGEROUS

		/* make certain the given inode is an ovlfs inode; this */
		/*  is also useful for preventing problems due to bad   */
		/*  programming ;).                                     */

	if ( ! is_ovlfs_inode(file->f_dentry->d_inode) )
	{
		DPRINT1(("OVLFS: %s called with non-ovlfs inode!\n",
		         __FUNCTION__));

		ret = -EINVAL;
		goto	ovlfs_readpage__err_out;
	}

#endif

		/* Get a reference file to use for accessing the base or */
		/*  storage filesystem's inode.                          */

	if ( file == NULL )
	{
		ret = get_page_io_file(inode, FALSE, &file);

		if ( ( ret != 0 ) || ( file == NULL ) )
		{
			DPRINT1(("%s: no reference file; err %d!\n",
			         __FUNCTION__, ret));
			goto	ovlfs_readpage__err_out;
		}

		need_put_f = TRUE;
	}


		/* Allocate buffers for the page if necessary. */
		/*  See "block_read_full_page()"               */

	if ( page->buffers == NULL )
		create_empty_buffers(page, inode->i_dev, inode->i_blksize);

	head   = page->buffers;
	one_bh = head;

#if HAVE_I_BLKBITS
	bits = inode->i_blkbits;
#else
	bits = ffs(inode->i_blksize) - 1;
	ASSERT(( 1 << bits ) == (inode->i_blksize));
#endif

	iblock = page->index << (PAGE_CACHE_SHIFT - bits);
	ret    = 0;

	do
	{
			/* Always read the buffer - only the real fs knows */
			/*  correctly whether the page is up-to-date.      */

		memset(one_bh->b_data, 0, one_bh->b_size);

		rc = ovlfs_read_block(file, iblock, one_bh, inode);

			/* Keep any error code. */

		if ( rc < 0 )
			ret = rc;

		if ( rc >= 0 )
		{
				/* BH_Mapped indicates the buffer has a */
				/*  disk mapping; this tells the kernel */
				/*  the blocknr and dev are valid.      */

			set_bit(BH_Uptodate, &(one_bh->b_state));
			set_bit(BH_Mapped, &(one_bh->b_state));
		}

		iblock++;
		one_bh = one_bh->b_this_page;
	} while ( ( one_bh != head ) && ( ret >= 0 ) );

	if ( ret >= 0 )
		SetPageUptodate(page);

ovlfs_readpage__err_out:

	if ( need_put_f )
		put_page_io_file(file);

	UnlockPage(page);

	DPRINTR2("ino %ld, page 0x%08x; ret = %d\n", inode->i_ino, (int) page,
	         ret);

	return  ret;
}



/**
 ** FUNCTION: ovlfs_write_block
 **
 **  PURPOSE: Given a file to use for I/O and a block (buffer header), write
 **           the block's contents to disk now.
 **/

static int	ovlfs_write_block (struct file *filp,
		                   struct buffer_head *out_bh)
{
	mm_segment_t	orig_fs;
	struct inode	*inode;
	loff_t		offset;
	int		ret_code;

	DPRINTC2("filp 0x%08x, iblock %lu, bh 0x%08x, b_data 0x%08x, b_size "
	         "%u\n", (int) filp, out_bh->b_blocknr, (int) out_bh,
	         (int) out_bh->b_data, out_bh->b_size);

	if ( ( filp->f_dentry != NULL ) &&
	     ( filp->f_dentry->d_inode != NULL ) )
	{
		inode = filp->f_dentry->d_inode;
	}
	else
	{
		ret_code = -EIO;
		goto	ovlfs_write_block__err_out;
	}


		/* Read the specified block (in units of i_blocksize) */

	offset = out_bh->b_blocknr * inode->i_blksize;

	ret_code = -ENOSYS;

	if ( ( filp->f_op != NULL ) && ( filp->f_op->write != NULL ) )
	{
		orig_fs = get_fs();
		set_fs(KERNEL_DS);

		ret_code = filp->f_op->write(filp, out_bh->b_data,
		                             out_bh->b_size, &offset);

		set_fs(orig_fs);
	}

ovlfs_write_block__err_out:

	DPRINTR2("filp 0x%08x, ilbock %ld, bh 0x%08x, b_data 0x%08x, b_size "
	         "%u; ret = %d\n", (int) filp, out_bh->b_blocknr, (int) out_bh,
	         (int) out_bh->b_data, out_bh->b_size, ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: ovlfs_writepage
 **
 **  PURPOSE: This is the overlay fs' writepage() entry point.
 **/

static int  ovlfs_writepage (struct page *page)
{
	struct file		*io_file;
	struct inode		*inode;
	struct buffer_head	*head;
	struct buffer_head	*one_bh;
	int			rc;
	int			ret;

	DPRINTC2("page 0x%08x\n", (int) page);

		/* If the page has no buffers mapped; don't do anything. */
		/*  Should this ever really happen?                      */

	if ( page->buffers == NULL )
	{
		ret = 0;
		goto	ovlfs_writepage__err_out;
	}

	ret = -EINVAL;

		/* This should never happen: */

	if ( ( page->mapping == NULL ) || ( page->mapping->host == NULL ) )
		goto	ovlfs_writepage__err_out;

	inode = page->mapping->host;

#if ! FASTER_AND_MORE_DANGEROUS

		/* make certain the given inode is an ovlfs inode; this */
		/*  is also useful for preventing problems due to bad   */
		/*  programming ;).                                     */

	if ( ! is_ovlfs_inode(inode) )
	{
		DPRINT1(("OVLFS: %s called with non-ovlfs inode!\n",
		         __FUNCTION__));

		goto	ovlfs_writepage__err_out;
	}

#endif

		/* Get the page I/O file that will be used to write the   */
		/*  data; note that only a file in the storage fs will be */
		/*  returned here!                                        */

	ret = get_page_io_file(inode, TRUE, &io_file);

	if ( ( ret != 0 ) || ( io_file == NULL ) )
	{
		DPRINT1(("OVLFS: %s: no page_io_file reference available; "
		         "err %d!\n", __FUNCTION__, ret));

		goto	ovlfs_writepage__err_out;
	}
	else if ( ( io_file->f_dentry == NULL ) ||
	          ( io_file->f_dentry->d_inode == NULL ) )
	{
		ret = -EIO;
		goto	ovlfs_writepage__file_err_out;
	}


	head = page->buffers;
	one_bh = head;

	do
	{
			/* Lock the buffer. */

		lock_buffer(one_bh);

		rc = ovlfs_write_block(io_file, one_bh);

			/* TBD: should unlock only be used after all the */
			/*      buffers for the page are locked?  See    */
			/*      block_write_full_page() as the model.    */

			/* Unlock now that the update is complete. */
		unlock_buffer(one_bh);

		if ( rc < 0 )
			ret = rc;
		else if ( rc != one_bh->b_size )
			DPRINT1(("OVLFS: %s: warning - only wrote %u of %u "
				 "bytes\n", __FUNCTION__, rc, one_bh->b_size));

		if ( rc >= 0 )
		{
				/* Mark the buffer as up-to-date. */

			set_bit(BH_Uptodate, &(one_bh->b_state));
		}

		one_bh = one_bh->b_this_page;
	} while ( ( one_bh != head ) && ( ret >= 0 ) );

ovlfs_writepage__file_err_out:

	put_page_io_file(io_file);

ovlfs_writepage__err_out:

		/* Always unlock the page! */
	UnlockPage(page);

	DPRINTR2("page 0x%08x; ret = %d\n", (int) page, ret);

	return  ret;
}



#else

/**
 ** FUNCTION: ovlfs_readpage
 **
 **  PURPOSE: This is the overlay fs' readpage() function.
 **/

static int  ovlfs_readpage (struct inode *inode, struct page *page)
{
    struct inode    *o_inode;
    int             ret;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_readpage(%ld)\n", inode->i_ino));

    ret = ovlfs_resolve_stg_inode(inode, &o_inode);

    if ( ret < 0 )
        ret = ovlfs_resolve_base_inode(inode, &o_inode);

    if ( ret >= 0 )
    {
        if ( ( o_inode->i_op == NULL ) || ( o_inode->i_op->readpage == NULL ) )
            ret = -ENOEXEC;
        else
            ret = o_inode->i_op->readpage(o_inode, page);

        IPUT(o_inode);
    }

    return  ret;
}

/* End of Post-2.0 section */
#endif


#if OVLFS_DEFINE_BMP

/**
 ** FUNCTION: ovlfs_bmap
 **
 **  PURPOSE: This is the overlay fs' bmap() inode operation.
 **/

#if POST_20_KERNEL_F
static int	ovlfs_bmap (struct address_space *mapping, long block)
#else
static int	ovlfs_bmap (struct inode *inode, int block)
#endif
{
#if POST_20_KERNEL_F
	struct inode	*inode;
#endif
	struct inode	*o_inode;
	int		ret;
	int		rc;

#if POST_20_KERNEL_F
	inode = mapping->host;
#endif

	if ( ! is_ovlfs_inode(inode) )
		return	0;

	ret = 0;

	rc = ovlfs_resolve_stg_inode(inode, &o_inode);

	if ( rc < 0 )
		rc = ovlfs_resolve_base_inode(inode, &o_inode);

	if ( rc >= 0 )
	{
# if POST_20_KERNEL_F
			/* Use the VFS method to do the work. */

		ret = bmap(o_inode, block);
#else
		if ( ( o_inode->i_op != NULL ) ||
		     ( o_inode->i_op->bmap != NULL ) )
			ret = o_inode->i_op->bmap(o_inode, block);
#endif

		IPUT(o_inode);
	}

	return	ret;
}

#endif



/**
 ** FUNCTION: ovlfs_truncate
 **
 **  PURPOSE: This is the overlay filesystem's truncate() inode operation.
 **
 ** NOTES:
 **	- This function may be called as the result of a setattr() call, so
 **	  keep this logic subordinate to that of setattr().  In other words,
 **	  do not perform any work that setattr() handles, and do not attempt
 **	  to use setattr() on the same inode.
 **/

#if POST_20_KERNEL_F

int	ovlfs_do_truncate (struct dentry *o_dent, unsigned int len)
{
	int		ret;
	struct iattr	attribs;

	DPRINTC2("ino %ld, len %u\n", o_dent->d_inode->i_ino, len);

		/* See do_truncate() as a model; too bad it's not exported. */

	attribs.ia_valid = ATTR_SIZE | ATTR_CTIME;
	attribs.ia_size  = len;
	attribs.ia_ctime = CURRENT_TIME;

	DOWN(&(o_dent->d_inode->i_sem));
	ret = notify_change(o_dent, &attribs);
	UP(&(o_dent->d_inode->i_sem));

	DPRINTR2("ino %ld, len %u; ret = %d\n", o_dent->d_inode->i_ino, len,
	         ret);

	return	ret;
}

static void	ovlfs_truncate (struct inode *inode)
{
	int			ret;
	struct dentry		*o_dent;
	ovlfs_inode_info_t	*i_info;

	DPRINTC2("ino %ld\n", inode->i_ino);

	ret = -EINVAL;
	if ( ! is_ovlfs_inode(inode) )
		goto	ovlfs_truncate__out;

	ret = ovlfs_resolve_stg_dentry(inode, &o_dent);
	if ( ret < 0 )
		goto	ovlfs_truncate__out;

	ret = get_write_access(o_dent->d_inode);

	if ( ret >= 0 )
	{
			/* NOTE: ovlfs_do_truncate() indirectly initiates a */
			/*  call to setattr for the given dentry; this is   */
			/*  safe, although redundant, because the dentry is */
			/*  not in this ovlfs filesystem.                   */

		ret = ovlfs_do_truncate(o_dent, inode->i_size);

		i_info = inode->u.generic_ip;
		i_info->s.flags |= OVL_IFLAG__SIZE_LIMIT;

		put_write_access(o_dent->d_inode);
	}

	dput(o_dent);

ovlfs_truncate__out:

	DPRINTR2("ino %ld; ret = %d\n", inode->i_ino, ret);
}

#else

int ovlfs_do_truncate (struct inode *o_inode, unsigned int len)
{
	int		ret;
	struct iattr	attribs;

	DPRINTC2("ino %ld, len %u\n", inode->i_ino, len);

	attribs.ia_valid = ATTR_SIZE | ATTR_CTIME;
	attribs.ia_size  = len;
	attribs.ia_ctime = CURRENT_TIME;

	if ( ( o_inode->i_sb != NULL ) &&
	     ( o_inode->i_sb->s_op->notify_change != NULL ) )
	{
		ret = o_inode->i_sb->s_op->notify_change(o_inode, &attribs);
	}

	if ( ret >= 0 )
	{
		o_inode->i_size = len;

		if ( ( o_inode->i_op != NULL ) &&
		     ( o_inode->i_op->truncate != NULL ) )
		{
			o_inode->i_op->truncate(o_inode);
		}
	}

	DPRINTR2("ino %ld, len %u; ret = %d\n", inode->i_ino, len, ret);

	return	ret;
}

static void	ovlfs_truncate (struct inode *inode)
{
	int		ret;
	struct inode	*o_inode;

	DPRINTC2("ino %ld\n", inode->i_ino);

	ret = ovlfs_resolve_stg_inode(inode, &o_inode);

	if ( ret >= 0 )
	{
		ret = ovlfs_use_inode(o_inode);

		if ( ret >= 0 )
		{
			ret = ovlfs_do_truncate(o_inode, inode->i_size);
			ovlfs_release_inode(o_inode);

			i_info = o_inode->u.generic_ip;
			i_info->s.flags |= OVL_IFLAG__SIZE_LIMIT;
		}

		IPUT(o_inode);
	}

	DPRINTR2("ino %ld; ret = %d\n", inode->i_ino, ret);
}

#endif



/**
 ** FUNCTION: ovlfs_permission
 **
 **  PURPOSE: This is the overlay filesystem's permission() inode operation.
 **/

static int  ovlfs_permission (struct inode *inode, int mask)
{
    struct inode    *o_inode;
    int             ret;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_permission(%ld, %d)\n", inode->i_ino,
             mask));

    if ( ! is_ovlfs_inode(inode) )
        return  -EACCES;

        /* Try and find the associated "real" file and use its permission  */
        /*  checking; if neither is found, just use the default permission */
        /*  check.  In this case, the real problem will be found at a more */
        /*  appropriate place.                                             */

    ret = ovlfs_resolve_stg_inode(inode, &o_inode);

    if ( ret < 0 )
        ret = ovlfs_resolve_base_inode(inode, &o_inode);

    if ( ret >= 0 )
    {
            /* If the inode of the real file does not have a permission */
            /*  method, just use the default permission checks.         */

        if ( ( o_inode->i_op != NULL ) &&
             ( o_inode->i_op->permission != NULL ) )
            ret = o_inode->i_op->permission(o_inode, mask);
        else
#if POST_20_KERNEL_F
            ret = vfs_permission(inode, mask);
#else
            ret = ovlfs_ck_def_perm(inode, mask);
#endif

        IPUT(o_inode);
    }
    else
#if POST_20_KERNEL_F
        ret = vfs_permission(inode, mask);
#else
        ret = ovlfs_ck_def_perm(inode, mask);
#endif

    return  ret;
}



#if POST_20_KERNEL_F

/**
 ** FUNCTION: ovlfs_revalidate
 **
 **  PURPOSE: This is the overlay filesystem's revalidate() inode operation.
 **
 ** TBD:
 **	- Consider any extra logic here to validate the dentry (e.g. check for
 **	  lost base or storage inode).
 **/

static int  ovlfs_revalidate (struct dentry *d_ent)
{
    struct dentry   *o_dent;
    int             ret;

    DPRINTC2("ino %ld\n", d_ent->d_inode->i_ino);

    if ( ! is_ovlfs_inode(d_ent->d_inode) )
        return  -EACCES;

        /* Try and find the associated "real" file and use its revalidation. */

    ret = ovlfs_resolve_stg_dentry(d_ent->d_inode, &o_dent);

    if ( ret < 0 )
        ret = ovlfs_resolve_base_dentry(d_ent->d_inode, &o_dent);

    if ( ret >= 0 )
    {
            /* If the inode of the real file does not have a permission */
            /*  method, just use the default permission checks.         */

        if ( ( o_dent->d_inode->i_op != NULL ) &&
             ( o_dent->d_inode->i_op->revalidate != NULL ) )
            ret = o_dent->d_inode->i_op->revalidate(o_dent);
        else
            ret = 0;

        dput(o_dent);
    }

    DPRINTR2("ino %ld; ret = %d\n", d_ent->d_inode->i_ino, ret);

    return  ret;
}



/**
 ** FUNCTION: ovlfs_setattr
 **
 **  PURPOSE: This if the overlay filesystem's setattr() inode operation.
 **/

static int	ovlfs_setattr (struct dentry *d_ent, struct iattr *attr)
{
	struct dentry		*o_dent;
	ovlfs_inode_info_t	*i_info;
	int			ret;

	if ( ! is_ovlfs_inode(d_ent->d_inode) )
		return	-ENOENT;

		/* If an storage inode exists, update it; otherwise, only */
		/*  update our own inode's information.                   */

	ret = ovlfs_resolve_stg_dentry(d_ent->d_inode, &o_dent);

	if ( ret >= 0 )
	{
		DOWN(&(o_dent->d_inode->i_sem));
		notify_change(o_dent, attr);
		UP(&(o_dent->d_inode->i_sem));
		dput(o_dent);
	}
	else if ( ret == -ENOENT )
	{
			/* It is OK to not find the storage inode, even if */
			/*  it existed some time in the past.              */

		ret = 0;
	}


		/* Now update the attributes of the psuedo-fs' inode; don't */
		/*  worry about locking - the caller should have that under */
		/*  control.                                                */

	if ( ret >= 0 )
	{
		ret = inode_change_ok(d_ent->d_inode, attr);

		if ( ret == 0 )
		{
				/* Just let inode_setattr() do the work. */

			ret = inode_setattr(d_ent->d_inode, attr);

				/* If all went well and the size changed,  */
				/*  indicate that the new size is the hard */
				/*  limit.                                 */

			if ( ( ret == 0 ) &&
			     ( attr->ia_attr_flags & ATTR_SIZE ) )
			{
				i_info = d_ent->d_inode->u.generic_ip;

				ASSERT(i_info != NULL);

				i_info->s.flags |= OVL_IFLAG__SIZE_LIMIT;
			}
		}
	}

	return	ret;
}

#endif



/**
 ** FUNCTION: mknod_cb
 **
 **  PURPOSE: Make a new node in the given directory inode with the specified
 **           filename and status information.
 **
 ** NOTES:
 **     - This is a callback function for use with the ovlfs_add_dirent_op
 **       function.
 **/

struct mknod_dat_struct {
    int mode;
    int dev;
} ;

static int  mknod_cb (struct inode *inode, void *ptr, const char *name,
                      int len)
{
    struct mknod_dat_struct *data;
    int                     ret;

        /* The given inode must not be an ovlfs inode or we will get into */
        /*  an infinite loop (since its mknod will come back here...)     */

    if ( is_ovlfs_inode(inode) )
        return -ELOOP;

    data = (struct mknod_dat_struct *) ptr;

    if ( ( ptr == NULL ) || ( inode == INODE_NULL ) ||
         ( inode->i_op == NULL ) || ( inode->i_op->mknod == NULL ) )
        return  -ENOTDIR;

    ret = do_mknod(inode, name, len, data->mode, data->dev);

    return ret;
}



/**
 ** FUNCTION: ovlfs_mknod
 **
 **  PURPOSE: This is the overlay filesystem's mknod() inode operation.
 **/

#if POST_20_KERNEL_F

static int  ovlfs_mknod (struct inode *inode, struct dentry *d_ent, int mode,
                         int dev)
{
    struct mknod_dat_struct data;
    int                     ret;

    DPRINTC2("i_ino %ld, name %.*s, len %d\n", inode->i_ino,
             (int) d_ent->d_name.len, d_ent->d_name.name, d_ent->d_name.len);

    data.mode = mode;
    data.dev  = dev;

    ret = ovlfs_add_dirent_op(inode, d_ent->d_name.name, d_ent->d_name.len,
                              d_ent, mknod_cb, (void *) &data);

    return  ret;
}

#else
	/* Pre-2.2 version */

static int  ovlfs_mknod (struct inode *inode, const char *name, int len,
                         int mode, int dev)
{
    struct mknod_dat_struct data;
    int                     ret;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_mknod(%ld, %.*s, %d)\n", inode->i_ino,
             len, name, len));

    data.mode = mode;
    data.dev  = dev;

    return  ovlfs_add_dirent_op(inode, name, len, mknod_cb, (void *) &data);
}

#endif



/**
 ** ovlfs_ino_ops: define the pseudo fs' operations for directories.
 **
 ** ovlfs_addr_ops: Entry points for the OVLFS address-space operations.
 **	flushpage	- Don't need.  This is called to drop pages when the
 **			  inode is truncated.  The standard kernel method when
 **			  this is NULL works fine.
 **	sync_page	- Called by the kernel to initiate the completion of
 **			  any page readahead operations (as per fs/nfs/file.c).
 **			  Ultimately results in unlocking of the page.  This
 **			  should not be needed with the current synchronous-
 **			  only I/O of the ovlfs.
 **	prepare_write	- Don't need unless the ovlfs eventually uses
 **			  generic_cont_expand() or block_symlink() - neither
 **			  of which make sense for a psuedo-filesystem.
 **	commit_write	- Same comments as prepare_write.
 **	direct_IO	- Only needed for filesystems using generic_read_file()
 **			  and generic_file_write().  TBD: research further to
 **			  more fully understand and decide whether to implement
 **			  in ovlfs.
 **/

#if POST_20_KERNEL_F

struct inode_operations ovlfs_ino_ops = {
    create:		ovlfs_create,
    lookup:		ovlfs_lookup,
    link:		ovlfs_link,
    unlink:		ovlfs_unlink,
    symlink:		ovlfs_symlink,
    mkdir:		ovlfs_mkdir,
    rmdir:		ovlfs_rmdir,
    mknod:		ovlfs_mknod,
    rename:		ovlfs_rename,
/*    readlink:		ovlfs_readlink, */	/* Only for symlinks! */
/*    follow_link:	ovlfs_follow_link, */	/* Only for symlinks! */
    truncate:		ovlfs_truncate,
    permission:		ovlfs_permission,
    revalidate:		ovlfs_revalidate,
    setattr:		ovlfs_setattr,
    getattr:		NULL,	/* TBD: is this used anywhere? */
} ;

struct address_space_operations	ovlfs_addr_ops = {
	readpage:	ovlfs_readpage,
	writepage:	ovlfs_writepage,

#if OVLFS_DEFINE_BMP
	bmap:		ovlfs_bmap,
#else
	bmap:		NULL,
#endif
} ;

#else
	/* Pre-2.2 Version */

struct inode_operations ovlfs_ino_ops = {
    &ovlfs_dir_fops,
    ovlfs_create,
    ovlfs_lookup,
    ovlfs_link,
    ovlfs_unlink,
    ovlfs_symlink,
    ovlfs_mkdir,
    ovlfs_rmdir,
    ovlfs_mknod,
    ovlfs_rename,
    ovlfs_readlink,
    ovlfs_follow_link,
    ovlfs_readpage,
    (int (*)(struct inode *, struct page *)) NULL,
# if OVLFS_DEFINE_BMP
    ovlfs_bmap,
# else
    (int (*)(struct inode *,int)) NULL,
# endif
    ovlfs_truncate,
    ovlfs_permission,
    (int (*)(struct inode *,int)) NULL
} ;

#endif



/**
 ** ovlfs_ino_ops: define the pseudo fs' operations for regular files.
 **/

#if POST_20_KERNEL_F

struct inode_operations ovlfs_symlink_ino_ops = {
    readlink:		ovlfs_readlink,
    follow_link:	ovlfs_follow_link,
    permission:		ovlfs_permission,
    revalidate:		ovlfs_revalidate,
    setattr:		ovlfs_setattr,
    getattr:		NULL,	/* TBD: is this used anywhere? */
} ;

#else

struct inode_operations ovlfs_file_ino_ops = {
    &ovlfs_file_ops,
    ovlfs_create,
    ovlfs_lookup,
    ovlfs_link,
    ovlfs_unlink,
    ovlfs_symlink,
    ovlfs_mkdir,
    ovlfs_rmdir,
    ovlfs_mknod,
    ovlfs_rename,
    ovlfs_readlink,
    ovlfs_follow_link,
    ovlfs_readpage,
    (int (*)(struct inode *, struct page *)) NULL,
# if OVLFS_DEFINE_BMP
    ovlfs_bmap,
# else
    (int (*)(struct inode *,int)) NULL,
# endif
    ovlfs_truncate,
    ovlfs_permission,
    (int (*)(struct inode *,int)) NULL
} ;

#endif
