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
##	- This file contains the dependencies and build rules for the overlay
##	  mount program.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	==============================================
## 2003-06-04	ARTHUR N.	Initial Release
## 2003-06-08	ARTHUR N.	Added Rules.mk and Suffix.mk, and moved
##				 installation location macro to Rules.mk.
## 2003-06-09	ARTHUR N.	Include the lex and yacc output by default in
##				 the release, with prepdist target, and don't
##				 remove with clean.
##
##############################################################################
##############################################################################

TOP=..

################################# Rules.mk #################################
include $(TOP)/Rules.mk

-include Override.mk

##
## COMPILATION MACROS
##

CFLAGS    ?= -g
INC       ?= -I../lib
LIB       ?= -L../lib -lasn

# If you have and use a real "lex", unset FLEX_FLAG.
FLEX_FLAG ?= -DFLEX
CFLAGS += $(FLEX_FLAG)

OVLFS_MOUNT_OBJ=\
	lex.yy.o \
	y.tab.o \
	ovlmount.o

DIST_TARGETS=\
	lex.yy.c	\
	y.tab.c		\
	y.tab.h



##
## STANDARD TARGETS
##

.PHONY: all fresh prepdist

all: ovlmount

fresh: clean all

prepdist: $(DIST_TARGETS)



##
## PROGRAM RULES
##

ovlmount: $(OVLFS_MOUNT_OBJ)
	@echo making ovlmount
	@$(CXX_FOR_TARGET) $(CFLAGS) -o ovlmount $(OVLFS_MOUNT_OBJ) $(LIB) -lfl



##
## INTERMEDIATE SOURCE RULES
##

ovlmount.o: ovlmount.c
	@$(CC_FOR_TARGET) $(CFLAGS) -c ovlmount.c $(INC)

lex.yy.o: lex.yy.c y.tab.h
	@echo making lex.yy.o
	@$(CC_FOR_TARGET) $(CFLAGS) -c lex.yy.c $(INC)

y.tab.o: y.tab.c
	@echo making y.tab.o
	@$(CC_FOR_TARGET) $(CFLAGS) -c y.tab.c $(INC)

lex.yy.c: ovlfs_tab.l
	@echo making lex.yy.c
	@$(LEX) ovlfs_tab.l

y.tab.c y.tab.h: ovlfs_tab.y
	@echo making y.tab.c and y.tab.h
	@$(YACC) -dv ovlfs_tab.y


##
## INSTALLATION RULES
##

.PHONY: install

install:
	@echo "Installing ovlmount in $(OVLFS_BINDIR)"
	@$(INSTALL) -d -m755 -o root -g root $(OVLFS_BINDIR)
	@$(INSTALL) -m555 -o root -g root ovlmount $(OVLFS_BINDIR)/ovlmount


##
## CLEANUP
##

.PHONY: clean cleanup source-clean

clean cleanup:
	$(RM) -f lex.yy.o y.tab.o y.output $(OVLFS_MOUNT_OBJ) ovlmount

source-clean: clean
	$(RM) -f lex.yy.c y.tab.[ch] y.output



################################# Suffix.mk #################################
include $(TOP)/Suffix.mk
