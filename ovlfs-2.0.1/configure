#!/bin/sh
##############################################################################
##############################################################################
##
## FILE: configure.sh
##
## FILE VERSION:
##	@(#)configure.sh	1.3	[03/07/27 22:20:38]
##
## DESCRIPTION:
##	This file contains a configuration script for the Overlay Filesystem.
##	It is intended to answer questions needed to compile the filesystem
##	so it will work on the system it is compiled on.
##
## NOTES:
##	- At this time, the CONFIG_FILE created is only there for show.  It
##	  may be safely removed.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	=============================================
## 04/19/1998	ARTHUR N.	Created.
## 06/08/2003	ARTHUR N.	Store make flags in config.make.
## 06/08/2003	ARTHUR N.	Only add config value when it is missing.
##
##############################################################################
##############################################################################

# Variables

IS_MODULE=1
CONFIG_MODVERS=1
CONFIG_FILE=config.current


##
## FUNCTION: check_root_dir
##
##  PURPOSE: Check if this looks like the root directory of the ovlfs source.
##

check_root_dir ()
{
		# check for the configure script and the fs directory

	if [ \( -x configure \) -a \( -d fs \) ]
	then
		:
	else
		echo "WARNING: this does not appear to be the root directory" \
		     "of the ovlfs source."
		echo "         This script must be run from the root " \
		     "directory."
	fi
}



##
## FUNCTION: set_config_value
##
##  PURPOSE: Given the name of a config variable and its value, update the
##           value of the config variable.
##

set_config_value ()
{
	vname="$1"
	shift
	val="$@"

	echo "${vname}=${val}" >> ${CONFIG_FILE}
	eval config_${vname}=\"${val}\"
}



##
## FUNCTION: check_modvers
##
##  PURPOSE: Check for module versions in the current kernel and/or the current
##           source tree for the kernel.
##

check_modvers ()
{
	if tools/modver_test
	then
		set_config_value CONFIG_MODVERS 1
	else
		set_config_value CONFIG_MODVERS 0
	fi
}



##
## FUNCTION: config_module
##
##  PURPOSE: Setup the configuration of the overlay filesystem as a module.
##

config_module ()
{
	if [ "$IS_MODULE" -ne 0 ]
	then
		check_modvers
	fi
}



##
## FUNCTION: update_for_config
##
##  PURPOSE: Update the source tree for the new/current configuration.
##

update_for_config ()
{
		# currently, only one small update to perform, so I will just
		#  do it here for now.  Later, if more is added, I will most
		#  likely make this more easily expandable

	if [ "$config_CONFIG_MODVERS" -ne 0 ]
	then
		modver_flag="-DMODVERSIONS"
	else
		modver_flag="-UMODVERSIONS"
	fi

	if [ ! -f config.make ]
	then
		cp /dev/null config.make
	fi

	awk -v modver_flag=$modver_flag	\
	    -v comment="# updated by configure $(date)"	\
		'BEGIN	{ printed = 0; }
			{ do_print=1 }
		 /^MODVERSION_FLAG/	\
			{
				printf("MODVERSION_FLAG=%s %s\n",
				        modver_flag, comment);
			 	do_print = 0;
				printed = 1;
			}
			{ if ( do_print ) print }
		 END	{
				if ( ! printed ) 
					printf("MODVERSION_FLAG=%s %s\n",
						modver_flag, comment);
			}'	\
		config.make >config.make.new && \
	mv -f config.make config.make.ORIG && \
	mv -f config.make.new config.make
}



##
## FUNCTION: parse_options
##
##  PURPOSE: Parse command line arguments given
##

parse_options ()
{
		# Nothing yet
	:
}



##############################################################################
##############################################################################
##
## MAIN PROGRAM STARTS HERE
##

parse_options "$@"


## See if this looks like the root directory of the source and warn if not

check_root_dir

##
## Start determining the configuration
##

	# move the current config file out of the way

if [ -f "$CONFIG_FILE" ]
then
	mv -f $CONFIG_FILE ${CONFIG_FILE%.current}.previous
fi

## configure module settings

config_module


## Perform the updates now

update_for_config
