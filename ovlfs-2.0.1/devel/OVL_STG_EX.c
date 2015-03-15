/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1999 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: OVL_STG_EXAMPLE.c
 **
 ** DESCRIPTION:
 **	This file contains an example, or template, for writing Storage Methods
 **	to use with the ovlfs.
 **
 ** NOTES:
 **	- As they say in court, Assumes Facts Not In Evidence.  This file
 **	  does not contain a complete definition of all structures or
 **	  functions.  Any data type or function that is not defined, but is
 **	  used for illustration, is named in all uppercase.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 01/22/1999	ARTHUR N.	Created.  Copied ovl_stg.c and cut out most
 **				of the code.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident ""

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "ovlfs.ver"
# endif
#endif

#include <linux/malloc.h>
#include <linux/fcntl.h>
#include <linux/fs.h>

#include "kernel_lists.h"	/* If kernel lists are used. */
#include "ovl_fs.h"
#include "ovl_stg.h"
#include "ovl_inlines.h"



/**
 ** STATIC PROTOTYPES:
 **/



/**
 ** FUNCTION: create_ino_map_list
 **
 **  PURPOSE: Create a new, empty, inode mapping list.
 **/

static inline list_t    create_ino_map_list ()
{
    DPRINTC((KDEBUG_CALL_PREFIX "create_inode_map_list\n"));

    return  klist_func(create_list)(NULL, (comp_func_t) comp_ino_map,
                                    (comp_func_t) comp_ino_map, NULL);
}



/**
 ** FUNCTION: create_dirent_list
 **
 **  PURPOSE: Create a new, empty, directory entry list.
 **/

static inline list_t    create_dirent_list ()
{
    DPRINTC((KDEBUG_CALL_PREFIX "create_dirent_list\n"));

    return  klist_func(create_list)(NULL, (comp_func_t) comp_dirent,
                                    (comp_func_t) comp_dirent, NULL);
}



/**
 ** FUNCTION: free_dir_entry
 **
 **  PURPOSE: Free the memory used by the given directory entry.
 **/

static void free_dir_entry (ovlfs_dir_info_t *d_ent)
{
    DPRINTC((KDEBUG_CALL_PREFIX "free_dir_entry\n"));

    if ( d_ent != (ovlfs_dir_info_t *) NULL )
    {
        if ( d_ent->name != (char *) NULL )
            FREE(d_ent->name);

        FREE(d_ent);
    }
}



/**
 ** FUNCTION: check_mapping
 **
 **  PURPOSE: Check to make sure the given entry does not already exist in
 **           the given map list, and update it if it does.
 **
 ** RETURNS:
 **	0	- If the mapping does not already exist.
 **	1	- If the mapping does already exist.
 **/

static int  check_mapping (list_t map_list, dev_t dev, _uint ino, _uint ovl_ino)
{
    ovlfs_ino_map_t *mapping;
    ovlfs_ino_map_t tmp;
    int             ret;

    if ( map_list != (list_t) NULL )
    {
        tmp.dev     = dev;
        tmp.ino     = ino;
        tmp.ovl_ino = ovl_ino;

        mapping = (ovlfs_ino_map_t *)
                  klist_func(search_for_element)(map_list, &tmp);

        if ( mapping == (ovlfs_ino_map_t *) NULL )
            ret = 0;
        else
        {
            mapping->ovl_ino = ovl_ino;
            ret = 1;
        }
    }
    else
        ret = 0;

    return ret;
}




			/************************
			 ************************
			 **                    **
			 ** STORAGE OPERATIONS **
			 **     INTERFACE      **
			 **                    **
			 ************************
			 ************************/




/**
 ** FUNCTION: ovlfs_STGM_Init
 **
 **  PURPOSE: Read the super block specified from storage and prepare the
 **           filesystem for accessing inodes.
 **/

static int	ovlfs_STGM_Init (struct super_block *sup_blk)
{
	ovlfs_inode_t		*root_ino;
	ovlfs_super_info_t	*s_info;
	MY_SUPER_PHYS_STRUCT	data;
	int			ret_code;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_STGM_Init\n"));

#if ! FASTER_AND_MORE_DANGEROUS

	if ( sup_blk == (struct super_block *) NULL )
	{
		DPRINT1(("OVLFS: ovlfs_STGM_Init: invalid super block"
		         "given\n"));

		return	-EINVAL;
	}

#endif

		/* Read the super block information from the device now. */

	ret_code = READ_FROM_PHYS_STG(sup_blk->s_dev, 0, sizeof(data), &data);

	if ( ret_code >= 0 )
	{
			/* Make certain the block read has the correct magic */
			/*  number.                                          */

		if ( data.magic != OVLFS_STGM_SUPER_MAGIC )
			return -EINVAL;


			/* Remember the necessary information. */

		s_info = (ovlfs_super_info_t *) sup_blk->u.generic_sbp;


			/* The offset of the root directory's inode. */

		s_info->u.STGM.root_ino_offset = data.root_ino_offset;


			/* The offset of the start of the mappings. */

		s_info->u.STGM.base_map_start = data.base_map_offset;
		s_info->u.STGM.stg_map_start  = data.stg_map_offset;

		s_info->u.STGM.tot_inode_count = data.tot_inode;
		s_info->u.STGM.next_inum       = data.next_inum;

		ret_code = 0;
	}

	return	ret_code;
}



/**
 ** FUNCTION: ovlfs_STGM_Release
 **
 **  PURPOSE: Given an ovlfs super block, free any and all memory used for
 **           storage.
 **/

static int	ovlfs_STGM_Release (struct super_block *sup_blk)
{
	ovlfs_super_info_t	*s_info;
	int			ret;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_STGM_Release\n"));

	if ( sup_blk == (struct super_block *) NULL )
		return -EINVAL;

		/* Free any allocated members of the super info's u element. */

	s_info = (ovlfs_super_info_t *) sup_blk->u.generic_sbp;

	if ( s_info->u.STGM.some_name != NULL )
	{
		FREE(s_info->u.STGM.some_name);
		s_info->u.STGM.some_name = NULL;
	}

	return	0;
}



/**
 ** FUNCTION: ovlfs_STGM_Sync
 **/

static int	ovlfs_STGM_Sync (struct super_block *sup_blk)
{
	ovlfs_super_info_t	*s_info;
	MY_SUPER_PHYS_STRUCT	data;
	int			ret_code;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_STGM_Sync\n"));


	if ( ( sup_blk == (struct super_block *) NULL ) ||
	     ( sup_blk->u.generic_sbp == NULL ) )
		return	-EINVAL;

	s_info = (ovlfs_super_info_t *) sup_blk->u.generic_sbp;


		/* Read the super block information from the device and  */
		/*  update the data retrieved with the appropriate info. */

	ret_code = READ_FROM_PHYS_STG(sup_blk->s_dev, 0, sizeof(data), &data);

	if ( ret_code >= 0 )
	{
		data.root_ino_offset = s_info->u.STGM.root_ino_offset ;
		data.base_map_offset = s_info->u.STGM.base_map_start;
		data.stg_map_offset  = s_info->u.STGM.stg_map_start;
		data.tot_inode       = s_info->u.STGM.tot_inode_count;
		data.next_inum       = s_info->u.STGM.next_inum;

		ret_code = WRITE_TO_PHYS_STG(sup_blk->s_dev, 0, sizeof(data),
		                             &data);
	}

	return	ret_code;
}



/**
 ** FUNCTION: ovlfs_STGM_Add_Inode
 **
 **  PURPOSE: Add a new inode to storage and obtain the inode through the inode
 **           cache and return it.
 **
 ** NOTES:
 **	- This function does NOT update the mapping information for the inode.
 **/

static int	ovlfs_STGM_Add_Inode (struct super_block *sb,
		                      struct inode *ref_inode,
		                      const char *name, int namelen,
		                      struct inode **result)
{
	MY_INODE_PHYS_STRUCT	data;
	int			ret;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_STGM_Add_Inode\n"));

	ret = STGM_ALLOC_INODE(sb, NULL, 0, &data);

	if ( ino == 0 )
	{
		result[0] = INODE_NULL;
		ret = -ENOMEM;
	}
	else
	{
			/* Set the attributes from the reference inode */

		if ( ref_inode != (struct inode *) NULL )
		{
			data->mode    = ref_inode->i_mode;
			data->size    = ref_inode->i_size;
			data->uid     = ref_inode->i_uid;
			data->gid     = ref_inode->i_gid;
			data->atime   = ref_inode->i_atime;
			data->mtime   = ref_inode->i_mtime;
			data->ctime   = ref_inode->i_ctime;
			data->nlink   = ref_inode->i_nlink;
			data->blksize = ref_inode->i_blksize;
			data->blocks  = ref_inode->i_blocks;
			data->version = ref_inode->i_version;
			data->nrpages = ref_inode->i_nrpages;

			if ( S_ISBLK(data->mode) || S_ISCHR(data->mode) )
				data->rdev = ref_inode->i_rdev;
		}

			/* Set the reference name, if given */

		if ( ( name != (char *) NULL ) && ( namelen > 0 ) )
		{
				/* If the update fails, ignore it; the */
				/*  file may have trouble if the user  */
				/*  makes updates to it, b */
			STGM_SAVE_INODE_REF_NAME(&data, name, namelen);

		}

			/* Retrieve the VFS inode for the new inode */

		result[0] = __iget(sb, ino->ino, 0);


			/* Make sure the result is valid */

		if ( result[0] == INODE_NULL )
		{
			DPRINT1(("OVLFS: ovlfs_stg1_add_inode: added inode %ld,"
                                 " but unable to iget() it?\n", ino->ino));

			ret = -ENOMEM;
		}
		else
			ret = 0;
	}

	DPRINTC(("ovlfs_stg1_add_inode returning\n"));

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_read_inode
 **
 **  PURPOSE: Read the inode's information from storage and store it in the
 **           given inode structure.
 **/

static int	ovlfs_stg1_read_inode (struct inode *rd_inode)
{
	ovlfs_inode_t		*ino;
	ovlfs_inode_info_t	*i_info;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_read_inode\n"));

		/* Find the storage information for this inode */

	ino = ovlfs_inode_get_ino(rd_inode);

	if ( ino == (ovlfs_inode_t *) NULL )
	{
		DPRINT1(("OVLFS: ovlfs_stg1_read_inode: inode %ld, dev 0x%x "
		         "not found in storage\n", rd_inode->i_ino,
		         rd_inode->i_dev));

		return	-ENOENT;
	}


		/* Update the inode's attributes from the storage info */

	rd_inode->i_mode    = ino->mode;
	rd_inode->i_size    = ino->size;
	rd_inode->i_uid     = ino->uid;
	rd_inode->i_gid     = ino->gid;
	rd_inode->i_atime   = ino->atime;
	rd_inode->i_mtime   = ino->mtime;
	rd_inode->i_ctime   = ino->ctime;
	rd_inode->i_nlink   = ino->nlink;
	rd_inode->i_blksize = ino->blksize;
	rd_inode->i_blocks  = ino->blocks;
	rd_inode->i_version = ino->version;
	rd_inode->i_nrpages = ino->nrpages;

	if ( S_ISBLK(ino->mode) || S_ISCHR(ino->mode) )
		rd_inode->i_rdev = ino->u.rdev;
	else
		rd_inode->i_rdev = 0;

	if ( rd_inode->u.generic_ip == NULL )
	{
		DPRINT1(("OVLFS: ovlfs_stg1_read_inode: inode given "
			 "has a null generic ip!\n"));
	}
	else
	{
		i_info = (ovlfs_inode_info_t *) rd_inode->u.generic_ip;

			/* Copy the "special" information */

		i_info->s = ino->s;


			/* If the reference name is set, copy it; it is not */
			/*  an error if it is not set.                      */

		if ( ( ino->s.name != NULL ) && ( ino->s.len > 0 ) )
		{
			i_info->s.name = dup_buffer(ino->s.name, ino->s.len);

			if ( i_info->s.name == (char *) NULL )
			{
				DPRINT1(("OVLFS: unable to allocate memory to "
				         "copy the inode's reference name, "
				         "%.*s\n", i_info->s.len,
				         i_info->s.name));


					/* Maybe this should be a failure,  */
					/*  but for now, just make sure the */
					/*  name is not used                */

				i_info->s.len = 0;
			}
		}
	}

	DPRINTC(("ovlfs_stg1_read_inode returning\n"));

	return	0;
}



/**
 ** FUNCTION: ovlfs_stg1_clear_inode
 **
 **  PURPSOE: Given that the specified inode is unused, clear it from storage
 **           so that it may be reused.  This function currently does not do
 **           any work for the storage file method - it may never.
 **/

static int	ovlfs_stg1_clear_inode (struct inode *inode)
{
	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_clear_inode\n"));

	inode = inode;	/* Prevent compiler warnings */

	DPRINTC(("ovlfs_stg1_clear_inode returning\n"));

	return	0;
}



/**
 ** FUNCTION: ovlfs_stg1_upd_inode
 **
 **  PURPOSE: Update the storage information for the given inode.
 **/

static int	ovlfs_stg1_upd_inode (struct inode *inode)
{
	ovlfs_inode_t	*ino;
	int		ret;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_upd_inode\n"));

	ino = ovlfs_inode_get_ino(inode);

	if ( ino != (ovlfs_inode_t *) NULL )
	{
		ino->mode    = inode->i_mode;
		ino->size    = inode->i_size;
		ino->uid     = inode->i_uid;
		ino->gid     = inode->i_gid;
		ino->atime   = inode->i_atime;
		ino->mtime   = inode->i_mtime;
		ino->ctime   = inode->i_ctime;
		ino->nlink   = inode->i_nlink;
		ino->blksize = inode->i_blksize;
		ino->blocks  = inode->i_blocks;
		ino->version = inode->i_version;
		ino->nrpages = inode->i_nrpages;

		if ( S_ISBLK(ino->mode) || S_ISCHR(ino->mode) )
			ino->u.rdev = inode->i_rdev;
		else
			ino->u.generic = NULL;

                if ( inode->u.generic_ip != NULL )
                {
                    ovlfs_inode_info_t *i_info;

                    i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;

                        /* Update the inode "special" ovlfs information */

                    ino->s.parent_ino = i_info->s.parent_ino;
                    ino->s.base_dev   = i_info->s.base_dev;
                    ino->s.base_ino   = i_info->s.base_ino;
                    ino->s.stg_dev    = i_info->s.stg_dev;
                    ino->s.stg_ino    = i_info->s.stg_ino;
                    ino->s.flags      = i_info->s.flags;

                    if ( ( i_info->s.name != NULL ) && ( i_info->s.len > 0 ) )
                    {
                        char *tmp;

                        tmp = dup_buffer(i_info->s.name, i_info->s.len);

                        if ( tmp == (char *) NULL )
                        {
                                /* Produce an error message, but leave the */
                                /*  current setting as is.                 */

                            DPRINT1(("OVLFS: unable to allocate memory to copy"
                                     " the inode's reference name, %.*s\n",
                                     i_info->s.len, i_info->s.name));
                        }
                        else
                        {
                                /* Release the existing name, if set */

                            if ( ( ino->s.name != (char *) NULL ) &&
                                 ( ino->s.len > 0 ) )
                            {
                                FREE(ino->s.name);
                            }

                            ino->s.name = tmp;
                            ino->s.len  = i_info->s.len;
                        }
                    }
                }

		ret = 0;
	}
	else
		ret = -ENOENT;

	DPRINTC(("ovlfs_stg1_upd_inode returning\n"));

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_num_dirent
 **
 **  PURPOSE: Determine the number of directory entries in the given directory
 **           inode.  If the inc_rm indicator is non-zero, all directory
 **           entries are counted; otherwise, only entries that are not marked
 **           as deleted are counted.
 **
 ** NOTES:
 **	- DO NOT use the return value from this function to determine the
 **	  number of elements to expect in the directory entry list.
 **/

static int	ovlfs_stg1_num_dirent (struct inode *inode, int inc_rm)
{
	ovlfs_inode_t	*ino;
	int		ret;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_num_dirent\n"));

	ino = ovlfs_inode_get_ino(inode);

	if ( ino == (ovlfs_inode_t *) NULL )
		ret = -ENOENT;
	else
		if ( ino->dir_entries == (list_t) NULL )
			ret = 0;
		else
			ret = klist_func(list_length)(ino->dir_entries);

	if ( ! inc_rm )
		ret -= ino->num_rm_ent;

	DPRINTC(("ovlfs_stg1_num_dirent returning\n"));

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_map_inode
 **
 **  PURPOSE: Given a inode in the pseudo filesystem and an inode in the
 **           specified filesystem, add the mapping information from the pseudo
 **           filesystem inode to the real inode.
 **
 ** NOTES:
 **	- The inode's stg_dev,stg_ino or base_dev,base_ino pair is updated, as
 **	  appropriate, in the storage even though that is not necessary since
 **	  the overlay filesystem will update that information in the real inode
 **	  and it will get updated later when the inode is put.
 **/

static int	ovlfs_stg1_map_inode (struct inode *p_inode,
		                      struct inode *o_inode, char which)
{
	int		ret = 0;
	ovlfs_inode_t	*ino;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_map_inode\n"));

	ino = ovlfs_inode_get_ino(p_inode);

	if ( ino == (ovlfs_inode_t *) NULL )
	{
		DPRINT1(("OVLFS: ovlfs_stg1_map_inode: no storage information "
		         "for inode #%ld, dev 0x%x\n", p_inode->i_ino,
		         p_inode->i_dev));

		ret = -ENOENT;
	}
	else
	{
		switch ( which )
		{
			case 'b':
				ret = ovlfs_inode_set_base(p_inode->i_sb, ino,
				                           o_inode->i_dev,
				                           o_inode->i_ino);
				break;

			case 'o':
			case 's':
				ret = ovlfs_inode_set_stg(p_inode->i_sb, ino,
				                          o_inode->i_dev,
				                          o_inode->i_ino);
				break;

			default:
				DPRINT1(("OVLFS: ovlfs_stg1_map_inode: invalid"
				         " value for which: %c\n", which));
				ret = -EINVAL;
				break;
		}
	}

	DPRINTC(("ovlfs_stg1_map_inode returning\n"));

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_map_lookup
 **
 **  PURPOSE: Given a inode in the specified filesystem, obtain the pseudo
 **           filesystem inode for the given inode.
 **
 ** ARGUMENTS:
 **	sb	- super block of the overlay filesystem.
 **	dev	- device of the filesystem that the inode resides in.
 **	inum	- inode number of the inode to obtain from the specified dev.
 **	result	- pointer to the location to store the resulting inode.
 **	which	- a character specifying whether the given inode is in the base
 **		  or storage filesystem.
 **/

static int	ovlfs_stg1_map_lookup (struct super_block *sb,
		                       kdev_t dev, _ulong inum,
		                       struct inode **result, char which)
{
	int		ret;
	_ulong		stg1_inum;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_map_lookup\n"));

	result[0] = INODE_NULL;


		/* Find the inode mapping entry */

	stg1_inum = ovlfs_lookup_ino(sb, dev, inum, which);

	if ( stg1_inum != 0 )
	{
			/* last arg = 0 ==> only return inode for given sb */

		result[0] = __iget(sb, stg1_inum, 0);

		if ( result[0] == INODE_NULL )
		{
			DPRINT1(("OVLFS: ovlfs_stg1_map_lookup: iget did not "
			         "find inode %ld for sb dev %d!\n", stg1_inum,
                                 sb->s_dev));

			ret = -ENOENT;
		}
		else
			ret = 0;
	}
	else
		ret = -ENOENT;

	DPRINTC(("ovlfs_stg1_map_lookup returning\n"));

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_get_dirent
 **
 **  PURPOSE: Obtain the nth directory entry from the given pseudo fs
 **           directory.
 **
 ** NOTES:
 **     - A pointer to the actual entry in the list is returned; the caller
 **       must be careful NOT to free the returned value.
 **/

static int	ovlfs_stg1_get_dirent (struct inode *inode, int pos,
		                       ovlfs_dir_info_t **result)
{
	ovlfs_inode_t	*ino;
	int		ret;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_get_dirent\n"));

	ino = ovlfs_inode_get_ino(inode);

	if ( ino == (ovlfs_inode_t *) NULL )
		ret = -ENOENT;
	else
	{
		ret = 0;

		if ( ino->dir_entries == (list_t) NULL )
			result[0] = (ovlfs_dir_info_t *) NULL;
		else
			result[0] = (ovlfs_dir_info_t *)
			            klist_func(nth_element)(ino->dir_entries,
			                                    pos);
	}

	DPRINTC(("ovlfs_stg1_get_dirent returning\n"));

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_lookup
 **/

static int	ovlfs_stg1_lookup (struct inode *inode, const char *name,
		                   int len, struct inode **result)
{
	ovlfs_inode_t		*ino;
	ovlfs_dir_info_t	*d_info;
	int			ret;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_lookup\n"));


		/* Get the storage information for the pseudo fs dir. inode */

	ino = ovlfs_inode_get_ino(inode);

	if ( ino == (ovlfs_inode_t *) NULL )
	{
		DPRINT1(("OVLFS: ovlfs_stg1_lookup: could not find storage "
		         "information for inode %ld in dev 0x%x\n",
		         inode->i_ino, inode->i_dev));

		ret = -ENOENT;
	}
	else
	{
		d_info = ovlfs_ino_find_dirent(ino, name, len);

			/* Make sure the entry is found and is not marked */
			/*  as UNLINKED.                                  */

		if ( ( d_info == (ovlfs_dir_info_t *) NULL ) ||
		     ( d_info->flags & OVLFS_DIRENT_UNLINKED ) )
			ret = -ENOENT;
		else
		{
			result[0] = __iget(inode->i_sb, d_info->ino,
			                   ovlfs_sb_xmnt(inode->i_sb));

			if ( result[0] != INODE_NULL )
				ret = 0;
			else
			{
				DPRINT1(("OVLFS: ovlfs_stg1_lookup: inode %ld "
                                         "dev %d not found\n", d_info->ino,
				         inode->i_sb->s_dev));

				ret = -ENOENT;
			}
		}
	}

	DPRINTC(("ovlfs_stg1_lookup returning\n"));

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_add_dirent
 **
 **  PURPOSE: Given a directory inode in the pseudo filesystem, the name of an
 **           entry to add to the directory, and the inode of the entry, add
 **           the entry to the directory.
 **/

static int 	ovlfs_stg1_add_dirent (struct inode *inode, const char *name,
		                       int len, _ulong inum)
{
	ovlfs_inode_t	*ino;
	int		ret;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_add_dirent\n"));

		/* Find the storage info for the given directory inode */

	ino = ovlfs_inode_get_ino(inode);

	if ( ino == (ovlfs_inode_t *) NULL )
		ret = -ENOENT;
	else
		ret = ovlfs_ino_add_dirent(ino, name, len, inum,
		                           OVLFS_DIRENT_NORMAL);

	DPRINTC(("ovlfs_stg1_add_dirent returning\n"));

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_rename
 **
 **  PURPOSE: Given a directory inode in the pseudo filesystem, the name of an
 **           entry in the directory, and a new name, change the name of the
 **           file in the directory.
 **/

static int  ovlfs_stg1_rename (struct inode *inode, const char *oname,
                               int olen, const char *nname, int nlen)
{
    ovlfs_inode_t       *ino;
    ovlfs_dir_info_t    *d_info;
    ovlfs_dir_info_t    *n_info;
    int                 ret;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_rename\n"));

    ino = ovlfs_inode_get_ino(inode);

    if ( ino == (ovlfs_inode_t *) NULL )
        ret = -ENOENT;
    else
    {
        d_info = ovlfs_ino_find_dirent(ino, oname, olen);

        if ( d_info == (ovlfs_dir_info_t *) NULL )
            ret = -ENOENT;
        else
        {
            n_info = ovlfs_ino_find_dirent(ino, nname, nlen);

                /* Check if the files are the same */

            if ( n_info == d_info )
                ret = 0;
            else if ( n_info != NULL )
            {
                _ulong inum;

                    /* Remove the old entry and update the "new" one. */

                inum = d_info->ino;

                ret = ovlfs_unlink_dirent(ino, oname, olen);

                if ( ret >= 0 )
                    n_info->ino = inum;
                else
                    DPRINT1(("OVLFS: ovlfs_stg1_rename: unlink old entry, %.*s"
                             " in dir %ld error %d\n", olen, oname, ino->ino,
                             ret));
            }
            else
            {
                char    *tmp;

                    /* Change the name of the existing directory entry */
                    /*  and then resort the list.                      */

                tmp = MALLOC(nlen);

                if ( tmp == (char *) NULL )
                    ret = -ENOMEM;
                else
                {
                    if ( d_info->name != (char *) NULL )
                        FREE(d_info->name);

                    memcpy(tmp, nname, nlen);

                    d_info->name = tmp;

                    klist_func(sort_list)(ino->dir_entries);
                    ret = 0;
                }
            }
        }
    }

    DPRINTC(("ovlfs_stg1_rename returning\n"));

    return  ret;
}



/**
 ** FUNCTION: ovlfs_stg1_unlink
 **
 **  PURPOSE: Given a directory inode in the pseudo filesystem and the name
 **           of an entry in the directory, mark the entry as unlinked.  The
 **           entry is not actually removed because it is necessary to track
 **           the files that are removed from the image of the base filesystem.
 **/

static int	ovlfs_stg1_unlink (struct inode *inode, const char *name,
		                   int len)
{
	ovlfs_inode_t	*ino;
	int		ret;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_unlink\n"));

	ino = ovlfs_inode_get_ino(inode);

	if ( ino == (ovlfs_inode_t *) NULL )
		ret = -ENOENT;
	else
		ret = ovlfs_unlink_dirent(ino, name, len);

	DPRINTC(("ovlfs_stg1_unlink returning\n"));

	return	ret;
}



/**
 ** Overlay Filesystem Storage File Operations
 **/

ovlfs_storage_op_t ovlfs_stg1_ops = {
	ovlfs_stg1_read_super,
	ovlfs_stg1_write_super,
	ovlfs_stg1_put_super,
	ovlfs_stg1_add_inode,
	ovlfs_stg1_read_inode,
	ovlfs_stg1_clear_inode,
	ovlfs_stg1_upd_inode,
	ovlfs_stg1_map_inode,
	ovlfs_stg1_map_lookup,
	ovlfs_stg1_num_dirent,
	ovlfs_stg1_get_dirent,
	ovlfs_stg1_lookup,
	ovlfs_stg1_add_dirent,
	ovlfs_stg1_rename,
	ovlfs_stg1_unlink,
	NULL,	/* File read - not used by the storage file method */
	NULL,	/* File write - not used by the storage file method */
	NULL,	/* Set symlink - not used by the storage file method */
	NULL	/* Read symlink - not used by the storage file method */
} ;
