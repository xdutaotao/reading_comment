/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_dops.c
 **
 ** DESCRIPTION:
 **	Directory cache operations for use by the Overlay Filesystem.
 **
 ** NOTES:
 **	- Most of the dentry operations could be passed through to the base
 **	  and storage filesystems, but it might not be safe to do so.
 **	- By clearing the ovlfs_inode_info_t reference members on d_delete(),
 **	  the need to directly call the other operations is eliminated; now
 **	  the VFS will handle the other filesystem's dentries as appropriate.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-02-24	ARTHUR N.	Initial Release
 ** 2003-02-28	ARTHUR N.	Improve performance by using d_iput() to
 **				 release inode references, and procfs logic
 **				 to the d_delete logic.
 ** 2003-03-11	ARTHUR N.	Added d_revalidate entry point.
 ** 2003-03-11	ARTHUR N.	Changed d_revalidate to static so it can be
 **				 found by gdb.
 ** 2003-06-09	ARTHUR N.	Corrected module version support.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_dops.c	1.5	[03/07/27 22:20:34]"

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
#include <linux/proc_fs.h>

#include "ovl_debug.h"
#include "ovl_fs.h"
#include "ovl_inlines.h"

#define OVLFS_DOP_REVALIDATE	1

#if POST_20_KERNEL_F

/**
 ** FUNCTION: is_proc_dentry_p
 **/

static inline int	is_proc_dentry_p (struct dentry *dentry)
{
	int	proc_f;

	proc_f = FALSE;

	if ( ( dentry != NULL ) && ( dentry->d_sb != NULL ) )
		if ( dentry->d_sb->s_magic == PROC_SUPER_MAGIC )
			proc_f = TRUE;

	return	proc_f;
}



/**
 ** FUNCTION: ovlfs_d_revalidate
 **
 **  PURPOSE: Revalidate the dentry.
 **
 ** RETURNS:
 **	0	- If the dentry is not valid.  Note that the VFS may not remove
 **		  the cache entry, such as when it is still in-use.
 **	1	- If the dentry is valid.
 **/

static int	ovlfs_d_revalidate (struct dentry *dent, int flags)
{
	int	ret_code;

	DPRINTC2("dent at 0x%08x, flags 0x%x\n", (int) dent, flags);

	ret_code = 0;

	if ( dent->d_inode == NULL )
		goto	ovlfs_d_revalidate__out;

	if ( inode_refs_valid_p(dent->d_inode) )
		ret_code = 1;

ovlfs_d_revalidate__out:
	DPRINTR2("dent at 0x%08x, flags 0x%x\n", (int) dent, flags);

	return	ret_code;
}



/**
 ** ENTRY POINT: ovlfs_d_delete
 **
 **     PURPOSE: Cleanup for the dentry as it's reference count in the cache
 **              reaches 0.
 **
 ** RULES:
 **	- Return 0 to "ignore".
 **	- Return 1 to force "unhash" of the dentry.
 **/

static int	ovlfs_d_delete (struct dentry *d_ent)
{
	ovlfs_inode_info_t	*i_info;
	int			ret_code;

	DPRINTC2("dentry 0x%08x (name %.*s, len %d)\n", (int) d_ent,
	         (int) d_ent->d_name.len, d_ent->d_name.name,
	         d_ent->d_name.len);

	ret_code = 0;

	if ( ( d_ent->d_inode == NULL ) ||
	     ( ! is_ovlfs_inode(d_ent->d_inode) ) )
	{
		goto	ovlfs_d_delete__out;
	}


		/* If the dentry given holds an active reference to a procfs */
		/*  inode, clear its references now; procfs can keep         */
		/*  references into this filesystem, such as open files and  */
		/*  these can prevent umounts.                               */

	i_info = (ovlfs_inode_info_t *) d_ent->d_inode->u.generic_ip;

#if STORE_REF_INODES
	if ( is_proc_dentry_p(i_info->base_dent) ||
	     is_proc_dentry_p(i_info->overlay_dent) )
	{
			/* Clear the inode's reference dentry members; this */
			/*  will lead to their delete and other dentry ops  */
			/*  in due time.                                    */

		ovlfs_clear_inode_refs(d_ent->d_inode);
	}
#endif

ovlfs_d_delete__out:

	DPRINTR2("dentry 0x%08x (name %.*s, len %d); ret = %d\n", (int) d_ent,
	         (int) d_ent->d_name.len, d_ent->d_name.name,
	         d_ent->d_name.len, ret_code);

	return	ret_code;
}



/**
 ** ENTRY POINT: ovlfs_d_iput
 **
 **     PURPOSE: Revalidate the dentry given.
 **
 ** RULES:
 **	- Return 0 to "ignore".
 **	- Return 1 to force "unhash" of the dentry.
 **/

static void	ovlfs_d_iput (struct dentry *d_ent, struct inode *inode)
{
	DPRINTC2("dentry 0x%08x (name %.*s, len %d), ino %lu\n", (int) d_ent,
	         (int) d_ent->d_name.len, d_ent->d_name.name,
	         d_ent->d_name.len, inode->i_ino);

	if ( ! is_ovlfs_inode(inode) )
		goto	ovlfs_d_iput__out;


		/* Clear the inode's reference dentry members; this will   */
		/*  lead to their delete and other dentry ops in due time. */

	ovlfs_clear_inode_refs(inode);

	IPUT(inode);

ovlfs_d_iput__out:

	DPRINTR2("dentry 0x%08x (name %.*s, len %d), ino %lu\n", (int) d_ent,
	         (int) d_ent->d_name.len, d_ent->d_name.name,
	         d_ent->d_name.len, inode->i_ino);
}


struct dentry_operations	ovlfs_dentry_ops =
{
	d_revalidate:	ovlfs_d_revalidate,
	d_delete:	ovlfs_d_delete,
	d_iput:		ovlfs_d_iput,
} ;

#endif
