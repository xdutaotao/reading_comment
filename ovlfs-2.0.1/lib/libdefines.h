/*****************************************************************************
 *****************************************************************************
 **
 ** COPYRIGHT (C) 1998-2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 **    FILE: libdefines.h
 **
 ** PURPOSE: This file contains definitions which are common to many library
 **          functions.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	=============================================
 ** 2003-05-25	ARTHUR N.	Initial Release (as part of ovlfs).
 **
 *****************************************************************************
 *****************************************************************************
 **/

#ifndef _LIB_DEFINES_INCLUDE
#define _LIB_DEFINES_INCLUDE	1

# ident "@(#)libdefines.h	1.1	[03/07/27 22:20:36]"

/**
 ** CONSTANTS:
 **/

#define BUFFERSIZE		1024

# ifndef TRUE
#  define TRUE	1
# endif
# ifndef FALSE
#  define FALSE	0
# endif

# ifndef SUCCESS
#  define SUCCESS	0
# endif

# ifndef FAILURE
#  define FAILURE	-1
# endif

# define FOPEN_READONLY		"r"
# define FOPEN_WRITEONLY	"w"

# define DEFAULT_VERBOSE	TRUE

/**
 ** PROTOTYPES
 **/

# ifndef _NO_PROTOTYPES
#  ifdef __cplusplus
extern "C" {
#  endif

int	is_number(char *);
int	highbit(unsigned int);
int	round(double);

#  ifdef __cplusplus
}
#  endif
# endif

#endif
