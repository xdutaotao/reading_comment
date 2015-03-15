##############################################################################
##############################################################################
##
## FILE: Makefile
##
## FILE VERSION:
##	@(#)Makefile	1.8	[03/07/27 22:20:32]
##
## DESCRIPTION:
##	- This file contains the dependencies and build rules for the
##	  documentation for the overlay filesystem.
##
## NOTES:
##	- run "make all" to build the overlay filesystem manual page.
##
##	- run "make install" to put the ovl.8 file, and the ovlfs.8 link, into
##	  the directory, $(prefix)/usr/man/man8.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	==============================================
## 03/09/1998	ARTHUR N.	Added to source code control.
## 03/10/1998	ARTHUR N.	Added the Troubleshooting file to the list of
##				 files to be installed.
## 03/11/1998	ARTHUR N.	Make installation directories.
## 03/17/1998	ARTHUR N.	Use a symbolic link for ovlfs.8 and do not
##				 install man page into doc directory; these
##				 changes copied from debian source updates
##				 made by Christoph Lameter.
## 03/29/1998	ARTHUR N.	Added dependency of ovl.8 on Version.
## 06/01/2003	ARTHUR N.	Added ovlfsmount.8 and ovlfstab.5.
## 06/04/2003	ARTHUR N.	Rename ovlfsmount to ovlmount.  Also, add
##				 TOP and include Rules.mk.
## 06/09/2003	ARTHUR N.	Added -f argument to RM.
##
##############################################################################
##############################################################################

TOP=..

include $(TOP)/Rules.mk

##
## MACRO DEFINITIONS
##

GS      ?= gs
INSTALL ?= install
MAN     ?= man
M4      ?= m4
MV      ?= mv
LN      ?= ln

DOC_FILES=\
	Author			\
	Description		\
	Options			\
	Readme			\
	Support			\
	Troubleshooting		\
	Version

SRC_FILES=\
	ovl.m		\
	ovlmount.m	\
	ovlfstab.m

MAN_FILES=\
	ovl.8		\
	ovlmount.8	\
	ovlfstab.5


## Intallation directories:

MAN5_INST_DIR=$(prefix)/usr/man/man5
MAN8_INST_DIR=$(prefix)/usr/man/man8
DOC_INST_DIR=$(prefix)/usr/doc/ovlfs


##############################################################################
##############################################################################
##
## DEFAULT TARGETS
##

.PHONY: all fresh install

all:	$(MAN_FILES)

fresh:	clean all



##############################################################################
##############################################################################
##
## MAN PAGE TARGETS
##

ovl.8: ovl.m Version

ovlmount.8: ovlmount.m Version

ovlfstab.5: ovlfstab.m Version



##############################################################################
##############################################################################
##
## MISCELLANEOUS TARGETS
##
## NOTE: Do not remove the existing manual pages on "clean" because the user
##       may not have m4 to create a new one.
##

.PHONY: clean install

clean:
	@$(RM) -f ovl.8.new ovlmount.8.new ovlfstab.5.new

install:
	@echo "Installing ovl.8 to $(MAN8_INST_DIR)"
	@$(INSTALL) -d -m 755 -o root -g root $(MAN8_INST_DIR)
	@$(INSTALL) -m444 -o root -g root ovl.8 $(MAN8_INST_DIR)/ovl.8
	@$(RM) -f $(MAN8_INST_DIR)/ovlfs.8
	@$(LN) -s ovl.8 $(MAN8_INST_DIR)/ovlfs.8
	@:
	@echo "Installing ovlmount.8 to $(MAN8_INST_DIR)"
	@$(INSTALL) -d -m 755 -o root -g root $(MAN8_INST_DIR)
	@$(INSTALL) -m444 ovlmount.8 $(MAN8_INST_DIR)/ovlmount.8
	@:
	@echo "Installing ovlfstab.5 to $(MAN5_INST_DIR)"
	@$(INSTALL) -d -m 755 -o root -g root $(MAN5_INST_DIR)
	@$(INSTALL) -m444 ovlfstab.5 $(MAN5_INST_DIR)/ovlfstab.5
	@:
	@echo "Installing $(DOC_FILES) to $(DOC_INST_DIR)"
	@$(INSTALL) -d -m 755 -o root -g root $(DOC_INST_DIR)
	@$(INSTALL) -m 444 -o root -g root $(DOC_FILES) $(DOC_INST_DIR)/



##############################################################################
##############################################################################
##
## Pattern Rules
##

%.8: %.m
	@echo "Compiling $*.8"
	@$(M4) "$<" >"$@.new"
	@$(MV) "$@.new" "$@"

%.5: %.m
	@echo "Compiling $@"
	@$(M4) "$<" >"$@.new"
	@$(MV) "$@.new" "$@"

%.ps: %.8
	@echo "Creating $@"
	@$(MAN) -t -l "$<" >"$@"

%.pdf: %.ps
	@echo "Creating $@"
	@echo quit | \
		$(GS) -sDEVICE=pdfwrite -sOutputFile="$@" -q -dNOPAUSE "$<" -
