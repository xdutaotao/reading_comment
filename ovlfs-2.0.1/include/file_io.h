/******************************************************************************
 ******************************************************************************
 **
 ** FILE: file_io.h
 **
 ** DESCRIPTION:
 **	Definitions for the kernel file I/O operations.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-03-11	ARTHUR N.	Initial Release
 ** 2003-06-09	ARTHUR N.	Add versioned symbol support.
 ** 2003-07-05	ARTHUR N.	Added seek constants.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifndef file_io__STDC_INCLUDE
#define file_io__STDC_INCLUDE	1

# ident "@(#)file_io.h	1.3	[03/07/27 22:20:35]"

# ifdef MODVERSIONS
#  ifndef __GENKSYMS__
#   include "file_io.ver"
#  endif
# endif

# define FILE_IO__SEEK_SET	0
# define FILE_IO__SEEK_CUR	1
# define FILE_IO__SEEK_END	2

	int	file_open(const char *, int, int);
	int	file_dup_filp(struct file *);
	void	file_close(int);
	size_t	file_read(int, char *, size_t);
	size_t	file_write(int, const char *, size_t);
	loff_t	file_lseek(int, loff_t, int);

#endif
