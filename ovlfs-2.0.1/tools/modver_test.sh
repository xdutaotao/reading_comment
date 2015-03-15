#!/bin/sh
##############################################################################
##############################################################################
##
## FILE: modver_test.sh
##
## FILE VERSION:
##	@(#)modver_test.sh	1.4	[03/07/27 22:20:38]
##
## DESCRIPTION:
##	This file contains the modver_test program which checks the existing
##	kernel to determine if it uses versioned symbols or not.
##
## NOTES:
##	- It might be a good idea to check for the modversion.h header file
##	  to be sure that the source will be able to compile with MODVERSIONS
##	  set.
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	=============================================
## 2003-06-08	ARTHUR N.	Change the search symbol.
## 2003-06-08	ARTHUR N.	Recognize "is not set" value in kernel
##				 config.  Also allow the user to specify the
##				 path to the kernel source.
## 2003-06-09	ARTHUR N.	Remove path to grep since it is not the same
##				 on all systems.
##
##############################################################################
##############################################################################

PROGNAME="$0"

GREP=grep

## Linux kernel source

DEF_LINUX_SRCDIR=/usr/src/linux
DEF_CONFIG_FILE=.config


## Running kernel information

DEF_PROC_DIR=/proc
DEF_KSYM_FILE=ksyms
DEF_SEARCH_SYM=printk

## Globals

KERNEL_PATH=""
KERNEL_VERS=""


##
## FUNCTION: warning
##
##  PURPOSE: Display the given warning information to standard error.
##

warning ()
{
	echo "warning: $@" >&2
}



##
## FUNCTION: modver_test_src
##
##  PURPOSE: Check the current source tree in order to determine whether the
##           kernel was compiled with module versions or not.
##
## ARGUMENTS:
##	srcdir		- optional; the location of the linux source tree.
##	configfile	- optional; the name of the kernel configuration file.
##

modver_test_src ()
{
	srcdir="${1:-$DEF_LINUX_SRCDIR}"
	configfile="${2:-$DEF_CONFIG_FILE}"

	path="$srcdir/$configfile"

	result=2

	if [ ! -f "$path" ]
	then
		warning "configuration file, $path, not found"
	else
		modver=$($GREP '^CONFIG_MODVERSIONS=' "$path" </dev/null
		         2>/dev/null)

		if [ -z "$modver" ]
		then
			modver=$($GREP '[^#]*CONFIG_MODVERSIONS' "$path"
			         </dev/null 2>/dev/null)
		fi

		if [ -n "$modver" ]
		then
			answer=${modver#*=}

			case "$answer" in
			    y | Y)
				result=0
				;;

			    n | N | *"is not set"*)
				result=1
				;;

			    *)
				warning "CONFIG_MODVERSIONS value in config" \
				        "file, $path, is not 'n' or 'y':" \
					"$answer"
				result=2 ;;
			esac
		else
			warning "CONFIG_MODVERSIONS not found in config" \
			        "file, $path"
		fi
	fi

	return	$result
}



##
## FUNCTION: modver_test_kernel
##
##  PURPOSE: Determine if the running kernel was compiled with versioned
##           symbols.
##
## NOTES:
##	- I am not sure of the correct way to do this, so I just chose a
##	  symbol that I know must be in the kernel and check if it has a
##	  version number attached to it.
##
##	- It is important that the name of the symbol used is not a substring
##	  of any other symbol in the kernel or a mismatch may occur.
##
## ARGUMENTS:
##	procdir		- optional; the location of the ksyms file.
##	ksymfile	- optional; the name of the kernel symbol file.
##	symbol		- optional; the name of the symbol to test.
##

modver_test_kernel ()
{
	procdir="${1:-$DEF_PROC_DIR}"
	ksymfile="${2:-$DEF_KSYM_FILE}"
	symbol="${3:-$DEF_SEARCH_SYM}"

	path="${procdir}/${ksymfile}"

	result=2

	if [ ! -f "$path" ]
	then
		warning "unable to find kernel symbol file at $path"
	else
		match=$($GREP "$symbol" "$path" </dev/null 2>/dev/null)

		if [ -n "$match" ]
		then
				# Check if the symbol is followed by _R in the
				#  matching string

			case "$match" in
				*${symbol}_R*)	result=0 ;;
				*)		result=1 ;;
			esac
		else
			warning "unable to find symbol, $symbol, in kernel" \
			        "symbol file, $path"
		fi
	fi

	return	$result
}



##
## FUNCTION: check_for_modvers
##


check_for_modvers ()
{
	modver_test_kernel
	kern_ind="$?"

	modver_test_src "$KERNEL_PATH"
	src_ind="$?"

		# Use the kernel setting, unless the kernel did not appear to
		#  tell us the answer.

	case "$kern_ind" in
		0)
			if [ "$src_ind" -ne 0 ]
			then
				warning "Active Kernel appears to have been" \
				        "compiled with modversions"
				warning "Kernel source appears not to be" \
				        "configured with modversions"
			fi

			result=0
			;;

		1)
			if [ "$src_ind" -eq 0 ]
			then
				warning "Kernel source appears to be" \
				        "configured with modversions"
				warning "Active kernel appears not to have" \
				        "been compiled with modversions"
			fi

			result=1
			;;

		2)
			result="$src_ind"
			;;
	esac

	return	$result
}



##
## FUNCTION: find_kernel_config
##

find_kernel_config ()
{
	if [ -z "$KERNEL_PATH" ]
	then
		if [ -z "$KERNEL_VERS" ]
		then
			KERNEL_PATH="$DEF_LINUX_SRCDIR"
		else
			case "$KERNEL_VERS" in
			    2.[01]*)
				KERNEL_PATH="$DEF_LINUX_SRCDIR"
				;;

			    2.[3-9]*)
				if [ -z "$KERNEL_VERS" ]
				then
					KERNEL_VERS="$(uname -r)"
				fi

				KERNEL_PATH="/lib/modules/${KERNEL_VERS}/build"
				;;
			esac
		fi
	fi
}



##
## FUNCTION: usage
##

usage ()
{
	printf 'Usage: %s [-h] [-k path] [-v vers]\n' "$PROGNAME"
	printf '\n'
	printf '    -h\t\tDisplay this help.\n'
	printf '    -k\tpath\tPath to kernel sources.\n'
	printf '    -v\tvers\tKernel version.\n'
}



##
## FUNCTION: parse_cmdline
##

parse_cmdline ()
{
	typeset opt

	while getopts 'hk:v:' opt
	do
		case "$opt" in
		    h)
			usage
			exit 0
			;;

		    k)
			KERNEL_PATH="$OPTARG"
			KERNEL_VERS=""
			;;

		    v)
			KERNEL_VERS="$OPTARG"
			KERNEL_PATH=""
			;;

		    *)
			usage >&2
			exit 1
			;;
		esac
	done
}



##############################################################################
##############################################################################
###
### MAIN BODY STARTS HERE
###

parse_cmdline "$@"

check_for_modvers
