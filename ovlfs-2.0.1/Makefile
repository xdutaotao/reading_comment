##############################################################################
##############################################################################
##
## FILE: Makefile
##
## FILE VERSION:
##	@(#)Makefile	1.8	[03/07/27 22:20:30]
##
## DESCRIPTION:
##	- This file contains the rules to build the overlay filesystem
##	  module.
##
## NOTES:
##	- if a package is needed for installing the software, set the prefix
##	  make variable to the name of an existing directory in order to
##	  install all files into that directory tree.  Then the makepkg, or
##	  equivalent command may be used to create a package.
##
##	- run "make all" to build the overlay filesystem module and the
##	  needed kernel lists module.
##
##	- run "make install" to install the modules:
##
##		$(prefix)/lib/module/<KERNEL VERSION>/fs/ovl.o
##		$(prefix)/lib/module/<KERNEL VERSION>/misc/klists.o
##
##	- run "make install_doc" to install the manual page and to install
##	  documents to the directory:
##
##		$(prefix)/usr/doc/ovlfs
##
##	- run "make clean" to cleanup.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	==============================================
## 03/08/1998	ARTHUR N.	Added to source code control.
## 03/11/1998	ARTHUR N.	Added the install_doc target.
## 03/11/1998	ARTHUR N.	Added more targets.
## 03/11/1998	ARTHUR N.	Added MAKEFLAGS to recursive makes.
## 04/19/1998	ARTHUR N.	Added the prepdist target which prepares the
##				 source tree, straight out of the source code
##				 control database, for distribution.
## 04/22/1998	ARTHUR N.	Removed MAKEFLAGS to recursive makes.  Argh,
##				 why does make have to be so stupid.
## 06/05/2003	ARTHUR N.	Added file_io, lib, mount, and third-party
##				 directories, and added pattern rules for
##				 the subdirectory targets.
## 06/09/2003	ARTHUR N.	Run prepdist in mount.
##
##############################################################################
##############################################################################

TOP=.

################################# Rules.mk #################################
include $(TOP)/Rules.mk

-include Override.mk


##
##
##

SUBDIRS=\
	lists		\
	file_io		\
	fs		\
	doc		\
	lib		\
	mount		\
	third_party	\
	tools

INSTALL_SUBDIRS=\
	lists		\
	file_io		\
	fs		\
	doc		\
	mount		\
	third_party	\
	tools

LOG_FILES=\
	All.log \
	Doc.log \
	Install.log \
	Instdoc.log

CONFIG_FILES=\
	config.current \
	config.previous


.PHONY: all klists ovl doc tools install clean fresh \
        install_klists install_ovl install_doc install_doc_subdir \
        fresh_klists fresh_ovl \
        clean_klists clean_ovl clean_tools

.PHONY: $(SUBDIRS) $(SUBDIRS:%=%-clean)



###
### STANDARD TARGETS
###

all: $(SUBDIRS)

clean: $(SUBDIRS:%=%-clean)

install: $(INSTALL_SUBDIRS:%=%-install)



###
### SUB-DIRECTORY DEPENDENCIES
###
### NOTES:
###	- The <subdir>-* targets do not use dependencies; they are "direct",
###	  one-subdirectory-only targets.
###
###	- Silly make assumes that we don't want implicit pattern-rule searches
###	  for phony targets.  Ugh.  ? u and me.
###

lists:

doc:

file_io:

fs: lists file_io doc

lib:

mount: lib

third_party:

tools: fs lists


###   prepdist: setup for distribution; users should never need to do this

prepdist:
	@echo "preparing source tree for distribution"
	@/bin/chmod 755 build
	@cd tools && $(MAKE) prepdist
	@/bin/chmod 555 configure  # the source is in tools
	@cd mount && $(MAKE) prepdist

deprecated-install: install_klists install_ovl

deprecated-clean: clean_klists clean_ovl clean_tools clean_local

fresh: fresh_klists fresh_ovl



##
## INSTALLATION
##

install_doc: install_doc_subdir
	@echo "Installing COPYING to $(DOC_INST_DIR)/"
	@$(INSTALL) -m 444 COPYING $(DOC_INST_DIR)/COPYING



##
## CLEANUP
##

clean_local:
	@/bin/rm -f $(LOG_FILES) $(CONFIG_FILES)



##
## SUBDIRECTORY TARGETS
##
##	These are not as bad as they may look ;-).  To perform a make of any
##	target in a subdirectory from the top-level directory, use the
##	following target syntax:
##
##		<subdir_name>-<target>
##
##	For example, to make the target, all, in the fs directory, use the
##	following:
##
##		fs-all
##

$(SUBDIRS):
	@cd "$@" && $(MAKE)

$(SUBDIRS:%=%-all):
	@subdir="$(@:%-all=%)" ; cd "$$subdir" && $(MAKE) all

$(SUBDIRS:%=%-install):
	@subdir="$(@:%-install=%)" ; cd "$$subdir" && $(MAKE) install

$(SUBDIRS:%=%-clean):
	@subdir="$(@:%-clean=%)" ; cd "$$subdir" && $(MAKE) clean

$(SUBDIRS:%=%-fresh):
	@subdir="$(@:%-fresh=%)" ; cd "$$subdir" && $(MAKE) fresh



###
### Pattern rule to match all subdir targets in the form <subdir>-<target>.
###
###	Yes, this is ugly.  But, it beats having to maintain the list of sub-
###	directories in more than one place.
###

$(foreach dir,$(SUBDIRS),$(dir)-%):
	@maintgt="$@";						\
	 subdir=$${maintgt%%-*}; target=$${maintgt#*-};		\
	 [ -n "$$subdir" ] && [ -n "$$target" ] &&		\
	 $(MAKE) -C "$$subdir" "$$target"


###
### DEPRECATED WARNING
###
###	The following are deprecated.  Please do not rely on them; they will be
###	removed soon.
###

install_klists:
	@cd lists && $(MAKE) install

install_ovl:
	@cd fs && $(MAKE) install

install_doc_subdir:
	@cd doc && $(MAKE) install


fresh_klists:
	@cd lists && $(MAKE) fresh

fresh_ovl:
	@cd lists && $(MAKE) fresh


clean_klists:
	@cd lists && $(MAKE) clean

clean_ovl:
	@cd fs && $(MAKE) clean

clean_tools:
	@cd tools && $(MAKE) clean
