/******************************************************************************
 ******************************************************************************
 **
 ** FILE: dumpi.h
 **
 ** DESCRIPTION: Place something descriptive here
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 02/27/1998	art            	Added to source code control.
 ** 02/15/2003	art            	Pre-release of port to 2.4.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)dumpi.h	1.3 [03/07/27 22:20:35]"

#ifndef __dumpi_STDC_INCLUDE
#define __dumpi_STDC_INCLUDE	1

/******************************************************************************
 ******************************************************************************
 **
 ** CONSTANTS:
 **/

#define INODE_NULL		(struct inode *) NULL

/* DEBUGGING */
#define DUMPI_DEBUG_FILE	"/tmp/inodes.debug"

/******************************************************************************
 ******************************************************************************
 **
 ** MACROS:
 **/

#undef dprintf
#define dprintf	dumpi_dprintf

/* The third parameter to open_namei() is the mode of the file; since we */
/*  are not creating files (yet?) this is set to 0.                      */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,0)
# define get_inode(f,i)		open_namei(f, 0, 0, i, (struct inode *) NULL)
# define get_inode_ro(f,i)	open_namei(f, 1, 0, i, (struct inode *) NULL)
# define get_inode_wo(f,i)	open_namei(f, 2, 0, i, (struct inode *) NULL)
# define get_inode_rw(f,i)	open_namei(f, 3, 0, i, (struct inode *) NULL)
#else

	int	get_inode(const char *name, struct inode **r_inode);
	int	get_inode_ro(const char *name, struct inode **r_inode);
	int	get_inode_wo(const char *name, struct inode **r_inode);
	int	get_inode_rw(const char *name, struct inode **r_inode);

#endif

#endif
