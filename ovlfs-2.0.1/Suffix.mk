##############################################################################
##############################################################################
##
## FILE: Suffix.mk
##
## FILE VERSION:
##	@(#)Suffix.mk	1.2	[03/07/27 22:20:31]
##
## DESCRIPTION:
##	Standar suffix and pattern rules.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	=============================================
## 2003-06-08	ARTHUR N.	Initial Release
## 2003-06-08	ARTHUR N.	Tell install when it is creating directories.
##
##############################################################################
##############################################################################

###
### STANDARD PATTERN RULES
###

(%.o): %.c
	@echo Compiling "$@($*.o)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c "$<" -o "$%"
	@$(AR) rv "$@" "$%"
	@$(RM) -f "$%"

(%.o): %.C
	@echo Compiling "$@($*.o)"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c "$<" -o "$%"
	@$(AR) rv "$@" "$%"
	@$(RM) -f "$%"


$(OVLFS_BINDIR) $(FS_MOD_INST_DIR):
	@$(INSTALL) -d -m 755 -o root -g root "$@"

$(FS_MOD_INST_DIR)/%: % $(FS_MOD_INST_DIR)
	@echo "Installing $@"
	@$(INSTALL) -m 444 -o root -g root "$<" "$@"

$(OVLFS_BINDIR)/%: % $(OVLFS_BINDIR)
	@echo "Installing $@"
	@$(INSTALL) -m 555 -o root -g root "$<" "$@"


# Prevent duplicate rule warnings when the two module directories are the same.

ifneq "$(FS_MOD_INST_DIR)" "$(MISC_MOD_INST_DIR)"

$(MISC_MOD_INST_DIR):
	@$(INSTALL) -d -m 755 -o root -g root "$@"

$(MISC_MOD_INST_DIR)/%: % $(MISC_MOD_INST_DIR)
	@echo "Installing $@"
	@$(INSTALL) -m 444 -o root -g root "$<" "$@"

endif
