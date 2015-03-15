/******************************************************************************
 ******************************************************************************
 **
 ** FILE: kernel_vers_spec.h
 **
 ** DESCRIPTION:
 **	Definitions used to distinguish, and work with, different versions of
 **	the kernel.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-06-08	ARTHUR N.	Initial Release
 ** 2003-06-09	ARTHUR N.	Include version.h since it is always needed
 **				 here, and is not included in the (past)
 **				 expected fashion when __GENKSYMS__ is set.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifndef kernel_vers_spec_STDC_INCLUDE
#define kernel_vers_spec_STDC_INCLUDE	1

# ident "@(#)kernel_vers_spec.h	1.2	[03/07/27 22:20:35]"

# ifndef DONT_INCLUDE_VERSION_H
#  include <linux/version.h>
# endif

# ifndef KERNEL_VERSION
#  define KERNEL_VERSION(a,b,c)	(((a) << 16) + ((b) << 8) + (c))
# endif

# if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) )
#  define POST_22_KERNEL_F	1
#  define PRE_24_KERNEL_F	0
#else
#  define POST_22_KERNEL_F	0
#  define PRE_24_KERNEL_F	1
# endif

# if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0) )
#  define POST_20_KERNEL_F	1
#  define PRE_22_KERNEL_F	0
# else
#  define POST_20_KERNEL_F	0
#  define PRE_22_KERNEL_F	1
# endif

#endif
