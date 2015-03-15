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
##	@(#)Makefile	1.5	[03/07/27 22:20:33]
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
## 2003-03-11	ARTHUR N.	Initial Release
## 2003-06-07	ARTHUR N.	Use Rules.mk and Suffix.mk.  Also added rules,
##				 install and fresh.
## 2003-06-09	ARTHUR N.	Support module versions.
##
##############################################################################
##############################################################################

TOP=..

################################# Rules.mk #################################
include $(TOP)/Rules.mk

-include Override.mk

###
### COMPILATION MACROS
###

INC=../include

KERNEL_DEBUG_CFLAGS ?= \
	-g -O2

KERNEL_CFLAGS ?= \
	$(KERNEL_INCLUDES)	\
	-I../include		\
	$(KERNEL_DEBUG_CFLAGS)	\
	-D__KERNEL__

MODULE_FLAGS ?= \
	-include $(KERNEL_INCLUDE_DIR)/linux/module.h	\
	-DEXPORT_SYMTAB					\
	-DMODULE					\
	$(MODVERSION_FLAG)

CFLAGS=\
	$(USER_CFLAGS)	\
	$(KERNEL_CFLAGS)

VER_FILE=$(INC)/file_io.ver



###
### TARGET MACROS
###

MODULES=\
	file_io.o

INSTALLED_MODULES=\
	$(MODULES:%=$(MISC_MOD_INST_DIR)/%)



###
### STANDARD TARGETS
###

.PHONY: all install fresh

all: file_io.o

install: modules_install

fresh: clean all



###
### MODULE TARGETS
###

file_io.o:	file_io.c $(VER_FILE)
	@echo "compiling $@"
	@$(CC_FOR_TARGET) $(CFLAGS) $(MODULE_FLAGS) -c "$<" -o "$@"

$(VER_FILE):	file_io.c
	@echo "Creating $@"
	@-$(CC_FOR_TARGET) $(CFLAGS) $(MODULE_FLAGS) -D__GENKSYMS__ -E "$<" | \
	 $(GENKSYMS) $(GENKSYMS_ARGS) >"$@"



###
### CLEANUP
###

.PHONY: clean clean-mod

clean: clean-mod

clean-mod:
	$(RM) -f file_io.o $(VER_FILE)



###
### INSTALLATION RULES:
###

.PHONY: modules_install

modules_install: $(INSTALLED_MODULES)



################################# Suffix.mk #################################
include $(TOP)/Suffix.mk
