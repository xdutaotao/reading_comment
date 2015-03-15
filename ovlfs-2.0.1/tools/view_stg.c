/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998-2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: view_stg.c
 **
 ** DESCRIPTION: This file contains the source code for the view_stg program
 **              which displays the contents of the storage file maintained
 **              by an overlay filesystem.
 **
 ** WARNING:
 **     - Don't be fooled by the ovlfs_inode_t; it is used to store information
 **       specific to the overlay filesystem for inodes.  Do not mix it up
 **       with the inode structure (struct inode).
 **
 ** ENHANCEMENTS:
 **	- Add command-line argument processing and include the ability to
 **	  specify whether base and storage mappings are in the file.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 09/27/1997	ARTHUR N.	Added to source code control.
 ** 02/27/1998	ARTHUR N.	Updated to latest version (Ovl2).
 ** 03/09/1998	ARTHUR N.	Added the copyright notice.
 ** 08/23/1998	ARTHUR N.	Added a header for the inode information.
 ** 02/06/1999	ARTHUR N.	Updated to latest version (Ovl3).
 ** 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
 ** 06/09/2003	ARTHUR N.	Moved latest version from fs dir to tools.
 ** 07/05/2003	ARTHUR N.	Support version 4 of the storage file and
 **				 better storage mapping support.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)view_stg.c	1.3	[03/07/27 22:20:38]"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#if WRITE_ENABLE
# include "abs_lists.h"
# include "ovl_fs.h"
#endif

#ifndef TRUE
# define TRUE	1
#endif

#ifndef FALSE
# define FALSE	0
#endif

/* Stolen from ovl_stg.h: */
#define OVL_STG_FLAG__BASE_MAPPINGS	1
#define OVL_STG_FLAG__STG_MAPPINGS	2


#if STORE_MAPPINGS
# define DEFAULT_MAPPING_F	TRUE
#else
# define DEFAULT_MAPPING_F	FALSE
#endif

typedef unsigned int    _uint;
typedef unsigned long   _ulong;

static int	Revision = 4;
static int	Base_mapping_f = DEFAULT_MAPPING_F;
static int	Stg_mapping_f  = DEFAULT_MAPPING_F;


#if WRITE_ENABLE

/**
 ** FUNCTION: write_integer
 **/

static int  write_integer (FILE *fptr, _uint value)
{
    unsigned char   buf[sizeof(_uint)];
    int             cur;
    int             ret;

    for ( cur = 0; cur < sizeof(_uint); cur++ )
    {
            /* Grab the lowest byte and then shift the value */
            /*  by one byte                                  */

        buf[cur] = value & 0xFF;
        value >>= 8;
    }

    ret = fwrite(buf, 1, sizeof(buf), fptr);

    if ( ret != sizeof(buf) )
    {
        fprintf(stderr, "write_integer: fwrite returned %d, errno is "
                "%d\n", ret, errno);

        return  -1;
    }

    return  ret;
}

#endif



/**
 ** FUNCTION: read_integer
 **/

static int  read_integer (FILE *fptr, _uint *result)
{
    unsigned char   buf[sizeof(_uint)];
    int             cur;
    int             ret;

    if ( result == (_uint *) NULL )
        return  -1;

    ret = fread(buf, 1, sizeof(buf), fptr);

    if ( ret != sizeof(buf) )
        return  -1;

    result[0] = 0;

    for ( cur = 0; cur < sizeof(_uint); cur++ )
    {
        result[0] |= ( buf[cur] << ( cur * 8 ) );
    }

    return  ret;
}



#if WRITE_ENABLE

/**
 ** FUNCTION: write_long
 **/

static int  write_long (FILE *fptr, _ulong value)
{
    unsigned char   buf[sizeof(_ulong)];
    int     cur;
    int     ret;

    for ( cur = 0; cur < sizeof(_ulong); cur++ )
    {
            /* Grab the lowest byte and then shift the value */
            /*  by one byte                                  */

        buf[cur] = value & 0xFF;
        value >>= 8;
    }

    ret = fwrite(buf, 1, sizeof(buf), fptr);

    if ( ret != sizeof(buf) )
        return  -1;

    return  ret;
}

#endif



/**
 ** FUNCTION: read_long
 **/

static int  read_long (FILE *fptr, _ulong *result)
{
    unsigned char   buf[sizeof(_ulong)];
    int     cur;
    int     ret;

    if ( result == (_ulong *) NULL )
        return  -EIO;

    ret = fread(buf, 1, sizeof(buf), fptr);

    if ( ret != sizeof(buf) )
        return  -1;

    result[0] = 0;

    for ( cur = 0; cur < sizeof(_ulong); cur++ )
    {
        result[0] |= ( buf[cur] << ( cur * 8 ) );
    }

    return  ret;
}



#if WRITE_ENABLE

/**
 ** FUNCTION: write_inode_details
 **
 **  PURPOSE: Write the information specific to the type of the inode given.
 **/

static int  write_inode_details (FILE *fptr, ovlfs_inode_t *ino)
{
    int ret;

    if ( ino == (ovlfs_inode_t *) NULL )
        return  -1;

    if ( S_ISBLK(ino->mode) || S_ISCHR(ino->mode) )
    {
        ret = write_integer(fptr, ino->rdev);
    }
    else if ( S_ISDIR(ino->mode) )
    {
        int tot_ent;
        int cur_ent;

            /* Write the number of directory entries and then */
            /*  write each entries information.               */

        if ( ino->dir_entries == (list_t) NULL )
            tot_ent = 0;
        else
            tot_ent = list_length(ino->dir_entries);

        ret = write_integer(fptr, tot_ent);

        for ( cur_ent = 1; ( cur_ent <= tot_ent ) && ( ret >= 0 );
              cur_ent++ )
        {
            ovlfs_dir_info_t    *entry;

            entry = (ovlfs_dir_info_t *)
                nth_element(ino->dir_entries, cur_ent);

            if ( entry == (ovlfs_dir_info_t *) NULL )
            {
                fprintf(stderr, "write_inode_details: null "
                        "entry (%d) in list\n", cur_ent);

                ret = -1;
            }
            else
            {
                ret = write_integer(fptr, entry->len);

                if ( ret >= 0 )
                {
                    ret = fwrite(entry->name, 1,
                                 entry->len, fptr);

                    if ( ret == entry->len )
                    {
                        ret = write_integer(fptr,
                                            entry->ino);

                        if ( ret >= 0 )
                            ret = write_integer(fptr, entry->flags);
                    }
                    else
                        if ( ret >= 0 )
                            ret = -1;
                }
            }
        }
    }
    else
        ret = 0;

    return  ret;
}

#endif



/**
 ** FUNCTION: read_inode_details
 **
 **  PURPOSE: Read the information specific to the type of the inode given.
 **/

static int  read_inode_details (FILE *fptr, int mode)
{
    int tmp_int;
    int ret;

    if ( S_ISBLK(mode) || S_ISCHR(mode) )
    {
        ret = read_integer(fptr, &tmp_int);

        if ( ret >= 0 )
            printf("\t%s DEVICE NUMBER 0x%x\n",
                   S_ISBLK(mode) ? "block" : "char", tmp_int);
    }
    else if ( S_ISDIR(mode) )
    {
        int tot_ent;
        int cur_ent;

            /* Read the number of directory entries and then read each */
            /*  entry's information.                                   */

        ret = read_integer(fptr, &tot_ent);

            /* Print a header */

        if ( ( ret >= 0 ) && ( tot_ent > 0 ) )
            printf("\t                 INODE\tFLAGS\tLEN\tNAME\n");

        for ( cur_ent = 1; ( cur_ent <= tot_ent ) && ( ret >= 0 ); cur_ent++ )
        {
            char    *name;
            int     len;
            int     ino;
            int     flags;

            ret = read_integer(fptr, &len);

            if ( ( ret >= 0 ) && ( len > 0 ) )
            {
                name = malloc(len);

                if ( name != (char *) NULL )
                {
                    ret = fread(name, 1, len, fptr);

                    if ( ret != len )
                        if ( ret >= 0 )
                            ret = -1;
                }
                else
                    ret = -1;
            }
            else
                name = (char *) NULL;

            if ( ret >= 0 )
            {
                ret = read_integer(fptr, &ino);

                if ( ret >= 0 )
                {
                    ret = read_integer(fptr, &flags);
                }
            }

            if ( ret >= 0 )
                printf("\tDirectory entry: %d\t%d\t%d\t%.*s\n", ino, flags, len,
                       len, name);

            if ( name != (char *) NULL )
                free(name);
        }
    }
    else
        ret = 0;

    return  ret;
}



/**
 ** FUNCTION: read_inode_spec
 **/

static int	read_inode_spec (FILE *fptr, int stg_flags)
{
	unsigned long	parent_ino;
	unsigned int	base_dev;
	unsigned long	base_ino;
	unsigned int	stg_dev;
	unsigned long	stg_ino;
	int		flags;
	int		len;
	char		*name = "";
	int		ret;

	base_dev = 0;
	base_ino = 0;
	stg_dev  = 0;
	stg_ino  = 0;

	ret = read_long(fptr, &parent_ino);

	if ( ret < 0 )
		return	ret;

	if ( stg_flags & OVL_STG_FLAG__BASE_MAPPINGS )
	{
		ret = read_integer(fptr, &base_dev);

		if ( ret < 0 )
			return	ret;


		ret = read_long(fptr, &base_ino);

		if ( ret < 0 )
			return	ret;
	}

	if ( stg_flags & OVL_STG_FLAG__STG_MAPPINGS )
	{
		ret = read_integer(fptr, &stg_dev);

		if ( ret < 0 )
			return	ret;

		ret = read_long(fptr, &stg_ino);

		if ( ret < 0 )
			return	ret;
	}

	ret = read_integer(fptr, &flags);

	if ( ret < 0 )
		return	ret;

	ret = read_integer(fptr, &len);

	if ( ret < 0 )
		return	ret;

	if ( len > 0 )
	{
		name = malloc(len);

		if ( name == NULL )
			return	-1;

		ret = fread(name, len, 1, fptr);

		if ( ( ret != 1 ) && ( ret >= 0 ) )
			return	-1;
	}

	if ( ret >= 0 )
	{
		printf("\t\tPARENT DIR: %-4lu   BDEV: %-2u  BINUM: %lu\n",
		       parent_ino, base_dev, base_ino);
		printf("\t\tFLAG: %-4u         SDEV: %-2u  SINUM: %lu\n",
		       flags, stg_dev, stg_ino);
		printf("\t\tREF NAME: %.*s (len %d)\n", len, name, len);
	}

	return	ret;
}



#if WRITE_ENABLE

/**
 ** FUNCTION: write_inode
 **/

static int  write_inode (FILE *fptr, ovlfs_inode_t *ino)
{
    off_t   pos;
    off_t   result_pos;
    int ret;

    if ( ino == (ovlfs_inode_t *) NULL )
        return  -1;

        /* Remember the current location so that the inode structure's */
        /*  size can be updated later.                                 */

    pos = ftell(fptr);

    if ( pos < 0 )
        return  pos;


        /* Write the inode's information */

    ret = write_integer(fptr, 0);

    if ( ret >= 0 )
    {
        ret = write_integer(fptr, ino->flags);

        if ( ret >= 0 )
        {
            ret = write_integer(fptr, ino->uid);

            if ( ret >= 0 )
            {
                ret = write_integer(fptr, ino->gid);

                if ( ret >= 0 )
                {
                    ret = write_integer(fptr, ino->mode);

                    if ( ret >= 0 )
                    {
                        ret = write_integer(fptr, ino->size);

                        if ( ret >= 0 )
                        {
                            ret = write_integer(fptr, ino->atime);

                            if ( ret >= 0 )
                            {
                                ret = write_integer(fptr, ino->mtime);

                                if ( ret >= 0 )
                                {
                                    ret = write_integer(fptr, ino->ctime);

                                    if ( ret >= 0 )
                                        ret = write_integer(fptr, ino->nlink);

                                    if ( ret >= 0 )
                                        ret = write_long(fptr, ino->blksize);

                                    if ( ret >= 0 )
                                        ret = write_long(fptr, ino->blocks);

                                    if ( ret >= 0 )
                                        ret = write_long(fptr, ino->version);

                                    if ( ret >= 0 )
                                        ret = write_long(fptr, ino->nrpages);

                                    if ( ret >= 0 )
                                        ret = write_inode_spec(fptr, &ino->s);

                                    if ( ret >= 0 )
                                        ret = write_inode_details(fptr, ino);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

        /* If all is well, seek back to the start of this inode's info and */
        /*  update the size.                                               */

    if ( ret >= 0 )
    {
        result_pos = ftell(fptr);

        if ( result_pos < 0 )
            ret = result_pos;
        else
        {
            ret = fseek(fptr, pos, SEEK_SET);

            if ( ret >= 0 )
            {
                ret = write_integer(fd, ( result_pos - pos ));

                if ( ret >= 0 )
                    ret = fseek(fptr, result_pos, SEEK_SET);
            }
        }
    }

    return  ret;
}

#endif



/**
 ** FUNCTION: read_inode
 **/

static int  read_inode (FILE *fptr, int stg_flags)
{
    int             tot_size;
    int             flags;
    int             mode;
    int             size;
    int             user;
    int             group;
    int             atime;
    int             ctime;
    int             mtime;
    int             ret;
    int             nlink;
#if TBD
    int             ovl_ino_flags;
#endif
    _ulong          blksize;
    _ulong          blocks;
    _ulong          version;
    _ulong          nrpages;


        /* Read the inode's information. First is the size used by this  */
        /*  inode; we don't need this, but it is useful when looking for */
        /*  one particular inode instead of trying to read all of them.  */

    ret = read_integer(fptr, &tot_size);
    if ( ret < 0 )
        goto    read_inode__out;

    printf("\tsize of inode data: %d bytes\n", tot_size);

    ret = read_integer(fptr, &flags);
    if ( ret < 0 )
        goto    read_inode__out;

            ret = read_integer(fptr, &user);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_integer(fptr, &group);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_integer(fptr, &mode);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_integer(fptr, &size);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_integer(fptr, &atime);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_integer(fptr, &mtime);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_integer(fptr, &ctime);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_integer(fptr, &nlink);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_long(fptr, &blksize);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_long(fptr, &blocks);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_long(fptr, &version);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_long(fptr, &nrpages);
    if ( ret < 0 )
        goto    read_inode__out;

    printf("\t\tUSER: %d  GROUP: %d  MODE: 0x%x  FLAGS: 0x%x\n",
           user, group, mode, flags);

    printf("\t\tSIZE: %d  ATIME: %d  CTIME: %d\n\t\tMTIME: %d\n",
           size, atime, ctime, mtime);

    printf("\t\tNLNK: %u  BLKSZ: %lu   BLKS: %d  VERS: %u\n", (int) nlink,
           blksize, (int) blocks, (int) version);

    printf("\t\tNPGS: %lu\n", nrpages);

    ret = read_inode_spec(fptr, stg_flags);
    if ( ret < 0 )
        goto    read_inode__out;

    ret = read_inode_details(fptr, mode);
    if ( ret < 0 )
        goto    read_inode__out;

read_inode__out:

    return  ret;
}



/**
 **
 **/

static int  read_mappings (FILE *fptr)
{
    int tot_ele;
    int cur_ele;
    int ret;

    ret = read_integer(fptr, &tot_ele);

    if ( ret >= 0 )
        printf("MAP LIST: %d elements\n", tot_ele);

    cur_ele = 1;

    while ( ( cur_ele <= tot_ele ) && ( ret >= 0 ) )
    {
        int ovl_ino;
        int dev;
        int ino;

        ret = read_integer(fptr, &dev);

        if ( ret >= 0 )
        {
            ret = read_integer(fptr, &ino);

            if ( ret >= 0 )
            {
                ret = read_integer(fptr, &ovl_ino);

                if ( ret >= 0 )
                {
                    printf("\tDEVICE %d  INODE %d:  OVL INO %d\n",
                           dev, ino, ovl_ino);
                }
            }
        }
            cur_ele++;
    }

    return  ret;
}



/**
 ** FUNCTION: read_file
 **/

#define MAGIC_WORD_V3		"Ovl3"
#define MAGIC_WORD_V4		"Ovl4"
#define MAGIC_WORD_LEN		4
#define OVLFS_SUPER_MAGIC	0xFE0F

static void  read_file (char *fname)
{
    FILE    *fptr;
    int     tot_inode;
    int     cur_inode;
    int     stg_flags;
    int     ret;
    char    magic_buf[MAGIC_WORD_LEN];
    int     magic_no;

    fptr = fopen(fname, "r");

    if ( fptr == (FILE *) NULL )
    {
        perror(fname);
        fprintf(stderr, "unable to open file %s for reading\n", fname);
    }
    else
    {
        ret = read_integer(fptr, &magic_no);

        if ( ret < 0 )
        {
            fprintf(stderr, "error %d reading magic #\n", ret);
            fclose(fptr);
            return;
        }

        if ( magic_no != OVLFS_SUPER_MAGIC )
        {
            fprintf(stderr, "magic number read does not match: "
                    "read 0x%x, expecting 0x%x\n", magic_no,
                     OVLFS_SUPER_MAGIC);

            fclose(fptr);
            return;
        }
        else
            printf("MAGIC NUMBER: 0x%x\n", magic_no);


        ret = fread(magic_buf, 1, MAGIC_WORD_LEN, fptr);

        if ( ret == MAGIC_WORD_LEN )
        {
            if ( memcmp(MAGIC_WORD_V3, magic_buf, MAGIC_WORD_LEN) == 0 )
            {
                Revision = 3;

                stg_flags = 0;

                if ( Base_mapping_f )
                    stg_flags |= OVL_STG_FLAG__BASE_MAPPINGS;

                if ( Stg_mapping_f )
                    stg_flags |= OVL_STG_FLAG__STG_MAPPINGS;
            }
            else if ( memcmp(MAGIC_WORD_V4, magic_buf, MAGIC_WORD_LEN) == 0 )
            {
                Revision = 4;

                ret = read_integer(fptr, &stg_flags);

                if ( ret <= 0 )
                {
                    fclose(fptr);
                    return;
                }
            }
            else
            {
                fprintf(stderr, "magic word read does not match; read '%.*s', "
                        "expecting %s\n", ret, magic_buf, MAGIC_WORD_V4);

                fclose(fptr);
                return;
            }

            printf("MAGIC WORD: %.*s\n", ret, magic_buf);
        }
        else
        {
            fprintf(stderr, "error; read magic word returned %d; "
                    "expecting %d\n", ret, MAGIC_WORD_LEN);

            fclose(fptr);
            return;
        }

        ret = read_integer(fptr, &tot_inode);

        if ( ret >= 0 )
            printf("reading %d inodes\n", tot_inode);

        cur_inode = 1;

        while ( ( cur_inode <= tot_inode ) && ( ret >= 0 ) )
        {
            printf("\nReading inode %d:\n", cur_inode);

            ret = read_inode(fptr, stg_flags);

            cur_inode++;
        }

        if ( ret >= 0 )
        {
            ret = read_mappings(fptr);

            if ( ret >= 0 )
                ret = read_mappings(fptr);
        }

        fclose(fptr);
    }
}



/***
 *** MAIN PROGRAM
 ***/

int	main (int argc, char **argv)
{
    if ( argc < 2 )
    {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return  1;
    }

    read_file(argv[1]);

    return 0;
}
