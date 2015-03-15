##############################################################################
##############################################################################
##
## COPYRIGHT (C) 2003 By Arthur Naseef
##
## This file is covered under the GNU General Public License Version 2.  For
##  more information, see the file COPYING.
##
##
## FILE: Makefile
##
## FILE VERSION:
##	@(#)Makefile	1.3	[03/07/27 22:20:37]
##
## DESCRIPTION:
##	- This file contains the dependencies and build rules for the tools
##	  delivered with the overlay filesystem.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	==============================================
## 05/25/2003	ARTHUR N.	Initial Release
## 06/08/2003	ARTHUR N.	Added Rules.mk and Suffix.mk.  Also added
##				 installation rules.
## 06/08/2003	ARTHUR N.	Support cross-compiling.
##
##############################################################################
##############################################################################

TOP=..

################################# Rules.mk #################################
include $(TOP)/Rules.mk

-include Override.mk



##
## MACRO DEFINITIONS
##

INC=../include

SRC_FILES=\
	cdinfo.c

OBJ_FILES=\
	cdinfo.o

PROG_FILES=\
	cdinfo

INSTALLED_PROGRAMS=\
	$(OVLFS_BINDIR)/cdinfo

### CFLAGS

CFLAGS=$(COPTS) -Wall -O2 -I$(INC)



##############################################################################
##############################################################################
##
## STANDARD TARGETS
##

.PHONY: all bak fresh install


all:		$(PROG_FILES)

fresh:		clean all

install:	install-programs



##############################################################################
##############################################################################
##
## PROGRAM TARGETS
##

cdinfo: cdinfo.c
	@echo "Compiling $@"
	@$(CC_FOR_TARGET) $(CFLAGS) -o "$@" $+



##############################################################################
##############################################################################
##
## INSTALLATION TARGETS:
##

.PHONY: install-programs

install-programs: $(INSTALLED_PROGRAMS)



##############################################################################
##############################################################################
##
## MISCELLANEOUS TARGETS
##

clean:
	@$(RM) -f $(OBJ_FILES)
	@$(RM) -f $(PROG_FILES)

dist-clean: clean



################################# Suffix.mk #################################
include $(TOP)/Suffix.mk
