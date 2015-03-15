/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_file.c
 **
 ** DESCRIPTION: This file contains the source code of the file operations
 **              for the overlay filesystem.
 **
 **
 ** NOTES:
 **	- The comments that end in "its complicated" are referring to the fact
 **	  that the base and storage filesystems may be changed while the
 **	  overlay filesystem is using them and expecting them to look a certain
 **	  way.
 **
 **	  The reason for the comment is to say that the overlay filesystem is
 **	  considering the possibility, but that the end result may be
 **	  unexpected.  Of course, if the real filesystems are changed, the
 **	  expected results are not so clear after all.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 09/27/1997	art            	Added to source code control.
 ** 02/24/1998	ARTHUR N.	Update the size of the pseudo-fs' inode when
 **				 write() extends the file's size.  Also, lock
 **				 inode's in the overlay fs before updating
 **				 their contents.
 ** 02/27/1998	ARTHUR N.	Added the select(), ioctl(), fasync(), and
 **				 mmap() file operations and moved the
 **				 ovlfs_open_inode() and ovlfs_close_inode()
 **				 functions into ovl_kern.c.
 ** 02/27/1998	ARTHUR N.	Corrected the #if ... compiler directive logic
 **				 used to remove support for file pointers that
 **				 are missing their private data.
 ** 02/28/1998	ARTHUR N.	Removed all support for file pointers that
 **				 are missing their private data.  Only files
 **				 opened via ovlfs_open() may be used here.
 ** 03/01/1998	ARTHUR N.	Allocate GFP_KERNEL priority memory only.
 ** 03/03/1998	ARTHUR N.	Added support for the noxmnt option.
 ** 03/03/1998	ARTHUR N.	Use the file operation's write instead of the
 **				 inode's write.
 ** 03/05/1998	ARTHUR N.	Added malloc-checking support.
 ** 03/06/1998	ARTHUR N.	Added the PROTECT_MMAP_WRITE compile flag.
 ** 03/09/1998	ARTHUR N.	Added the copyright notice.
 ** 03/09/1998	ARTHUR N.	Update the ctime, mtime, and atime attributes
 **				 of the inodes when read and written.
 ** 03/10/1998	ARTHUR N.	Improved comments.
 ** 03/22/1998	ARTHUR N.	Modified ovlfs_create_stg_inode so that it
 **				 just uses the storage filesystem's dir. Also,
 **				 fixed removal of the file if the copy fails.
 ** 03/22/1998	ARTHUR N.	Fixed minor mistake.
 ** 03/22/1998	ARTHUR N.	Don't read more of a file than the overlay
 **				 filesystem believes there is, and fill any
 **				 extra with zero bytes.
 ** 03/22/1998	ARTHUR N.	Don't copy more of a file than the overlay
 **				 filesystem believes there is when copying
 **				 it into the storage filesystem.
 ** 03/24/1998	ARTHUR N.	Seek back to the original file position after
 **				 copying the file in ovlfs_write().
 ** 03/26/1998	ARTHUR N.	Updated to support storage operations.
 ** 06/27/1998	ARTHUR N.	Made ovlfs_read, ovlfs_write, ovlfs_fsync,
 **				 and ovlfs_release static.
 ** 08/23/1998	ARTHUR N.	Corrected DPRINTC statement in ovlfs_copy_file.
 ** 02/01/1999	ARTHUR N.	Corrected the default return value from
 **				 ovlfs_select.
 ** 02/06/1999	ARTHUR N.	Updated use of do_map_inode() for changes to
 **				 the Storage System interface.
 ** 02/07/1999	ARTHUR N.	Update the inode information structure's
 **				 storage mapping.
 ** 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
 ** 02/24/2003	ARTHUR N.	Create post-2.0-only version of
 **				 ovlfs_copy_file() and corrected file-size of
 **				 the resulting file.
 ** 02/27/2003	ARTHUR N.	Don't update ctime on ovlfs_read().
 ** 03/11/2003	ARTHUR N.	Moved do_create to ovl_misc.c.
 ** 03/13/2003	ARTHUR N.	Use generic_file_mmap().
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_file.c	1.26	[03/07/27 22:20:34]"

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
#include <linux/slab.h>
#include <linux/poll.h>
#include <asm/statfs.h>
#if POST_20_KERNEL_F
# include <asm/uaccess.h>
#else
# include <asm/segment.h>
#endif

#include "ovl_debug.h"
#include "ovl_fs.h"
#include "ovl_inlines.h"

#if POST_20_KERNEL_F
# define FILP_GET_INODE(filp)	\
		( ( (filp) == NULL ) ?	\
			INODE_NULL : ( (filp)->f_dentry == NULL ) ? \
				INODE_NULL : (filp)->f_dentry->d_inode )
#else
# define FILP_GET_INODE(filp)	\
		( ( (filp) == NULL ) ? INODE_NULL : (filp)->f_inode )
#endif


                       /****                        ****/
                       /****  FILE-LOCAL FUNCTIONS  ****/
                       /****                        ****/

/**
 ** FUNCTION: update_file_flags
 **
 **  PURPOSE: Given a file structure for an ovlfs inode, update the flags for
 **           the reference file.
 **
 ** NOTES:
 **	- Updating the file flags is necessary in order for fcntl(), and other
 **	  system calls that change the file flags, to work properly.
 **
 ** RULES:
 **	- Only prepared file structure may be passed (i.e. the private_data
 **	  member must point to a valid ovlfs_file_info_t structure).
 **	- No locking is required, and non is used.
 **/

static inline void	update_file_flags (struct file *ovl_file)
{
	ovlfs_file_info_t	*f_info;

		/* Just copy the flags - rather simple. */

	f_info = (ovlfs_file_info_t *) ovl_file->private_data;

	if ( f_info->file != NULL )
		f_info->file->f_flags = ovl_file->f_flags;
}



/**
 ** FUNCTION: ovlfs_create_stg_inode
 **
 **  PURPOSE: Given an ovlfs directory inode, the name of a new file, and
 **           an inode to use as a reference, create the file in the ovlfs
 **           directory.
 **/

static int  ovlfs_create_stg_inode (struct inode *inode, const char *name,
                                    int len, struct inode *ref, int *created,
#if USE_DENTRY_F
                                    struct dentry **result
#else
                                    struct inode  **result
#endif
                                   )
{
    struct inode    *o_dir;
    int             ret;


        /* Make sure the given arguments are valid */

    if ( ! is_ovlfs_inode(inode) )
    {
        DPRINT1(("OVLFS: ovls_create_stg_inode: invalid inode given: inum %ld, "
                 " dev 0x%x.\n", inode->i_ino, inode->i_dev));

        return  -ENOTDIR;
    }

    ret = ovlfs_resolve_stg_inode(inode, &o_dir);

    if ( ret < 0 )
        ret = ovlfs_make_hierarchy(inode, &o_dir, 0);

    if ( ret >= 0 )
    {
            /* Try to get the file from the directory */

        if ( ( o_dir->i_op != NULL ) && ( o_dir->i_op->lookup != NULL ) )
        {
#if USE_DENTRY_F
            ret = do_lookup2(o_dir, name, len, result, TRUE);
#else
            ret = do_lookup(o_dir, name, len, result, TRUE);
#endif

            if ( ret == -ENOENT )
            {
                    /* Did not find the file; try to create it */

                if ( o_dir->i_op->create != NULL )
                {
                    ret = do_create(o_dir, name, len, ref->i_mode, result);

                    if ( ret >= 0 )
                        created[0] = 1;
                }
                else
                    ret = -ENOTDIR;
            }
        }
        else
            ret = -ENOTDIR;

        IPUT(o_dir);
    }

    return  ret;
}



/**
 ** FUNCTION: do_read
 **
 **  PURPOSE: Read up to the specified number of bytes from the given file
 **           into the given buffer.
 **/

static inline
#if POST_20_KERNEL_F
ssize_t
#else
int
#endif
                    do_read (struct inode *inode, struct file *file,
                             char *buf, int count, int to_kmem)
{
#if POST_20_KERNEL_F
    ssize_t         ret;
    mm_segment_t    orig_fs;
#else
    int             ret;
    unsigned long   orig_fs;
#endif

    DPRINTC2("i_ino %ld\n", inode->i_ino);

    if ( ( file->f_op != NULL ) && ( file->f_op->read != NULL ) )
    {
        if ( to_kmem )
        {
            orig_fs = get_fs();
            set_fs(KERNEL_DS);
        }

#if POST_20_KERNEL_F
        ret = file->f_op->read(file, buf, count, &(file->f_pos));
#else
        ret = file->f_op->read(inode, file, buf, count);
#endif

        if ( to_kmem )
        {
                /* Restore the user data segment. */
            set_fs(orig_fs);
        }
    }
    else
        ret = -EINVAL;

    DPRINTR2("i_ino %ld; ret = %d\n", inode->i_ino, (int) ret);

    return  ret;
}



/**
 ** FUNCTION: do_write
 **
 **  PURPOSE: Write the specified number of bytes from the given buffer to
 **           the given file.
 **
 ** NOTES:
 **     - The entire buffer must be written; therefore, to be safe, this
 **       function loops until the total byte count written = the specified
 **       count, or an error occurs.
 **/

static inline
#if POST_20_KERNEL_F
ssize_t
#else
int
#endif
                    do_write (struct inode *inode, struct file *file,
                              char *buf, int count, int from_kmem)
{
#if POST_20_KERNEL_F
    mm_segment_t    orig_fs;
    ssize_t         total;
    ssize_t         ret;
#else
    unsigned long   orig_fs;
    int             total;
    int             ret;
#endif

    total = count;

    DPRINTC2("i_ino %ld\n", inode->i_ino);

    if ( ( file->f_op == NULL ) || ( file->f_op->write == NULL ) )
    {
        ret = -EINVAL;
        goto    do_write__out;
    }

    ret = 0;

    if ( from_kmem )
    {
        orig_fs = get_fs();
        set_fs(KERNEL_DS);
    }

    while ( ( count > 0 ) && ( ret >= 0 ) )
    {
#if POST_20_KERNEL_F
        ret = file->f_op->write(file, buf, count, &(file->f_pos));
#else
        ret = file->f_op->write(inode, file, buf, count);
#endif

        if ( ret > 0 )
        {
            count -= ret;
            buf += ret;
        }
        else if ( ret == 0 )
        {
            DPRINT1(("OVLFS: do_write: file operation write returned 0 on "
                     "write %d bytes\n", count));

            count = 0;  /* We can't keep trying if nothing is writing */
        }
    }

    if ( from_kmem )
    {
        set_fs(orig_fs);
    }

    if ( ret >= 0 )
        ret = total;

do_write__out:

    DPRINTR2("i_ino %ld; ret = %d\n", inode->i_ino, (int) ret);

    return  ret;
}



/**
 ** FUNCTION: ovlfs_copy_file
 **
 **  PURPOSE: Copy the base inode file given into the storage filesystem in
 **           the directory given with the specified name.
 **/

#if POST_20_KERNEL_F

int	ovlfs_copy_file (struct inode *dir, const char *fname, int len,
	                 off_t max_size, struct inode *base_i,
	                 struct dentry **result)
{
	struct dentry	*new_ent;
	struct inode	*o_dir;
	struct file	*out_file;
	struct file	*in_file;
	char		*buffer;
	int		count;
	int		is_created = 0;
	int		ret;

	DPRINTC2("dir ino %ld, base ino %ld, name %.*s, len %d\n",
	         dir->i_ino, base_i->i_ino, len, fname, len);

	ret = -EINVAL;

#if ! FASTER_AND_MORE_DANGEROUS
	if ( ( dir == INODE_NULL ) || ( base_i == INODE_NULL ) ||
	     ( dir->u.generic_ip == NULL ) || ( ! is_ovlfs_inode(dir) ) )
	{
		DPRINT1(("OVLFS: ovlfs_copy_file called with invalid arguments"
                 "(0x%x, 0x%x)\n", (int) dir, (int) base_i));

		goto	ovlfs_copy_file__out;
	}

	if ( ! S_ISREG(base_i->i_mode) )
	{
		DPRINT1(("OVLFS: ovlfs_copy_file called with non-regular "
		         "file\n"));

		goto	ovlfs_copy_file__out;
	}
#endif


		/* First, create the file in the storage filesystem; note   */
		/*  that this is not the same as creating it in the pseudo  */
		/*  filesystem - it already exists there in most cases, but */
		/*  may be missing in others.                               */

	if ( ( dir->i_op == NULL ) || ( dir->i_op->create == NULL ) )
	{
		DPRINT1(("OVLFS: ovlfs_copy_file: ovlfs directory lacking "
                 "create()!\n"));

		ret = -ENOTDIR;
		goto	ovlfs_copy_file__out;
	}

	DPRINT2(("OVLFS: ovlfs_copy_file: creating inode named %.*s length "
	         "%d\n", len, fname, len));

	ret = ovlfs_create_stg_inode(dir, fname, len, base_i, &is_created,
	                             &new_ent);

	if ( ret < 0 )
		goto	ovlfs_copy_file__out;


		/* Allocate memory for the buffer now. */

	ret = -ENOMEM;
	buffer = (char *) MALLOC(1024);
	if ( buffer == NULL )
		goto	ovlfs_copy_file__out_unlink;


		/* Set the ownership of the new inode to the ownership of */
		/*  the inode in the base filesystem.                     */

	ovlfs_chown(INODE_FROM_INO20_DENT22(new_ent), base_i->i_uid,
				base_i->i_gid);


		/* Now read the original and write the new inode */

	ret = ovlfs_open_inode(INODE_FROM_INO20_DENT22(new_ent), O_WRONLY,
	                       &out_file);

	if ( ret < 0 )
	{
		DPRINT1(("OVLFS: ovlfs_copy_file: error %d opening new inode "
		         "for writing\n", ret));

		goto	ovlfs_copy_file__out_malloc;
	}


		/* Now open the original for reading */

	ret = ovlfs_open_inode(base_i, O_RDONLY, &in_file);

	if ( ret < 0 )
	{
		DPRINT1(("OVLFS: ovlfs_copy_file: error %d opening base inode"
                 " for reading\n", ret));

		goto	ovlfs_copy_file__out_outfile;
	}

	do
	{
		if ( max_size > 1024 )
			count = do_read(base_i, in_file, buffer, 1024, 1);
		else
			count = do_read(base_i, in_file, buffer, max_size, 1);

		if ( count > 0 )
		{
			max_size -= count;

				/* do_write() will write the entire amount */
				/*  or return zero.                        */

			ret = do_write(new_ent->d_inode, out_file, buffer,
			               count, 1);
			if ( ret == 0 )
				ret = -EIO;
		}
		else
		{
#if KDEBUG
# if KDEBUG < 9
			if ( count < 0 )
# endif
				printk("OVLFS: ovlfs_copy_file: do_read "
				       "returned %d", count);
#endif

			ret = count;
		}
	} while ( ( ret > 0 ) && ( max_size > 0 ) );

	if ( ret < 0 )
		goto	ovlfs_copy_file__out_infile;


		/* Close the input and output files now. */

	ovlfs_close_file(in_file);
	ovlfs_close_file(out_file);


		/* Return the new entry. */
	result[0] = new_ent;

		/* Release the memory buffer allocated above. */
	FREE(buffer);

	goto	ovlfs_copy_file__out;


ovlfs_copy_file__out_infile:
	ovlfs_close_file(in_file);

ovlfs_copy_file__out_outfile:
	ovlfs_close_file(out_file);

ovlfs_copy_file__out_malloc:
	FREE(buffer);

ovlfs_copy_file__out_unlink:
	dput(new_ent);

	if ( is_created )
	{
		if ( ovlfs_resolve_stg_inode(dir, &o_dir) >= 0 )
		{
			if ( ( o_dir->i_op != NULL ) &&
			     ( o_dir->i_op->unlink != NULL ) )
			{
				do_unlink(o_dir, fname, len);
			}

			IPUT(o_dir);
		}
	}

ovlfs_copy_file__out:
	DPRINTR2("dir ino %ld, base ino %ld, name %.*s, len %d; ret = %d\n",
             dir->i_ino, base_i->i_ino, len, fname, len, ret);

	return	ret;
}

#else
	/* Pre-2.2 Version */

int ovlfs_copy_file (struct inode *dir, const char *fname, int len,
                     off_t max_size, struct inode *base_i,
                     struct inode  **result)
{
    struct inode        *new_ent;
    struct file         *out_file;
    struct file         *in_file;
    char                *buffer;
    int                 count;
    int                 is_created = 0;
    int                 ret;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_copy_file(%ld, %ld)\n",
             dir->i_ino, base_i->i_ino));

#if ! FASTER_AND_MORE_DANGEROUS
    if ( ( dir == INODE_NULL ) || ( base_i == INODE_NULL ) ||
         ( dir->u.generic_ip == NULL ) || ( ! is_ovlfs_inode(dir) ) )
    {
        DPRINT1(("OVLFS: ovlfs_copy_file called with invalid arguments"
                 "(0x%x, 0x%x)\n", (int) dir, (int) base_i));

        return  -EINVAL;
    }
    else
        if ( ! S_ISREG(base_i->i_mode) )
        {
            DPRINT1(("OVLFS: ovlfs_copy_file called with non-regular file\n"));

            return  -EINVAL;
        }
#endif


        /* First, create the file in the storage filesystem; note that this */
        /*  is not the same as creating it in the pseudo filesystem - it    */
        /*  already exists there in most cases, but may be missing in       */
        /*  others.                                                         */

    if ( ( dir->i_op != NULL ) && ( dir->i_op->create != NULL ) )
    {
        DPRINT2(("OVLFS: ovlfs_copy_file: creating inode named %.*s length "
                 "%d\n", len, fname, len));

        ret = ovlfs_create_stg_inode(dir, fname, len, base_i, &is_created,
                                     &new_ent);
    }
    else
    {
        DPRINT1(("OVLFS: ovlfs_copy_file: ovlfs directory lacking "
                 "create()!\n"));

        ret = -ENOTDIR;
    }

    if ( ret >= 0 )
    {
            /* Allocate memory for the buffer now. */

        buffer = (char *) MALLOC(1024);

        if ( buffer == NULL )
        {
            ret = -ENOMEM;
        }
        else
        {
                /* Set the ownership of the new inode to the ownership of the */
                /*  inode in the base filesystem.                             */

            ovlfs_chown(INODE_FROM_INO20_DENT22(new_ent), base_i->i_uid,
                        base_i->i_gid);


                /* Now read the original and write the new inode */

            ret = ovlfs_open_inode(INODE_FROM_INO20_DENT22(new_ent), O_WRONLY,
                                   &out_file);
        }

        if ( ret < 0 )
        {
            DPRINT1(("OVLFS: ovlfs_copy_file: error %d opening new inode for"
                     " writing\n", ret));
        }
        else
        {
                /* Now open the original for reading */

            ret = ovlfs_open_inode(base_i, O_RDONLY, &in_file);

            if ( ret < 0 )
            {
                DPRINT1(("OVLFS: ovlfs_copy_file: error %d opening base inode"
                         " for reading\n", ret));
            }
            else
            {
                do
                {
                    if ( max_size > 1024 )
                        count = do_read(base_i, in_file, buffer, 1024, 1);
                    else
                        count = do_read(base_i, in_file, buffer, max_size, 1);

                    if ( count > 0 )
                    {
                        max_size -= count;

                        DPRINT9(("OVLFS: ovlfs_copy_file: do_read returned %d",
                                 count));

                        ret = do_write(INODE_FROM_INO20_DENT22(new_ent),
                                       out_file, buffer, count, 1);

                        DPRINT9(("OVLFS: ovlfs_copy_file: do_write returned %d",
                                 ret));
                    }
                    else
                    {
#if KDEBUG
# if KDEBUG < 9
                        if ( count < 0 )
# endif
                                printk("OVLFS: ovlfs_copy_file: do_read "
                                       "returned %d", count);
#endif

                        ret = count;
                    }
                } while ( ( ret > 0 ) && ( max_size > 0 ) );

                ovlfs_close_file(in_file);
            }

            ovlfs_close_file(out_file);
        }

            /* If the copy failed for any reason, remove the created file. */

        if ( ret < 0 )
        {
            IPUT(new_ent);

            if ( is_created )
            {
                struct inode *o_dir;

                if ( ovlfs_resolve_stg_inode(dir, &o_dir) >= 0 )
                {
                    if ( ( o_dir->i_op != NULL ) &&
                         ( o_dir->i_op->unlink != NULL ) )
                    {
                        do_unlink(o_dir, fname, len);
                    }

                    IPUT(o_dir);
                }
            }
        }
        else
            result[0] = new_ent;


            /* Release the memory buffer allocated above, if any. */

        if ( buffer != NULL )
            FREE(buffer);
    }

    return  ret;
}

#endif



/**
 ** FUNCTION: ovlfs_inode_copy_file
 **
 **  PURPOSE: Copy the base inode specified into the storage filesystem in
 **           the directory that the ovlfs_inode resides in.
 **/

int ovlfs_inode_copy_file (struct inode *ovlfs_inode, struct inode *base_i,
#if USE_DENTRY_F
                           struct dentry **result
#else
                           struct inode  **result
#endif
                           )
{
    ovlfs_inode_info_t  *i_info;
    struct inode        *ovlfs_dir;
    char                *fname;
    int                 len;
    int                 ret;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_inode_copy_file(%ld, %ld)\n",
             ovlfs_inode->i_ino, base_i->i_ino));

#if ! FASTER_AND_MORE_DANGEROUS
    if ( ( ovlfs_inode == INODE_NULL ) || ( base_i == INODE_NULL ) ||
         ( ovlfs_inode->u.generic_ip == NULL ) ||
         ( ! is_ovlfs_inode(ovlfs_inode) ) )
    {
        DPRINT1(("OVLFS: ovlfs_inode_copy_file called with invalid arguments"
                 "(0x%x, 0x%x)\n", (int) ovlfs_inode, (int) base_i));

        return  -EINVAL;
    }
    else
        if ( ! S_ISREG(base_i->i_mode) )
        {
            DPRINT1(("OVLFS: ovlfs_inode_copy_file called with non-regular "
                     "file\n"));

            return  -EINVAL;
        }
#endif


        /* Obtain the name from the inode's information */

    i_info = (ovlfs_inode_info_t *) ovlfs_inode->u.generic_ip;

    fname = i_info->s.name;
    len   = i_info->s.len;

    if ( ( fname == (char *) NULL ) || ( fname[0] == '\0' ) || ( len <= 0 ) )
    {
        DPRINT1(("OVLFS: ovlfs_inode_copy_file: ovlfs inode #%ld, dev 0x%x "
                 "file name missing\n", ovlfs_inode->i_ino,
	         ovlfs_inode->i_dev));

        return  -EINVAL;
    }


        /* Obtain the parent directory of the ovlfs inode */

    ovlfs_dir = ovlfs_get_parent_dir(ovlfs_inode);

    if ( ovlfs_dir == INODE_NULL )
    {
        DPRINT1(("OVLFS: ovlfs_inode_copy_file: ovlfs inode #%ld, dev 0x%x "
                 "lacking directory reference\n", ovlfs_inode->i_ino,
                 ovlfs_inode->i_dev));

        return  -EINVAL;
    }


        /* Create the copy of the file in the storage filesystem */

    ret = ovlfs_copy_file(ovlfs_dir, fname, len, ovlfs_inode->i_size,
                          base_i, result);

    if ( ret >= 0 )
    {
            /* If the size of the result does not match the size of the */
            /*  ovlfs inode, truncate it now.                           */

            /* TBD: when this fails, shouldn't the resulting file be */
            /*      removed?                                         */
# if USE_DENTRY_F
        if ( result[0]->d_inode->i_size != ovlfs_inode->i_size )
            ret = ovlfs_do_truncate(result[0], ovlfs_inode->i_size);
#else
        if ( result->i_size != ovlfs_inode->i_size )
            ret = ovlfs_do_truncate(result[0], ovlfs_inode->i_size);
#endif
    }

    if ( ret >= 0 )
    {
            /* Now set the storage inode for the given inode to the new one */

#if STORE_REF_INODES
# if REF_DENTRY_F
        dget(result[0]);	/* "used" by i_info structure: */
        i_info->overlay_dent  = result[0];
	i_info->s.stg_dev     = result[0]->d_inode->i_dev;
	i_info->s.stg_ino     = result[0]->d_inode->i_ino;

# else
#  if USE_DENTRY_F
        IMARK(result->d_inode);	/* "used" by i_info structure: */
        i_info->overlay_inode = result[0]->d_inode;
	i_info->s.stg_dev     = result[0]->d_inode->i_dev;
	i_info->s.stg_ino     = result[0]->d_inode->i_ino;
#  else
        IMARK(result[0]);	/* "used" by i_info structure: */
        i_info->overlay_inode = result[0];
	i_info->s.stg_dev     = result[0]->i_dev;
	i_info->s.stg_ino     = result[0]->i_ino;
#  endif
# endif
#else
# error "Update to support !STORE_REF_INODES"
#endif

#if USE_DENTRY_F
        do_map_inode(ovlfs_inode->i_sb, ovlfs_inode->i_ino,
                     result[0]->d_inode->i_dev, result[0]->d_inode->i_ino,
                     's');
#else
        do_map_inode(ovlfs_inode->i_sb, ovlfs_inode->i_ino,
                     result[0]->i_dev, result[0]->i_ino, 's');
#endif
    }

    IPUT(ovlfs_dir);

    return  ret;
}



/**
 ** FUNCTION: default_lseek
 **
 **  PURPOSE: Perform the default lseek behavior when a file/inode does not
 **           implement the lseek entry point.
 **/

static inline int   default_lseek (struct file *file, off_t offset,
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
#if POST_20_KERNEL_F
            tmp = file->f_dentry->d_inode->i_size + offset;
#else
            tmp = file->f_inode->i_size + offset;
#endif
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
        file->f_version = ++event;
    }

    return  ret;
}



/**
 ** FILE: ovlfs_write_op
 **/

#if POST_20_KERNEL_F

	/* >= 2.2.0 version */

ssize_t  ovlfs_write_op (struct inode *inode, struct inode *o_inode,
                         struct file *file, const char *buf, size_t count,
                         loff_t *r_offset)
{
    ssize_t ret;

    if ( ( o_inode != INODE_NULL ) && ( file->f_op != NULL ) &&
         ( file->f_op->write != NULL ) )
    {
            /* Don't lock the inode's semaphore - it is already locked. */

        ret = file->f_op->write(file, buf, count, r_offset);


            /* Update the size of the file in the existing psuedo fs inode */
            /*  and the storage information for the inode if the file has  */
            /*  grown.                                                     */

        if ( r_offset[0] > inode->i_size )
        {
            inode->i_size = r_offset[0];
            INODE_MARK_DIRTY(inode);
        }
    }
    else
    {
        DPRINT2(("OVLFS: ovlfs_do_write: inode has no write() function; ino = "
                 "%ld\n", o_inode->i_ino));

        ret = -EINVAL;
    }

    return  ret;
}


#else

	/* Pre-2.2.0 version */

static int  ovlfs_write_op (struct inode *inode, struct inode *o_inode,
                            struct file *file, const char *buf, int count)
{
    int ret;

    if ( ( o_inode != INODE_NULL ) && ( file->f_op != NULL ) &&
         ( file->f_op->write != NULL ) )
    {
        DOWN(&o_inode->i_sem);
        ret = file->f_op->write(o_inode, file, buf, count);
        UP(&o_inode->i_sem);

            /* Update the size of the file in the existing psuedo fs inode */
            /*  and the storage information for the inode if the file has  */
            /*  grown.                                                     */

        if ( file->f_pos > inode->i_size )
        {
            inode->i_size = file->f_pos;
            INODE_MARK_DIRTY(inode);
        }
    }
    else
    {
        DPRINT2(("OVLFS: ovlfs_do_write: inode has no write() function; ino = "
                 "%ld\n", o_inode->i_ino));

        ret = -EINVAL;
    }

    return  ret;
}

#endif



                           /****                ****/
                           /****  ENTRY POINTS  ****/
                           /****                ****/

/**
 ** FUNCTION: ovlfs_lseek
 **/

#if POST_20_KERNEL_F
static loff_t   ovlfs_lseek (struct file *file, loff_t offset, int from_where)
#else
static int  ovlfs_lseek (struct inode *inode, struct file *file, off_t offset,
                         int from_where)
#endif
{
#if POST_20_KERNEL_F
    struct inode *inode;
#endif
    int           ret;

#if POST_20_KERNEL_F
    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_lseek(0x%x, %d, %d)\n",
             (int) file, (int) offset, from_where));

    if ( file->f_dentry != NULL )
        inode = file->f_dentry->d_inode;
    else
        inode = NULL;
#else
    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_lseek(%ld, 0x%x, %d, %d)\n",
             inode->i_ino, (int) file, (int) offset, from_where));
#endif

#if ! FASTER_AND_MORE_DANGEROUS

        /* make certain the given inode is an ovlfs inode; this is also */
        /*  useful for preventing problems due to bad programming ;).   */

    if ( ! is_ovlfs_inode(inode) )
    {
        DPRINT1(("OVLFS: ovlfs_lseek called with non-ovlfs inode!\n"));

        return  -EINVAL;
    }

#endif

    if ( file->private_data == NULL )
        return  -EINVAL;
    else
    {
        struct file         *filp;
        ovlfs_file_info_t   *f_info;

        f_info = (ovlfs_file_info_t *) file->private_data;
        filp = file_info_filp(f_info);

        update_file_flags(file);

#if POST_20_KERNEL_F

        if ( ( filp->f_op != NULL ) && ( filp->f_op->llseek != NULL ) )
            ret = filp->f_op->llseek(filp, offset, from_where);
        else
            ret = default_lseek(filp, offset, from_where);

#else

        if ( ( filp->f_inode != INODE_NULL ) && ( filp->f_op != NULL ) &&
             ( filp->f_op->lseek != NULL ) )
            ret = filp->f_op->lseek(f_inode, filp, offset, from_where);
        else
            ret = default_lseek(filp, offset, from_where);

#endif

        file->f_pos = filp->f_pos;
    }

    if ( ret >= 0 )
        ret = file->f_pos;

    return  ret;
}



/**
 ** FUNCTION: zero_fill
 **
 **  PURPOSE: Fill the specified user buffer with the indicated number of zero
 **           bytes.
 **/

#if POST_20_KERNEL_F

static inline int	zero_fill (char *buf, int size)
{
	int	rc;
	int	ret;

	ret = 0;

		/* Use clear_user() to simply the task. */
	rc = clear_user(buf, size);

	if ( rc )
		ret = -EFAULT;

	return	ret;
}

#else

	/* Pre-2.2 version. */

static const char	zero_buffer[1024] = { 0, };

static inline int	zero_fill (char *buf, int size)
{
	int	copy_amount;
	int	rc;
	int	ret;

	ret = 0;

		/* TBD: need to use verify area before memcpy_tofs()? */

	while ( size > 0 )
	{
		if ( size > sizeof(zero_buffer) )
		{
			memcpy_tofs(buf, zero_buffer, sizeof(zero_buffer));
			size -= sizeof(zero_buffer);
		}
		else
		{
			memcpy_tofs(buf, zero_buffer, size);
			size = 0;
		}
	}
}

#endif



#if POST_20_KERNEL_F

/**
 ** FUNCTION: ovlfs_read
 **
 **  PURPOSE: This is the overlay filesystem's read() file operation.
 **
 ** NOTES:
 **	- Reads come in two styles here - old and new: old style reads maintain
 **	  the file position in the file itself while new style reads store the
 **	  file position in another location.
 **
 **	- In order to get the most accurate results, the style of read is
 **	  detected here and used on the real inode; note that, if the caller
 **	  does modify f_pos on a new-style read, the results may not match at a
 **	  later time.
 **/

static ssize_t  ovlfs_read (struct file *file, char *buf, size_t count,
                            loff_t *r_offset)
{
	loff_t			max_size;
	loff_t			result;
	struct inode		*inode;
	struct file		*o_filp;
	ovlfs_inode_info_t	*i_info;
	ovlfs_file_info_t	*f_info;
	struct inode		*f_inode;
	int			old_style_f;
	int			fill_amount;
	int			write_count;
	int			ret;

	inode = FILP_GET_INODE(file);

	DPRINTC2("ino %ld, file 0x%x, buf 0x%x, count %d\n", inode->i_ino,
             (int) file, (int) buf, count);

	write_count = 0;

#if ! FASTER_AND_MORE_DANGEROUS

		/* Make certain the given inode is an ovlfs inode. */

	if ( ! is_ovlfs_inode(inode) )
	{
		DPRINT1(("OVLFS: ovlfs_read called with non-ovlfs inode!\n"));

		return	-EINVAL;
	}

#endif

		/* Obtain the file information added by the open() call */

	ret = -EINVAL;

	if ( file->private_data == NULL )
	{
		DPRINT1(("OVLFS: ovlfs_read(): file given has null private "
		         "data\n"));

		goto	ovlfs_read__out;
	}

		/* Get the ovlfs information from the file. */

	f_info = (ovlfs_file_info_t *) file->private_data;
	o_filp = file_info_filp(f_info);

        update_file_flags(file);

		/* Make sure the file information is valid */

	if ( ( o_filp->f_dentry == NULL ) ||
	     ( o_filp->f_dentry->d_inode == NULL ) ||
	     ( o_filp->f_op == NULL ) ||
	     ( o_filp->f_op->read == NULL ) )
	{
		goto	ovlfs_read__out;
	}

	f_inode = o_filp->f_dentry->d_inode;


		/* If the output location for the resulting file position is */
		/*  the file's f_pos attribute, this is an "old-style" read. */

	if ( r_offset == &(file->f_pos) )
		old_style_f = TRUE;
	else
		old_style_f = FALSE;


		/* Determine the maximum size of the file based on our own */
		/*  info, but only for regular files, and only if the file */
		/*  is flagged as having a hard limit.                     */

	ret         = 0;
	max_size    = -1;
	i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;

	ASSERT(i_info != NULL);

	if ( ( S_ISREG(inode->i_mode) ) &&
	     ( i_info->s.flags & OVL_IFLAG__SIZE_LIMIT ) )
	{
		max_size = inode->i_size;

		if ( r_offset[0] > max_size )
			goto	ovlfs_read__out;

			/* If the request passes the end of file, as known   */
			/*  to the ovlfs, truncate it.  This is important to */
			/*  do before reading from the inode of the other fs */
			/*  since it could be longer and return too much.    */

		result = r_offset[0] + count;

		if ( result > max_size )
		{
			count -= ( result - max_size );

			if ( count < 0 )
			{
					/* This should never happen */

				WARN("adjusted read count < 0: %d!\n", count);
				goto	ovlfs_read__out;
			}
		}
	}

		/* Call the read function on the real inode now.  If the   */
		/*  read is an old-fashioned read (i.e. the file position  */
		/*  is maintained by the file), use the same for the other */
		/*  file; otherwise, use the requested location for the    */
		/*  offset.                                                */

	if ( old_style_f )
		ret = o_filp->f_op->read(o_filp, buf, count, &(o_filp->f_pos));
	else
		ret = o_filp->f_op->read(o_filp, buf, count, r_offset);


		/* Check for an error. */

	if ( ret < 0 )
		goto	ovlfs_read__out;

		/* Update the count of written data. */

	write_count += ret;


		/* Synchronize the file positions for old-style reads, but */
		/*  ONLY when data has been received from the real file.   */

	if ( ( old_style_f ) && ( ret > 0 ) )
		file->f_pos = o_filp->f_pos;


		/* If the entire amount was not read, check if we think the */
		/*  file is bigger than the real file, and the current      */
		/*  offset is past the end of the real file.                */

	if ( ( ret < count ) && ( r_offset[0] >= f_inode->i_size ) &&
	     ( max_size > r_offset[0] ) )
	{
		fill_amount = count - ret;	/* remainder of the request. */

			/* Bound the request to the file size we know. */

		if ( ( r_offset[0] + fill_amount ) >= max_size )
			fill_amount = ( max_size - r_offset[0] );

		if ( fill_amount > 0 )
		{
			ret = zero_fill(buf + ret, fill_amount);

			if ( ret < 0 )
				goto	ovlfs_read__out;

				/* Add the amount to the result. */
			write_count += fill_amount;
			r_offset[0] += fill_amount;
		}
	}

	if ( ! IS_RDONLY(inode) )
	{
		UPDATE_ATIME(inode);

		INODE_MARK_DIRTY(inode);
	}

ovlfs_read__out:

	if ( ret >= 0 )
		ret = write_count;

	DPRINTR2("ino %ld, file 0x%x, buf 0x%x, count %d; ret = %d\n",
             inode->i_ino, (int) file, (int) buf, count, ret);

	return	ret;
}

#else

	/* Pre-2.2 Kernel Version */

/**
 ** FUNCTION: ovlfs_read
 **
 **  PURPOSE: This is the overlay filesystem's read() file operation.
 **/

static int  ovlfs_read (struct inode *inode, struct file *file, char *buf,
                        int count)
{
    loff_t  max_size;
    loff_t  result;
    int     ret;

    DPRINTC2("ino %ld, file 0x%x, buf 0x%x, count %d\n", inode->i_ino,
             (int) file, (int) buf, count);

#if ! FASTER_AND_MORE_DANGEROUS

        /* make certain the given inode is an ovlfs inode; this */
        /*  is also useful for preventing problems due to bad   */
        /*  programming ;).                                     */

    if ( ! is_ovlfs_inode(inode) )
    {
        DPRINT1(("OVLFS: ovlfs_read called with non-ovlfs inode!\n"));

        return  -EINVAL;
    }

#endif

        /* Determine the maximum size of the file based on our own info, */
        /*  but only for regular files.                                  */

    if ( S_ISREG(inode->i_mode) )
    {
        max_size = inode->i_size;

        if ( file->f_pos > max_size )
            return 0;
        else
        {
            result = file->f_pos + count;

            if ( result > max_size )
            {
                count -= ( result - max_size );

                if ( count < 0 )
                {
                        /* This should never happen */

                    printk("OVLFS: ovlfs_read: adjusted read count < 0: %d!\n",
                           count);

                    return  0;
                }
            }
        }
    }
    else
        max_size = -1;


        /* Obtain the file information added by the open() call */

    if ( file->private_data == NULL )
    {
        DPRINT1(("OVLFS: ovlfs_read(): file given has null private data\n"));

        ret = -EINVAL;
    }
    else
    {
        struct file         *filp;
        ovlfs_file_info_t   *f_info;
        struct inode        *f_inode;

        f_info = (ovlfs_file_info_t *) file->private_data;
        filp = file_info_filp(f_info);

        f_inode = filp->f_inode;

        update_file_flags(file);

            /* Make sure the file information is valid */

        if ( ( f_inode != INODE_NULL ) && ( filp->f_op != NULL ) &&
             ( filp->f_op->read != NULL ) )
        {
                /* Call the read function on the real inode. */

            DOWN(&f_inode->i_sem);
            ret = filp->f_op->read(f_inode, filp, buf, count);
            UP(&f_inode->i_sem);

            if ( ret >= 0 )
            {
                result = file->f_pos;

                    /* If the entire amount was not read, check if we think */
                    /*  the file is bigger than the real file.              */

                if ( ret < count )
                {
                    file->f_pos += ret;

                    if ( ( file->f_pos >= f_inode->i_size ) &&
                         ( max_size > file->f_pos ) )
                    {
                        int tmp;

                        tmp = count - ret;	/* remainder of the request. */

                            /* If the result calculated would exceed the end */
                            /*  of the file as we see it, truncate the req.  */

                        if ( ( file->f_pos + tmp ) >= max_size )
                            tmp = ( max_size - file->f_pos );

                        if ( tmp > 0 )
                        {
                            zero_fill(buf + ret, tmp);

                            ret += tmp;
                            file->f_pos += tmp;
                        }
                    }
                }

                if ( ! IS_RDONLY(inode) )
                {
                    inode->i_atime = CURRENT_TIME;

                    INODE_MARK_DIRTY(inode);
                }

                file->f_pos = filp->f_pos;
            }
        }
        else
            ret = -EINVAL;
    }

    DPRINTR2("ino %ld, file 0x%x, buf 0x%x, count %d; ret = %d\n",
             inode->i_ino, (int) file, (int) buf, count, ret);

    return  ret;
}

#endif



#if POST_20_KERNEL_F

/**
 ** FUNCTION: ovlfs_write
 **
 ** NOTES:
 **     - Only regular files are copied into the storage filesystem; other
 **       types of files that reside in the base filesystem may be written
 **       normally.
 **/

static ssize_t	ovlfs_write (struct file *file, const char *buf, size_t count,
		             loff_t *r_offset)
{
	struct dentry		*o_ent;
	loff_t			cur_pos;
	struct inode		*inode;
	ovlfs_file_info_t	*f_info;
	struct file		*o_filp;
	struct file		*new_filp;
	struct inode		*f_inode;
	int			flags;
	loff_t			*or_offset;
	int			old_style_f;
	ssize_t			ret;


	inode = FILP_GET_INODE(file);

	DPRINTC2("i_ino %ld, file 0x%x, buf 0x%x, count %d\n", inode->i_ino,
	         (int) file, (int) buf, count);

#if ! FASTER_AND_MORE_DANGEROUS

		/* Make certain the given inode is an ovlfs inode. */

	if ( ! is_ovlfs_inode(inode) )
	{
		DPRINT1(("OVLFS: ovlfs_write called with non-ovlfs inode!\n"));

		ret = -EINVAL;
		goto	ovlfs_write__out;
	}

#endif

	ASSERT(file->private_data != NULL);

		/* If the output location for the resulting file position */
		/*  is the file's f_pos, this is an "old-style" write.    */

	if ( r_offset == &(file->f_pos) )
		old_style_f = TRUE;
	else
		old_style_f = FALSE;


		/* If append-only mode is in use, move the file position to */
		/*  the end of the file.                                    */

	f_info = (ovlfs_file_info_t *) file->private_data;
	o_filp = file_info_filp(f_info);

	if ( ( o_filp == NULL ) || ( o_filp->f_dentry == NULL ) ||
	     ( o_filp->f_dentry->d_inode == NULL ) )
	{
		ret = -EIO;
		goto	ovlfs_write__out;
	}

	f_inode = o_filp->f_dentry->d_inode;

        update_file_flags(file);

	if ( file->f_flags & O_APPEND )
		r_offset[0] = inode->i_size;


		/* If the file was opened in the base filesystem, and it is */
		/*  a regular file, then it must be closed and copied, and  */
		/*  the result of the copy must be opened.                  */

	if ( ( f_info->is_base ) && ( S_ISREG(f_inode->i_mode) ) )
	{
			/** COPY ON WRITE **/

			/* Remember the current position within the file */

		cur_pos = o_filp->f_pos;

		ret = ovlfs_inode_copy_file(inode, o_filp->f_dentry->d_inode,
		                            &o_ent);

		if ( ret < 0 )
			goto	ovlfs_write__out;


			/* The file copied ok, try the update on the new */
			/*  file.  Turn off the truncate flag since we   */
			/*  certainly do not want to truncate it.        */

		flags = o_filp->f_flags & ~O_TRUNC;
		ret = ovlfs_open_dentry(o_ent, flags, &new_filp);
		dput(o_ent);

		if ( ret < 0 )
			goto	ovlfs_write__out;


			/* Opened the storage file; replace the existing file */
			/*  refernce with the new one.                        */

		ovlfs_close_file(o_filp);

		o_filp       = new_filp;

		f_info->file	= new_filp;
		f_info->is_base = 0;

		if ( o_filp->f_op != NULL )
		{
				/* Reset the position in the file to the */
				/*  position in the original file.       */

			if ( o_filp->f_op->llseek != NULL )
				ret = o_filp->f_op->llseek(o_filp, cur_pos, 0);
			else
				o_filp->f_pos = cur_pos;
		}
	}


		/* Use f_pos of the other file structure if this is an "old */
		/*  style" write; otherwise, just pass the pointer given.   */

	if ( old_style_f )
		or_offset = &(o_filp->f_pos);
	else
		or_offset = r_offset;


		/* Perform the requested write now. */

	ret = ovlfs_write_op(inode, f_inode, o_filp, buf, count, or_offset);

	if ( ret < 0 )
		goto	ovlfs_write__out;

	inode->i_ctime = inode->i_mtime = CURRENT_TIME;
	INODE_MARK_DIRTY(inode);


		/* If this is an old-sytle write, update the ovlfs file's */
		/*  position to match the final result.                   */

	if ( ( old_style_f ) && ( ret > 0 ) )
		file->f_pos = o_filp->f_pos;

ovlfs_write__out:

	DPRINTR2("i_ino %ld, file 0x%x, buf 0x%x, count %d; ret = %d\n",
             inode->i_ino, (int) file, (int) buf, count, ret);

	return	ret;
}

#else

	/* Pre-2.2 Version */

/**
 ** FUNCTION: ovlfs_write
 **
 ** NOTES:
 **     - Only regular files are copied into the storage filesystem; other
 **       types of files that reside in the base filesystem may be written
 **       normally.
 **/

static int  ovlfs_write (struct inode *inode, struct file *file,
                         const char *buf, int count)
{
    struct inode        *o_ent;
    off_t               cur_pos;
    int                 ret;
    ovlfs_file_info_t   *f_info;
    struct file         *o_filp;
    struct file         *new_filp;
    struct inode        *f_inode;
    int                 flags;


    DPRINTC2("i_ino %ld, file 0x%x, buf 0x%x, count %d\n", inode->i_ino,
             (int) file, (int) buf, count);

#if ! FASTER_AND_MORE_DANGEROUS

        /* make certain the given inode is an ovlfs inode; this is also */
        /*  useful for preventing problems due to bad programming ;).   */

    if ( ! is_ovlfs_inode(inode) )
    {
        DPRINT1(("OVLFS: ovlfs_write called with non-ovlfs inode!\n"));

        return  -EINVAL;
    }

#endif

    if ( file->private_data == NULL )
    {
        BUG();
        ret = -EBADF;
        goto    ovlfs_write__out;
    }

        /* If append-only mode is in use, move the file position to the end */
        /*  of the file.                                                    */

    if ( file->f_flags & O_APPEND )
        file->f_pos = inode->i_size;

    f_info = (ovlfs_file_info_t *) file->private_data;
    o_filp = file_info_filp(f_info);

    f_inode = o_filp->f_inode;

    update_file_flags(file);

        /* If the file was opened in the base filesystem, and it is a */
        /*  regular file, then it must be closed and copied, and the  */
        /*  result of the copy must be opened.                        */

    if ( ( f_info->is_base ) && ( S_ISREG(f_inode->i_mode) ) )
    {
            /** COPY ON WRITE **/

            /* Remember the current position within the file */

        cur_pos = o_filp->f_pos;

        ret = ovlfs_inode_copy_file(inode, f_info->file.f_inode, &o_ent);

        if ( ret < 0 )
            goto    ovlfs_write__out;


            /* The file copied ok, try the update on the new file */

            /* Detach the current inode from the file. */

        IPUT(o_filp->f_inode);
        o_filp->f_inode  = o_ent;
        o_filp->f_op     = o_ent->i_op->default_file_ops;

        f_info->is_base = 0;

        if ( o_filp->f_op != NULL )
        {
            if ( o_filp->f_op->open != NULL )
            {
                ret = o_filp->f_op->open(INODE_FROM_INO20_DENT22(o_ent),o_filp);

                if ( ret < 0 )
                    goto    ovlfs_write__out;
            }

                /* Reset the position in the file to the position in the */
                /*  original file.                                       */

            if ( o_filp->f_op->lseek != NULL )
                ret = o_filp->f_op->lseek(o_filp->f_inode, o_filp, cur_pos, 0);
            else
                o_filp->f_pos = cur_pos;
        }
    }

        /* Perform the requested write now.  First, copy the file flags in */
        /*  case any have changed.                                         */

    o_filp->f_flags = file->f_flags;
    ret = ovlfs_write_op(inode, o_filp->f_inode, o_filp, buf, count);

    if ( ret < 0 )
        goto    ovlfs_write__out;

    inode->i_ctime = inode->i_mtime = CURRENT_TIME;
    INODE_MARK_DIRTY(inode);

    file->f_pos = o_filp->f_pos;

    DPRINTR2("i_ino %ld, file 0x%x, buf 0x%x, count %d; ret = %d\n",
             inode->i_ino, (int) file, (int) buf, count, ret);

ovlfs_write__out:

    return  ret;
}

#endif



#if PRE_22_KERNEL_F

/**
 ** FUNCTION: ovlfs_select
 **
 **  PURPOSE: This is the overlay filesystem's select() file operation.
 **/

static int  ovlfs_select (struct inode *inode, struct file *file, int flags,
                          select_table *tbl)
{
    int ret;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_select(%ld, 0x%x, %d, 0x%x)\n",
             inode->i_ino, (int) file, (int) flags, (int) tbl));

#if ! FASTER_AND_MORE_DANGEROUS

        /* make certain the given inode is an ovlfs inode; this is also */
        /*  useful for preventing problems due to bad programming ;).   */

    if ( ! is_ovlfs_inode(inode) )
    {
        DPRINT1(("OVLFS: ovlfs_select called with non-ovlfs inode!\n"));

        return  -EINVAL;
    }

#endif

    if ( file->private_data == NULL )
        ret = 0;
    else
    {
        struct file         *filp;
        ovlfs_file_info_t   *f_info;

        f_info = (ovlfs_file_info_t *) file->private_data;
        filp = file_info_filp(f_info);

        update_file_flags(file);

        if ( ( filp->f_inode != INODE_NULL ) && ( filp->f_op != NULL ) &&
             ( filp->f_op->select != NULL ) )
        {
            ret = filp->f_op->select(filp->f_inode, filp, flags, tbl);
        }
        else
            ret = 1;
    }


        /* On error, just return 1 becuase the return value is taken as */
        /*  a boolean.                                                  */

    if ( ret < 0 )
        ret = 0;

    return  ret;
}

#endif



#if POST_20_KERNEL_F

/**
 ** FUNCTION: ovlfs_poll
 **
 **  PURPOSE: This is the overlay filesystem's poll() file operation.
 **/

static unsigned int	ovlfs_poll (struct file *file,
			            struct poll_table_struct *tbl)
{
	struct file		*filp;
	ovlfs_file_info_t	*f_info;
	int			ret;

	DPRINTC2("file 0x%x, tbl 0x%x\n", (int) file, (int) tbl);

#if ! FASTER_AND_MORE_DANGEROUS

		/* make certain the given inode is an ovlfs inode; this is */
		/*  useful for preventing problems due to bad programming. */

	if ( ! is_ovlfs_inode(FILP_GET_INODE(file)) )
	{
		DPRINT1(("OVLFS: ovlfs_poll called with non-ovlfs inode!\n"));

		return	-EINVAL;
	}

#endif

	ret = POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM;

	if ( file->private_data == NULL )
		goto	ovlfs_poll__out;

	f_info = (ovlfs_file_info_t *) file->private_data;
	filp = file_info_filp(f_info);

	update_file_flags(file);

		/* If the poll function is not defined for the   */
		/*  access inode, return the default value; this */
		/*  value was copied from fs/select.c            */

	if ( ( FILP_GET_INODE(filp) != INODE_NULL ) &&
	     ( filp->f_op != NULL ) && ( filp->f_op->poll != NULL ) )
	{
		ret = filp->f_op->poll(filp, tbl);
	}

ovlfs_poll__out:

		/* On error, just return 0 becuase the return value is taken */
		/*  as a mask of flags.                                      */ 

	if ( ret < 0 )
		ret = 0;

	DPRINTR2("file 0x%x, tbl 0x%x; ret = %d\n", (int) file, (int) tbl,
	         ret);

	return	ret;
}



/**
 ** FUNCTION: ovlfs_flush
 **
 **  PURPOSE: This is the overlay filesystem's flush() file operation.
 **/

static int  ovlfs_flush (struct file *file)
{
	struct file		*filp;
	ovlfs_file_info_t	*f_info;
	int			ret;


	DPRINTC2("file 0x%08x\n", (int) file);

#if ! FASTER_AND_MORE_DANGEROUS

		/* Make certain the given inode is an ovlfs inode; this is */
		/*  also useful for preventing problems due to bad         */
		/*  programming ;).                                        */

	if ( ! is_ovlfs_inode(FILP_GET_INODE(file)) )
	{
		DPRINT1(("OVLFS: ovlfs_poll called with non-ovlfs inode!\n"));

		return	-EINVAL;
	}
#endif

	ret = 0;

	if ( file->private_data != NULL )
	{
		f_info = (ovlfs_file_info_t *) file->private_data;
		filp = file_info_filp(f_info);

		update_file_flags(file);

		if ( ( FILP_GET_INODE(filp) != INODE_NULL ) &&
		     ( filp->f_op != NULL ) && ( filp->f_op->flush != NULL ) )
		{
			ret = filp->f_op->flush(filp);
		}
	}

	DPRINTR2("file 0x%08x; ret = %d\n", (int) file, ret);

	return  ret;
}

#endif



/**
 ** FUNCTION: ovlfs_ioctl
 **
 **  PURPOSE: This is the overlay filesystem's ioctl() file operation.
 **/

static int	ovlfs_ioctl (struct inode *inode, struct file *file,
		             unsigned int cmd, unsigned long arg)
{
	struct file		*filp;
	ovlfs_file_info_t	*f_info;
	int			ret;

	DPRINTC2("ino %ld, file 0x%x, cmd %ud, arg %ld\n", inode->i_ino,
	         (int) file, cmd, arg);

#if ! FASTER_AND_MORE_DANGEROUS

		/* make certain the given inode is an ovlfs inode. */

	if ( ! is_ovlfs_inode(inode) )
	{
		DPRINT1(("OVLFS: ovlfs_ioctl called with non-ovlfs inode!\n"));

		return	-EINVAL;
	}

#endif

	ret = -ENOTTY;

	if ( file->private_data == NULL )
		goto	ovlfs_ioctl__out;

	f_info = (ovlfs_file_info_t *) file->private_data;
	filp = file_info_filp(f_info);

        update_file_flags(file);

	if ( ( FILP_GET_INODE(filp) != INODE_NULL ) &&
	     ( filp->f_op != NULL ) && ( filp->f_op->ioctl != NULL ) )
	{
		ret = filp->f_op->ioctl(FILP_GET_INODE(filp), filp, cmd, arg);
	}

ovlfs_ioctl__out:

	return	ret;
}



/**
 ** FUNCTION: ovlfs_fsync
 **/

#if POST_20_KERNEL_F
static int	ovlfs_fsync (struct file *file, struct dentry *d_ent,
		             int datasync)
#else
static int	ovlfs_fsync (struct inode *inode, struct file *file)
#endif
{
#if POST_20_KERNEL_F
	struct inode		*inode;
#endif
	struct file		*filp;
	ovlfs_file_info_t	*f_info;
	int			ret;

#if POST_20_KERNEL_F
	inode = FILP_GET_INODE(file);
#endif

	DPRINTC2("ino %ld, file 0x%x\n", inode->i_ino, (int) file);

#if ! FASTER_AND_MORE_DANGEROUS

		/* make certain the given inode is an ovlfs inode. */

	if ( ! is_ovlfs_inode(inode) )
	{
		DPRINT1(("OVLFS: ovlfs_fsync called with non-ovlfs inode!\n"));

		return	-EINVAL;
	}
#endif

	ret = -EINVAL;
	if ( file->private_data == NULL )
		goto	ovlfs_fsync__out;

	f_info = (ovlfs_file_info_t *) file->private_data;
	filp = file_info_filp(f_info);

	update_file_flags(file);

	if ( ( FILP_GET_INODE(filp) != INODE_NULL ) &&
	     ( filp->f_op != NULL ) && ( filp->f_op->fsync != NULL ) )
	{
#if POST_20_KERNEL_F
		ret = filp->f_op->fsync(filp, filp->f_dentry, datasync);
#else
		ret = filp->f_op->fsync(filp->f_inode, filp);
#endif
	}

ovlfs_fsync__out:

	return	ret;
}



/**
 ** FUNCTION: ovlfs_release
 **/

#if POST_20_KERNEL_F
static int	ovlfs_release (struct inode *inode, struct file *file)
#else
static void	ovlfs_release (struct inode *inode, struct file *file)
#endif
{
	struct file		*filp;
	ovlfs_file_info_t	*f_info;

	DPRINTC2("ino %ld, file 0x%08x\n", inode->i_ino, (int) file);

#if ! FASTER_AND_MORE_DANGEROUS

	if ( ! is_ovlfs_inode(inode) )
	{
		DPRINT1(("OVLFS: ovlfs_release called with non-ovlfs "
		         "inode!\n"));


		goto	ovlfs_release__out;
	}
#endif

	if ( file->private_data != NULL )
	{
			/* Release the base or storage fs access file. */

		f_info = (ovlfs_file_info_t *) file->private_data;
		filp = file_info_filp(f_info);

		update_file_flags(file);

#if POST_20_KERNEL_F
		ovlfs_close_file(filp);
#else
			/* We control the open and release of this file, so */
			/*  call the file's release operation now.          */

		if ( ( FILP_GET_INODE(filp) != INODE_NULL ) &&
		     ( filp->f_op != NULL ) && ( filp->f_op->release != NULL ) )
		{
			filp->f_op->release(filp->f_inode, filp);
		}

		IPUT(filp->f_inode);
#endif
		FREE(f_info);
	}

ovlfs_release__out:

#if POST_20_KERNEL_F
    return  0;
#endif
}



/**
 ** FUNCTION: ovlfs_mmap
 **
 **  PURPOSE: This is the overlay filesystem's mmap() file operation.
 **/

#if PRE_22_KERNEL_F

static int	ovlfs_mmap (struct inode *inode, struct file *file,
                            struct vm_area_struct *vm_str)
{
	struct file		*filp;
	ovlfs_file_info_t	*f_info;
	int				ret;

	DPRINTC2("ino %lu, file 0x%08x, vm_str 0x%08x\n", inode->i_ino,
	         (int) file, (int) vm_str);

	ret = -EINVAL;

	if ( ( ! is_ovlfs_inode(inode) ) || ( file->private_data == NULL ) )
		goto	ovlfs_mmap__err_out;


	f_info = (ovlfs_file_info_t *) file->private_data;
	filp = file_info_filp(f_info);

	update_file_flags(file);

		/* Make sure the referenced file's information is valid */

	if ( ( FILP_GET_INODE(filp) != INODE_NULL ) &&
	     ( filp->f_op != NULL ) && ( filp->f_op->mmap != NULL ) )
	{
		ret = -EINVAL;

		if ( vm_str == NULL )
			goto	ovlfs_mmap__err_out;


#if PROTECT_MMAP_WRITE
			/* Do not allow shared write access to a file in the */
			/*  base filesystem.                                 */

		if ( ( f_info->is_base ) && ( vm_str->vm_flags & VM_WRITE ) &&
		     ( vm_str->vm_flags & VM_SHARED ) )
		{
			ret = -EACCES;
			goto	ovlfs_mmap__err_out;
		}
#endif

/* TBD: consider this more carefully; the vm area's vm_ops could be an */
/*      issue.  Should be bother to call the other filesystem's mmap?  */

		ret = filp->f_op->mmap(FILP_GET_INODE(filp), filp, vm_str);
	}
	else
		ret = -ENODEV;

	DPRINTR2("ino %lu, file 0x%08x, vm_str 0x%08x; ret = %d\n",
	          inode->i_ino, (int) file, (int) vm_str, ret);

ovlfs_mmap__err_out:

	return ret;
}

#endif	/* PRE_22_KERNEL_F */



/**
 ** FUNCTION: ovlfs_fasync
 **
 **  PURPOSE: This is the overlay filesystem's fasync() file operation.
 **
 ** NOTES:
 **	- This is called when the file's FASYNC flag is changed.
 **	- This is only used by TTY device nodes.  It appears to enable
 **	  signalling of the TTY's owner when a notable event occurs (such as
 **	  I/O now possible).  Search on kill_fasync() and fasync_helper().
 **	- The file descriptor is provided to the process when it gets the
 **	  SIGIO; it otherwise appears to be unused by the kernel :).
 **/

#if POST_20_KERNEL_F
static int	ovlfs_fasync (int fd, struct file *file, int flags)
#else
static int	ovlfs_fasync (struct inode *inode, struct file *file,
		              int flags)
#endif
{
#if POST_20_KERNEL_F
	struct inode		*inode;
#endif
	struct file		*filp;
	ovlfs_file_info_t	*f_info;
	int			ret;

#if POST_20_KERNEL_F
	inode = FILP_GET_INODE(file);
#endif

	if ( ! is_ovlfs_inode(inode) )
		return	-EINVAL;

	ret = 0;

	if ( file->private_data != NULL )
	{
		f_info = (ovlfs_file_info_t *) file->private_data;
		filp = file_info_filp(f_info);

		update_file_flags(file);

		if ( ( FILP_GET_INODE(filp) != INODE_NULL ) &&
		     ( filp->f_op != NULL ) &&
		     ( filp->f_op->fasync != NULL ) )
		{
#if POST_20_KERNEL_F
			ret = filp->f_op->fasync(fd, filp, flags);
#else
			ret = filp->f_op->fasync(filp->f_inode, filp, flags);
#endif
		}
	}

	return ret;
}



#if POST_20_KERNEL_F

/**
 ** FUNCTION: ovlfs_file_lock
 **
 **  PURPOSE: This is the overlay filesystem's lock() file operation.
 **/

static int	ovlfs_file_lock (struct file *file, int flags,
		                 struct file_lock *f_lock)
{
	struct inode		*inode;
        struct file		*o_file;
        ovlfs_file_info_t	*f_info;
	int			ret;

	DPRINTC2("file 0x%08x, flags %x, f_lock 0x%08x\n", (int) file, flags,
	         (int) f_lock);

	inode = FILP_GET_INODE(file);

	if ( ! is_ovlfs_inode(inode) )
	{
		ret = -EINVAL;
		goto	ovlfs_file_lock__out;
	}

		/* Default to success.  The VFS will do the rest of the */
		/*  work - regardless of the reason for the success.    */

	ret = 0;

	if ( file->private_data != NULL )
	{
		f_info = (ovlfs_file_info_t *) file->private_data;
		o_file = f_info->file;

		update_file_flags(file);

		if ( ( o_file->f_dentry->d_inode != INODE_NULL ) &&
		     ( o_file->f_op != NULL ) &&
		     ( o_file->f_op->lock != NULL ) )
		{
			ret = o_file->f_op->lock(o_file, flags, f_lock);
		}
        }

ovlfs_file_lock__out:

	DPRINTR2("file 0x%08x, flags %x, f_lock 0x%08x; ret = %d\n",
	         (int) file, flags, (int) f_lock, ret);

	return ret;
}

#endif



/**
 ** ENTRY POINT: ovlfs_open
 **
 **     PURPOSE: Prepare an inode (file or directory) for I/O.
 **/

#if POST_20_KERNEL_F
	/* Post-2.0 Version: */

static int	ovlfs_open (struct inode *inode, struct file *file)
{
	struct dentry		*o_dent;
	ovlfs_file_info_t	*f_info;
	int			flags;
	int			ret;

	DPRINTC2("i_ino %ld, 0x%x\n", inode->i_ino, (int) file);

#if ! FASTER_AND_MORE_DANGEROUS

		/* make certain the given inode is an ovlfs inode. */

	if ( ! is_ovlfs_inode(inode) )
	{
		DPRINT1(("OVLFS: ovlfs_open called with non-ovlfs inode!\n"));

		return	-EINVAL;
	}

#endif

	f_info = MALLOC(sizeof(ovlfs_file_info_t));

	if ( f_info == NULL )
		return	-ENOMEM;


		/* Look for the inode to "open" in the storage fs first; */
		/*  if it does not exist there, look for it in the base  */
		/*  filesystem.                                          */

	f_info->is_base = 0;
	ret = ovlfs_resolve_stg_dentry(inode, &o_dent);

	if ( ret < 0 )
	{
		f_info->is_base = 1;
		ret = ovlfs_resolve_base_dentry(inode, &o_dent);
	}

	if ( ret < 0 )
		goto	ovlfs_open__out;

	if ( o_dent == NULL )
	{
		ret = -ENOMEM;
		goto	ovlfs_open__out_dput;
	}

	flags = file->f_flags & ~ ( O_APPEND | O_TRUNC );
        ret = ovlfs_open_dentry(o_dent, flags, &(f_info->file));
	dput(o_dent);

	if ( ret < 0 )
		goto	ovlfs_open__out;

	file->private_data = f_info;

		/* Only success exit point */
	return	ret;

ovlfs_open__out_dput:
	dput(o_dent);

ovlfs_open__out:
	if ( f_info != NULL )
		FREE(f_info);

	return	ret;
}

#else
	/* Pre-2.2 Version: */

static int  ovlfs_open (struct inode *inode, struct file *file)
{
    struct inode        *o_inode;
    ovlfs_file_info_t   tmp;
    int                 ret;

    DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_open(%ld, 0x%x)\n", inode->i_ino,
             (int) file));

#if ! FASTER_AND_MORE_DANGEROUS

        /* make certain the given inode is an ovlfs inode; this */
        /*  is also useful for preventing problems due to bad   */
        /*  programming ;).                                     */

    if ( ! is_ovlfs_inode(inode) )
    {
        DPRINT1(("OVLFS: ovlfs_open called with non-ovlfs inode!\n"));

        return  -EINVAL;
    }

#endif


    file->private_data = NULL;

        /* Look for the inode to "open" in the storage fs first; if it */
        /*  does not exist there, look for it in the base filesystem.  */

    ret = ovlfs_resolve_stg_inode(inode, &o_inode);

    if ( ret < 0 )
    {
        tmp.is_base = 1;
        ret = ovlfs_resolve_base_inode(inode, &o_inode);
    }
    else
        tmp.is_base = 0;

    if ( ret >= 0 )
    {
        tmp.file         = file[0];
        tmp.file.f_next  = NULL;
        tmp.file.f_prev  = NULL;
        tmp.file.f_inode = o_inode;

        if ( o_inode->i_op != NULL )
        {
            tmp.file.f_op = o_inode->i_op->default_file_ops;

                /* Call the real inode's open if it has one */

            if ( ( tmp.file.f_op != NULL ) &&
                 ( tmp.file.f_op->open != NULL ) )
            {
                ret = tmp.file.f_op->open(o_inode, &(tmp.file));
            }
        }
        else
            tmp.file.f_op = NULL;

        if ( ret >= 0 )
        {
                /* Copy the temporary file's info and store a reference */
                /*  to it in the given file's private data.             */

            file->private_data = dup_file_info(&tmp);

            if ( file->private_data == NULL )
            {
                ret = -ENOMEM;

                if ( tmp.file.f_op->release != NULL )
                    tmp.file.f_op->release(o_inode, &(tmp.file));
            }
        }

            /* If an error was encountered, release the inode */

        if ( ret < 0 )
            IPUT(o_inode);
    }

    return  ret;
}

#endif

#if POST_20_KERNEL_F

/* File operations not needed: */
/*	- readv() and writev() - the kernel will cycle through the buffers   */
/*	  and pass to read() and write().  However, it could be useful still */
/*	  to add - may improve performance.  Just be careful when underlying */
/*	  fs does not have these operations.                                 */
/*	- sendpage() - used by the sendfile() system call to pass data       */
/*	  between two files without user-mode interaction.  The VFS will     */
/*	  call write with set_fs(KERNEL_DS) instead.  Again, this may be     */
/*	  good for performance.                                              */
/*	- get_unmapped_area() - used by memory mapping to determine the      */
/*	  address to which the map will be assigned.  For example, used by   */
/*	  /proc to assign PCI address mappings on (some?) PCI entries.       */

struct file_operations  ovlfs_file_ops = {
	llseek:		ovlfs_lseek,
	read:		ovlfs_read,
	write:		ovlfs_write,
	readdir:	NULL,		/* Files should not define this. */
	poll:		ovlfs_poll,
	flush:		ovlfs_flush,
	ioctl:		ovlfs_ioctl,
	mmap:		generic_file_mmap,
	open:		ovlfs_open,
	release:	ovlfs_release,
	fsync:		ovlfs_fsync,
	fasync:		ovlfs_fasync,
	lock:		ovlfs_file_lock,
			NULL,
} ;

#else

struct file_operations  ovlfs_file_ops = {
    ovlfs_lseek,
    ovlfs_read,
    ovlfs_write,
    (int (*)(struct inode *, struct file *, void *, filldir_t)) NULL,
    ovlfs_select,
    ovlfs_ioctl,
    ovlfs_mmap,
    ovlfs_open,
    ovlfs_release,
    ovlfs_fsync,
    ovlfs_fasync,
    (int (*)(kdev_t)) NULL,   /* How could we make use of this? need a file */
    (int (*)(kdev_t)) NULL    /* How could we make use of this? need a file */
} ;

#endif
