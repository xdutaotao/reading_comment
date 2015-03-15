##############################################################################
##############################################################################
##
## COPYRIGHT (C) 1998-2003 By Arthur Naseef
##
## This file is covered under the GNU General Public License Version 2.  For
##  more information, see the file COPYING.
##
##
## FILE: Makefile
##
## FILE ID:
##	@(#)Makefile	1.3	[03/07/27 22:20:36]
##
## DESCRIPTION:
##	- This file contains the dependencies and build rules for the overlay
##	  filesystem.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	==============================================
## 2003-05-25	ARTHUR N.	Initial Release (as part of ovlfs).
## 2003-06-07	ARTHUR N.	Use Rules.mk and Suffix.mk.  Also, support
##				 cross-compiling.
## 2003-06-09	ARTHUR N.	Added -f argument to RM.
##
##############################################################################
##############################################################################

TOP=..

################################# Rules.mk #################################
include $(TOP)/Rules.mk

-include Override.mk

CFLAGS   ?= -g
CXXFLAGS ?= -g

LIBNAME=libasn.a


###
### STANDARD TARGETS
###

.PHONY: all clean

all:	$(LIBNAME)

clean:	clean-libs



###
### LIBRARY RULES
###

$(LIBNAME)::	$(LIBNAME)(list.o)
$(LIBNAME)::	$(LIBNAME)(list_cint.o)
$(LIBNAME)::	$(LIBNAME)(list_index.o)

$(LIBNAME)(%): AR=$(AR_FOR_TARGET)
$(LIBNAME)(%): CXX=$(CXX_FOR_TARGET)



###
### CLEANUP
###

.PHONY: clean-libs

clean-libs:
	@$(RM) -f $(LIBNAME)

################################# Suffix.mk #################################
include $(TOP)/Suffix.mk
