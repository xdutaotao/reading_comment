/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998-2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_stg.c
 **
 ** DESCRIPTION: This file contains the definition of the "Storage File"
 **              Storage System for the ovlfs.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 09/27/1997	art            	Added to source code control.
 ** 02/23/1998	ARTHUR N.	ovlfs_add_inode(): set the super block AFTER
 **				 initializing the new result to all 0's.
 ** 02/26/1998	ARTHUR N.	Added four more inode fields to the retained
 **				 information for each inode.
 ** 02/28/1998	ARTHUR N.	Set the superblock to be dirty on any change.
 ** 03/01/1998	ARTHUR N.	Added support for the maxmem fs option and
 **				 changed all kmalloc's to allocate KERNEL
 **				 priority memory instead of high priority mem.
 ** 03/01/1998	ARTHUR N.	Removed unused link_op functions and changed
 **				 ovlfs_unlink_dirent() so that it does not
 **				 update the link count of the unlinked file.
 ** 03/02/1998	ARTHUR N.	Use the stored inode from the superblock for
 **				 the storage file.
 ** 03/02/1998	ARTHUR N.	Do not fail the ovlfs_stg_read_super() if the
 **				 storage file is empty.
 ** 03/03/1998	ARTHUR N.	Allow the storage file to be empty.  This is
 **				 necessary since the change for keeping the
 **				 file's inode was made.
 ** 03/05/1998	ARTHUR N.	Added support for the mapping store options
 **				 and changed update of mapping lists so that
 **				 one inode, within a specific device, only
 **				 has one mapping entry within a single list.
 ** 03/05/1998	ARTHUR N.	Added printk() statements for some error
 **				 conditions, and started use of DEBUG_PRINT1.
 ** 03/05/1998	ARTHUR N.	Malloc-checking support added.
 ** 03/09/1998	ARTHUR N.	Added the copyright notice.
 ** 03/10/1998	ARTHUR N.	Don't set the superblock as dirty if told so
 **				 by mount options.
 ** 03/14/1998	ARTHUR N.	Fixed open_stg_file() so that it returns
 **				 -ENOENT if no storage file was specified.
 ** 03/15/1998	ARTHUR N.	Added support for the ovlfs_storage_op_t
 **				 access to the storage file and cleaned up use
 **				 of debug macros.
 ** 03/16/1998	ARTHUR N.	Added prototype for ovlfs_stg1_write_super().
 ** 03/29/1998	ARTHUR N.	Added more storage operations, fixed bug when
 **				 the storage file is empty, and removed unused
 **				 functions.
 ** 08/23/1998	ARTHUR N.	Added the refernce inode to the "add inode"
 **				 storage operation, added many DPRINTC
 **				 statements, and corrected the releasing of
 **				 memory in the ovlfs_add_inode function when
 **				 an error occurs adding it to the inode list.
 ** 02/02/1999	ARTHUR N.	Completed the storage method interface.  Also,
 **				 cleaned up some code (a little).
 ** 02/06/1999	ARTHUR N.	Added some debugging and free the old reference
 **				 name before changing it.
 ** 02/06/1999	ARTHUR N.	Reworked the Storage System interface to
 **				 elminate serving of VFS inodes by the Storage
 **				 System.
 ** 02/06/1999	ARTHUR N.	Store the "special" inode information. The
 **				 information is redundant, but this is the
 **				 easiest way to work with the new Storage
 **				 System interface.
 ** 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
 ** 02/24/2003	ARTHUR N.	Corrected debug statements.
 ** 02/28/2003	ARTHUR N.	Added Get_mapping() storage method.
 ** 03/10/2003	ARTHUR N.	Added relink and disabled base reference flag
 **				 support.
 ** 03/11/2003	ARTHUR N.	Added ovlfs_stg1_delete_dirent().
 ** 03/11/2003	ARTHUR N.	Added flags argument to add_inode op.
 ** 03/16/2003	ARTHUR N.	Corrected inode map updates for the storage
 **				 filesystem.
 ** 03/26/2003	ARTHUR N.	Modified the Read_dir() entry point to use
 **				 an loff_t *.
 ** 05/24/2003	ARTHUR N.	Added offset to directory entries, and a list
 **				 index to order the entries by offset.
 ** 07/05/2003	ARTHUR N.	Corrected seek back to inode size when writing
 **				 inode information.
 ** 07/05/2003	ARTHUR N.	Store flags in storage file which indicate the
 **				 existence of mapping information.
 ** 07/08/2003	ARTHUR N.	Permanently use new directory handling code.
 ** 07/16/2003	ARTHUR N.	Correct ovlfs_stg1_find_next() to look at the
 **				 last possible search position.
 ** 07/17/2003	ARTHUR N.	Don't dereference a null pointer in
 **				 ovlfs_stg1_num_dirent.
 **
 ** TBD:
 **	- Change all references to super_info to use a macro so that changing
 **	  the ovlfs to be part of the base O/S would require minimal effort.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_stg.c	1.37	[03/07/27 22:20:35]"

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "ovlfs.ver"
# endif
#endif

#include <linux/version.h>

#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/file.h>

#include "file_io.h"
#include "kernel_lists.h"
#include "ovl_debug.h"
#include "ovl_fs.h"
#include "ovl_stg.h"
#include "ovl_inlines.h"

#if POST_20_KERNEL_F
# include <asm/uaccess.h>
#else
# include <asm/segment.h>
#endif



/**
 ** STATIC PROTOTYPES:
 **/

static int	stg1_read_super(struct super_block *);
static int	ovlfs_stg1_write_super(struct super_block *);



/**
 ** FUNCTION: comp_ovl_ino
 **/

static int  comp_ovl_ino (ovlfs_inode_t *ino1, ovlfs_inode_t *ino2)
{
    DPRINTC((KDEBUG_CALL_PREFIX "comp_ovl_ino\n"));

    if ( ino1 == (ovlfs_inode_t *) NULL )
        return  1;
    else if ( ino2 == (ovlfs_inode_t *) NULL )
        return  -1;


        /* Sort first by device number, then by inode number */

    if ( ino1->s.base_dev == ino2->s.base_dev )
        return ino1->s.base_ino - ino2->s.base_ino;
    else
        return ino1->s.base_dev - ino2->s.base_dev;
}



/**
 ** FUNCTION: comp_ino_map
 **/

static int  comp_ino_map (ovlfs_ino_map_t *map1, ovlfs_ino_map_t *map2)
{
    DPRINTC((KDEBUG_CALL_PREFIX "comp_ino_map\n"));

    if ( map1 == (ovlfs_ino_map_t *) NULL )
        return  1;
    else if ( map2 == (ovlfs_ino_map_t *) NULL )
        return  -1;


        /* Sort first by device number, then by inode number */

    if ( map1->dev == map2->dev )
        return  map1->ino - map2->ino;
    else
        return  map1->dev - map2->dev;
}



/**
 ** FUNCTION: comp_dirent
 **/

static int  comp_dirent (ovlfs_dir_info_t *ent1, ovlfs_dir_info_t *ent2)
{
    DPRINTC((KDEBUG_CALL_PREFIX "comp_dirent\n"));

    if ( ent1 == (ovlfs_dir_info_t *) NULL )
        return  1;
    else if ( ent2 == (ovlfs_dir_info_t *) NULL )
        return  -1;

        /* Sort first by name length, then name; this should be faster on */
        /*  better than average.                                          */

    if ( ent1->len == ent2->len )
        return  strncmp(ent1->name, ent2->name, ent1->len);
    else
        return  ent1->len - ent2->len;
}



/**
 ** FUNCTION: comp_dirent_pos
 **/

static int  comp_dirent_pos (ovlfs_dir_info_t *ent1, ovlfs_dir_info_t *ent2)
{
    DPRINTC2("ent1 0x%08x, ent2 0x%08x\n", (int) ent1, (int) ent2);

    if ( ent1 == (ovlfs_dir_info_t *) NULL )
        return  1;
    else if ( ent2 == (ovlfs_dir_info_t *) NULL )
        return  -1;

        /* Sort first by name length, then name; this should be faster on */
        /*  better than average.                                          */

    return  ent1->pos - ent2->pos;
}



/**
 ** FUNCTION: create_inode_list
 **
 **  PURPOSE: Create a new, empty, inode list
 **/

static inline list_t    create_inode_list ()
{
    DPRINTC((KDEBUG_CALL_PREFIX "create_inode_list\n"));

    return  klist_func(create_list)(NULL, (comp_func_t) comp_ovl_ino,
                                    (comp_func_t) comp_ovl_ino, NULL);
}



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

static inline list_t    create_dirent_list (klist_index_id_t *r_index_id)
{
    list_t  ret_code;
    int     rc;

    DPRINTC((KDEBUG_CALL_PREFIX "create_dirent_list\n"));

    ret_code = klist_func(create_list)(NULL, (comp_func_t) comp_dirent,
                                       (comp_func_t) comp_dirent, NULL);

    if ( ret_code != NULL )
    {
        rc = klist_func(add_index)(ret_code, (comp_func_t) comp_dirent_pos,
	                           0, r_index_id);

        if ( rc != 0 )
        {
            klist_func(destroy_list)(ret_code);
            ret_code = NULL;
        }
    }

    return  ret_code;
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
 ** FUNCTION: free_ovlfs_inode
 **/

static void free_ovlfs_inode (ovlfs_inode_t *ino)
{
    DPRINTC((KDEBUG_CALL_PREFIX "free_ovlfs_inode\n"));

    if ( ino != (ovlfs_inode_t *) NULL )
    {
        if ( ino->dir_entries != (list_t) NULL )
        {
            klist_func(foreach_node)(ino->dir_entries,
                                     (foreach_func_t) free_dir_entry);
            klist_func(destroy_list)(ino->dir_entries);
        }

        if ( ( ino->s.name != (char *) NULL ) && ( ino->s.len > 0 ) )
        {
            FREE(ino->s.name);
            ino->s.name = (char *) NULL;
            ino->s.len = 0;
        }

        FREE(ino);
    }
}



/**
 ** FUNCTION: alloc_inode
 **
 **  PURPOSE: Allocate space for an inode in our storage.
 **/

static inline ovlfs_inode_t	*alloc_inode (struct super_block *sb)
{
	ovlfs_inode_t	*result;
	int		nomem;
	struct sysinfo	mem_info;

		/* Before attempting to allocate the memory, check memory if */
		/*  the option was set for this filesystem.                  */

	nomem = 0;

	if ( ovlfs_sb_opt(sb, storage_opts) & OVLFS_STG_CK_MEM )
	{
			/* Obtain the memory information and make sure that */
			/*  the free ram available is >= 10% of the total.  */

		si_meminfo(&mem_info);

		if ( mem_info.freeram < ( mem_info.totalram * 0.1 ) )
			nomem = 1;
	}

	if ( ! nomem )
		result = (ovlfs_inode_t *) MALLOC(sizeof(ovlfs_inode_t));
	else
		result = (ovlfs_inode_t *) NULL;

	return	result;
}



/**
 ** FUNCTION: ovlfs_add_inode
 **
 **  PURPOSE: Given the super block for an overlay filesystem, add a new
 **           inode to the filesystem.
 **/


ovlfs_inode_t   *ovlfs_add_inode (struct super_block *sup_blk, const char *name,
                                  int len)
{
    ovlfs_inode_t       *result;
    ovlfs_super_info_t  *s_info;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_add_inode\n"));

#if ! FASTER_AND_MORE_DANGEROUS

    if ( ( sup_blk == (struct super_block *) NULL ) ||
         ( sup_blk->u.generic_sbp == NULL ) ||
         ( sup_blk->s_magic != OVLFS_SUPER_MAGIC ) )
    {
        DPRINT1(("OVLFS: ovlfs_add_inode: invalid super block given\n"));

        return  (ovlfs_inode_t *) NULL;
    }

#endif

    s_info = (ovlfs_super_info_t *) sup_blk->u.generic_sbp;

    if ( s_info->u.stg1.inodes == (list_t) NULL )
    {
        s_info->u.stg1.inodes = create_inode_list();

        if ( s_info->u.stg1.inodes == (list_t) NULL )
        {
            DPRINT1(("OVLFS: unable to allocate needed memory\n"));

            return  (ovlfs_inode_t *) NULL;
        }

        if ( ! (ovlfs_sb_opt(sup_blk, storage_opts) & OVLFS_UPD_UMOUNT_ONLY) )
        {
            sup_blk->s_dirt = 1;
        }
    }


    result = alloc_inode(sup_blk);

    if ( result != (ovlfs_inode_t *) NULL )
    {
        memset(result, '\0', sizeof(ovlfs_inode_t));

            /* Notice how this storage system never deletes inodes...  This */
            /*  will decay over time to unrecoverable space.                */
        result->ino = klist_func(list_length)(s_info->u.stg1.inodes) + 1;
        result->sb           = sup_blk;
        result->s.base_dev   = NODEV;
        result->s.stg_dev    = NODEV;
        result->att_valid_f  = FALSE;	/* Attributes not yet updated. */
        result->last_dir_pos = 0;


            /* If a name was given, save the name; this is not critical, */
            /*  so do not fail the entire function if this fails.        */

        if ( ( name != (char *) NULL ) && ( len > 0 ) )
        {
            result->s.name = dup_buffer(name, len);

            if ( result->s.name != (char *) NULL )
            {
                result->s.len = len;
            }
            else
            {
                DPRINT1(("OVLFS: ovlfs_add_inode: unable to allocate space for"
                         " name\n"));

                result->s.len = 0;
            }
        }
                

        if ( klist_func(insert_at_end)(s_info->u.stg1.inodes, result) == NULL )
        {
            DPRINT1(("OVLFS: ovlfs_add_inode: unable to add new inode to "
                     "list\n"));

            free_ovlfs_inode(result);
            result = (ovlfs_inode_t *) NULL;
        }
        else
            if ( ! ( ovlfs_sb_opt(sup_blk, storage_opts) &
                     OVLFS_UPD_UMOUNT_ONLY ) )
            {
                sup_blk->s_dirt = 1;
            }
    }
    else
        DPRINT2(("OVLFS: unable to allocate memory for a new storage inode\n"));

    return  result;
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



/**
 ** FUNCTION: stg1_inode_set_other
 **
 **  PURPOSE: Given the super block for the filesystem, the pointer to an
 **           ovlfs inode structure, the device and inode number of the other
 **           fs's inode, and an indication of which filesystem the other inode
 **           is in, set the mapping between that inode and the ovlfs inode.
 **/

static
int  stg1_inode_set_other (struct super_block *sup_blk, ovlfs_inode_t *ino,
                           dev_t dev, _uint inum, char other_fs)
{
    ovlfs_ino_map_t     *mapping;
    ovlfs_super_info_t  *s_info;
    list_t              *m_list;
    int                 ret_code = 0;

    DPRINTC((KDEBUG_CALL_PREFIX "stg1_inode_set_other\n"));

#if ! FASTER_AND_MORE_DANGEROUS
    if ( ino == (ovlfs_inode_t *) NULL )
    {
        DPRINT1(("OVLFS: stg1_inode_set_other: null inode info given\n"));

        return  -EINVAL;
    }
    else if ( ( sup_blk == (struct super_block *) NULL ) ||
              ( sup_blk->u.generic_sbp == NULL ) ||
              ( sup_blk->s_magic != OVLFS_SUPER_MAGIC ) )
    {
        DPRINT1(("OVLFS: stg1_inode_set_other: invalid super block given\n"));

        return  -EINVAL;
    }
#endif

    s_info = (ovlfs_super_info_t *) sup_blk->u.generic_sbp;

    switch ( other_fs )
    {
        case 'b':
            ino->s.base_dev = dev;
            ino->s.base_ino = inum;
            m_list = &(s_info->u.stg1.base_to_ovl);
            break;

        case 'o':
        case 's':
            ino->s.stg_dev = dev;
            ino->s.stg_ino = inum;
            m_list = &(s_info->u.stg1.stg_to_ovl);
            break;

        default:
            DPRINT1(("OVLFS: stg1_inode_set_other: invalid fs specifier "
                     "given\n"));
            return  -EINVAL;
    }


        /* Set the base device and inode number in storage to the values */
        /*  values given.  First see if the mapping already exists and   */
        /*  update it if so.                                             */

    if ( check_mapping(m_list[0], dev, inum, ino->ino) )
        ;
    else
    {
            /* Setup the mapping from the base inode to the overlay fs inode. */

        mapping = create_ino_map();

        if ( mapping != (ovlfs_ino_map_t *) NULL )
        {
            mapping->dev = dev;
            mapping->ino = inum;
            mapping->ovl_ino = ino->ino;

                /* Now insert this element into the mapping list */

            if ( m_list[0] == (list_t) NULL )
            {
                m_list[0] = create_ino_map_list();

                if ( m_list[0] == (list_t) NULL )
                {
                    DPRINT1(("OVLFS: ovlfs_ino_set_other: unable to create a "
                             "list for mapping\n"));

                    FREE(mapping);
                    ret_code = -ENOMEM;
                }
            }

            if ( ret_code >= 0 )
            {
                if ( klist_func(insert_into_list_sorted)(m_list[0],
                                                         mapping) == NULL )
                {
                    DPRINT1(("OVLFS: ovlfs_ino_set_other: unable to insert "
                             "element into mapping list\n"));

                    FREE(mapping);
                    ret_code = -ENOMEM;
                }
                else
                    if ( ! ( ovlfs_sb_opt(sup_blk, storage_opts) &
                             OVLFS_UPD_UMOUNT_ONLY ) )
                    {
                        sup_blk->s_dirt = 1;
                    }
            }
        }
        else
        {
            DPRINT1(("OVLFS: ovlfs_ino_set_other: unable to allocate "
                     "%d bytes\n", sizeof(ovlfs_ino_map_t)));

            ret_code = -ENOMEM;
        }
    }

    return  ret_code;
}



/**
 ** FUNCTION: ovlfs_get_ino
 **/

ovlfs_inode_t   *ovlfs_get_ino (struct super_block *sup_blk, _ulong ino)
{
    ovlfs_super_info_t  *s_info;
    ovlfs_inode_t       *result;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_get_ino(%ld)\n", ino));

#if ! FASTER_AND_MORE_DANGEROUS
    if ( ( sup_blk == (struct super_block *) NULL ) ||
         ( sup_blk->s_magic != OVLFS_SUPER_MAGIC ) ||
         ( sup_blk->u.generic_sbp == NULL ) )
    {
        DPRINT1(("OVLFS: ovlfs_get_ino: invalid super block given\n"));

        return  (ovlfs_inode_t *) NULL;
    }
#endif

    if ( ino == 0 )
        return  (ovlfs_inode_t *) NULL;

    s_info = (ovlfs_super_info_t *) sup_blk->u.generic_sbp;

    if ( s_info->u.stg1.inodes == (list_t) NULL )
        result = (ovlfs_inode_t *) NULL;
    else
        result = (ovlfs_inode_t *)
                 klist_func(nth_element)(s_info->u.stg1.inodes, ino);

    return  result;
}



/**
 ** FUNCTION: set_ino_flags
 ** FUNCTION: clear_ino_flags
 **
 **  PURPOSE: Set or clear flags for the specified inode.
 **
 **/

static inline void	set_ino_flags (struct super_block *sup_blk, _ulong ino,
			               int set_flags)
{
	ovlfs_inode_t   *ovl_ino;

	ovl_ino = ovlfs_get_ino(sup_blk, ino);

	if ( ovl_ino != NULL )
		ovl_ino->s.flags |= set_flags;
	else
		DPRINT1(("OVLFS: %s: failed to retrieve inode %lu\n",
		         __FUNCTION__, ino));
}

static inline void	clear_ino_flags (struct super_block *sup_blk,
			                 _ulong ino, int clear_flags)
{
	ovlfs_inode_t   *ovl_ino;

	ovl_ino = ovlfs_get_ino(sup_blk, ino);

	if ( ovl_ino != NULL )
		ovl_ino->s.flags &= ~clear_flags;
	else
		DPRINT1(("OVLFS: %s: failed to retrieve inode %lu\n",
		         __FUNCTION__, ino));
}



/**
 ** FUNCTION: ovlfs_ino_add_dirent
 **
 **  PURPOSE: Given an ovlfs inode structure, add the named directory entry
 **           to the inode's list of entries.
 **
 ** NOTES:
 **     - Due to the manner in which entries are inserted into the directory's
 **       list of entries, this function must not be called between the start
 **       and end of a readdir() call; otherwise, entries may appear more than
 **       once, and other entries will be missing.
 **/

int ovlfs_ino_add_dirent (ovlfs_inode_t *ino, const char *name, int len,
                          _ulong ent_ino, int flags)
{
    int                 ret_code;
    ovlfs_dir_info_t    *dirent;

    DPRINTC2("ent %ld\n", ent_ino);

#if ! FASTER_AND_MORE_DANGEROUS
    if ( ( ino == (ovlfs_inode_t *) NULL ) || ( name == (char *) NULL ) )
    {
            DPRINT1(("OVLFS: ovlfs_ino_add_dirent: null argument given\n"));

            ret_code = -EINVAL;
            goto    ovlfs_ino_add_dirent__out;
    }
#endif

    ret_code = 0;

    if ( ino->dir_entries == (list_t) NULL )
        ino->dir_entries = create_dirent_list(&(ino->pos_index));

        /* See if the directory already has the entry; if it does, check */
        /*  if the entry is marked as unlinked and just remove it first  */
        /*  if so.                                                       */

    dirent = ovlfs_ino_find_dirent(ino, name, len);

    if ( dirent != (ovlfs_dir_info_t *) NULL )
    {
        if ( dirent->flags & OVLFS_DIRENT_UNLINKED )
        {

/* TBD: modify this to update directly to eliminate the chance of failure */
/*      removing the unlink-indicated entry and not creating a relink-    */
/*      indicated entry.                                                  */

            ret_code = ovlfs_remove_dirent(ino, name, len);

            if ( ret_code < 0 )
                goto    ovlfs_ino_add_dirent__out;

                /* Add the re-linked indicator for the new entry.  Also set */
                /*  the inode's "no base reference" flag.                   */
            flags |= OVLFS_DIRENT_RELINKED;
                /* TBD: give more consideration to issues involving linking */
                /*      when the original link has a base reference and the */
                /*      new one was relinked.                               */
            set_ino_flags(ino->sb, ent_ino, OVL_IFLAG__NO_BASE_REF);
        }
        else
        {
            DPRINT5(("OVLFS: ovlfs_ino_add_dirent: directory entry already "
                     "exists\n"));

            ret_code = -EEXIST;
            goto    ovlfs_ino_add_dirent__out;
        }
    }

    if ( ino->dir_entries != (list_t) NULL )
    {
            /* Allocate memory for the directory entry */

        dirent = (ovlfs_dir_info_t *) MALLOC(sizeof(ovlfs_dir_info_t));

        if ( dirent == (ovlfs_dir_info_t *) NULL )
        {
            DPRINT1(("OVLFS: ovlfs_ino_add_dirent: unable to allocate %d "
                     "bytes\n", sizeof(ovlfs_dir_info_t)));

            ret_code = -ENOMEM;
        }
        else
        {
                /* Copy the name; first allocate space for it.  Don't bother */
                /*  to null-terminate the value since its length is kept.    */

            dirent->name = (char *) MALLOC(len);

            if ( dirent->name == (char *) NULL )
            {
                DPRINT1(("OVLFS: ovlfs_ino_add_dirent: unable to allocate %d "
                         "bytes\n", len));

                FREE(dirent);
                ret_code = -ENOMEM;
            }
            else
            {
                memcpy(dirent->name, name, len);
                dirent->len   = len;
                dirent->ino   = ent_ino;
                dirent->flags = flags;

			/* TBD: add locking */
                dirent->pos   = ino->last_dir_pos++;

                    /* Add the entry to the list of entries for this inode */

                if ( klist_func(insert_into_list_sorted)(ino->dir_entries,
                                                         dirent) == NULL )
                {
                    DPRINT1(("OVLFS: ovlfs_ino_add_dirent: error adding entry "
                             "to entry list\n"));

                    FREE(dirent->name);
                    FREE(dirent);

                    ret_code = -ENOMEM;
                }
                else
                    if ( dirent->flags & OVLFS_DIRENT_UNLINKED )
                    {
                            /* Update the number of unlinked entries */

                        ino->num_rm_ent++;
                    }
            }
        }
    }
    else
    {
        DPRINT1(("OVLFS: ovlfs_ino_add_dirent: unable to create a dirent "
                 "list\n"));

        ret_code = -ENOMEM;
    }

ovlfs_ino_add_dirent__out:

    DPRINTR2("ent %ld; ret_code = %d\n", ent_ino, ret_code);

    return  ret_code;
}



/**
 ** FUNCTION: ovlfs_remove_dirent
 **
 **  PURPOSE: Given an overlay filesystem inode and the name and length of
 **           a directory entry, remove the entry from the inode's list of
 **           directory entries.
 **/

int ovlfs_remove_dirent (ovlfs_inode_t *ino, const char *name, int len)
{
    ovlfs_dir_info_t    tmp;
    int                 ret_code;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_remove_dirent\n"));

#if ! FASTER_AND_MORE_DANGEROUS
    if ( ( ino == (ovlfs_inode_t *) NULL ) || ( name == (char *) NULL ) )
    {
            DPRINT1(("OVLFS: ovlfs_remove_dirent: null argument given\n"));

            return  -EINVAL;
    }
#endif

    ret_code = 0;

    if ( ino->dir_entries != (list_t) NULL )
    {
        ovlfs_dir_info_t    *ret;

            /* cast to prevent warning about losing const */

        tmp.name = (char *) name;
        tmp.len  = len;

        ret = (ovlfs_dir_info_t *)
              klist_func(remove_element)(ino->dir_entries, &tmp);

            /* The entry was not found? */

        if ( ( ret == (ovlfs_dir_info_t *) NULL ) ||
             ( ret == (ovlfs_dir_info_t *) -1 ) )
            ret_code = -ENOENT;
        else
        {
                /* If removing an entry marked as unlinked, update the */
                /*  unlinked entry count for the inode.                */

            if ( ret->flags & OVLFS_DIRENT_UNLINKED )
                ino->num_rm_ent--;

            free_dir_entry(ret);
        }
    }

    return  ret_code;
}



/**
 ** FUNCTION: ovlfs_unlink_dirent
 **
 **  PURPOSE: Mark the specified directory entry in the given directory
 **           inode as unlinked.  This is the correct method of unlinking
 **           a file since removing it from the directory listing will
 **           allow the file to be found next time the directory listing
 **           is recreated/updated.
 **/

int ovlfs_unlink_dirent (ovlfs_inode_t *ino, const char *name, int len)
{
    ovlfs_dir_info_t    *dirent;
    ovlfs_dir_info_t    tmp;
    int                 ret_code;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_unlink_dirent\n"));

#if ! FASTER_AND_MORE_DANGEROUS
    if ( ( ino == (ovlfs_inode_t *) NULL ) || ( name == (char *) NULL ) )
    {
            DPRINT1(("OVLFS: ovlfs_unlink_dirent: null argument given\n"));

            return  -EINVAL;
    }
#endif

    ret_code = 0;

    if ( ino->dir_entries != (list_t) NULL )
    {
            /* cast to prevent warning about losing const */

        tmp.name = (char *) name;
        tmp.len  = len;

        dirent = (ovlfs_dir_info_t *)
                 klist_func(search_for_element)(ino->dir_entries, &tmp);

            /* The entry was not found?  Or was it already unlinked? */

        if ( ( dirent == (ovlfs_dir_info_t *) NULL ) ||
             ( dirent == (ovlfs_dir_info_t *) -1 ) ||
             ( dirent->flags & OVLFS_DIRENT_UNLINKED ) )
            ret_code = -ENOENT;
        else
        {
            dirent->flags |= OVLFS_DIRENT_UNLINKED;
            ino->num_rm_ent++;  /* update count of unlinked entries */

                /* Do NOT touch the link count of the directory entry's */
                /*  storage information; it will be updated when the    */
                /*  inode is written out since the caller must update   */
                /*  the VFS's inode's i_nlink count.                    */
        }
    }

    return  ret_code;
}



/**
 ** FUNCTION: ovlfs_ino_find_dirent
 **
 **  PURPOSE: Obtain the inode information for the inode, in the given
 **           directory, whose name matches the name given.
 **/

ovlfs_dir_info_t    *ovlfs_ino_find_dirent (ovlfs_inode_t *ino,
                                            const char *name, int len)
{
    ovlfs_dir_info_t    *result;
    ovlfs_dir_info_t    tmp;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_ino_find_dirent\n"));

    if ( ( ino == (ovlfs_inode_t *) NULL ) ||
         ( ino->dir_entries == (list_t) NULL ) )
        return  (ovlfs_dir_info_t *) NULL;

    tmp.name = (char *) name;
    tmp.len = len;

    result = (ovlfs_dir_info_t *)
             klist_func(search_for_element)(ino->dir_entries, &tmp);

    return  result;
}



/**
 ** FUNCTION: stg1_free_super_info
 **
 **  PURPOSE: Given an ovlfs super block, free any and all memory used for
 **           storage.
 **/

static void stg1_free_super_info (struct super_block *sup_blk)
{
    ovlfs_super_info_t  *s_info;

    DPRINTC((KDEBUG_CALL_PREFIX "stg1_free_super_info\n"));

        /* Make sure the super block given is valid. */

    if ( sup_blk == (struct super_block *) NULL )
        return;


    s_info = sup_blk->u.generic_sbp;

    if ( s_info != (ovlfs_super_info_t *) NULL )
    {
        if ( s_info->u.stg1.inodes != (list_t) NULL )
        {
            klist_func(foreach_node)(s_info->u.stg1.inodes,
                                     (foreach_func_t) free_ovlfs_inode);
            klist_func(destroy_list)(s_info->u.stg1.inodes);
            s_info->u.stg1.inodes = (list_t) NULL;
        }

        if ( s_info->u.stg1.base_to_ovl != (list_t) NULL )
        {
            klist_func(foreach_node)(s_info->u.stg1.base_to_ovl,
                                     (foreach_func_t) Free);
            klist_func(destroy_list)(s_info->u.stg1.base_to_ovl);
            s_info->u.stg1.base_to_ovl = (list_t) NULL;
        }

        if ( s_info->u.stg1.stg_to_ovl != (list_t) NULL )
        {
            klist_func(foreach_node)(s_info->u.stg1.stg_to_ovl,
                                     (foreach_func_t) Free);
            klist_func(destroy_list)(s_info->u.stg1.stg_to_ovl);
            s_info->u.stg1.stg_to_ovl = (list_t) NULL;
        }

	if ( s_info->u.stg1.stg_name != (char *) NULL )
	{
		FREE(s_info->u.stg1.stg_name);

		s_info->u.stg1.stg_name = (char *) NULL;
		s_info->u.stg1.stg_name_len = 0;
	}

	if ( s_info->u.stg1.stg_inode != INODE_NULL )
	{
		IPUT(s_info->u.stg1.stg_inode);

		s_info->u.stg1.stg_inode = (struct inode *) NULL;
	}

            /* Don't free the super block information structure */
            /*  here; there is other information that needs to  */
            /*  be handled by ovlfs_put_super(). Besides, it    */
            /*  does not belong to us.                          */
    }
}



/**
 ** FUNCTION: ovlfs_stg1_put_super
 **
 **  PURPOSE: Given an ovlfs super block, free any and all memory used for
 **           storage, and save any information to disk that needs to be saved.
 **/

int     ovlfs_stg1_put_super (struct super_block *sup_blk)
{
    int ret;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_put_super\n"));

    if ( sup_blk == (struct super_block *) NULL )
        return -EINVAL;

        /* First, save the information to disk */

    ret = ovlfs_stg1_write_super(sup_blk);

#if KDEBUG
    if ( ret < 0 )
    {
        printk("OVFLS: ovlfs_stg1_put_super: write super returned error %d\n",
               ret);
    }
#endif

    stg1_free_super_info(sup_blk);

    return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_statfs
 **
 **  PURPOSE: Given an ovlfs super block, determine the used and available
 **           space.  Of course, since all information is stored in memory for
 **           This storage method, space is memory and is not guaranteed to
 **           be available when it is needed.
 **/

int     ovlfs_stg1_statfs (struct super_block *sup_blk, struct statfs *fs_info,
                           int size)
{
	ovlfs_super_info_t	*s_info;
	struct sysinfo		mem_info;

	DPRINTC2("sup_blk 0x%08x, s_dev 0x%x\n", (int) sup_blk, sup_blk->s_dev);

	if ( size < sizeof(struct statfs) )
	{
		DEBUG_PRINT1(("OVLFS: statfs structure size mismatch; called"
		              " size is %d, expected %d\n", size,
		              sizeof(struct statfs)));
		return -EINVAL;
	}

		/* How do we measure available and used blocks without */
		/*  some storage space?  For now, use the available    */
		/*  free memory as available space and calculate an    */
		/*  approximate memory use based on the number of      */
		/*  inodes "owned" by the psuedo filesystem.           */

	si_meminfo(&mem_info);

	fs_info->f_type = OVLFS_SUPER_MAGIC;
	fs_info->f_bsize = PAGE_SIZE;
	fs_info->f_bfree = 0;
	fs_info->f_files = 0;
	fs_info->f_ffree = 0xFF;
	fs_info->f_namelen = NAME_MAX;

		/* Just set available space to the same as free space; for */
		/*  memory, it is all the same...                          */

#if POST_22_KERNEL_F
	fs_info->f_bfree  = mem_info.freeram;
#else
	fs_info->f_bfree  = mem_info.freeram / PAGE_SIZE;
#endif
	fs_info->f_bavail = fs_info->f_bfree;

	if ( ( sup_blk != (struct super_block *) NULL ) &&
	     ( sup_blk->u.generic_sbp != NULL ) )
	{
		s_info = (ovlfs_super_info_t *) sup_blk->u.generic_sbp;

		if ( s_info->u.stg1.inodes == NULL )
			fs_info->f_blocks = fs_info->f_bfree;
		else
		{
			int	result;
			int	n_ino;

			n_ino = klist_func(list_length)(s_info->u.stg1.inodes);
			result = (n_ino * sizeof(ovlfs_inode_t)) / BLOCK_SIZE;

				/* Set the total number of blocks to the */
				/*  free space plus the used space.      */

			fs_info->f_blocks = fs_info->f_bfree + result;
		}
	}

	DPRINTR2("sup_blk 0x%08x, s_dev 0x%x\n", (int) sup_blk, sup_blk->s_dev);

	return 0;
}



/**
 ** FUNCTION: stg1_parse_one_opt
 **
 **  PURPOSE: Given the pointer to an OVLFS stg1 option structure, and the name
 **           and value of an option, parse the option and update the
 **           structure.
 **/

static int	stg1_parse_one_opt (ovlfs_stg1_t *opts, char *name, char *value)
{
	int	ret_code;

	ret_code = 0;

		/* See if a value was given.  Note that the value could */
		/*  be empty, so only check for a NULL pointer.         */

	if ( value != NULL )
	{
			/* Currently, only one option is supported, so */
			/*  just check for it.                         */

		if ( ( strcmp(name, "stg_file") == 0 ) ||
		     ( strcmp(name, "sf") == 0 ) )
		{
				/* Free the existing one, if it is set. */

			if ( opts->stg_name != (char *) NULL )
				FREE(opts->stg_name);

			opts->stg_name = dup_string(value);

			if ( opts->stg_name == (char *) NULL )
				ret_code = -ENOMEM;
			else
				opts->stg_name_len = strlen(opts->stg_name);
		}
		else
		{
			DPRINT1(("OVLFS: unrecognized option, %s.\n", name));

			ret_code = -EINVAL;
		}
	}
	else
	{
			/* No switch options, at least not yet. */

		DPRINT1(("OVLFS: unrecognized option, %s.\n", name));

		ret_code = -EINVAL;
	}

	return	ret_code;
}



/**
 ** FUNCTION: stg1_parse_opts
 **
 **  PURPOSE: Parse the option string given for options supported by the
 **           "Storage File" storage method.
 **/

static int	stg1_parse_opts (ovlfs_stg1_t *opts, char *opt_str)
{
	char	*tmp;
	char	*value;
	int	ret_code;

	ret_code = 0;

		/* Loop until no more options are found in the string. */

	while ( ( opt_str != (char *) NULL ) && ( ret_code == 0 ) )
	{
		tmp = strchr(opt_str, ',');

		if ( tmp != NULL )
		{
			tmp[0] = '\0';
			tmp++;
		}

		DPRINT2(("OVLFS:  stg1 option '%s'\n", opt_str));

			/* Check for a name=value option. */

		value = strchr(opt_str, '=');

		if ( value != (char *) NULL )
		{
			value[0] = '\0';
			value++;
		}

			/* Parse this option. */

		ret_code = stg1_parse_one_opt(opts, opt_str, value);

		opt_str = tmp;
	}

	return	ret_code;
}



/**
 ** FUNCTION: get_stg_file_inode
 **
 **  PURPOSE: Obtain the inode for the storage file for the filesystem.  If
 **           the file does not exist, it is created.
 **/

static struct inode	*get_stg_file_inode (const char *stg_name)
{
	struct inode	*result = INODE_NULL;
	int		fd;
#if POST_20_KERNEL_F
	mm_segment_t	orig_fs;
#else
	unsigned long	orig_fs;
#endif

	if ( get_inode(stg_name, &result) < 0 )
	{
			/* Try to create the file; don't modify it now */

		orig_fs = get_fs();
		set_fs(KERNEL_DS);

    		fd = file_open(stg_name, O_RDONLY | O_CREAT, 0644);

		if ( fd >= 0 )
		{
			file_close(fd);

			if ( get_inode(stg_name, &result) < 0 )
				DPRINT1(("OVLFS: unable to obtain inode for "
				         "storage file, %s: error %d\n",
				         stg_name, fd));
		}
		else
			DPRINT1(("OVLFS: unable to open or create storage file"
			         ", %s: error %d\n", stg_name, fd));

		set_fs(orig_fs);
	}

	return	result;
}



/**
 ** FUNCTION: stg1_fill_in_opts
 **
 **  PURPOSE: Given an initialized storage file super block information
 **           structure that has been filled as directed by options given by
 **           the user, fill in any information that has not yet been defined.
 **
 ** NOTES:
 **	- The list members are not allocated because they will be allocated
 **	  when they are needed.  The list members are inodes, base_to_ovl, and
 **	  stg_to_ovl.
 **/

static int	stg1_fill_in_opts (ovlfs_stg1_t *opts)
{
		/* If the storage file was specified, get its inode. If the */
		/*  inode is not found, the file must not yet exist; this   */
		/*  is acceptable.  This storage method will create its     */
		/*  physical storage as needed (for better or worse).       */

	if ( opts->stg_inode == INODE_NULL )
	{
		if ( opts->stg_name != (char *) NULL )
		{
			opts->stg_inode = get_stg_file_inode(opts->stg_name);
		}
		else
		{
			DPRINT1(("OVLFS: no storage file specified\n"));
		}
	}

	return 0;
}



/**
 ** FUNCTION: ovlfs_stg1_read_super
 **
 **  PURPOSE: Initialize the storage method for the given super block.  This
 **           includes parsing mount options, preparing any data structures
 **           needed, and reading the given super block's information from
 **           storage.
 **
 ** NOTES:
 **	- If this function returns an error, it MUST release any resources
 **	  that it allocated.  No other storage method function will be called
 **	  for this filesystem after the error is returned.
 **/

static int  ovlfs_stg1_read_super (struct super_block *sup_blk, char *opt_str)
{
    ovlfs_inode_t       *root_ino;
    ovlfs_super_info_t  *s_info;
    int                 ret_code;

    DPRINTC2("sup_blk 0x%08x, opt_str '%s'\n", (int) sup_blk, opt_str);

#if ! FASTER_AND_MORE_DANGEROUS

    if ( ( sup_blk == (struct super_block *) NULL ) ||
         ( sup_blk->u.generic_sbp == NULL ) )
    {
        DPRINT1(("OVLFS: ovlfs_stg1_read_super: invalid super block given\n"));

        return  -EINVAL;
    }

#endif

        /* Initialize the Storage File super block data and parse the */
        /*  options given.                                            */

    s_info = (ovlfs_super_info_t *) sup_blk->u.generic_sbp;

    s_info->u.stg1.stg_inode     = (struct inode *) NULL;
    s_info->u.stg1.stg_name      = (char *) NULL;
    s_info->u.stg1.stg_name_len  = 0;
    s_info->u.stg1.inodes        = (list_t) NULL;
    s_info->u.stg1.base_to_ovl   = (list_t) NULL;
    s_info->u.stg1.stg_to_ovl    = (list_t) NULL;
    s_info->u.stg1.tot_dirent    = 0;
    s_info->u.stg1.tot_rm_dirent = 0;

    ret_code = stg1_parse_opts(&(s_info->u.stg1), opt_str);


	/* Fill in any needed members that have not yet been defined. */

    if ( ret_code >= 0 )
        ret_code = stg1_fill_in_opts(&(s_info->u.stg1));

    if ( ret_code < 0 )
    {
        stg1_free_super_info(sup_blk);

        return ret_code;
    }


        /* First, see if the inode information exists, and read it if so. */

    ret_code = stg1_read_super(sup_blk);

    if ( ret_code >= 0 )
    {
            /* Make sure the root inode was read */

        if ( sup_blk->u.generic_sbp != NULL )
        {
            if ( klist_func(list_length)(s_info->u.stg1.inodes) > 0 )
                return  0;
        }
    }
    else
    {
        if ( ret_code != -ENOENT )
        {
            stg1_free_super_info(sup_blk);

            return  ret_code;
        }
    }


        /* File did not appear to exist; start fresh */

        /* The root directory does not need a name since the name is only */
        /*  used for copying files into the storage fs.                   */

    root_ino = ovlfs_add_inode(sup_blk, NULL, 0);

    if ( ( root_ino == (ovlfs_inode_t *) NULL ) ||
         ( root_ino->ino != OVLFS_ROOT_INO ) )
    {
#if KDEBUG
        if ( root_ino == (ovlfs_inode_t *) NULL )
            printk("OVLFS: ovlfs_stg1_read_super: unable to allocate needed "
                   "memory\n");
        else
            printk("OVLFS: ovlfs_stg1_read_super: failed to obtain inode 1 "
                   "for root directory\n");
#endif

        stg1_free_super_info(sup_blk);

        ret_code = -ENOMEM;
    }
    else
    {
        root_ino->s.parent_ino = OVLFS_ROOT_INO;
        root_ino->mode |= S_IFDIR | S_IRUGO | S_IXUGO;
        ret_code = 0;
    }

    if ( ret_code < 0 )
        stg1_free_super_info(sup_blk);

    return  ret_code;
}



            /**** FILE I/O ****/

/**
 ** NOTES:
 **     - This is NOT efficient, but it is used infrequently.  Also, it will
 **       be replaced in the near future.
 **
 **     - All output must be done from the same process.
 **/

/**
 ** FUNCTION: read_bytes
 **
 **  PURPOSE: Read from the specified file until no more data is available, or
 **           the entire amount requested is received.
 **/

static int	read_bytes (int fd, uint8_t *buf, int amount)
{
	int	ret_code;
	int	rd_amount;
	int	tot_rd;

	tot_rd    = 0;
	rd_amount = 0;
	ret_code  = 0;

		/* Loop until the total amount is read, or an error is */
		/*  caught.                                            */

	while ( ( tot_rd < amount ) && ( ret_code == 0 ) )
	{
		rd_amount = file_read(fd, buf + rd_amount, amount - rd_amount);

		if ( rd_amount > 0 )
			tot_rd += rd_amount;
		else if ( rd_amount == 0 )
			ret_code = -EIO;
		else
			ret_code = rd_amount;
	}

	if ( ret_code == 0 )
		ret_code = rd_amount;

	return	ret_code;
}



/**
 ** FUNCTION: write_bytes
 **
 **  PURPOSE: Write to the specified file until the entire amount requested is
 **           written, or an error occurs.
 **/

static int	write_bytes (int fd, const uint8_t *buf, int amount)
{
	int	ret_code;
	int	wr_amount;

	wr_amount = 0;
	ret_code  = 0;

		/* Loop until the total amount is written, or an error is */
		/*  caught.                                               */

	while ( ( wr_amount < amount ) && ( ret_code >= 0 ) )
	{
		ret_code = file_write(fd, buf + wr_amount, amount - wr_amount);

		if ( ret_code > 0 )
			wr_amount += ret_code;
	}

	if ( ret_code >= 0 )
		ret_code = wr_amount;

	return	ret_code;
}



/**
 ** FUNCTION: write_one_byte
 **
 **  PURPOSE: Write to the specified file until the entire amount requested is
 **           written, or an error occurs.
 **/

__attribute__((unused))
static int	write_one_byte (int fd, const uint8_t byte)
{
	int	ret_code;

	ret_code = file_write(fd, &byte, 1);

	return	ret_code;
}



/**
 ** FUNCTION: write_integer
 **/

static int  write_integer (int fd, _uint value, int change_mode)
{
#if POST_20_KERNEL_F
    mm_segment_t   orig_fs;
#else
    unsigned long   orig_fs;
#endif
    unsigned char   buf[sizeof(_uint)];
    int             cur;
    int             ret;

    if ( change_mode )
    {
        orig_fs = get_fs();
        set_fs(KERNEL_DS);
    }

    for ( cur = 0; cur < sizeof(_uint); cur++ )
    {
            /* Grab the lowest byte and then shift the value by one byte */

        buf[cur] = value & 0xFF;
        value >>= 8;
    }

    ret = write_bytes(fd, buf, sizeof(buf));

    if ( change_mode )
        set_fs(orig_fs);

    if ( ret != sizeof(buf) )
    {
        DPRINT1(("OVLFS: write_integer: write returned %d, expected %d\n", ret,
                 sizeof(buf)));

        if ( ret >= 0 )
            return  -EIO;
        else
            return  ret;
    }

    return  ret;
}



/**
 ** FUNCTION: read_integer
 **/

static int  read_integer (int fd, _uint *result, const int change_mode)
{
#if POST_20_KERNEL_F
    mm_segment_t    orig_fs;
#else
    unsigned long   orig_fs;
#endif
    unsigned char   buf[sizeof(_uint)];
    int             cur;
    int             ret;

    if ( result == (_uint *) NULL )
        return  -EIO;

    if ( change_mode )
    {
        orig_fs = get_fs();
        set_fs(KERNEL_DS);
    }

    ret = read_bytes(fd, buf, sizeof(buf));

    if ( ret < 0 )
    {
        DPRINT1(("OVLFS: error %d returned from read of storage file\n", ret));
        return  ret;
    }
    else if ( ret != sizeof(buf) )
    {
        DPRINT1(("OVLFS: error only %d bytes read from storage file; %d bytes "
                 "expected\n", ret, sizeof(buf)));

        return  -EIO;
    }

    result[0] = 0;

    for ( cur = 0; cur < sizeof(_uint); cur++ )
    {
        result[0] |= ( buf[cur] << ( cur * 8 ) );
    }

    if ( change_mode )
        set_fs(orig_fs);

    return  ret;
}



/**
 ** FUNCTION: write_long
 **/

static int  write_long (int fd, _ulong value, int change_mode)
{
#if POST_20_KERNEL_F
    mm_segment_t    orig_fs;
#else
    unsigned long   orig_fs;
#endif
    unsigned char   buf[sizeof(_ulong)];
    int             cur;
    int             ret;

    if ( change_mode )
    {
        orig_fs = get_fs();
        set_fs(KERNEL_DS);
    }

    for ( cur = 0; cur < sizeof(_ulong); cur++ )
    {
            /* Grab the lowest byte and then shift the value by one byte */

        buf[cur] = value & 0xFF;
        value >>= 8;
    }

    ret = write_bytes(fd, buf, sizeof(buf));

    if ( change_mode )
        set_fs(orig_fs);

    if ( ret != sizeof(buf) )
        return  -EIO;

    return  ret;
}



/**
 ** FUNCTION: read_long
 **/

static int  read_long (int fd, _ulong *result, int change_mode)
{
#if POST_20_KERNEL_F
    mm_segment_t    orig_fs;
#else
    unsigned long   orig_fs;
#endif
    unsigned char   buf[sizeof(_ulong)];
    int             cur;
    int             ret;

    if ( result == (_ulong *) NULL )
        return  -EIO;

    if ( change_mode )
    {
        orig_fs = get_fs();
        set_fs(KERNEL_DS);
    }

    ret = read_bytes(fd, buf, sizeof(buf));

    if ( ret < 0 )
    {
        DPRINT1(("OVLFS: error %d returned from read of storage file\n", ret));

        return  ret;
    }
    else if ( ret != sizeof(buf) )
    {
        DPRINT1(("OVLFS: error only %d bytes read from storage file; %d bytes "
                 "expected\n", ret, sizeof(buf)));

        return  -EIO;
    }

    result[0] = 0;

    for ( cur = 0; cur < sizeof(_ulong); cur++ )
    {
        result[0] |= ( buf[cur] << ( cur * 8 ) );
    }

    if ( change_mode )
        set_fs(orig_fs);

    return  ret;
}



/**
 ** FUNCTION: stg1_write_inode_det
 **
 **  PURPOSE: Write the detail information specific to the type of the inode
 **           given.
 **/

static int  stg1_write_inode_det (int fd, ovlfs_inode_t *ino, int change_mode)
{
#if POST_20_KERNEL_F
    mm_segment_t    orig_fs;
#else
    unsigned long   orig_fs;
#endif
    int             ret;

    if ( ino == (ovlfs_inode_t *) NULL )
        return  -EIO;

    if ( change_mode )
    {
        orig_fs = get_fs();
        set_fs(KERNEL_DS);
    }

    if ( S_ISBLK(ino->mode) || S_ISCHR(ino->mode) )
    {
        ret = write_integer(fd, ino->u.int_value, 0);
    }
    else if ( S_ISDIR(ino->mode) )
    {
        int tot_ent;
        int cur_ent;

            /* Write the number of directory entries and then write each */
            /*  entry's information.                                     */

        if ( ino->dir_entries == (list_t) NULL )
            tot_ent = 0;
        else
            tot_ent = klist_func(list_length)(ino->dir_entries);

        ret = write_integer(fd, tot_ent, 0);

        for ( cur_ent = 1; ( cur_ent <= tot_ent ) && ( ret >= 0 ); cur_ent++ )
        {
            ovlfs_dir_info_t    *entry;

            entry = (ovlfs_dir_info_t *)
                    klist_func(nth_element)(ino->dir_entries, cur_ent);

            if ( entry == (ovlfs_dir_info_t *) NULL )
            {
                DPRINT1(("OVLFS: stg1_write_inode_det: null directory entry "
                         "(%d) retrieved from list\n", cur_ent));

                ret = -EIO;
            }
            else
            {
                ret = write_integer(fd, entry->len, 0);

                if ( ret >= 0 )
                {
                    ret = write_bytes(fd, entry->name, entry->len);

                    if ( ret == entry->len )
                    {
                        ret = write_integer(fd, entry->ino, 0);

                        if ( ret >= 0 )
                            ret = write_integer(fd, entry->flags, 0);
                    }
                    else
                        if ( ret >= 0 )
                            ret = -EIO;
                }
            }
        }
    }
    else
        ret = 0;

    if ( change_mode )
        set_fs(orig_fs);

    return  ret;
}



/**
 ** FUNCTION: stg1_read_inode_det
 **
 **  PURPOSE: Read the detail information specific to the type of the inode
 **           given.
 **/

static int  stg1_read_inode_det (int fd, ovlfs_inode_t *ino, int change_mode)
{
#if POST_20_KERNEL_F
    mm_segment_t    orig_fs;
#else
    unsigned long   orig_fs;
#endif
    int             tmp_int;
    int             ret;

    if ( ino == (ovlfs_inode_t *) NULL )
        return  -EIO;

    if ( change_mode )
    {
        orig_fs = get_fs();
        set_fs(KERNEL_DS);
    }

    if ( S_ISBLK(ino->mode) || S_ISCHR(ino->mode) )
    {
        ret = read_integer(fd, &tmp_int, 0);

        if ( ret >= 0 )
            ino->u.int_value = tmp_int;
    }
    else if ( S_ISDIR(ino->mode) )
    {
        int tot_ent;
        int cur_ent;

            /* Read the number of directory entries and then read each */
            /*  entry's information.                                   */

        ret = read_integer(fd, &tot_ent, 0);

        for ( cur_ent = 1; ( cur_ent <= tot_ent ) && ( ret >= 0 ); cur_ent++ )
        {
            ovlfs_dir_info_t    tmp;

                /* Read the entry's name; first read the length of the name */

            ret = read_integer(fd, &tmp_int, 0);
            tmp.len = tmp_int;

            if ( ( ret >= 0 ) && ( tmp.len > 0 ) )
            {
                tmp.name = MALLOC(tmp.len);

                if ( tmp.name == (char *) NULL )
                {
                    DPRINT1(("OVLFS: stg1_read_inode_det: unable to allocate "
                             " %d bytes for name", tmp.len));

                    ret = -ENOMEM;
                }
                else
                {
                    ret = read_bytes(fd, tmp.name, tmp.len);

                    if ( ret != tmp.len )
                        if ( ret >= 0 )
                            ret = -EIO;
                }
            }
            else
                tmp.name = (char *) NULL;

            if ( ret >= 0 )
            {
                ret = read_integer(fd, &tmp_int, 0);
                tmp.ino = tmp_int;

                if ( ret >= 0 )
                {
                    ret = read_integer(fd, &tmp_int, 0);
                    tmp.flags = tmp_int;
                }
            }

            if ( ret >= 0 )
                ret = ovlfs_ino_add_dirent(ino, tmp.name, tmp.len, tmp.ino,
                                           tmp.flags);

            if ( tmp.name != (char *) NULL )
                FREE(tmp.name);
        }
    }
    else
        ret = 0;

    if ( change_mode )
        set_fs(orig_fs);

    return  ret;
}



/**
 ** FUNCTION: stg1_write_inode_spec
 **
 **  PURPOSE: Write the special attributes of the inode.  This includes the
 **           parent directory inode number, reference name, and inode
 **           mappings.
 **/

static int	stg1_write_inode_spec (int fd, struct super_block *sup_blk,
		                       ovlfs_inode_special_t *s,
		                       int change_mode)
{
#if POST_20_KERNEL_F
	mm_segment_t	orig_fs;
#else
	unsigned long	orig_fs;
#endif
	int		ret;

	if ( change_mode )
	{
		orig_fs = get_fs();
		set_fs(KERNEL_DS);
	}

	ret = write_long(fd, s->parent_ino, 0);

	if ( ret >= 0 )
	{
			/* Only include the source to store the mapping */
			/*  information if compiled with that ability.  */

#if STORE_MAPPINGS
				/* Store the base mappings, unless the */
				/*  fs was mounted with base mapping   */
				/*  storage turned off.                */

		if ( ovlfs_sb_opt(sup_blk, storage_opts) &
		     OVLFS_STORE_BASE_MAP )
		{
			ret = write_integer(fd, s->base_dev, 0);

			if ( ret >= 0 )
				ret = write_long(fd, s->base_ino, 0);
		}


				/* Store the stg mappings, unless the */
				/*  fs was mounted with stg mapping   */
				/*  storage turned off.               */

		if ( ( ret >= 0 ) &&
		     ( ( ovlfs_sb_opt(sup_blk, storage_opts) &
		         OVLFS_STORE_STG_MAP ) ) )
		{
			ret = write_integer(fd, s->stg_dev, 0);

			if ( ret >= 0 )
				ret = write_long(fd, s->stg_ino, 0);
		}
#endif

		if ( ret >= 0 )
		{
			ret = write_integer(fd, s->flags, 0);

			if ( ret >= 0 )
			{
				if ( s->len < 0 )	/* just to be safe */
					ret = write_integer(fd, 0, 0);
				else
					ret = write_integer(fd, s->len, 0);

				if ( ( ret >= 0 ) && ( s->len > 0 ) )
				{
					ret = write_bytes(fd, s->name, s->len);
				}
			}
		}
	}


	if ( change_mode )
		set_fs(orig_fs);

	return	ret;
}



/**
 ** FUNCTION: stg1_read_inode_spec
 **
 **  PURPOSE: Read the special attributes of the inode.  This includes the
 **           parent directory inode number, reference name, and inode
 **           mappings.
 **/

static int	stg1_read_inode_spec (int fd, struct super_block *sup_blk,
		                      ovlfs_inode_special_t *s, int stg_flags,
		                      int change_mode)
{
#if POST_20_KERNEL_F
	mm_segment_t	orig_fs;
#else
	unsigned long	orig_fs;
#endif
	unsigned long	tmp_long;
	unsigned int	tmp_int;
	unsigned int	tmp_dev;
	unsigned long	tmp_ino;
	int		ret;

	if ( change_mode )
	{
		orig_fs = get_fs();
		set_fs(KERNEL_DS);
	}

	ret = read_long(fd, &tmp_long, FALSE);
	if ( ret < 0 )
		goto	stg1_read_inode_spec__out;

	s->parent_ino = tmp_long;


		/* Read the mapping information when the file contains it. */
		/*  If mappings are disabled, just discard the info.       */

		/* Read base filesystem mappings. */

	if ( stg_flags & OVL_STG_FLAG__BASE_MAPPINGS )
	{
			/* Read the device number. */
		ret = read_integer(fd, &tmp_dev, FALSE);

		if ( ret < 0 )
			goto	stg1_read_inode_spec__out;


			/* Read the inode number. */
		ret = read_long(fd, &tmp_ino, FALSE);

		if ( ret < 0 )
			goto	stg1_read_inode_spec__out;

#if STORE_MAPPINGS
			/* Check whether the current mount enables them. */

		if ( ovlfs_sb_opt(sup_blk, storage_opts) &
		     OVLFS_STORE_BASE_MAP )
		{
			s->base_dev = tmp_dev;
			s->base_ino = tmp_ino;
		}
#endif
	}

		/* Again for storage filesystem mappings. */

	if ( stg_flags & OVL_STG_FLAG__STG_MAPPINGS )
	{
			/* Read the device number. */
		ret = read_integer(fd, &tmp_dev, FALSE);

		if ( ret < 0 )
			goto	stg1_read_inode_spec__out;


			/* Read the inode number. */
		ret = read_long(fd, &tmp_ino, FALSE);

		if ( ret < 0 )
			goto	stg1_read_inode_spec__out;

#if STORE_MAPPINGS
			/* Check whether the current mount enables them. */

		if ( ovlfs_sb_opt(sup_blk, storage_opts) &
		     OVLFS_STORE_STG_MAP )
		{
			s->stg_dev = tmp_dev;
			s->stg_ino = tmp_ino;
		}
#endif
	}


		/* Read the inode flags. */

	ret = read_integer(fd, &tmp_int, FALSE);
	if ( ret < 0 )
		goto	stg1_read_inode_spec__out;

	s->flags = tmp_int;


		/* Read the inode's name length. */

	ret = read_integer(fd, &tmp_int, FALSE);
	if ( ret < 0 )
		goto	stg1_read_inode_spec__out;

	s->len = tmp_int;

	if ( s->len > 0 )
	{
		s->name = MALLOC(s->len);

		if ( s->name == NULL )
		{
			ret = -ENOMEM;
			goto	stg1_read_inode_spec__out;
		}

		ret = read_bytes(fd, s->name, s->len);

		if ( ret < 0 )
			goto	stg1_read_inode_spec__out;
	}


stg1_read_inode_spec__out:

	if ( change_mode )
		set_fs(orig_fs);

	return	ret;
}



/**
 ** FUNCTION: stg1_write_inode
 **/

static int  stg1_write_inode (int fd, struct super_block *sup_blk,
                              ovlfs_inode_t *ino, int change_mode)
{
#if POST_20_KERNEL_F
	mm_segment_t	orig_fs;
#else
	unsigned long	orig_fs;
#endif
	off_t		pos;
	off_t		result_pos;
	int		ret;

	if ( ino == (ovlfs_inode_t *) NULL )
		return	-EIO;

		/* Remember the current location so that the inode */
		/*  structure's size can be updated later.         */

	pos = file_lseek(fd, 0, FILE_IO__SEEK_CUR);

	if ( pos < 0 )
	{
		ret = pos;
		goto	stg1_write_inode__out;
	}

	if ( change_mode )
	{
		orig_fs = get_fs();
		set_fs(KERNEL_DS);
	}

		/* Write the inode's information. */

		/* Place a filler for the size of this entry since entries */
		/*  are variable length.                                   */

	ret = write_integer(fd, 0, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_integer(fd, ino->s.flags, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_integer(fd, ino->uid, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_integer(fd, ino->gid, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

		/* If the attributes for this inode have not been set, just */
		/*  write 0 for mode; otherwise, write the real mode.       */

	if ( ino->att_valid_f )
		ret = write_integer(fd, ino->mode, FALSE);
	else
		ret = write_integer(fd, 0, FALSE);

	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_integer(fd, ino->size, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_integer(fd, ino->atime, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_integer(fd, ino->mtime, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_integer(fd, ino->ctime, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_integer(fd, ino->nlink, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_long(fd, ino->blksize, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_long(fd, ino->blocks, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_long(fd, ino->version, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_long(fd, ino->nrpages, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = stg1_write_inode_spec(fd, sup_blk, &ino->s, FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = stg1_write_inode_det(fd,ino,FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;


		/* All is well; seek back to the start of this inode's info */
		/*  and update the size.  Remeber the current location and  */
		/*  return after the update so the file is positioned just  */
		/*  passed the end of this inode.                           */

	result_pos = file_lseek(fd, 0, FILE_IO__SEEK_CUR);

	if ( result_pos < 0 )
	{
		ret = result_pos;
		goto	stg1_write_inode__out_fs;
	}

	ret = file_lseek(fd, pos, FILE_IO__SEEK_SET);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = write_integer(fd, ( result_pos - pos ), FALSE);
	if ( ret < 0 )
		goto	stg1_write_inode__out_fs;

	ret = file_lseek(fd, result_pos, FILE_IO__SEEK_SET);

stg1_write_inode__out_fs:

	if ( change_mode )
		set_fs(orig_fs);

stg1_write_inode__out:

	return	ret;
}



/**
 ** FUNCTION: stg1_read_inode
 **/

static int	stg1_read_inode (int fd, struct super_block *sup_blk,
		                 ovlfs_inode_t *ino, int stg_flags,
		                 int change_mode)
{
#if POST_20_KERNEL_F
	mm_segment_t	orig_fs;
#else
	unsigned long	orig_fs;
#endif
	int			tot_size;
	int			tmp_int;
	long			tmp_long;
	int			ret;

	if ( ino == (ovlfs_inode_t *) NULL )
	{
		ret = -EIO;
		goto	stg1_read_inode__out;
	}

	if ( change_mode )
	{
		orig_fs = get_fs();
		set_fs(KERNEL_DS);
	}

		/* Read the inode's information. First is the size used by */
		/*  this inode; we don't need this, but it is useful when  */
		/*  looking for one particular inode instead of trying to  */
		/*  read all of them.                                      */

	ret = read_integer(fd, &tot_size, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ret = read_integer(fd, &tmp_int, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->s.flags = tmp_int;

	ret = read_integer(fd, &tmp_int, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;


	ino->uid = tmp_int;

	ret = read_integer(fd, &tmp_int, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;


	ino->gid = tmp_int;

	ret = read_integer(fd, &tmp_int, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->mode = tmp_int;

		/* If the mode value is 0, then the attributes for this */
		/*  inode have not been set.                            */

	if ( ino->mode == 0 )
		ino->att_valid_f = FALSE;
	else
		ino->att_valid_f = TRUE;


	ret = read_integer(fd, &tmp_int, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->size = tmp_int;

	ret = read_integer(fd, &tmp_int, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->atime = tmp_int;

	ret = read_integer(fd, &tmp_int, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->mtime = tmp_int;

	ret = read_integer(fd, &tmp_int, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->ctime = tmp_int;

	ret = read_integer(fd, &tmp_int, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->nlink = tmp_int;

	ret = read_long(fd, &tmp_long, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->blksize = tmp_long;

	ret = read_long(fd, &tmp_long, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->blocks = tmp_long;

	ret = read_long(fd, &tmp_long, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->version = tmp_long;

	ret = read_long(fd, &tmp_long, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ino->nrpages = tmp_long;

	ret = stg1_read_inode_spec(fd, sup_blk, &ino->s, stg_flags, FALSE);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

	ret = stg1_read_inode_det(fd, ino, 0);
	if ( ret < 0 )
		goto	stg1_read_inode__out_fs;

stg1_read_inode__out_fs:

	if ( change_mode )
		set_fs(orig_fs);

stg1_read_inode__out:

	return	ret;
}



#if STORE_MAPPINGS

/**
 ** FUNCTION: ovlfs_write_mappings
 **/

static int  ovlfs_write_mappings (int fd, list_t map_list, int change_mode,
                                  int skip)
{
    int                 tot_ent;
    int                 cur_ent;
    ovlfs_ino_map_t     *one_map;
#if POST_20_KERNEL_F
    mm_segment_t    orig_fs;
#else
    unsigned long   orig_fs;
#endif
    int                 ret;

    if ( change_mode )
    {
        orig_fs = get_fs();
        set_fs(KERNEL_DS);
    }

    if ( ( map_list == (list_t) NULL ) || ( skip ) )
        tot_ent = 0;
    else
        tot_ent = klist_func(list_length)(map_list);

    ret = write_integer(fd, tot_ent, 0);

    cur_ent = 1;

    while ( ( cur_ent <= tot_ent ) && ( ret >= 0 ) )
    {
        one_map = (ovlfs_ino_map_t *)
                  klist_func(nth_element)(map_list, cur_ent);

        if ( one_map == (ovlfs_ino_map_t *) NULL )
        {
            DPRINT1(("OVLFS: ovlfs_write_mappings: null element (%d) in list\n",
                     cur_ent));

            ret = -EIO;
        }
        else
        {
            ret = write_integer(fd, one_map->dev, 0);

            if ( ret >= 0 )
            {
                ret = write_integer(fd, one_map->ino, 0);

                if ( ret >= 0 )
                    ret = write_integer(fd, one_map->ovl_ino, 0);
            }
        }

        cur_ent++;
    }

    if ( change_mode )
        set_fs(orig_fs);

    return  ret;
}



/**
 ** FUNCTION: ovlfs_read_mappings
 **/

static int  ovlfs_read_mappings (int fd, list_t *map_list, int change_mode,
                                 int skip)
{
    int                 tot_ent;
    int                 cur_ent;
    ovlfs_ino_map_t     *one_map;
#if POST_20_KERNEL_F
    mm_segment_t    orig_fs;
#else
    unsigned long   orig_fs;
#endif
    int                 ret;

    if ( map_list == (list_t *) NULL )
        return  -EIO;

    if ( change_mode )
    {
        orig_fs = get_fs();
        set_fs(KERNEL_DS);
    }

    ret = read_integer(fd, &tot_ent, 0);

        /* Only work on the list if there is at least one entry */

    if ( ( ret >= 0 ) && ( tot_ent > 0 ) )
    {
        if ( skip )
        {
            _uint tot_size;

            tot_size = ( sizeof(_uint) * 3 * tot_ent ) ;

                /* We need to skip the mapping entries; just seek */

            if ( file_lseek(fd, tot_size, 1) == -1 )    /* 1 => SEEK_CUR */
                ret = -EIO;
        }
        else
        {
            if ( map_list[0] == (list_t) NULL )
            {
                map_list[0] = create_ino_map_list();
    
                if ( map_list[0] == (list_t) NULL )
                {
                    DPRINT1(("OVLFS: unable to create inode mapping list\n"));

                    ret = -ENOMEM;
                }
            }
    
            cur_ent = 1;
    
            while ( ( cur_ent <= tot_ent ) && ( ret >= 0 ) )
            {
                one_map = create_ino_map();
    
                if ( one_map == (ovlfs_ino_map_t *) NULL )
                {
                    DPRINT1(("OVLFS: unable to allocate memory for an inode "
                             "mapping\n"));

                    ret = -ENOMEM;
                }
                else
                {
                    int ovl_ino;
                    int dev;
                    int ino;
    
                    ret = read_integer(fd, &dev, 0);
    
                    if ( ret >= 0 )
                    {
                        ret = read_integer(fd, &ino, 0);
    
                        if ( ret >= 0 )
                        {
                            ret = read_integer(fd, &ovl_ino, 0);
    
                            if ( ret >= 0 )
                            {
                                one_map->dev     = dev;
                                one_map->ino     = ino;
                                one_map->ovl_ino = ovl_ino;
    
                                if ( klist_func(insert_into_list_sorted)
                                               (map_list[0], one_map) == NULL )
                                {
                                    DPRINT1(("OVLFS: error attempting to insert"
                                             " mapping into mapping list\n"));

                                    ret = -ENOMEM;
                                }
                            }
                        }
                    }
    
                    if ( ret < 0 )
                        FREE(one_map);
                }
    
                cur_ent++;
            }
        }
    }

    if ( change_mode )
        set_fs(orig_fs);

    return  ret;
}

#endif



/**
 ** FUNCTION: open_stg_file
 **
 **  PURPOSE: Given the super block of an overlay filesystem and the mode to
 **           open its storage file with, open the storage file.
 **/

static int	open_stg_file (struct super_block *sb, int mode, int perm)
{
    ovlfs_super_info_t  *s_info;
    int                 fd;
    int                 ret;

    ret = -ENOENT;

    s_info = sb->u.generic_sbp;

        /* If there is no place to store the information, just throw  */
        /*  it to the old bit bucket.  The filesystem state will not  */
        /*  be persistent, but the filesystem will be usable as such. */

    if ( s_info->u.stg1.stg_inode == INODE_NULL )
    {
        if ( s_info->u.stg1.stg_name == (char *) NULL )
            fd = -ENOENT;
        else
            fd = file_open(s_info->u.stg1.stg_name, mode, perm);
    }
    else
    {
#if POST_20_KERNEL_F
        struct file     *filp;
        struct dentry   *d_ent;

        d_ent = ovlfs_inode2dentry(s_info->u.stg1.stg_inode);

        if ( IS_ERR(d_ent) )
        {
            ret = PTR_ERR(d_ent);
        }
        else if ( d_ent != NULL )
        {
            filp = dentry_open(d_ent, NULL, mode);

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
                ret = file_dup_filp(filp);

                if ( ret < 0 )
                {
                    filp_close(filp, current->files);
                    put_filp(filp);
                }
            }
        }
#else
        ret = open_inode(s_info->u.stg1.stg_inode, mode);
#endif
    }

    return ret;
}



/**
 ** FUNCTION: ovlfs_stg1_write_super
 **/

#define MAGIC_WORD_V3	"Ovl3"
#define MAGIC_WORD_V4	"Ovl4"
#define MAGIC_WORD	MAGIC_WORD_V4
#define MAGIC_WORD_LEN	4

static int  ovlfs_stg1_write_super (struct super_block *sup_blk)
{
	ovlfs_super_info_t	*s_info;
	char			magic_buf[] = MAGIC_WORD;
	int			tot_ino;
	int			cur_ino;
	int			fd;
	int			ret;
	int			skip;
	int			stg_flags;
#if POST_20_KERNEL_F
	mm_segment_t		orig_fs;
#else
	unsigned long		orig_fs;
#endif

	DPRINTC2("sup_blk 0x%08x\n", (int) sup_blk);


	if ( ( sup_blk == (struct super_block *) NULL ) ||
	     ( sup_blk->u.generic_sbp == NULL ) )
	{
		ret = -EINVAL;
		goto ovlfs_stg1_write_super__out;
	}

	s_info = sup_blk->u.generic_sbp;

	orig_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = open_stg_file(sup_blk, O_WRONLY | O_TRUNC | O_CREAT, 0644);

	if ( fd < 0 )
	{
		ret = fd;
		goto	ovlfs_stg1_write_super__out_set_fs;
	}

	ret = write_integer(fd, sup_blk->s_magic, FALSE);

	if ( ret < 0 )
		goto	ovlfs_stg1_write_super__out_close;

	ret = write_bytes(fd, magic_buf, MAGIC_WORD_LEN);

	if ( ret < 0 )
		goto	ovlfs_stg1_write_super__out_close;


		/* Determine the storage flags needed and write them now. */

	stg_flags = 0;

#if STORE_MAPPINGS
	if ( ovlfs_sb_opt(sup_blk, storage_opts) & OVLFS_STORE_BASE_MAP )
		stg_flags |= OVL_STG_FLAG__BASE_MAPPINGS;

	if ( ovlfs_sb_opt(sup_blk, storage_opts) & OVLFS_STORE_STG_MAP )
		stg_flags |= OVL_STG_FLAG__STG_MAPPINGS;
#endif

	ret = write_integer(fd, stg_flags, FALSE);


		/* Loop through all of the inodes and write each one out. */

	cur_ino = 1;
	tot_ino = klist_func(list_length)(s_info->u.stg1.inodes);

	ret = write_integer(fd, tot_ino, FALSE);

	while ( ( cur_ino <= tot_ino ) && ( ret >= 0 ) )
	{
		ovlfs_inode_t	*ino;

		ino = (ovlfs_inode_t *)
		      klist_func(nth_element)(s_info->u.stg1.inodes, cur_ino);

		ret = stg1_write_inode(fd, sup_blk, ino, 0);

		cur_ino++;
	}

	if ( ret < 0 )
		goto	ovlfs_stg1_write_super__out_close;

#if STORE_MAPPINGS
		/* Write base fs mapping information now. */

	skip = ! ( ovlfs_sb_opt(sup_blk, storage_opts) &
		   OVLFS_STORE_BASE_MAP );

	ret = ovlfs_write_mappings(fd, s_info->u.stg1.base_to_ovl,
				   0, skip);

	if ( ret < 0 )
		goto	ovlfs_stg1_write_super__out_close;


		/* Write storage fs mapping information now. */

	skip = ! ( ovlfs_sb_opt(sup_blk, storage_opts) &
		   OVLFS_STORE_STG_MAP );

	ret = ovlfs_write_mappings(fd, s_info->u.stg1.stg_to_ovl, 0, skip);
#endif

ovlfs_stg1_write_super__out_close:
	file_close(fd);

ovlfs_stg1_write_super__out_set_fs:
	set_fs(orig_fs);

ovlfs_stg1_write_super__out:
	return	ret;
}



/**
 ** FUNCTION: is_empty
 **
 **  PURPOSE: Determine if the file, whose descriptor is given, is empty.
 **/

static int	is_empty (int fd)
{
	int	ret;
	int	rc;
	off_t	pos;


		/* Check the current position; it must be 0 if the file is  */
		/*  empty.  This function must return with the file pointer */
		/*  referring to the same place in the file as when called. */

	pos = file_lseek(fd, 0, 1);		/* 1 = SEEK_CUR */

	if ( pos != 0 )
	{
		ret = 0;	/* not empty */
	}
	else
	{
			/* Seek to the end and see if that is offset 0 */

		pos = file_lseek(fd, 0, 2);		/* 2 = SEEK_END */

		if ( pos != 0 )
			ret = 0;	/* not empty */
		else
			ret = 1;	/* empty */


			/* Now return to the start of the file */

		if ( ( rc = file_lseek(fd, 0, 0) ) < 0 )    /* 0 = SEEK_SET */
		{
			DPRINT1(("OVLFS: is_empty(): file_lseek(%d, 0, 0) "
			         "returned err %d\n", fd, rc));

		}
	}

	return	ret;
}



/**
 ** FUNCTION: stg1_read_magic
 **
 **  PURPOSE: Read the magic word from the storage file, validate it, and
 **           set the storage flags as-appropriate for the version of the
 **           storage file.
 **/

int	stg1_read_magic (int fd, int *p_flags)
{
	int	ret;
	char	magic_buf[MAGIC_WORD_LEN];

		/* Read the magic word from the storage file. */
	ret = read_bytes(fd, magic_buf, MAGIC_WORD_LEN);

	if ( ret < 0 )
		goto	stg1_read_magic__out;

		/* Check that the magic word is valid and determine the    */
		/*  version of the storage file with which we are working. */

	if ( memcmp(MAGIC_WORD_V4, magic_buf, MAGIC_WORD_LEN) == 0 )
	{
			/* This version supports flags stored in the file. */
			/*  And, the flags need to be read now.  Just let  */
			/*  any error fall through.                        */

		ret = read_integer(fd, p_flags, FALSE);
	}
	else if ( memcmp(MAGIC_WORD_V3, magic_buf, MAGIC_WORD_LEN) == 0 )
	{
			/* No flags in this version.  It always stores    */
			/*  mappings, when they are compiled into the     */
			/*  driver.  If the file and driver do not match, */
			/*  it will fail.                                 */

#if STORE_MAPPINGS
		p_flags[0] = OVL_STG_FLAG__BASE_MAPPINGS |
		             OVL_STG_FLAG__STG_MAPPINGS;
#else
		p_flags[0] = 0;
#endif
	}
	else
	{
		DPRINT1(("OVLFS: ovlfs_read_super: magic word does not "
		         "match; expected '%.*s' and read '%.*s'\n",
		         MAGIC_WORD_LEN, MAGIC_WORD, ret, magic_buf));

		ret = -EIO;
	}

stg1_read_magic__out:
	return	ret;
}



/**
 ** FUNCTION: stg1_read_super
 **/

static int  stg1_read_super (struct super_block *sup_blk)
{
	ovlfs_super_info_t	*s_info;
	int			fd;
	int			tmp_int;
	int			ret;
	int			stg_flags;
	int			tot_ino;
	int			cur_ino;
	int			skip;
#if POST_20_KERNEL_F
	mm_segment_t		orig_fs;
#else
	unsigned long		orig_fs;
#endif

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_read_super\n"));

	if ( ( sup_blk == (struct super_block *) NULL ) ||
	     ( sup_blk->u.generic_sbp == NULL ) )
	{
		ret = -EINVAL;
		goto	stg1_read_super__out;
	}

	s_info = sup_blk->u.generic_sbp;

	orig_fs = get_fs();
	set_fs(KERNEL_DS);

	ret = 0;

		/* Open the storage file now. */

	fd = open_stg_file(sup_blk, O_RDONLY, 0);

	if ( fd < 0 )
	{
			/* The error -ENOENT is returned if no stg file */
			/*  was specified.  It is also acceptable to    */
			/*  continue when no storage file yet exists.   */

		if ( fd != -ENOENT )
		{
			DPRINT1(("OVLFS: ovlfs_stg1_write_super: open storage "
			         "file %s returned %d\n",
			         s_info->u.stg1.stg_name, fd));

			ret = fd;
			goto	stg1_read_super__out_set_fs;
		}
	}
	else if ( is_empty(fd) )
	{
		ret = -ENOENT;	/* pretend the file does not exist */
		goto	stg1_read_super__out_close;
	}


		/* Read the magic number. */

	ret = read_integer(fd, &tmp_int, 0);

	if ( tmp_int != sup_blk->s_magic )
	{
		DPRINT1(("OVLFS: ovlfs_read_super: magic number in storage "
		         "file, 0x%x, does not match super block's, 0x%x\n",
		         tmp_int, (int) sup_blk->s_magic));

		ret = -EIO;
		goto	stg1_read_super__out_close;
	}


		/* Read the magic word and version-specific information. */

	ret = stg1_read_magic(fd, &stg_flags);

	if ( ret < 0 )
		goto	stg1_read_super__out_close;



		/* Read the number of inodes in the file. */

	ret = read_integer(fd, &tot_ino, 0);


		/* Loop through all of the inodes and read each one now. */

	cur_ino = 1;

	while ( cur_ino <= tot_ino )
	{
		ovlfs_inode_t	*ino;

		ino = ovlfs_add_inode(sup_blk, NULL, 0);

		if ( ino == (ovlfs_inode_t *) NULL )
		{
			DPRINT1(("OVLFS: ovlfs_read_super: unable to add a new"
			         " inode to the super block at 0x%x\n",
			         (int) sup_blk));

			ret = -ENOMEM;
			goto	stg1_read_super__out_close;
		}

		ret = stg1_read_inode(fd, sup_blk, ino, stg_flags, FALSE);

		if ( ret < 0 )
		{
			DPRINT1(("OVLFS: ovlfs_read_super: error reading "
			         "inode information from the storage file\n"));

			goto	stg1_read_super__out_close;
		}

		cur_ino++;
	}

#if STORE_MAPPINGS
	skip = ! (ovlfs_sb_opt(sup_blk, storage_opts) & OVLFS_STORE_BASE_MAP);

	ret = ovlfs_read_mappings(fd, &(s_info->u.stg1.base_to_ovl), 0, skip);

	if ( ret < 0 )
	{
		DPRINT1(("OVLFS: ovlfs_read_super: error reading "
		         "stg fs map from the storage file\n"));

		goto	stg1_read_super__out_close;
	}

	skip = ! ( ovlfs_sb_opt(sup_blk, storage_opts) & OVLFS_STORE_STG_MAP );

	ret = ovlfs_read_mappings(fd, &(s_info->u.stg1.stg_to_ovl), 0, skip);

	if ( ret < 0 )
	{
		DPRINT1(("OVLFS: ovlfs_read_super: error reading base "
		         "fs map from the storage file\n"));

		goto	stg1_read_super__out_close;
	}
#endif

stg1_read_super__out_close:
	file_close(fd);

stg1_read_super__out_set_fs:
	set_fs(orig_fs);

stg1_read_super__out:
	return	ret;
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
 ** FUNCTION: ovlfs_stg1_add_inode
 **
 **  PURPOSE: Add a new inode to storage and obtain the inode through the inode
 **           cache and return it.
 **
 ** NOTES:
 **	- This function does NOT update the mapping information for the inode.
 **/

static int	ovlfs_stg1_add_inode (struct super_block *sb,
		                      _ulong parent_dir_inum,
		                      const char *name, int namelen,
		                      _ulong *result, int flags)
{
	ovlfs_inode_t	*ino;
	int		ret;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_add_inode(%ld, %.*s, %d)\n",
	         parent_dir_inum, namelen, name, namelen));

	ret = 0;

	ino = ovlfs_add_inode(sb, NULL, 0);

	if ( ino == (ovlfs_inode_t *) NULL )
	{
		ret = -ENOMEM;
	}
	else
	{
		ino->s.flags = flags;
		ino->s.parent_ino = parent_dir_inum;

			/* Set the reference name, if given. */

		if ( ( name != (char *) NULL ) && ( namelen > 0 ) )
		{
			ino->s.name = dup_buffer(name, namelen);

			if ( ino->s.name != (char *) NULL )
				ino->s.len = namelen;
			else
			{
				ino->s.len = 0;

				DPRINT1(("OVLFS: unable to duplicate reference"
				         "name (len %d) at %s line %d\n",
				         namelen, __FILE__, __LINE__));
			}
		}

		result[0] = ino->ino;
	}

	DPRINTC((KDEBUG_RET_PREFIX "ovlfs_stg1_add_inode returning\n"));

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_read_inode
 **
 **  PURPOSE: Read the inode's information from storage and store it in the
 **           given inode structure.
 **/

static int	ovlfs_stg1_read_inode (struct inode *rd_inode, int *r_valid_f)
{
	ovlfs_inode_t		*ino;
	ovlfs_inode_info_t	*i_info;

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_stg1_read_inode(%ld)\n",
	         rd_inode->i_ino));

		/* Find the storage information for this inode */

	ino = ovlfs_inode_get_ino(rd_inode);

	if ( ino == (ovlfs_inode_t *) NULL )
	{
		DPRINT1(("OVLFS: ovlfs_stg1_read_inode: inode %ld, dev 0x%x "
		         "not found in storage\n", rd_inode->i_ino,
		         rd_inode->i_dev));

		return	-ENOENT;
	}


		/* Update the inode's attributes from the storage info, but */
		/*  only if they have been updated at some point.           */

	*r_valid_f = ino->att_valid_f;

	if ( ino->att_valid_f )
	{
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
#if PRE_22_KERNEL_F
		rd_inode->i_nrpages = ino->nrpages;
#endif

		if ( S_ISBLK(ino->mode) || S_ISCHR(ino->mode) )
			rd_inode->i_rdev = ino->u.rdev;
		else
			rd_inode->i_rdev = 0;
	}

	if ( rd_inode->u.generic_ip == NULL )
	{
		DPRINT1(("OVLFS: ovlfs_stg1_read_inode: inode given "
			 "has a null generic ip!\n"));
	}
	else
	{
		i_info = (ovlfs_inode_info_t *) rd_inode->u.generic_ip;


			/* Free the existing name, if any. */

		if ( ( i_info->s.name != (char *) NULL ) &&
		     ( i_info->s.len != 0 ) )
			FREE(i_info->s.name);


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
	DPRINTC2("ino %lu\n", inode->i_ino);

	inode = inode;	/* Prevent compiler warnings */

	DPRINTR2("ino %lu\n", inode->i_ino);

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

	DPRINTC2("ino %lu\n", inode->i_ino);

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
#if PRE_22_KERNEL_F
		ino->nrpages = inode->i_nrpages;
#else
		ino->nrpages = 0;
#endif

		if ( S_ISBLK(ino->mode) || S_ISCHR(ino->mode) )
			ino->u.rdev = inode->i_rdev;
		else
			ino->u.generic = NULL;

			/* Indicate the inode attributes are now valid. */

		ino->att_valid_f = TRUE;

                if ( inode->u.generic_ip != NULL )
                {
                    ovlfs_inode_info_t *i_info;

                    i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;

                        /* Update the inode "special" ovlfs information */

                    ino->s.parent_ino  = i_info->s.parent_ino;
                    ino->s.base_dev    = i_info->s.base_dev;
                    ino->s.base_ino    = i_info->s.base_ino;
                    ino->s.stg_dev     = i_info->s.stg_dev;
                    ino->s.stg_ino     = i_info->s.stg_ino;
                    ino->s.flags       = i_info->s.flags;

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
	{
		ret = -ENOENT;
	}

	DPRINTR2("ino %lu; ret = %d\n", inode->i_ino, ret);

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

	DPRINTC2("ino %lu, inc_rm %d\n", inode->i_ino, inc_rm);

	ino = ovlfs_inode_get_ino(inode);

	if ( ino == (ovlfs_inode_t *) NULL )
	{
		ret = -ENOENT;
	}
	else
	{
		if ( ino->dir_entries == (list_t) NULL )
		{
			ret = 0;
		}
		else
		{
			ret = klist_func(list_length)(ino->dir_entries);

			if ( ! inc_rm )
				ret -= ino->num_rm_ent;
		}
	}

	DPRINTR2("ino %lu, inc_rm %d; ret = %d\n", inode->i_ino, inc_rm, ret);

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

static int	ovlfs_stg1_map_inode (struct super_block *sb, _ulong inum1,
		                      kdev_t dev2, _ulong inum2, char which)
{
	int		ret = 0;
	ovlfs_inode_t	*ino;

	DPRINTC2("sb 0x%08x, dev %d: (%lu) => (%d, %lu), %c\n", (int) sb,
	         sb->s_dev, inum1, dev2, inum2, which);

	ino = ovlfs_get_ino(sb, inum1);

	if ( ino == (ovlfs_inode_t *) NULL )
	{
		DPRINT1(("OVLFS: ovlfs_stg1_map_inode: no storage information "
		         "for inode #%ld, dev 0x%x\n", inum1, sb->s_dev));

		ret = -ENOENT;
	}
	else
	{
		ret = stg1_inode_set_other(sb, ino, dev2, inum2, which);
	}

	DPRINTR2("sb 0x%08x, dev %d: (%lu) => (%d, %lu), %c; ret %d\n",
	         (int) sb, sb->s_dev, inum1, dev2, inum2, which, ret);

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
		                       _ulong *result, char which)
{
	ovlfs_super_info_t	*s_info;
	ovlfs_ino_map_t		*i_map;
	ovlfs_ino_map_t		tmp;
	int			ret;
	list_t			map_list;

	DPRINTC2("sb 0x%08x, s_dev %d, dev %d, inum %ld, %c\n",
	         (int) sb, sb->s_dev, dev, inum, which);

		/* Verify the arguments: the super block is required and */
		/*  must be the super block for an overlay filesystem.   */

#if ! FASTER_AND_MORE_DANGEROUS

	if ( ( sb == (struct super_block *) NULL ) ||
	     ( sb->s_magic != OVLFS_SUPER_MAGIC ) ||
	     ( sb->u.generic_sbp == NULL ) )
	{
		DPRINT1(("OVLFS: ovlfs_stg1_map_lookup: invalid super block "
		         "given\n"));

		return	0;
	}

#endif

		/* Find the inode mapping entry */

	s_info = (ovlfs_super_info_t *) sb->u.generic_sbp;

		/* Determine which list to look in for the mapping */

	switch ( which )
	{
		case 'b':
			map_list = s_info->u.stg1.base_to_ovl;
			break;

		case 's':
		case 'o':
			map_list = s_info->u.stg1.stg_to_ovl;
			break;

		default:
			DPRINT1(("OVLFS: ovlfs_stg1_map_lookup: invalid flag, "
			         "'%c'.\n", which));
			map_list = (list_t) NULL;
			break;
	}

		/* If the map list is not set, it is empty - no match. */

	if ( map_list == (list_t) NULL )
	{
		ret = -ENOENT;
	}
	else
	{
			/* Look for the mapping from the specified device */
			/*  and inode #                                   */

		tmp.dev = dev;
		tmp.ino = inum;
		i_map = (ovlfs_ino_map_t *)
				klist_func(search_for_element)(map_list, &tmp);

		if ( i_map != (ovlfs_ino_map_t *) NULL )
		{
			result[0] = i_map->ovl_ino;
			ret = 0;
		}
		else
			ret = -ENOENT;
	}

	DPRINTR2("sb 0x%08x, s_dev %d, dev %d, inum %ld, %c; ret = %d\n",
	         (int) sb, sb->s_dev, dev, inum, which, ret);

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_get_mapping
 **
 **  PURPOSE: Given an inode number and reference filesystem indicator, find
 **           the device and inode number of the reference inode.
 **
 ** ARGUMENTS:
 **	sb	- super block of the overlay filesystem.
 **	inum	- inode number of the ovlfs inode.
 **	which	- a character specifying whether the reference for the base or
 **		  storage filesystem is desired.
 **	r_dev	- return location for the device of the filesystem on which the
 **		  reference inode resides.
 **	r_ino	- return location for the number of the reference inode.
 **
 ** RULES:
 **	- Returns success even when the mapping is "empty".
 **	- Empty mappings are identified by the device number, NODEV.
 **/

static int	ovlfs_stg1_get_mapping (struct super_block *sb, _ulong inum,
		                        char which, kdev_t *r_dev,
		                        _ulong *r_ino)
		                       
{
	ovlfs_inode_t	*ino;
	int		ret_code;

	DPRINTC2("dev 0x%x, ino %lu, which %c\n", sb->s_dev, inum, which);

		/* Find the storage information for this inode */

	ino = ovlfs_get_ino(sb, inum);

	if ( ino == (ovlfs_inode_t *) NULL )
	{
		DPRINT1(("OVLFS: ovlfs_stg1_get_mapping: inode %lu, dev 0x%x "
		         "not found in storage\n", inum, sb->s_dev));

		ret_code = -ENOENT;
		goto	ovlfs_stg1_get_mapping__out;
	}

		/* Got the inode information, now return the requested */
		/*  information.                                       */

	ret_code = 0;

	switch ( which )
	{
	    case 'b':
		r_dev[0] = ino->s.base_dev;
		r_ino[0] = ino->s.base_ino;
		break;

	    case 'o':
	    case 's':
		r_dev[0] = ino->s.stg_dev;
		r_ino[0] = ino->s.stg_ino;
		break;

	    default:
		ret_code = -EINVAL;
		break;
	}

ovlfs_stg1_get_mapping__out:

	DPRINTR2("dev 0x%x, ino %lu, which %c; ret = %d\n", sb->s_dev, inum,
	         which, ret_code);

	return	ret_code;
}



/**
 ** FUNCTION: ovlfs_stg1_find_next
 **/

static int	ovlfs_stg1_find_next (ovlfs_inode_t *ino, loff_t *pos,
		                      ovlfs_dir_info_t **result)
{
	ovlfs_dir_info_t	*dent;
	ovlfs_dir_info_t	tmp;
	ovlfs_dir_info_t	*best;
	int			high;
	int			low;
	int			cur;
	int			loc;
	int			rc;
	int			ret_code;

	ret_code = 0;

		/* First look for an element with the current position. */
	tmp.name  = "";
	tmp.ino   = 0;
	tmp.len   = 0;
	tmp.flags = 0;
	tmp.pos   = pos[0];

	ret_code = klist_func(search_index)(ino->dir_entries, ino->pos_index,
	                                    &tmp, &loc);

	if ( ret_code == 0 )
	{
			/* TBD: locking?  Isn't it possible to get a   */
			/*      different entry here than found by the */
			/*      search?                                */

		rc = klist_func(read_index)(ino->dir_entries, ino->pos_index,
		                            loc, (void **) result);

		if ( rc == -ENOENT )
		{
			ret_code  = 0;
			result[0] = NULL;
		}

		goto	ovlfs_stg1_find_next__out;
	}
	else if ( ret_code != -ENOENT )
	{
		goto	ovlfs_stg1_find_next__out;
	}


		/* No entry found, use a binary search method to hunt down */
		/*  the next highest entry, if one exists.                 */

	dent     = NULL;
	low      = 1;
	high     = klist_func(list_length)(ino->dir_entries);
	best     = NULL;
	ret_code = 0;

	while ( ( low <= high ) && ( ret_code == 0 ) )
	{
		cur = ( low + high ) / 2;
		rc = klist_func(read_index)(ino->dir_entries, ino->pos_index,
		                            cur, (void **) &dent);

		if ( rc != 0 )
		{
				/* This should only happen when reading */
				/*  past the end of the list...         */

			high = cur - 1;
		}
		else
		{
			if ( dent->pos >= pos[0] )	
			{
					/* Found a candidate; keep looking */
					/*  until the lowest matching      */
					/*  position is found.             */

				best = dent;
				high = cur - 1;
			}
			else
			{
				low = cur + 1;
			}
		}
	}

	result[0] = best;

ovlfs_stg1_find_next__out:
	return	ret_code;
}



/**
 ** FUNCTION: ovlfs_stg1_get_dirent
 **
 **  PURPOSE: Obtain the nth directory entry from the given pseudo fs
 **           directory.
 **
 ** RULES:
 **	- Returns 0 on success.
 **	- Returns -1 * errno on error.
 **	- Sets result to NULL and returns 0 when no more entries are available.
 **	- Updates the position to the one immediately following the last one
 **	  searched.  No update occurs if the inode has no directory entries.
 **
 ** NOTES:
 **     - A pointer to the actual entry in the list is returned; the caller
 **       must be careful NOT to free the returned value.
 **/

static int	ovlfs_stg1_get_dirent (struct inode *inode, loff_t *pos,
		                       ovlfs_dir_info_t **result)
{
	ovlfs_inode_t		*ino;
	int			ret;

	DPRINTC2("ino %lu, pos %d\n", inode->i_ino, (int) pos[0]);

	ino = ovlfs_inode_get_ino(inode);

	if ( ino == (ovlfs_inode_t *) NULL )
	{
		ret = -ENOENT;
	}
	else
	{
		ret = 0;

		if ( ino->dir_entries == (list_t) NULL )
		{
			result[0] = (ovlfs_dir_info_t *) NULL;
		}
		else
		{
			ret = ovlfs_stg1_find_next(ino, pos, result);

				/* If successful, update the position to */
				/*  the next potential element.          */

			if ( ( ret == 0 ) && ( result[0] != NULL ) )
				pos[0] = result[0]->pos + 1;
		}
	}

	DPRINTR2("ino %lu, pos %d; ret = %d\n", inode->i_ino, (int) pos[0],
	         ret);

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_lookup
 **/

static int	ovlfs_stg1_lookup (struct inode *inode, const char *name,
		                   int len, _ulong *result, int *r_flags)
{
	ovlfs_inode_t		*ino;
	ovlfs_dir_info_t	*d_info;
	int			ret;

	DPRINTC2("ino %lu, %.*s, %d\n", inode->i_ino, len, name, len);

	ret = 0;

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

		if ( d_info == (ovlfs_dir_info_t *) NULL )
		{
			ret = -ENOENT;
		}
		else
		{
			result[0] = d_info->ino;

			r_flags[0] = d_info->flags;
		}
	}

	DPRINTR2("ino %lu, %.*s, %d; ret = %d\n", inode->i_ino, len, name, len,
	         ret);

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

	DPRINTC2("ino %lu, %.*s, %d, %lu\n", inode->i_ino, len, name, len,
	         inum);

		/* Find the storage info for the given directory inode */

	ino = ovlfs_inode_get_ino(inode);

	if ( ino == (ovlfs_inode_t *) NULL )
		ret = -ENOENT;
	else
		ret = ovlfs_ino_add_dirent(ino, name, len, inum,
		                           OVLFS_DIRENT_NORMAL);

	DPRINTR2("ino %lu, %.*s, %d, %lu\n", inode->i_ino, len, name, len,
	         inum);

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_rename
 **
 **  PURPOSE: Given a directory inode in the pseudo filesystem, the name of an
 **           entry in the directory, and a new name, change the name of the
 **           file in the directory.
 **
 ** NOTES:
 **	- Only called when the renaming the entry of a directory; if the entry
 **	  moves between directories, this method is not used.
 **/

static int	ovlfs_stg1_rename (struct inode *inode, const char *oname,
		                   int olen, const char *nname, int nlen)
{
	ovlfs_inode_t		*dir_ino;
	ovlfs_dir_info_t	*d_info;
	ovlfs_dir_info_t	*n_info;
	int			ret;

	DPRINTC2("ino %lu, %.*s, %d, %.*s, %d\n", inode->i_ino, olen, oname,
	         olen, nlen, nname, nlen);

	ret = -ENOENT;
	dir_ino = ovlfs_inode_get_ino(inode);
	if ( dir_ino == (ovlfs_inode_t *) NULL )
		goto	ovlfs_stg1_rename__out;

	d_info = ovlfs_ino_find_dirent(dir_ino, oname, olen);

	if ( ( d_info == (ovlfs_dir_info_t *) NULL ) ||
	     ( d_info->flags & OVLFS_DIRENT_UNLINKED ) )
		goto	ovlfs_stg1_rename__out;

		/* See if the new name is already there. */
	n_info = ovlfs_ino_find_dirent(dir_ino, nname, nlen);


		/* If the new name and the old name result in the same */
		/*  directory entry, there is nothing to do.           */

	ret = 0;
	if ( n_info == d_info )
		goto	ovlfs_stg1_rename__out;


		/* Mark the original entry as deleted. */
	d_info->flags |= OVLFS_DIRENT_UNLINKED;
	dir_ino->num_rm_ent++;	/* update count of unlinked entries */

	if ( n_info != NULL )
	{
			/* If the target is marked as unlinked, mark it as  */
			/*  linked and update the directory's unlink count. */

		if ( n_info->flags & OVLFS_DIRENT_UNLINKED )
		{
			n_info->flags &= ~OVLFS_DIRENT_UNLINKED;
			n_info->flags |= OVLFS_DIRENT_RELINKED;
			dir_ino->num_rm_ent--;
		}

			/* Set the new entry to refer to the old one's inode. */

		n_info->ino = d_info->ino;
	}
	else
	{
/* TBD: change this - it should only add the new name */
#if 1
		ret = ovlfs_ino_add_dirent(dir_ino, nname, nlen, d_info->ino,
		                           OVLFS_DIRENT_NORMAL);

			/* If that failed, re-instate the original. */
		if ( ret < 0 )
		{
			d_info->flags &= ~OVLFS_DIRENT_UNLINKED;
			dir_ino->num_rm_ent--;
		}
#else
		char	*tmp;

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
			d_info->len  = nlen;

			klist_func(sort_list)(dir_ino->dir_entries);
			ret = 0;
		}
#endif
	}

ovlfs_stg1_rename__out:

	DPRINTR2("ino %lu, %.*s, %d, %.*s, %d; ret = %d\n", inode->i_ino, olen,
             oname, olen, nlen, nname, nlen, ret);

	return	ret;
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

	DPRINTC2("ino %lu, %.*s, %d\n", inode->i_ino, len, name, len);

	ino = ovlfs_inode_get_ino(inode);

	if ( ino == (ovlfs_inode_t *) NULL )
		ret = -ENOENT;
	else
		ret = ovlfs_unlink_dirent(ino, name, len);

	DPRINTR2("ino %lu, %.*s, %d; ret = %d\n", inode->i_ino, len, name, len,
	         ret);

	return	ret;
}



/**
 ** FUNCTION: ovlfs_stg1_delete_dirent
 **
 **  PURPOSE: Given a directory inode in the pseudo filesystem and the name
 **           of an entry in the directory, remove the entry from the directory
 **           listing.
 **
 ** NOTES:
 **	- This is a permanent removal as opposed to the unlink which marks the
 **	  entry as deleted.  If a new entry is found with the same name in the
 **	  one of the directory's reference inodes, it will be added after the
 **	  entry is deleted.
 **/

static int	ovlfs_stg1_delete_dirent (struct inode *inode,
		                          const char *name, int len)
{
	ovlfs_inode_t	*ino;
	int		ret;

	DPRINTC2("ino %lu, %.*s, %d\n", inode->i_ino, len, name, len);

	ino = ovlfs_inode_get_ino(inode);

	if ( ino == (ovlfs_inode_t *) NULL )
		ret = -ENOENT;
	else
		ret = ovlfs_remove_dirent(ino, name, len);

	DPRINTR2("ino %lu, %.*s, %d; ret = %d\n", inode->i_ino, len, name, len,
	         ret);

	return	ret;
}



/**
 ** Overlay Filesystem Storage File Operations
 **/

ovlfs_storage_op_t ovlfs_stg1_ops = {
	ovlfs_stg1_read_super,
	ovlfs_stg1_write_super,
	ovlfs_stg1_put_super,
	ovlfs_stg1_statfs,
	ovlfs_stg1_add_inode,
	ovlfs_stg1_read_inode,
	ovlfs_stg1_clear_inode,
	ovlfs_stg1_upd_inode,
	ovlfs_stg1_map_inode,
	ovlfs_stg1_map_lookup,
	ovlfs_stg1_get_mapping,
	ovlfs_stg1_num_dirent,
	ovlfs_stg1_get_dirent,
	ovlfs_stg1_lookup,
	ovlfs_stg1_add_dirent,
	ovlfs_stg1_rename,
	ovlfs_stg1_unlink,
	ovlfs_stg1_delete_dirent,
	NULL,	/* File read - not used by the storage file method */
	NULL,	/* File write - not used by the storage file method */
	NULL,	/* Set symlink - not used by the storage file method */
	NULL	/* Read symlink - not used by the storage file method */
} ;
