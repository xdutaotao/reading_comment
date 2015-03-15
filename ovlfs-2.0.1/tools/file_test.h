/******************************************************************************
 ******************************************************************************
 **
 ** FILE: file_test.h
 **
 ** DESCRIPTION:
 **	Definitions for the file_test program.
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

#ifndef file_test_STDC_INCLUDE
#define file_test_STDC_INCLUDE	1

# ident "@(#)file_test.h	1.1	[03/07/27 22:20:38]"

/* NOTE: stdint.h would normally be included here, but the getdent system */
/*       call uses linux/types.h which conflicts with it; include the     */
/*       correct version of the header file when using this file.         */

# ifdef __cplusplus
extern "C" {
# endif

int		getents(uint, uint8_t *, uint);
const char	*getent_name(uint8_t *);
uint8_t		*getent_next(uint8_t *);

# ifdef __cplusplus
}
# endif

#endif
