/******************************************************************************
 ******************************************************************************
 **
 ** FILE: file_io_priv.h
 **
 ** DESCRIPTION:
 **	Definitions for the implementation of the kernel file I/O operations.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-03-11	ARTHUR N.	Initial Release
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifndef file_io_priv__STDC_INCLUDE
#define file_io_priv__STDC_INCLUDE	1

# ident "@(#)file_io_priv.h	1.1	[03/07/27 22:20:33]"

# ifndef TRUE
#  define TRUE	1
# endif

# ifndef FALSE
#  define FALSE	0
# endif

# ifndef FIO_MAX_FILE
#  define FIO_MAX_FILE	256
# endif

# define FIO_LOCK()	spin_lock(&FIO_lock)
# define FIO_UNLOCK()	spin_unlock(&FIO_lock)



/**
 ** TYPE: fio_file_info_t
 **
 ** DESCRIPTION:
 **	Maintains data for each file descriptor.
 **/

struct fio_file_info_struct {
	struct file	*filp;
	int		in_use;
} ;

typedef struct fio_file_info_struct	fio_file_info_t;

#endif
