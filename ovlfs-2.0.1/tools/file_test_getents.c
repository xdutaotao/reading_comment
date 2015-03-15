/******************************************************************************
 ******************************************************************************
 **
 ** FILE: file_test_getents.c
 **
 ** DESCRIPTION:
 **	Sources for the getents() function for the file_test program which
 **	directly calls the getdents system call.
 **
 ** NOTES:
 **	- A separate source file is needed to allow both getdents and
 **	  readdir to be used in the same program; the definitions of the
 **	  dirent struct is not the same for both.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-07-05	ARTHUR N.	Initial Release
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)file_test_getents.c	1.1	[03/07/27 22:20:38]"

#include <linux/types.h>
#include <linux/dirent.h>
#include <linux/unistd.h>

#include "file_test.h"

_syscall3(int, getdents, uint, fd, struct dirent *, dirp, uint, count);

/* TBD: better assertions */

#define ASSERT(expr)	( (expr) ? 0 : abort() )

#ifndef TRUE
# define TRUE	1
#endif

#ifndef FALSE
# define FALSE	0
#endif


/**
 ** FUNCTION: getents
 **/

int	getents (uint fd, uint8_t *buffer, uint buf_size)
{
	int	rc;

	rc = getdents(fd, (struct dirent *) buffer, buf_size);

	return	rc;
}



/**
 **
 **/

const char	*getent_name (uint8_t *ent_ptr)
{
	struct dirent	*dent;

	dent = (struct dirent *) ent_ptr;

	return	dent->d_name;
}



/**
 **
 **/

uint8_t	*getent_next (uint8_t *ent_ptr)
{
	struct dirent	*dent;
	uint8_t		*ret;

	dent = (struct dirent *) ent_ptr;

	ASSERT(dent->d_reclen > 0);

	ret = ent_ptr + dent->d_reclen;

	return	ret;
}
