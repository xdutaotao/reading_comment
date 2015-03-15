##############################################################################
##############################################################################
##
## FILE: Rules.mk
##
## FILE VERSION:
##	@(#)Rules.mk	1.2	[03/07/27 22:20:30]
##
## NOTES:
##	- In order to build against a specific kernel version other than the
##	  currently running kernel, the following can be used:
##
##		- Set OS_REV to the revision of the kernel against which to
##		  build.  This will still result in the standard locations
##		  being searched, so make sure the end result is correct.
##
##		- Set KERNEL_INCLUDE_DIR to the patch of the include
##		  directory under which the correct kernel headers live.
##
##	- In order to set a variable, any of the following methods can safely
##	  be used:
##
##		- Pass the variable definition to make on the command line;
##		  for example:
##
##			make OS_REV=2.2.0
##
##		- Create a config.make file in the ovlfs top-level source
##		  directory (the one containing this file) with the variable
##		  definitions.
##
##	- The kernel include logic here is intended to be simple, yet
##	  effective with little or no effort.  However, it is certainly not
##	  perfect; the intent is that a configure script or procedure could
##	  be used to give more options and adapt to different environments.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	=============================================
## 2003-06-08	ARTHUR N.	Initial Release
## 2003-06-09	ARTHUR N.	Added program macros.  Changed OS_REV_SPLIT
##				 to work with any type of "extraversion".
##				 RM only specifies the program by default.
##				 Don't depend XX_FOR_TARGET and XX_FOR_BUILD
##				 macros on their standard counterparts.
##
##############################################################################
##############################################################################

# Include a configuration file, but silently ignore it if missing.

sinclude $(TOP)/config.make

###
### Compiler and other program macros
###

# XX_FOR_TARGET - compile for target system (the one that will use the results)
AR_FOR_TARGET  ?= ar
CC_FOR_TARGET  ?= cc
CXX_FOR_TARGET ?= c++
LD_FOR_TARGET  ?= ld

# XX_FOR_BUILD - compile for build system (the one doing the compiling)
AR_FOR_BUILD  ?= ar
CC_FOR_BUILD  ?= cc
CXX_FOR_BUILD ?= c++
LD_FOR_BUILD  ?= ld

GENKSYMS      ?= /sbin/genksyms
GENKSYMS_ARGS ?= -k "$(MAJOR_REV).$(MINOR_REV).$(SUBLEVEL_REV)"

LEX      ?= lex
YACC     ?= yacc

RM      ?= rm
INSTALL ?= install

SED     ?= sed



###
### Installation locations:
###

# Note that prefix is not hard-coded into any results; change it freely during
#  the build and install.  INSTALL_ROOT is just an "alias" to clarify this.

prefix       ?= $(INSTALL_ROOT)

DOC_INST_DIR ?= $(prefix)/usr/share/doc/ovlfs
OVLFS_BINDIR ?= $(prefix)/sbin



###
### Kernel-Version Macros
###

OS_REV ?= $(shell uname -r)

ifeq "$(strip $(OS_REV))" ""
$(error "unable to determine the OS revision. try make OS_REV=x.y.z")
endif

OS_REV_SPLIT = $(shell echo "$(OS_REV)" | "$(SED)" 's,[^0-9], ,g')

MAJOR_REV    = $(word 1,$(OS_REV_SPLIT))
MINOR_REV    = $(word 2,$(OS_REV_SPLIT))
SUBLEVEL_REV = $(word 3,$(OS_REV_SPLIT))


###
### Decide where to find header files based on the kernel version.  All 2.2 and
###  2.4 headers should be found in /lib/modules/$(OS_REV)/build/include while
###  all 2.0 headers are likely in /usr/include.
###
### Note that direct support for the odd-numbered minor revisions is not
###  included; they are assumed to be the same as they next-higher even-
###  numbered version.
###
### Allow the user to set this information in any convenient manner.
###

ifneq "$(MAJOR_REV)" "2"
$(error Only kernel versions 2.x are supported!)
endif

ifeq "$(MINOR_REV)" "0"

KERNEL_INCLUDE_DIR ?= /usr/include
MOD_INST_DIR       ?= $(prefix)/lib/modules/$(OS_REV)
FS_MOD_INST_DIR    ?= $(MOD_INST_DIR)/fs
MISC_MOD_INST_DIR  ?= $(MOD_INST_DIR)/misc

else

# Notice that all modules are installed into the same module directory; this
#  should be appropriate since none of the modules built by this package is
#  "intended" for use outside the ovlfs.  Don't take that to mean they can't
#  be used - if they need to be separate module "packages", then they will get
#  their own homes.

KERNEL_INCLUDE_DIR ?= /lib/modules/$(OS_REV)/build/include
MOD_INST_DIR       ?= $(prefix)/lib/modules/$(OS_REV)
FS_MOD_INST_DIR    ?= $(MOD_INST_DIR)/kernel/fs/ovlfs
MISC_MOD_INST_DIR  ?= $(FS_MOD_INST_DIR)

endif

KERNEL_INCLUDES ?= -I$(KERNEL_INCLUDE_DIR)
