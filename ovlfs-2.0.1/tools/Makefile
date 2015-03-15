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
## FILE VERSION:
##	@(#)Makefile	1.10	[03/07/27 22:20:37]
##
## DESCRIPTION:
##	- This file contains the dependencies and build rules for the tools
##	  delivered with the overlay filesystem.
##
## NOTES:
##	- run "make all" to build the tools.
##
##	- There currently is no installation for these tools.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	==============================================
## 03/11/1998	ARTHUR N.	Added to source code control.
## 03/24/1998	ARTHUR N.	Added the mount_ovl program.
## 04/19/1998	ARTHUR N.	Added the modver_test program.
## 04/22/1998	ARTHUR N.	Added the configure program.
## 02/15/2003	ARTHUR N.	Added mmap_test and truncate.
## 05/25/2003	ARTHUR N.	Added file_test.
## 06/08/2003	ARTHUR N.	Added Rules.mk and Suffix.mk.  Also added
##				 installation rules.
## 06/09/2003	ARTHUR N.	RM macro only defines the program by default.
## 06/09/2003	ARTHUR N.	Don't remove modver_test or ../configure in
##				 clean.  Also added source-clean to remove
##				 them.
## 07/05/2003	ARTHUR N.	Added file_test_getents.c to file_test.
## 07/05/2003	ARTHUR N.	Define STORE_MAPPINGS by default.
##
##############################################################################
##############################################################################

TOP=..

################################# Rules.mk #################################
include $(TOP)/Rules.mk

-include Override.mk

##
## PROGRAM MACROS
##

CC_FOR_TARGET ?= $(CC)

MV      ?= mv
RM      ?= rm
INSTALL ?= install



##
## COMPILATION MACROS
##

INC=../include

SRC_FILES=\
	mount_ovl.sh \
	view_stg.c

OBJ_FILES=\
	view_stg.o

DIST_PROGS=\
	../configure	\
	modver_test

PROG_FILES=\
	configure	\
	file_test	\
	mount_ovl	\
	mmap_test	\
	truncate	\
	view_stg

INSTALLABLE_PROGS=\
	view_stg

INSTALLED_PROGRAMS=\
	$(INSTALLABLE_PROGS:%=$(OVLFS_BINDIR)/%)
	


### CFLAGS

# Make sure these match the kernel module for easy operation...
OVLFS_FLAGS ?= -DSTORE_MAPPINGS

CFLAGS ?= $(COPTS) -Wall -O2 -I$(INC) $(OVLFS_FLAGS)



##############################################################################
##############################################################################
##
## STANDARD TARGETS
##

.PHONY: all fresh install


all:		$(PROG_FILES)

prepdist:	$(DIST_PROGS)

fresh:		clean all

install:	install-progs



##############################################################################
##############################################################################
##
## PROGRAM TARGETS
##

configure: configure.sh
	@echo "Building configure"
	@$(INSTALL) -m 555 configure.sh configure

../configure: configure
	@echo "Building ../configure"
	@$(MV) -f configure ../configure

file_test: file_test.o file_test_getents.o
	@echo "Compiling $@"
	@$(CC_FOR_TARGET) $(CFLAGS) -o "$@" $+


modver_test: modver_test.sh
	@echo "Building modver_test"
	@$(INSTALL) -m 555 modver_test.sh modver_test

mount_ovl: mount_ovl.sh
	@echo "Building mount_ovl"
	@$(INSTALL) -m 555 mount_ovl.sh mount_ovl

view_stg: view_stg.o
	@echo "Compiling view_stg"
	@$(CC_FOR_TARGET) $(CFLAGS) -o view_stg view_stg.o

view_stg.o: view_stg.c
	@echo "Compiling view_stg.o"
	@$(CC_FOR_TARGET) -c $(CFLAGS) view_stg.c

mmap_test: mmap_test.c
	@echo "Compiling $@"
	@$(CC_FOR_TARGET) $(CFLAGS) -o mmap_test mmap_test.c

truncate: truncate.c
	@echo "Compiling $@"
	@$(CC_FOR_TARGET) $(CFLAGS) -o "$@" $+


##############################################################################
##############################################################################
##
## INSTALLATION RULES
##

.PHONY: install-progs

install-progs: $(INSTALLED_PROGRAMS)



##############################################################################
##############################################################################
##
## MISCELLANEOUS TARGETS
##

.PHONY: clean dist-clean source-clean

clean:
	$(RM) -f $(OBJ_FILES)
	$(RM) -f $(PROG_FILES)

dist-clean: clean

source-clean: clean
	$(RM) -f $(DIST_PROGS)



################################# Suffix.mk #################################
include $(TOP)/Suffix.mk
