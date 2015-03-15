/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_alloc.c
 **
 ** DESCRIPTION: This file contains functions to handle the allocation of
 **              kernel memory for the overlay (i.e. psuedo) filesystem.
 **
 **
 ** NOTES:
 **	- These functions are not always used; their purpose is to verify
 **	  that all memory allocated by the overlay filesystem is freed by
 **	  it also.
 **
 **	- If VERIFY_MALLOC is not set, this file ends up being empty.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 03/08/1998	ARTHUR N.	Added to source code control.
 ** 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#) ovl_alloc.c 1.3"

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "ovlfs.ver"
# endif
#endif

#include <linux/stddef.h>

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/slab.h>
#include <asm/statfs.h>

#include "ovl_fs.h"


#if VERIFY_MALLOC

/**
 ** STATIC GLOBALS
 **/

static unsigned int	alloced_mem = 0;
static unsigned int	max_alloced_mem = 0;
static unsigned int	alloced_overhead = 0;
static unsigned int	max_alloced_overhead = 0;



/**
 ** FUNCTION: ovlfs_start_alloc
 **
 **  PURPOSE: Given that a new kernel resource will start allocating memory,
 **           display the current memory use information so that it may be
 **           used as a reference when allocation is completed.
 **
 ** FUNCTION: ovlfs_stop_alloc
 **
 **  PURPOSE: Given that memory allocation activity has completed for a kernel
 **           resource, display the resulting memory use information.
 **/

void	ovlfs_start_alloc (const char *msg)
{
	if ( msg != (char *) NULL )
		printk("OVLFS: start_alloc: memory use: %d, max %d, overhead "
		       "%d, max %d: %s\n",
		       alloced_mem, max_alloced_mem, alloced_overhead,
		       max_alloced_overhead, msg);
	else
		printk("OVLFS: start_alloc: memory use: %d, max %d, overhead"
		       "%d, max %d\n",
		       alloced_mem, max_alloced_mem, alloced_overhead,
		       max_alloced_overhead);
}

void	ovlfs_stop_alloc (const char *msg)
{
	if ( msg != (char *) NULL )
		printk("OVLFS: stop_alloc: memory use: %d, max %d, overhead "
		       "%d, max %d: %s\n",
		       alloced_mem, max_alloced_mem, alloced_overhead,
		       max_alloced_overhead, msg);
	else
		printk("OVLFS: stop_alloc: memory use: %d, max %d, overhead"
		       "%d, max %d\n",
		       alloced_mem, max_alloced_mem, alloced_overhead,
		       max_alloced_overhead);
}



/**
 ** FUNCTION: ovlfs_alloc
 **
 **  PURPOSE: Allocate memory for the overlay filesystem.
 **/

void	*ovlfs_alloc (unsigned int size)
{
	unsigned int	*ptr;

	if ( size == 0 )
		return	NULL;

	ptr = (unsigned int *) kmalloc(size + sizeof(ptr[0]), GFP_KERNEL);

	if ( ptr != (unsigned int) NULL )
	{
		ptr[0] = size + sizeof(ptr[0]);	/* Count the extra */

		alloced_mem += ptr[0];
		alloced_overhead += sizeof(ptr[0]);

		if ( alloced_mem > max_alloced_mem )
			max_alloced_mem = alloced_mem;

		if ( alloced_overhead > max_alloced_overhead )
			max_alloced_overhead = alloced_overhead;

		ptr++;
	}

	return	(void *) ptr;
}



/**
 ** FUNCTION: ovlfs_free
 ** FUNCTION: ovlfs_free2
 **
 **  PURPOSE: Free memory allocated with ovlfs_alloc.
 **
 ** NOTES:
 **	- This function must only be given pointers returned by ovlfs_alloc.
 **/

void	ovlfs_free2 (const void *ptr, unsigned int vsize)
{
	unsigned int	*tmp;

	tmp = (unsigned int *) ptr;
	tmp--;

	if ( tmp[0] < 4 )
	{
		printk("OVLFS: ovlfs_free: invalid address, 0x%x, given "
		       "(size is %u)\n", (int) ptr, tmp[0]);
	}
	else
	{
		unsigned int	tot;

		tot = tmp[0] - sizeof(unsigned int);

			/* Check the size for a match, unless the expected  */
			/*  size is 0 or 1: 0 is given when the size is not */
			/*  determinable, and 1 results from a char buffer  */
			/*  being freed.                                    */

		if ( ( vsize > 1 ) && ( vsize != tot ) )
			printk("OVLFS: Warning: ovlfs_free: block size "
			       "mismatch: got %d expected %d\n", tot, vsize);

		alloced_mem -= tmp[0];
		alloced_overhead -= sizeof(unsigned int);

		kfree(tmp);
	}
}

void	ovlfs_free (const void *ptr)
{
	ovlfs_free2(ptr, 0);
}

#endif
/* VERIFY_MALLOC */
