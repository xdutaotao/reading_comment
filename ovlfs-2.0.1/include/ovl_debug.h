/******************************************************************************
 ******************************************************************************
 **
 ** FILE: ovl_debug.h
 **
 ** DESCRIPTION:
 **	Definitions used by the Overlay filesystem for debugging and diagnostic
 **	features of the driver.
 **
 **
 ** REVISION HISTORY
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-06-08	ARTHUR N.	Initial Release
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifndef __ovl_debug_STDC_INCLUDE
#define __ovl_debug_STDC_INCLUDE	1

# ident "@(#)ovl_debug.h	1.1	[03/07/27 22:20:35]"

	/* Log message standard information */

# define CALL_INFO	__FUNCTION__, __FILE__, __LINE__
# define CALL_FMT	"OVLFS: %s [%s, %d]: "


	/* Log message generation */

# define MSG(lvl, args...)				\
		( printk(lvl CALL_FMT, CALL_INFO),	\
		  printk(args) )

# define INFO(args...)	MSG(KERN_INFO, args)
# define WARN(args...)	MSG(KERN_WARNING, args)
# define ERROR(args...)	MSG(KERN_CRIT, args)
# define EMERG(args...)	MSG(KERN_EMERG, args)


	/* Assertions */

# ifdef BUG
#  define OVLFS_BUG	BUG
# else
#  define OVLFS_BUG	panic("ovlfs BUG at %s [%s, %d]!\n", __FUNCTION__, \
			      __FILE__, __LINE__)
# endif

# define ASSERT_MSG(x)	EMERG("FAILED ASSERTION: %s\n",	#x)

# if ASSERT_FATAL_F
#  define ASSERT(x)	(x) ? 0 : ASSERT_MSG(x), OVLFS_BUG()
# else
#  define ASSERT(x)	(x) ? 0 : ASSERT_MSG(x)
# endif

#endif
