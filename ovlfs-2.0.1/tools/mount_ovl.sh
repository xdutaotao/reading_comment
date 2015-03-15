#!/bin/sh
##############################################################################
##############################################################################
##
## COPYRIGHT (C) 1998 By Arthur Naseef
##
## This file is covered under the GNU General Public License Version 2.  For
##  more information, see the file COPYING.
##
##
## FILE: mount_ovl.sh
##
## SCCS ID: @(#) mount_ovl.sh 1.1
##
## DESCRIPTION:
##	This file contains the shell script for the mount_ovl program which
##	prompts the user for information needed to mount an overlay
##	filesystem and displays or executes the appropriate mount command.
##
## NOTES:
##	- Every option has two main states, default or user-defined.  For
##	  flag options, this implies that there are three states: on, off,
##	  or default.  The distinction is important because the filesystem
##	  driver may be compiled with different defaults, and the user should
##	  only need to specify values that override defaults or are important
##	  to the user's self.
##
##
## REVISION HISTORY:
##
## DATE		AUTHOR		DESCRIPTION
## ==========	===============	=============================================
## 03/24/1998	ARTHUR N.	Initial Release.
##
##############################################################################
##############################################################################

## Program locations

AWK=/usr/bin/awk
CLEAR=/usr/bin/clear
DMESG=/bin/dmesg
GREP=/usr/bin/grep
MOUNT=/bin/mount
PRINTF=/usr/bin/printf
TAIL=/usr/bin/tail

DEFAULT_BASE=/base
DEFAULT_STG=/stg
DEFAULT_MNT=./mnt

DEFAULT_NUM_KERN_MSG=5

FLAG_ON_OPTIONS="basemagic basemap maxmem ovlmagic showmagic stgmap storage
                 storemaps xmnt"

FLAG_OFF_OPTIONS="hidemagic nobasemagic nobasemap noovlmagic nostgmap
                  nostorage nostoremaps noxmnt"

VAR_OPTIONS="bmagic smagic"

OPTION_LIST="$(echo $FLAG_ON_OPTIONS $VAR_OPTIONS | sort)"


## Menu Constants

NUM_MENU_ENTRY=8
MENU_ENTRY_1_STR="Set the root of the base filesystem"
MENU_ENTRY_2_STR="Set the root of the storage filesystem"
MENU_ENTRY_3_STR="Set the mount point"
MENU_ENTRY_4_STR="Set the storage file"
MENU_ENTRY_5_STR="Display mount options"    # Displays verbose state of options
MENU_ENTRY_6_STR="Set mount options"
MENU_ENTRY_7_STR="Display mount command"
MENU_ENTRY_8_STR="Execute mount command"

MENU_ENTRY_1_CMD='do_set_base_root'
MENU_ENTRY_2_CMD='do_set_stg_root'
MENU_ENTRY_3_CMD='do_set_mount_point'
MENU_ENTRY_4_CMD='do_set_stg_file'
MENU_ENTRY_5_CMD='do_display_options'
MENU_ENTRY_6_CMD='do_set_options'
MENU_ENTRY_7_CMD='do_display_mount'
MENU_ENTRY_8_CMD='do_execute_mount'

## Variables

# Execution options

DO_CLEAR=1
DO_RUN_MENU=${DO_RUN_MENU:-1}

# Overlay Filesystem mount parameters

MNT_POINT=""
BASE_ROOT=""		# Root of the base (overlayed) filesystem
STG_ROOT=""		# Root of the storage (overlaying) filesystem
STG_FILE=""		# Storage file
USER_OPTS=""

OPTIONS=""

NUM_KERN_MSG=$DEFAULT_NUM_KERN_MSG

result=""		# temporary result



##
## FUNCTION: clear_scr
##

clear_scr ()
{
	if [ "$DO_CLEAR" -ne 0 ]
	then
		$CLEAR
	else
		$PRINTF '\n\n\n'
	fi
}



##
## FUNCTION: read_str
##
##  PURPOSE: Prompt the user for input and return the response.  The response
##           is placed in the shell variable result.
##

read_str ()
{
	MSG="$@"
	$AWK -vmsg="$MSG" 'BEGIN { printf("%s", msg); exit; }'
	read result

		# Remove all leading and trailing whitespace:

	result=$(echo $result)
}



##
## FUNCTION: old_style_prompts
##
##  PURPOSE: Provide the original prompting style for information needed to
##           mount the filesystem.
##

old_style_prompts ()
{
		## Read the base filesystem root

	read_str "Enter the directory to overlay (default $DEFAULT_BASE): "
	BASE_ROOT=$result

	if [ -z "$BASE_ROOT" ]; then BASE_ROOT="$DEFAULT_BASE"; fi


		## Read the storage filesystem root

	read_str "Enter the directory for storage (default $DEFAULT_STG): "
	STG_ROOT=$result

	if [ -z "$STG_ROOT" ]; then STG_ROOT="$DEFAULT_STG"; fi


		## Read the mount point for the overlay filesystem

	read_str "Enter the mount point (default $DEFAULT_MNT): "
	MNT_POINT=$result

	if [ -z "$MNT_POINT" ]; then MNT_POINT="$DEFAULT_MNT"; fi


		## Read the mount point for the overlay filesystem

	read_str "Enter the storage file: "
	STG_FILE=$result


		## Read any extra options

	read_str "Enter additional options, separated by commas: "
	USER_OPTS=$result
}



##
## FUNCTION: add_option
##

add_option ()
{
	zoptvar="$1"
	zopts="$2"

	eval zoptval=\$$zoptvar

	if [ -n "$zoptval" ]
	then
		eval $zoptvar="$zoptval,$zopts"
	else
		eval $zoptvar="$zopts"
	fi
}



##
## FUNCTION: prepare_options
##

prepare_options ()
{
	OPTIONS=""

	if [ -n "$BASE_ROOT" ]
	then
		add_option OPTIONS "root=${BASE_ROOT}"
	fi

	if [ -n "$STG_ROOT" ]
	then
		add_option OPTIONS "storage_root=${STG_ROOT}"
	fi

	if [ -n "$STG_FILE" ]
	then
		add_option OPTIONS "stg_file=${STG_FILE}"
	fi

	if [ -n "$USER_OPTS" ]
	then
		add_option OPTIONS "$USER_OPTS"
	fi
}



##
## FUNCTION: is_in_list
##

is_in_list ()
{
	LIST="$1"
	ELE="$2"

	set -- $LIST

	while [ "$#" -gt 0  -a  "$ELE" != "$1" ]
	do
		shift
	done

	if [ "$#" -gt 0 ]
	then
		return	0
	else
		return	1
	fi
}



##
## FUNCTION: parse_mnt_opt
##

parse_mnt_opt ()
{
		# Remove all of the option from the equal sign to the end

	opt_name=${1%%=*}

		# Check if any equal sign was found

	if [ "$opt_name" = "$1" ]
	then
			# option

		if is_in_list "$FLAG_ON_OPTIONS" "$opt_name"
		then
			eval $opt_name="on"
		elif is_in_list "$FLAG_OFF_OPTIONS" "$opt_name"
		then
				# Remove the leading "no"
			flag=${opt_name#no}

			if [ "$flag" = "$opt_name" ]
			then
					# Must be hidemagic
				showmagic=off
			else
				eval $flag="off"
			fi
		else
			if [ -n "$MISC_OPTS" ]
			then
				MISC_OPTS="$MISC_OPTS,$opt_name"
			else
				MISC_OPTS="$opt_name"
			fi
		fi
	else
			# option=value

		value=${1#${opt_name}=}

		if is_in_list "$VAR_OPTIONS" "$opt_name"
		then
			eval $opt_name=$value
		else
			if [ -n "$MISC_OPTS" ]
			then
				MISC_OPTS="$MISC_OPTS,$opt_name=$value"
			else
				MISC_OPTS="$opt_name=$value"
			fi
		fi
	fi
}



##
## FUNCTION: prepare_mnt_opts
##

prepare_mnt_opts ()
{
		# Initialize the list as unset

	MISC_OPTS=""

	for var in $OPTION_LIST
	do
		eval $var=\"default or not set\"
	done


		# Parse the current options

	OIFS="$IFS"
	IFS=,

	set -- $USER_OPTS

	IFS="$OIFS"

	for opt
	do
		parse_mnt_opt "$opt"
	done
}



##
## FUNCTION: build_option_str
##
##  PURPOSE: Given that options have been prepared by prepare_mnt_opts() and
##           potentially modified, rebuild the OPTION string.
##

build_option_str ()
{
	USER_OPTS=""

		# Loop through all of the standard options and determine if
		#  the option is set

	for var in $FLAG_ON_OPTIONS
	do
		eval value=\$$var

		if [ "$value" != "default or not set" ]
		then
			if [ "$value" = "on" ]
			then
				add_option USER_OPTS "$var"
			else
				if [ "$var" = "showmagic" ]
				then
					add_option USER_OPTS "hidemagic"
				else
					add_option USER_OPTS "no$var"
				fi
			fi
		fi
	done

	for var in $VAR_OPTIONS
	do
		eval value=\$$var

		if [ "$value" != "default or not set" ]
		then
			add_option USER_OPTS "$var=$value"
		fi
	done

	if [ -n "$MISC_OPTS" ]
	then
		add_option USER_OPTS "$MISC_OPTS"
	fi
}



##
## FUNCTION: do_read_option
##

do_read_option ()
{
	VARNAME="$1"
	VALUE="$2"

	if [ -n "$VALUE" ]
	then
		echo "The current value of $VARNAME is $VALUE"
	else
		echo "$VARNAME is not set or set to the default"
	fi

		# If the option is a string option, just read the value

	if is_in_list "$VAR_OPTIONS" "$VARNAME"
	then
		read_str "Please enter a new value (return for default): "

		eval $VARNAME=$result
	else
			# This is a flag option, ask for on, off, or default

		flag=1

		while [ "$flag" -ne 0 ]
		do
			read_str "Please enter on, off, or return for default: "

			case "$result" in
				on | off)
					flag=0
					eval $VARNAME="$result"
					;;

				"")
					flag=0
					eval $VARNAME=\"default or not set\"
					;;

				*)
					echo "Invalid response, $result," \
					     "please try again"
					;;
			esac
		done
	fi
}



##
## FUNCTION: do_display_options
## FUNCTION: do_set_options
## FUNCTION: do_display_mount
## FUNCTION: do_execute_mount
## FUNCTION: do_set_mount_point
## FUNCTION: do_set_base_root
## FUNCTION: do_set_storage_root
## FUNCTION: do_set_stg_file
##

do_display_options ()
{
		# Parse the options

	prepare_mnt_opts


		# Now, display all of the values

	for var in $OPTION_LIST
	do
		eval value="\$$var"
		$PRINTF '%20s = %s\n' "$var" "$value"
	done

	if [ -n "$MISC_OPTS" ]
	then
		$PRINTF '%20s = %s\n' "MISCELLANEOUS" "$MISC_OPTS"
	fi

	echo
	read_str "Press return"
}


do_set_options ()
{
		# Parse the options

	prepare_mnt_opts

	finished=0
	set_err_msg=""

	while [ "$finished" -eq 0 ]
	do
		clear_scr
		$PRINTF '%45s\n\n' "SET OPTIONS"

		x=0
		for var in $OPTION_LIST
		do
			x=$((x+1))
			eval value="\$$var"
			$PRINTF '%2d. %-15s [%s]\n' "$x" "$var" "$value"

			eval value$x="\$$var"	# remember the name and value
			eval var$x=$var
		done

		x=$((x+1))
		misc=$x
		$PRINTF '%2d. %-15s [%s]\n' "$x" "miscellaneous" "$MISC_OPTS"

		x=$((x+1))
		clear_misc=$x
		$PRINTF '%2d. %-15s\n' "$x" "clear miscellaneous"

		echo " Q. Return"
		echo

		if [ -n "$set_err_msg" ]
		then
			echo "$set_err_msg"
		fi

		read_str "Enter option to modify: "

		set_err_msg=""
		case "$result" in
			$misc)
				echo
				read_str "Enter the miscellaneous option: "

				if [ -n "$result" ]
				then
					if [ -n "$MISC_OPTS" ]
					then
						MISC_OPTS="$MISC_OPTS,$result"
					else
						MISC_OPTS="$result"
					fi
				fi
				;;

			$clear_misc)
				MISC_OPTS=""
				;;

			[0-9]*)
				if [ "$result" -ge 1  -a "$result" -le "$x" ]
				then
					eval var=\$var$result
					eval value=\$value$result

					echo
					do_read_option "$var" "$value"
				else
					a="between 1 and $x"
					set_err_msg="Please enter a value $a"
				fi
				;;

			[qQ]*)
				finished=1
				;;
		esac
	done

	build_option_str
}

do_display_mount ()
{
#	OPTIONS="root=${BASE_ROOT},storage_root=${STG_ROOT},stg_file=${STG_FILE}"
	prepare_options

	if [ -n "$OPTIONS" ]
	then
		echo $MOUNT -t ovl ovl ${MNT_POINT} -o $OPTIONS
	else
		echo $MOUNT -t ovl ovl ${MNT_POINT}
	fi

	echo
	read_str "Press return"
}

do_execute_mount ()
{
	prepare_options

	perform_mount

	echo
	read_str "Press return"
}

do_set_mount_point ()
{
	read_str "Enter the mount point: "
	MNT_POINT="$result"
}

do_set_base_root ()
{
	read_str "Enter the root of the base filesystem: "
	BASE_ROOT="$result"
}

do_set_stg_root ()
{
	read_str "Enter the root of the storage filesystem: "
	STG_ROOT="$result"
}

do_set_stg_file ()
{
	read_str "Enter the storage file to use: "
	STG_FILE="$result"
}



##
## FUNCTION: perform_menu_op
##
##  PURPOSE: Perform the selected menu operation.
##

perform_menu_op ()
{
	ENT_NO="$1"

	echo

	eval \$MENU_ENTRY_${ENT_NO}_CMD
}



##
## FUNCTION: display_current_status
##
##  PURPOSE: Display the current status information.
##

disp_value () { if [ -z "$*" ]; then echo "not defined"; else echo "= $*"; fi }

display_current_status ()
{
	$PRINTF '%47s\n\n' "CURRENT STATUS"
	$PRINTF '\t   Base Root %s\n' "$(disp_value $BASE_ROOT)"
	$PRINTF '\tStorage Root %s\n' "$(disp_value $STG_ROOT)"
	$PRINTF '\t Mount Point %s\n' "$(disp_value $MNT_POINT)"
	$PRINTF '\tStorage File %s\n' "$(disp_value $STG_FILE)"
	$PRINTF '\t     Options %s\n' "$(disp_value $USER_OPTS)"

	echo
}



##
## FUNCTION: display_menu
##

display_menu ()
{
	clear_scr

	display_current_status

	$PRINTF '%44s\n\n' "MAIN MENU"

	x=1

	while [ "$x" -le "$NUM_MENU_ENTRY" ]
	do
		str="$(eval echo \$"MENU_ENTRY_${x}_STR")"
		$PRINTF '%d) %s\n' $x "$str"
		x=$((x + 1))
	done
}



##
## FUNCTION: run_menu
##
##  PURPOSE: Provide the user with a menu of options for setting up an overlay
##           filesystem mount point.
##
## FEATURES TO ADD:
##	1. Dump an fstab entry.
##	2. Display the mount command instead of executing it.
##	3. Execute the mount command.
##	4. Display a summary of the options selected.
##	5. Validate the options.
##	6. Display status of all options (on, off, default)
##

run_menu ()
{
	quit=0
	err_msg=""

	while [ "$quit" -eq 0 ]
	do
			# display the menu and the quit option
		display_menu

		$PRINTF '%s\n\n' "Q) Quit"

			# display any error message from the previous run
		if [ -n "$err_msg" ]
		then
			echo "$err_msg"
		fi


			# Prompt the user for a selection

		read_str "Please enter your selection: "
		selection=$result


			# Make sure the entry is numeric

		case "$result" in
			[0-9]*)
				if [ \( "$result" -ge 1 \)  -a  \
				     \( "$result" -le "$NUM_MENU_ENTRY" \) ]
				then
					  valid=1
				else
					  valid=0
				fi
				;;

			[qQ]*)	quit=1; valid=0
				;;

			*)	valid=0
				;;
		esac

		if [ "$valid" -ne 0 ]
		then
			err_msg=""
			perform_menu_op "$result"
		else
			err_msg="Invalid selection, $result, please reenter"
		fi
	done
}



##
## FUNCTION: perform_mount
##
##  PURPOSE: Given that all information needed to mount the filesystem has been
##           determined, mount the filesystem.
##

perform_mount ()
{
	prepare_options

	if [ -n "$OPTIONS" ]
	then
		$MOUNT -t ovl ovl ${MNT_POINT} -o $OPTIONS
	else
		$MOUNT -t ovl ovl ${MNT_POINT}   # Can this really work?
	fi

	ret="$?"

	if [ "$ret" -ne 0 ]
	then
		echo "LAST $NUM_KERN_MSG KERNEL MESSAGES:"

		$DMESG | $GREP -e '^OVL' -e 'ovl fs' | $TAIL -n $NUM_KERN_MSG
	fi
}



##
## FUNCTION: parse_options
##
##  PURPOSE: Parse all command line arguments for options supplied by the user.
##

parse_options ()
{
	# currently, none

	:
}



##############################################################################
##############################################################################
##
## MAIN BODY STARTS HERE
##

parse_options "$@"


if [ "$DO_RUN_MENU" -ne 0 ]
then
	run_menu
else
		### Read the user input

	old_style_prompts


		### OK, do the mount

	perform_mount
fi
