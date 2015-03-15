##############################################################################
##############################################################################
##
## COPYRIGHT (C) 1998 By Arthur Naseef
##
## This file is covered under the GNU General Public License Version 2.  For
##  more information, see the file COPYING.
##
##
## FILE: Makefile
##
## SCCS ID: @(#)Makefile	1.18	[03/07/27 22:20:33]
##
## DESCRIPTION:
##	- This file contains the dependencies and build rules for the overlay
##	  filesystem.
##
## NOTES:
##	- run "make all" to build the overlay filesystem module.
##
##	- run "make install" to put the ovl.o file into the directory,
##	  $(prefix)/lib/module/<KERNEL VERSION>/fs.
##
##	- The user may use the COPTS makefile variable to add compiler
##	  options to the command line (before all other options).
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	==============================================
## 02/26/1998	ARTHUR N.	Added to source code control.
## 02/27/1998	ARTHUR N.	Updated the build rules for dump_inode.o
## 03/03/1998	ARTHUR N.	Added the precompiler suffix rule.
## 03/05/1998	ARTHUR N.	Added the ovl_alloc.c file.
## 03/06/1998	ARTHUR N.	Improved the install actions, and added macros
##				 for module installation directories.  Also,
##				 improved comments.
## 03/08/1998	ARTHUR N.	Added the PROTECT_MMAP_WRITE flag.
## 03/08/1998	ARTHUR N.	Continue even if genksyms complains, and turn
##				 off the warnings from that program.
## 03/09/1998	ARTHUR N.	Added the copyright notice to the file.
## 03/09/1998	ARTHUR N.	Set the default for VERIFY_MALLOC to 0.
## 03/11/1998	ARTHUR N.	Make the installation directories.
## 03/29/1998	ARTHUR N.	Added the release number and fixed the install
##				 rule for the dump_inode.o module.
## 04/22/1998	ARTHUR N.	Added configurable variable section.
## 02/08/1999	ARTHUR N.	Added more compile flags for reference.
## 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
## 02/24/2003	ARTHUR N.	Don't define DEBUG_FLAGS if they are already
##				 defined.  Also, added ovl_dops.c.
## 03/11/2003	ARTHUR N.	Added ovl_misc.c and ovl_ref.c.
## 06/07/2003	ARTHUR N.	Release cleanup.
## 06/09/2003	ARTHUR N.	Corrected genksyms use.
##
##############################################################################
##############################################################################

TOP=..

include $(TOP)/Rules.mk

-include Override.mk

ifndef KERNEL_INCLUDES
KERNEL_INCLUDES=-I/lib/modules/$(OS_REV)/build/include
endif


###
### MACRO DEFINITIONS
###

MV ?= mv
RM ?= rm

## Release Number

OVL_RELEASE=$(shell /usr/bin/awk '{print $1; exit}' $(TOP)/doc/Version)

## Intallation directories:

INC=../include

MISC_FILES=\
	IDEAS \
	NOTES \
	TODO

DEBUG_FILES=\
	ovl_debug.c

DEBUG_OBJ=\
	ovl_debug.o

SRC_FILES=\
	$(DEBUG_FILES)	\
	ovl_main.c	\
	ovl_alloc.c	\
	ovl_dcache.c	\
	ovl_dir.c	\
	ovl_dops.c	\
	ovl_file.c	\
	ovl_ino.c	\
	ovl_kern.c	\
	ovl_misc.c	\
	ovl_ref.c	\
	ovl_stg.c

OBJ_FILES=\
	$(DEBUG_OBJ)	\
	ovl_main.o	\
	ovl_alloc.o	\
	ovl_dcache.o	\
	ovl_dir.o	\
	ovl_dops.o	\
	ovl_file.o	\
	ovl_ino.o	\
	ovl_kern.o	\
	ovl_misc.o	\
	ovl_ref.o	\
	ovl_stg.o

OVL_OBJ=ovl.o

VER_FILE=ovlfs.ver

INSTALL_OBJ=ovl.o

MODULES=\
	ovlfs.o

INSTALLED_MODULES=\
	$(FS_MOD_INST_DIR)/ovl.o


##############################################################################
##############################################################################
##
## COMPILER OPTIONS
##

## The following defines may be used on the compile line:
##
##	MODULE		- Set to 1 in order to compile the ovlfs.o file as
##			  a loadable module.  If this is not set, the file
##			  must be linked into the kernel to be used.
##
##	KDEBUG		- Set to turn on debug messages; set to > 1 to get
##			  more verbose error messages (range is 1 to 3).
##
##	KDEBUG_CALLS	- Display debug messages at the start of each ovlfs
##			  function called.
##
##	KDEBUG_IREF	- Debug the marking and putting of inodes by the ovlfs.
##			  If non-zero, every time the ovlfs increases or
##			  decreases an inode's use count, a kernel message is
##			  printed.
##
##	PRINTK_USE_DEBUG
##			- If set to a non-zero value, all of the ovlfs'
##			  debugging and error messages are printed to the debug
##			  file instead of using printk.
##
##	OVLFS_DEBUG_LEVEL
##			- The default level of debug messages to include in the
##			  debug file.  The debug messages must be compiled in
##			  by use of the appropriate compiler flags, such as
##			  KDEBUG.
##
##	USE_MAGIC	- Set to non-zero value to turn on the use of the
##			  "magic" and "magicr" directory entries which allow
##			  the user to work with the storage filesystem
##			  directory and the base filesystem directory,
##			  respectively.  If set, the magic filesystem options
##			  may be used to modify the settings on a mounted-
##			  filesystem basis.
##
##	STORE_REF_INODES
##			- Set to store reference inodes when mounting and
##			  getting inodes.  If set, the storage and overlay
##			  roots will be impervious to chroot() calls,
##			  otherwise the overlay and storage roots will be
##			  relative to the current root directory.  For better
##			  security (and most likely the expected behavior),
##			  set this.
##
##	STORE_MAPPINGS	- Turn on to allow inode mappings to be stored in the
##			  storage file.  If this is set, the "storemaps" and
##			  related fs options may be used to set the storage of
##			  inode maps on a per-mounted fs basis.
##
##	VERIFY_MALLOC	- Set to 1 to turn on checking of total memory
##			  allocated and freed by the overlay filesystem (except
##			  the kernel lists).  In addition, if this value is
##			  set to an integer greater than 1, check the size of
##			  memory being freed.
##
##	DEBUG_BUSY_OVLFS
##			- Add debug functions which can be used to track down
##			  the cause of a busy Overlay Filesystem.
##
##
## NOTES:
##	- The -O2 is necessary unless the kernel is compiled with debugging;
##	  without it, external inline functions defined in the kernel header
##	  files will be missing in the ovlfs.o file and, as a result, the
##	  file will be unusable.


### OVLFS compile flags:

DATE_STAMP=-DDATE_STAMP="`date`"

ifndef DEBUG_FLAGS
DEBUG_FLAGS=-DKDEBUG=1			\
            -DKDEBUG_CALLS=0		\
            -DKDEBUG_IREF=1		\
            -DKDEBUG_SEMA=0		\
            -DPRINTK_USE_DEBUG=0	\
            -DOVLFS_DEBUG_LEVEL=1	\
            -DVERIFY_MALLOC=0		\
            -DDEBUG_CHECK_STACK=0
endif

OVLFS_FLAGS=$(DEBUG_FLAGS) $(DATE_STAMP) \
            -DUSE_MAGIC=1 \
            -DSTORE_REF_INODES=1 \
            -DOVLFS_BANNER=1 \
            -DSTORE_MAPPINGS \
            -DPROTECT_MMAP_WRITE=0 \
            -DOVLFS_RELEASE=$(OVL_RELEASE)

### Kernel flags:

MODULE_FLAGS=-DMODULE=1 \
             $(MODVERSION_FLAG) \
             -DEXPORT_SYMTAB

KERNEL_FLAGS=\
	$(MODULE_FLAGS)				\
	$(KERNEL_INCLUDES)			\
	-D__KERNEL__


### CFLAGS
DEBUG_CFLAGS=-g -fno-omit-frame-pointer
CFLAGS=\
	$(COPTS)	\
	-Wall -O2	\
	$(DEBUG_CFLAGS)	\
	$(KERNEL_FLAGS)	\
	$(OVLFS_FLAGS)	\
	-I$(INC)



##############################################################################
##############################################################################
##
## DEFAULT TARGETS
##

.PHONY: all bak fresh modules_install install


all:	$(OVL_OBJ)

fresh:	clean all



##############################################################################
##############################################################################
##
## OBJECT TARGETS
##

$(VER_FILE): ovl_main.c
	@echo "Compiling $@"
	@-$(CC_FOR_TARGET) $(CFLAGS) -D__GENKSYMS__ -E "$<" | \
	 $(GENKSYMS) $(GENKSYMS_ARGS) >"$@"

$(OVL_OBJ): $(OBJ_FILES) $(VER_FILE)
	@echo Linking "$@"
	@$(LD_FOR_TARGET) $(LDFLAGS) -o $(OVL_OBJ) -r $(OBJ_FILES)

dump_inode.o: dump_inode.c $(INC)/dumpi.h $(INC)/ovl_fs.h
	@echo Compiling "$@"
	@$(CC_FOR_TARGET) $(CFLAGS) -c dump_inode.c -o dump_inode0.o
	@$(LD_FOR_TARGET) $(LDFLAGS) -o dump_inode.o -r dump_inode0.o ovl_sys.o
	@$(RM) -f dump_inode0.o



##############################################################################
##############################################################################
##
## MISCELLANEOUS TARGETS
##

clean:
	@$(RM) -f $(OBJ_FILES) $(OVL_OBJ) $(VER_FILE)

bak:
	@ARCHIVE_NAME=OVLFS bak $(SRC_FILES) $(MISC_FILES) Makefile



##############################################################################
##############################################################################
##
## INSTALLATION RULES
##

.PHONY: install modules_install

install:	modules_install

modules_install: $(INSTALLED_MODULES)



##############################################################################
##############################################################################
##
## OBJECT FILE DEPENDENCIES
##

KLIST_HDRS=$(INC)/kernel_lists.h $(INC)/klists.ver

ovl_main.o: ovl_main.c $(INC)/ovl_fs.h $(VER_FILE) $(KLIST_HDRS)

ovl_ino.o: ovl_ino.c $(INC)/ovl_fs.h $(VER_FILE) $(KLIST_HDRS)

ovl_dir.o: ovl_dir.c $(INC)/ovl_fs.h $(VER_FILE) $(KLIST_HDRS)

ovl_file.o: ovl_file.c $(INC)/ovl_fs.h $(VER_FILE) $(KLIST_HDRS)

ovl_kern.o: ovl_kern.c $(INC)/ovl_fs.h $(VER_FILE) $(KLIST_HDRS)

ovl_stg.o: ovl_stg.c $(INC)/ovl_fs.h $(VER_FILE) $(KLIST_HDRS)

ovl_debug.o: ovl_debug.c $(INC)/ovl_fs.h $(VER_FILE) $(KLIST_HDRS)

ovl_sys.o: ovl_sys.c $(INC)/ovl_fs.h $(VER_FILE) $(KLIST_HDRS)



##############################################################################
##############################################################################
##
## Suffixes
##

include $(TOP)/Suffix.mk

.SUFFIXES: .c .i .o

.c.o:
	@echo "Compiling $*.o"
	@$(CC_FOR_TARGET) $(CFLAGS) -c $*.c

.c.i:
	@echo "Compiling $*.i"
	@$(CC_FOR_TARGET) $(CFLAGS) -E $*.c >$*.i
