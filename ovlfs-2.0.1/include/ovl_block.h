/******************************************************************************
 ******************************************************************************
 **
 ** FILE: ovl_block.h
 **
 ** DESCRIPTION:
 **	Definitions used by the Overlay Filesystem for holding and tracking
 **	data blocks.
 **
 ** NOTES:
 **	- This is very preliminary.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-06-08	ARTHUR N.	Initial Release.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifndef __ovl_block_STDC_INCLUDE
#define __ovl_block_STDC_INCLUDE	1

# ident "@(#)ovl_block.h	1.1	[03/07/27 22:20:35]"

# include "ovl_types.h"

/**
 ** DESCRIPTION:
 **	This file contains definitions for the ovlfs block data types which
 **	are used to hold and track data blocks.  I would like to use the
 **	regular blocks, but it is not clear how they interact with devices;
 **	particularly a pseudo-device.  It does not seem worthwhile to have
 **	my own pseudo block device just so that I can avoid writing this code.
 **
 **	On the other hand, there are some features of the existing buffer
 **	cache that are useful, such as caching.  But, the physical interface
 **	that underlies these "pseudo-blocks" will use the buffer cache, so
 **	perhaps I should not worry about it, and simply have these blocks
 **	come and go as used. (good answer :))
 **/

# include <linux/wait.h>

/* lock_mode is intended to allow multiple read_only locks, but only */
/*  one exclusive lock.  Should I care?                              */

struct ovlfs_block_struct
{
	char			*ptr;
	_uint			size;
	int			use_count;
	int			lock_mode;	/* read or excluse? */
	struct semaphore	sem;	/* Control simultaneous access */
} ;

typedef struct ovlfs_block_struct	ovlfs_block_t;

/* need to have more than one process be able to use a block at one time, */
/*  and release the block after no more processes are using it.           */

#endif
