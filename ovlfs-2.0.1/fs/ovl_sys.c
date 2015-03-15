/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_sys.c
 **
 ** DESCRIPTION: This file configures function pointers for system calls
 **              needed within the kernel for the overlay filesystem.
 **
 ** NOTES:
 **	- I am certain that someone will tell me why it is a good idea not to
 **	  define the standard C system call functions (e.g. open and read) in
 **	  the kernel.  I do not intend to argue that point with this source;
 **	  if there are important considerations one way or the other, please
 **	  feel free to update this source.
 **
 **	- The ovlfs_setup_syscalls() function MUST be called before any of
 **	  the functions setup here or ENOSYS will be returned.  Of course,
 **	  any system calls that are not defined in the kernel will continue
 **	  to return ENOSYS.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 09/27/1997	art            	Added to source code control.
 ** 02/27/1998	ARTHUR N.	Improved comments.
 ** 03/09/1998	ARTHUR N.	Added the copyright notice.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#) ovl_sys.c 1.3"

#include <linux/modversions.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/unistd.h>

#include "ovl_fs.h"



	/** The sys_call_table[] is the kernel table of system calls **/

extern syscall_t	sys_call_table[];

/**
 ** FUNCTION: dummy_syscall
 **
 **  PURPOSE: This is the default function handler used by all system call
 **           functions defined in this file since it is not possible to
 **           perform the initializations in the same manner that
 **           ovlfs_setup_syscalls() does without calling a function.
 **/

static int	dummy_syscall ()
{
	printk("OVLFS: dummy syscall function called!\n");

	return	-ENOSYS;
}



/**
 ** SYSTEM CALLS: initialize all function pointers to the dummy syscall
 **               function in case anyone attempts to use them before calling
 **               ovlfs_setup_syscalls().
 **/

syscall_t	open = dummy_syscall;
syscall_t	close = dummy_syscall;
syscall_t	read = dummy_syscall;
syscall_t	lseek = dummy_syscall;
syscall_t	write = dummy_syscall;



/**
 ** FUNCTION: ovlfs_setup_syscalls
 **
 **  PURPOSE: Setup the pointers for all system call functions being supported
 **           here by referencing the appropriate element of the system call
 **           table.
 **/

void	ovlfs_setup_syscalls ()
{
	if ( sys_call_table[__NR_open] != NULL )
		open = sys_call_table[__NR_open];

	if ( sys_call_table[__NR_close] != NULL )
		close = sys_call_table[__NR_close];

	if ( sys_call_table[__NR_read] != NULL )
		read = sys_call_table[__NR_read];

	if ( sys_call_table[__NR_lseek] != NULL )
		lseek = sys_call_table[__NR_lseek];

	if ( sys_call_table[__NR_write] != NULL )
		write = sys_call_table[__NR_write];
}
