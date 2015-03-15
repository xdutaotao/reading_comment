##############################################################################
##############################################################################
##
## FILE: Makefile
##
## FILE VERSION:
##	@(#)Makefile	1.12	[03/07/27 22:20:36]
##
## DESCRIPTION: This file contains the source dependencies and build rules for
##              the kernel lists loadable module.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	=============================================
## 02/27/1998	ARTHUR N.	Initial checkin.
## 03/08/1998	ARTHUR N.	Continue even if genksyms complains.
## 03/08/1998	ARTHUR N.	Turn off warnings from genksyms.
## 03/08/1998	ARTHUR N.	Added clean and fresh rules.
## 03/11/1998	ARTHUR N.	Added the installation directory macros.
## 03/11/1998	ARTHUR N.	Make the installation directories.
## 03/24/1998	ARTHUR N.	Display message when installing the module.
## 02/15/2003	ARTHUR N.	Updated to support kernel version 2.4.
## 03/26/2003	ARTHUR N.	Added kernel list test module and date stamp
##				 compilation macro.
## 06/08/2003	ARTHUR N.	Added Rules.mk and Suffix.mk.  Modified
##				 installation rules.  Added cross compiler
##				 support.
## 06/09/2003	ARTHUR N.	Corrected use of genksyms.  RM only defines
##				 the program.
##
##############################################################################
##############################################################################

TOP=..

################################# Rules.mk #################################
include $(TOP)/Rules.mk

-include Override.mk

INC=../include

MV ?= mv
RM ?= rm

# Variables

DATE_STAMP=-DDATE_STAMP="$(date)"

MODVERSION_FLAG ?= -DMODVERSIONS

MODULE_FLAGS    ?= \
	-DMODULE=1 		\
	-DEXPORT_SYMTAB		\
	$(MODVERSION_FLAG)

CFLAGS ?= \
	-g -O2				\
	$(DATE_STAMP)			\
	-I$(KERNEL_INCLUDE_DIR)		\
	$(MODULE_FLAGS)			\
	-D__KERNEL__			\
	$(COPTS)			\
	-I$(INC)

VER_FILE=$(INC)/klists.ver

MODULES=\
	klists.o

INSTALLED_MODULES=\
	$(MODULES:%=$(FS_MOD_INST_DIR)/%)



##
## STANDARD RULES
##

.PHONY: all test install clean fresh modules_install

all: $(MODULES)

test: klist_test.o

install: modules_install

clean:
	@$(RM) -f $(MODULES) $(VER_FILE) *.o *.ver

fresh: clean all



##
## OBJECT RULES
##

klist_test.o: klist_test.c $(VER_FILE)
	@echo Compiling "$@"
	@$(CC_FOR_TARGET) -c $(CFLAGS) klist_test.c

klists.o: kernel_lists.o $(VER_FILE)
	@echo Compiling "$@"
	@$(LD_FOR_TARGET) $(LDFLAGS) -o klists.o -r kernel_lists.o

kernel_lists.o: kernel_lists.c $(INC)/kernel_lists.h $(VER_FILE)
	@echo Compiling "$@"
	@$(CC_FOR_TARGET) -c $(CFLAGS) kernel_lists.c

$(VER_FILE): kernel_lists.c
	@echo "Creating $@"
	@-$(CC_FOR_TARGET) $(CFLAGS) -D__GENKSYMS__ -E "$<" | \
	 $(GENKSYMS) $(GENKSYMS_ARGS) >"$@"



##
## INSTALLATION RULES:
##

modules_install: $(INSTALLED_MODULES)


################################# Suffix.mk #################################
include $(TOP)/Suffix.mk
