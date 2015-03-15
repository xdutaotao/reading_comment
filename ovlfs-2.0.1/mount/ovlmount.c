/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998-2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovlmount.c
 **
 ** DESCRIPTION:
 **	This file contains the source code for the ovlmount program which
 **	supports the ovlfs.tab file for mounting and unmounting ovlfs file-
 **	systems.
 **
 ** NOTES:
 **	- This is not the prettiest code, but it does the job ;-).
 **
 ** NICE TO HAVE:
 **	- Ability to specify the filesystem type name.  This is useful for
 **	  two reasons:
 **
 **		1. allows the ovlfs to add new type names in the future without
 **		   forcing this program to change.
 **
 **		2. allows the user to potentially use this program for other
 **		   filesystems, even though it may not be very appropriate.
 **
 **
 ** REVISION HISTORY
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-06-04	ARTHUR N.	Initial Release.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovlmount.c	1.1	[03/07/27 22:20:37]"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>

#include <unistd.h>

#include "ovlfs_tab.h"


/**
 ** GLOBALS:
 **/

extern List_t	FS_list;	/* parser interface: filesystem list */
extern List_t	FS_sel_list;	/* parser interface: fs selector list */

static char	*CD_vec[] = { CD_SELECTOR_CMD };	/* CD selector cmd */



/**
 ** FUNCTION: usage
 **
 **  PURPOSE: Display a usage information message on standard output.
 **/

static void	usage (char *progname)
{
	int	padlen;

	padlen = strlen(progname) + 8;

	printf("Usage: %s [-MmnPpsuvxz] [-i tab-file] [-K selector-name]\n"
	       "%*c[-o mount-prog] [-S selector-name] [-U mtab-file]\n"
	       "%*c[fsname ...]\n", progname, padlen, ' ', padlen, ' ');
}



/**
 ** FUNCTION: fatal
 **
 **  PURPOSE: Display an error message and exit the program.
 **
 ** ARGUMENTS:
 **	msg	- The message format string, given to vfprintf.
 **	...	- Arguments for the message format string passed to vprintf.
 **/

static void	fatal (char *msg, ...)
{
	va_list	arg_list;

	va_start(arg_list, msg);

	fprintf(stderr, "Fatal error: ");
	vfprintf(stderr, msg, arg_list);
	fprintf(stderr, ".\n");

	va_end(arg_list);

	exit(1);
}



/**
 ** FUNCTION: trim_spaces
 **
 **  PURPOSE: Trim leading and trailing whitespace from the given string.
 **/

static char	*trim_spaces (char *str)
{
	char	*ptr;

		/* Skip over leading whitespace. */

	while ( isspace(str[0]) )
		str++;

	ptr = str;

		/* Find the end of string */

	while ( ptr[0] != '\0' )
		ptr++;


		/* Back up until the previous character is not a space or */
		/*  the beginning of the string is found.                 */

	while ( ( ptr > str ) && ( isspace(ptr[-1]) ) )
		ptr--;

	ptr[0] = '\0';


	return	str;
}



			/****                       ****/
			/****  REPORTING FUNCTIONS  ****/
			/****                       ****/

/**
 ** FUNCTION: dump_one_fs
 **
 **  PURPOSE: Display the information for the given filesystem structure.
 **
 ** ARGUMENTS:
 **	one_fs	- The pointer to the filesystem option structure to display.
 **/

static void	dump_one_fs (fs_opt_t *one_fs)
{
	printf("FILESYSTEM %s\n", one_fs->fs_name);

	if ( one_fs->mnt_pt != (char *) NULL )
		printf("\t   MOUNT POINT: %s\n", one_fs->mnt_pt);

	if ( one_fs->base_root != (char *) NULL )
		printf("\t     BASE ROOT: %s\n", one_fs->base_root);

	if ( one_fs->stg_root != (char *) NULL )
		printf("\t  STORAGE ROOT: %s\n", one_fs->stg_root);

	if ( one_fs->stg_method != (char *) NULL )
		printf("\tSTORAGE METHOD: %s\n", one_fs->stg_method);

	if ( one_fs->stg_file != (char *) NULL )
		printf("\t  STORAGE FILE: %s\n", one_fs->stg_file);

	if ( one_fs->options != (char *) NULL )
		printf("\t       OPTIONS: %s\n", one_fs->options);
}



/**
 ** FUNCTION: comp_fs_opt
 **
 **  PURPOSE: Compare the two fs option structures whose addresses are given.
 **/

int	comp_fs_opt (fs_opt_t *fs1, fs_opt_t *fs2)
{
	if ( fs1 == NULL )
		return 1;
	else
		if ( fs2 == NULL )
			return -1;

	return strcmp(fs1->fs_name, fs2->fs_name);
}



/**
 ** FUNCTION: comp_fs_sel_tbl_ele
 **
 **  PURPOSE: Compare the two fs selector structures whose addresses are given.
 **/

int	comp_fs_sel_tbl_ele (fs_sel_tbl_ele_t *ele1, fs_sel_tbl_ele_t *ele2)
{
	if ( ele1 == NULL )
		return 1;
	else
		if ( ele2 == NULL )
			return -1;

	return strcmp(ele1->key, ele2->key);
}



/**
 ** FUNCTION: comp_fs_sel
 **
 **  PURPOSE: Compare the two fs selector structures whose addresses are given.
 **/

int	comp_fs_sel (fs_sel_t *sel1, fs_sel_t *sel2)
{
	if ( sel1 == NULL )
		return 1;
	else
		if ( sel2 == NULL )
			return -1;

	return strcmp(sel1->name, sel2->name);
}



/**
 ** FUNCTION: find_named_fs
 **
 **  PURPOSE: Given a filesystem list and the name of a filesystem, find the
 **           named filesystem and return it.
 **
 ** ARGUMENTS:
 **	list	- The filesystem list structure to search.
 **	name	- The name of the filesystem to return.
 **/

static fs_opt_t	*find_named_fs (List_t fs_list, char *name)
{
	fs_opt_t	tmp;
	int		cur;
	fs_opt_t	*result;
	uint		ele;

	memset(&tmp, '\0', sizeof(tmp));
	tmp.fs_name = name;

	ele = List_find_element(fs_list, (List_el_t) &tmp);

	if ( ele != 0 )
		result = (fs_opt_t *) List_nth(fs_list, ele);
	else
		result = (fs_opt_t *) NULL;

	return	result;
}



/**
 ** FUNCTION: dump_fs_list
 **
 **  PURPOSE: Display the contents of the given filesystem list.
 **
 ** ARUGMENTS:
 **	list	- A pointer to the filesystem list to display.
 **	mounts	- A list of filesystems to display.
 **/

static void	dump_fs_list (List_t list, char **mounts)
{
	fs_opt_t	*one_fs;
	int		tot_ele;
	int		cur;

		/* If the user did not specify any filesystems, display all */
		/*  of them.                                                */

	if ( ( mounts == (char **) NULL ) || ( mounts[0] == (char *) NULL ) )
	{
		tot_ele = List_length(list);

		if ( tot_ele == 1 )
			printf("FS LIST: 1 entry\n");
		else
			printf("FS LIST: %d entries\n", tot_ele);

		for ( cur = 1; cur <= tot_ele; cur++ )
		{
			one_fs = (fs_opt_t *) List_nth(list, cur);
			fputc('\n', stdout);
			dump_one_fs(one_fs);
		}
	}
	else
	{
			/* Loop through all of the requested filesystems */

		for ( cur = 0; mounts[cur] != (char *) NULL; cur++ )
		{
			if ( cur > 0 )
				fputc('\n', stdout);

			one_fs = find_named_fs(list, mounts[cur]);

			if ( one_fs == (fs_opt_t *) NULL )
				fatal("unable to find filesystem, %s",
				      mounts[cur]);

			dump_one_fs(one_fs);
		}
	}
}



/**
 ** FUNCTION: dump_one_sel_key
 **
 **  PURPOSE: Display the selector table element whose address is given.
 **/

static void	dump_one_sel_key (List_el_t one_ele)
{
	fs_sel_tbl_ele_t	*ele;

	ele = (fs_sel_tbl_ele_t *) one_ele;

	if ( ele == (fs_sel_tbl_ele_t *) NULL )
		printf("\t\tNULL key element!\n");
	else
		printf("\t\tKey: %s\tFS: %s\n", ele->key, ele->fs_name);
}



/**
 ** FUNCTION: dump_command
 **
 **  PURPOSE: Given a list of arguments as a command, display the command on
 **           standard output.
 **/

static void	dump_command (List_t cmd)
{
	uint	tot_ele;
	uint	cur_ele;
	char	*one_ele;

	tot_ele = List_length(cmd);

	printf("COMMAND:");

	for ( cur_ele = 1; cur_ele <= tot_ele; cur_ele++ )
	{
		one_ele = (char *) List_nth(cmd, cur_ele);

		if ( one_ele == (char *) NULL )
			printf(" (NULL)");
		else
			printf(" %s", one_ele);
	}

	printf("\n");
}



/**
 ** FUNCTION: dump_command_vec
 **
 **  PURPOSE: Given a vector of arguments as a command, display the command on
 **           standard output.
 **/

static void	dump_command_vec (char **cmd)
{
	uint	cur_ele;
	char	*one_ele;

	printf("COMMAND:");

	for ( cur_ele = 0; cmd[cur_ele] != NULL; cur_ele++ )
	{
		printf(" %s", cmd[cur_ele]);
	}

	printf("\n");
}



/**
 ** FUNCTION: dump_one_sel
 **
 **  PURPOSE: Display one filesystem selector on standard output.
 **/

static void	dump_one_sel (fs_sel_t *sel)
{
	int	num_ele;

	if ( sel == (fs_sel_t *) NULL )
	{
		printf("NULL Selector.\n");
		return;
	}

	switch ( sel->type )
	{
		case CD_SELECTOR:
			printf("CD Selector %s:\n", sel->name);
			break;

		case SPECIAL_SELECTOR:
			printf("Special Selector %s:\n\t", sel->name);
			dump_command(sel->cmd);
			break;

		default:
			printf("Selector type %d unrecognized\n");
			printf("\tName: %s\n", sel->name);
			break;
	}

	if ( sel->list != (List_t) NULL )
	{
		num_ele = List_length(sel->list);

		if ( num_ele == 0 )
			printf("\tNo table entries.\n");
		else
		{
			if ( num_ele == 1 )
				printf("\t1 table entry.\n");
			else
				printf("\t%d table entries.\n", num_ele);

			List_forall(sel->list, dump_one_sel_key);
		}
	}
	else
		printf("\tNo table entries.\n");
}



/**
 ** FUNCTION: dump_sel_list
 **
 **  PURPOSE: Display the contents of the given filesystem selector list.
 **
 ** ARUGMENTS:
 **	list	- A pointer to the filesystem list to display.
 **/

static void	dump_sel_list (List_t list)
{
	fs_sel_t	*one_sel;
	int		tot_ele;
	int		cur;

		/* If the user did not specify any filesystems, display all */
		/*  of them.                                                */

	tot_ele = List_length(list);

	printf("\nSELECTOR LIST: %d entries\n", tot_ele);

	for ( cur = 1; cur <= tot_ele; cur++ )
	{
		one_sel = (fs_sel_t *) List_nth(list, cur);
		fputc('\n', stdout);
		dump_one_sel(one_sel);
	}
}



/**
 ** FUNCTION: prefilter
 **
 **  PURPOSE: Given a string to use as a prefilter for the file on standard
 **           input, execute the prefilter and continue parsing from the output
 **           of that filter.
 **
 ** ARGUMENTS:
 **	cmd	- A string that contains the commands to execute as the pre-
 **		  filter.  This string is passed to the shell as follows:
 **
 **			/bin/sh -c <cmd>
 **
 **		  where <cmd> is replaced with the value of this argument.
 **		  This value is passed as a single argument to the sh command,
 **		  so whitespace does not need to be escaped except as a normal
 **		  part of the command.
 **/

int	prefilter (char *cmd)
{
	pid_t	c_pid;
	int	p_fd[2];
	int	rc;

		/* Create a pipe to read the output of the filter process. */

	rc = pipe(p_fd);

	if ( rc == -1 )
		fatal("unable to create a pipe for prefilter %s.\n"
		      "error %d: %s\n", cmd, errno, strerror(errno));


		/* Create the child process to start the filter. */

	c_pid = fork();

	if ( c_pid == -1 )
	{
		fatal("unable to create a child process for filter %s.\n"
		      "error %d: %s\n", cmd, errno, strerror(errno));
	}
	else if ( c_pid == 0 )
	{
			/* This is the child; have the child process the */
			/*  original input (i.e. leave its stdin alone), */
			/*  and have it write to the pipe.               */

		dup2(p_fd[1], 1);
		close(p_fd[0]);		/* The child does not need this. */
		close(p_fd[1]);		/* Don't need this fd: it's dup'ed. */


			/* Now execute the command using /bin/sh. */

		execl("/bin/sh", "sh", "-c", cmd, NULL);

		fatal("unable to execute /bin/sh.\nerror %d: %s\n", errno,
		      strerror(errno));
	}
	else
	{
			/* This is the parent.  Read from the pipe as our */
			/*  standard input.  I hope the lexer does not    */
			/*  buffer its input...                           */

		dup2(p_fd[0], 0);
		close(p_fd[0]);		/* Don't need this fd: it's dup'ed. */
		close(p_fd[1]);		/* The parent does not need this. */
	}

	return	0;
}


			/****                      ****/
			/****  SELECTOR FUNCTIONS  ****/
			/****                      ****/


/**
 ** FUNCTION: run_selector_cmd
 **
 **  PURPOSE: Run the selector command whose command is given as an argument
 **           vector and return the result.
 **
 ** ARGUMENTS:
 **	cmd_args	- The pointer to the table of strings to use as the
 **			  program name and arguments for the selector command.
 **/

static void	run_selector_cmd (char **cmd_args, char **result)
{
	int	p_fd[2];
	char	buf[16384];
	pid_t	child;
	char	*tmp;
	char	*ptr;
	int	c_stat;
	int	data_size;
	int	rc;
	int	ret_code;

	ret_code = 0;

	rc = pipe(p_fd);

	if ( rc == -1 )
		fatal("selector: unable to create a pipe: %s",
		      strerror(errno));

	child = fork();

	if ( child == (pid_t) -1 )
		fatal("selector: unable to create a child process: %s",
		      strerror(errno));

	if ( child == 0 )
	{
			/* This is the child process.  Set the output to */
			/*  go to the pipe's input.                      */

		dup2(p_fd[1], 1);	/* stdout to write side of pipe */
		close(p_fd[0]);		/* do not need */
		close(p_fd[1]);		/* dup'ed */

		execvp(cmd_args[0], cmd_args);

		fatal("selector: unable to start program %s: %s", cmd_args[0],
		      strerror(errno));
	}


		/* This is the parent process.  Read from the pipe. */

	close(p_fd[1]);		/* do not need */

		/* Loop on reading from the pipe until no more data  */
		/*  is read or an error occurs, ignoring interrupted */
		/*  system calls, of course.                         */

	tmp = (char *) NULL;
	ptr = (char *) NULL;
	data_size = 0;

	do
	{
		rc = read(p_fd[0], buf, sizeof(buf));

		if ( rc > 0 )
		{
			if ( tmp == (char *) NULL )
			{
				data_size = rc;

				tmp = (char *) malloc(data_size + 1);

				if ( tmp == (char *) NULL )
					fatal("selector: unable to allocate "
					      "needed memory");

				memcpy(tmp, buf, data_size);
				ptr = tmp + data_size;
			}
			else
			{
				data_size += rc;

				tmp = (char *)
				      realloc(tmp, data_size + 1);

				if ( tmp == (char *) NULL )
					fatal("selector: unable to allocate "
					      "needed memory");

				memcpy(ptr, buf, rc);
				ptr += rc;
			}

			tmp[data_size] = '\0';
		}
	}
	while ( ( rc > 0 ) || ( ( rc == -1 ) && ( errno == EINTR ) ) );

	if ( rc == -1 )
		fatal("selector: error reading child's output; %s",
		      strerror(errno));


		/* Wait for the child to exit and warn the user if the */
		/*  exit status is not 0.                              */

	rc = waitpid(child, &c_stat, 0);

	if ( rc == -1 )
	{
		fprintf(stderr, "warning: selector: received error waiting "
		        "for child: %s.\n", strerror(errno));

		sleep(2);	/* Give the user a chance to terminate */
	}
	else if ( WIFEXITED(c_stat) )
	{
		if ( WEXITSTATUS(c_stat) != 0 )
		{
			fprintf(stderr, "warning: selector: command exited "
			        "with status %d.\n", WEXITSTATUS(c_stat));

			sleep(2);    /* Give the user a chance to terminate */
		}
	}
	else
	{
		if ( WIFSIGNALED(c_stat) )
			fprintf(stderr, "warning: selector: child exited on "
			        "signal %d.\n", WTERMSIG(c_stat));
		else
			fprintf(stderr, "warning: selector: unrecognized "
			        "exit status from child: %d.\n", c_stat);

		sleep(2);	/* Give the user a chance to terminate */
	}

	result[0] = tmp;
}



/**
 ** FUNCTION: list_to_vec
 **
 **  PURPOSE: Given a list containing strings, convert the list into a vector
 **           of strings with each element of the vector pointing to an element
 **           in the list, in order.
 **
 ** ARGUMENTS:
 **	v_list	- List of strings to store in the vector.
 **	addit	- Number of additional elements to allocate in the vector.
 **/

static char	**list_to_vec (List_t v_list, int addit)
{
	uint	num_ele;
	uint	cur_ele;
	char	*one_ele;
	char	**vec;

	num_ele = List_length(v_list);

	if ( num_ele == 0 )
	{
		vec = (char **) NULL;
	}
	else
	{
		vec = (char **) malloc( (num_ele + addit) * sizeof(char *) );

		if ( vec == (char **) NULL )
			fatal("unable to allocate needed memory");

		for ( cur_ele = 1; cur_ele <= num_ele; cur_ele++ )
		{
			one_ele = (char *) List_nth(v_list, cur_ele);

			vec[cur_ele - 1] = one_ele;
		}
	}

	return	vec;
}



/**
 ** FUNCTION: determine_sel_key
 **
 **  PURPOSE: Determine the current key of the given selector.
 **/

static char	*determine_sel_key (fs_sel_t *sel, ovlmount_opt_t *opts)
{
	char	**cmd_vec;
	char	*cmd_res;
	char	*key;

	switch ( sel->type )
	{
		case CD_SELECTOR:
			cmd_vec = CD_vec;
			break;

		case SPECIAL_SELECTOR:
				/* Create the command vector from the list */
				/*  of command arguments.                  */

			cmd_vec = list_to_vec(sel->cmd, 1);

			if ( cmd_vec == (char **) NULL )
				fatal("get selector key: selector, %s, has "
				      "a null command!\n", sel->name);

			cmd_vec[List_length(sel->cmd)] = (char *) NULL;
			break;

		default:
			fatal("get selector key: unrecognized selector type "
			      "%d for selector %s.\n", sel->type, sel->name);
			break;
	}

	if ( opts->show_cmd )
	{
		printf("SELECTOR ");
		dump_command_vec(cmd_vec);
	}

		/* Run the selector's command now to get the result. */

	run_selector_cmd(cmd_vec, &cmd_res);


		/* Now convert the output to a usable key. */

	key = strdup(trim_spaces(cmd_res));

	if ( key == (char *) NULL )
		fatal("unable to allocate needed memory");

	free(cmd_res);

	return	key;
}



/**
 ** FUNCTION: match_selector
 **
 **  PURPOSE: Perform the action necessary to find the element in the given
 **           filesystem selector that matches the criteria for the selector.
 **/

static int	match_selector (fs_sel_t *sel, ovlmount_opt_t *opts,
		                fs_sel_tbl_ele_t **result)
{
	char			*key;
	fs_sel_tbl_ele_t	tmp_ele;
	int			num_ele;
	int			match;

		/* Determine the current key for the selector. */

	key = determine_sel_key(sel, opts);

	if ( opts->show_sel_key )
	{
		printf("Selector key is \"%s\"\n", key);
	}

		/* Look for the returned key in the table. */

	tmp_ele.key = key;
	match = List_find_element(sel->list, (List_el_t) &tmp_ele);


		/* If the match was found, get the element from the table. */

	if ( match > 0 )
	{
		result[0] = (fs_sel_tbl_ele_t *) List_nth(sel->list, match);
	}

	free(key);

	return	match;
}



			/****                   ****/
			/****  MOUNT FUNCTIONS  ****/
			/****                   ****/



/**
 ** FUNCTION: add_to_opt_str
 **
 **  PURPOSE: Given the pointer to an option string variable and strings to
 **           attach
 **/

static void	add_to_opt_str(char **result, ...)
{
	va_list	arg_list;
	char	*one_arg;
	char	*tmp;
	int	len;

	va_start(arg_list, result);
	one_arg = va_arg(arg_list, char *);

	len = 0;

	while ( one_arg != (char *) NULL )
	{
		len += strlen(one_arg);
		one_arg = va_arg(arg_list, char *);
	}

	if ( len > 0 )
	{
		if ( result[0] != (char *) NULL )
			len += strlen(result[0]) + 1;	/* 1 for comma */


			/* Allocate enough memory for the result. */

		tmp = (char *) malloc(len + 1);

		if ( tmp == (char *) NULL )
			fatal("unable to allocate needed memory");


			/* Copy the previous value, if any. */

		if ( result[0] != (char *) NULL )
		{
			sprintf(tmp, "%s,", result[0]);
		}
		else
			tmp[0] = '\0';


			/* Now loop through the remaining strings and      */
			/*  concatenate them together, without separators. */

		va_start(arg_list, result);
		one_arg = va_arg(arg_list, char *);

		while ( one_arg != (char *) NULL )
		{
			strcat(tmp, one_arg);
			one_arg = va_arg(arg_list, char *);
		}

		if ( result[0] != (char *) NULL )
			free(result[0]);

		result[0] = tmp;
	}
}



/**
 ** FUNCTION: build_opt_str
 **
 **  PURPOSE: Build the option string needed for mounting the filesystem whose
 **           option structure is given.
 **
 ** ARGUMENTS:
 **	one_fs	- The structure of options for the filesystem to mount.
 **	opts	- Options for the ovlmount program, which may affect the
 **		  result.
 **/

static char	*build_opt_str (fs_opt_t *one_fs, ovlmount_opt_t *opts)
{
	char	*result;
	char	*strings[256];
	int	cur;

	result = (char *) NULL;
	cur = 0;

	if ( one_fs->base_root != (char *) NULL )
		add_to_opt_str(&result, "base_root=", one_fs->base_root, NULL);

	if ( one_fs->stg_root != (char *) NULL )
		add_to_opt_str(&result, "storage=", one_fs->stg_root, NULL);

	if ( one_fs->stg_method != (char *) NULL )
		add_to_opt_str(&result, "stg_method=", one_fs->stg_method,
		               NULL);

	if ( one_fs->stg_file != (char *) NULL )
		add_to_opt_str(&result, "stg_file=", one_fs->stg_file, NULL);

	if ( one_fs->options != (char *) NULL )
		add_to_opt_str(&result, one_fs->options, NULL);

	return	result;
}



/**
 ** FUNCTION: update_mtab
 **
 **  PURPOSE: Given a filesystem that was successfully mounted, update the
 **           mount tab file with the filesystem's information.
 **
 ** NOTES:
 **	- Options set in the rwflag are not currently added to the mtab file.
 **/

static void	update_mtab (const char *spec, const char *mnt_pt,
		             const char *fs_type, int rwflag,
		             const char *opt_str, ovlmount_opt_t *opts)
{
	FILE	*fptr;

		/* Open the mount tab file for appending so that there  */
		/*  is no need to worry about two programs updating the */
		/*  file at the same time.                              */

	fptr = fopen(opts->mtab_file, "a");

	if ( fptr == (FILE *) NULL )
		fatal("unable to open the mtab file, %s, for update: %s",
		      opts->mtab_file, strerror(errno));


		/* Write the entry and flush the file pointer. */

	fprintf(fptr, "%s %s %s %s 0 0\n", spec, mnt_pt, fs_type, opt_str);

	fflush(fptr);


		/* If the file has an error, indicate this. */

	if ( ferror(fptr) )
	{
		fatal("error updating mtab file, %s: %s", opts->mtab_file,
		      strerror(errno));
	}

	fclose(fptr);
}



/**
 ** FUNCTION: mount_fs
 **
 **  PURPOSE: Mount the filesystem whose filesystem option structure is given.
 **
 ** ARGUMENTS:
 **	one_fs	- The pointer to the filesystem option structure of the
 **		  filesystem to mount.
 **
 **	opts	- Program options which may affect the mount.
 **/

static void	mount_fs (fs_opt_t *one_fs, ovlmount_opt_t *opts)
{
	char	*special;
	char	*mount_pt;
	char	*fs_type = "ovl";
	char	*option_str;
	char	*cmd[8];
	int	rwflag = 0;
	int	cur;
	int	rc;

		/* Set the flag value to the magic number to indicate the */
		/*  correct version of mount being called.                */

	rwflag = MS_MGC_VAL;

	if ( one_fs->dev_file == (char *) NULL )
		special = one_fs->fs_name;	/* non-dev stg. system? */

	if ( one_fs->mnt_pt == (char *) NULL )
	{
			/* Perhaps prompt?  For now, just error... */

		fatal("unable to mount %s: a mount point must be specified.",
		      one_fs->fs_name);
	}
	else
		mount_pt = one_fs->mnt_pt;


		/* Build the option string. */

	option_str = build_opt_str(one_fs, opts);


		/* Build the mount command, if the user requested to see it */
		/*  or to use the mount program to perform the mount.       */

	if ( ( opts->show_cmd ) || ( opts->use_mount_prog ) )
	{
			/* Command: "mount <special> <mnt-pt> -o <options>" */

		cmd[0] = opts->mount_prog;
		cmd[1] = special;
		cmd[2] = mount_pt;
		cmd[3] = "-t";
		cmd[4] = fs_type;

		if ( option_str != (char *) NULL )
		{
			cmd[5] = "-o";
			cmd[6] = option_str;
			cmd[7] = (char *) NULL;
		}
		else
			cmd[5] = (char *) NULL;

		if ( opts->show_cmd )
		{
			fputs(cmd[0], stdout);

			for ( cur = 1; cmd[cur] != (char *) NULL; cur++ )
				printf(" %s", cmd[cur]);

			putchar('\n');
		}
	}

	if ( opts->do_mount )
	{
		if ( opts->use_mount_prog )
		{
			execv(opts->mount_prog, cmd);

			fatal("unable to execute the mount program, %s: %s",
			      opts->mount_prog, strerror(errno));
		}
		else
		{
				/* Perform the mount now */

			if ( option_str == (char *) NULL )
				rc = mount(special, mount_pt, fs_type, rwflag,
					   "");
			else
				rc = mount(special, mount_pt, fs_type, rwflag,
					   option_str);


				/* If the mount failed, tell the user & quit */

			if ( rc == -1 )
				fatal("unable to mount %s on %s: %s", special,
				      mount_pt, strerror(errno));


				/* If requested, update the mtab file now. */

			if ( opts->upd_mtab )
				update_mtab(special, mount_pt, fs_type, rwflag,
				            option_str, opts);
		}
	}


		/* Free the memory allocated for the option string, if any. */

	if ( option_str != (char *) NULL )
		free(option_str);
}



/**
 ** FUNCTION: copy_opts
 **
 **  PURPOSE: Copy the option information from the given filesystem option
 **           structure into the target structure whose address is given.
 **
 ** NOTES:
 **	- The filesystem name is NEVER copied.
 **
 **	- Only non-null (i.e. set) fields in the source are copied; unset
 **	  fields in the source are left unaffected in the target.
 **/

static void	copy_opts (fs_opt_t *tgt, fs_opt_t src)
{
	if ( src.dev_file != (char *) NULL )
		tgt->dev_file = src.dev_file;

	if ( src.mnt_pt != (char *) NULL )
		tgt->mnt_pt = src.mnt_pt;

	if ( src.base_root != (char *) NULL )
		tgt->base_root = src.base_root;

	if ( src.stg_root != (char *) NULL )
		tgt->stg_root = src.stg_root;

	if ( src.stg_method != (char *) NULL )
		tgt->stg_method = src.stg_method;

	if ( src.stg_file != (char *) NULL )
		tgt->stg_file = src.stg_file;

	if ( src.options != (char *) NULL )
		tgt->options = src.options;
}



/**
 ** FUNCTION: mount_requested
 **
 **  PURPOSE: Given a list of requested filesystems to mount and a list of all
 **           filesystems, mount the requested filesystems now.
 **
 ** ARGUMENTS:
 **	fs_list	- The list of all filesystems.
 **	mounts	- The list of requested filesystems to mount.
 **	opts	- Program options structure.
 **/

static void	mount_requested (List_t fs_list, char **mounts,
		                 ovlmount_opt_t *opts)
{
	fs_opt_t		*one_fs;
	fs_opt_t		tmp_fs_opt;
	fs_sel_tbl_ele_t	*sel_ele;
	fs_sel_t		*sel;
	fs_sel_t		tmp_sel;
	int			rc;
	int			cur;

		/* Loop through all of the requested filesystems and */
		/*  mount each one.                                  */

	for ( cur = 0; mounts[cur] != (char *) NULL; cur++ )
	{
		one_fs = find_named_fs(fs_list, mounts[cur]);

		if ( one_fs == (fs_opt_t *) NULL )
			fatal("unable to find filesystem, %s", mounts[cur]);

		mount_fs(one_fs, opts);
	}

		/* If a selector was specified, mount that now as well. */

	if ( opts->selector != (char *) NULL )
	{
			/* Find the named selector */

		tmp_sel.name = opts->selector;
		rc = List_find_element(FS_sel_list, (List_el_t) &tmp_sel);

		if ( rc == 0 )
			fatal("selector: unable to find selector %s",
			      opts->selector);
		else
		{
			sel = (fs_sel_t *) List_nth(FS_sel_list, rc);

			if ( sel == (fs_sel_t *) NULL )
				fatal("selector: internal error: NULL "
				      "element matched for selector %s (%d)",
				      opts->selector, rc);
		}

			/* Find the selector table element that matches. */

		rc = match_selector(sel, opts, &sel_ele);

		if ( rc == 0 )
			fatal("selector: no element matched selector %s",
			      opts->selector);


			/* Lookup the filesystem identified by the selector */
			/*  element and adjust its settings by the options  */
			/*  for the selector.                               */


		one_fs = find_named_fs(fs_list, sel_ele->fs_name);

		if ( one_fs == (fs_opt_t *) NULL )
			fatal("matched selector filesystem, %s, not found",
			      sel_ele->fs_name);

			/* Copy the default values for the fs and override */
			/*  with values for the selector.                  */

		tmp_fs_opt = one_fs[0];

		copy_opts(&tmp_fs_opt, sel_ele->more_opt);


			/* Now attempt to mount the filesystem. */

		mount_fs(&tmp_fs_opt, opts);
	}
}



/**
 ** FUNCTION: perform_key_calc
 **
 **  PURPOSE: Determine the current key for the specified selector and display
 **           it for the user.
 **/

static void	perform_key_calc (ovlmount_opt_t *opts, char *sel_name)
{
	char		*key;
	char		*ptr;
	fs_sel_t	*sel;
	fs_sel_t	tmp_sel;
	int		rc;

	tmp_sel.name = sel_name;
	rc = List_find_element(FS_sel_list, (List_el_t) &tmp_sel);

	if ( rc == 0 )
	{
		fatal("selector: unable to find selector %s", sel_name);
	}
	else
	{
		sel = (fs_sel_t *) List_nth(FS_sel_list, rc);

		if ( sel == (fs_sel_t *) NULL )
			fatal("selector: internal error: NULL element matched "
			      "for selector %s (%d)", sel_name, rc);
	}

		/* Now "calculate" the key */

	key = determine_sel_key(sel, opts);


		/* Spit out the key in quoted format: */

	putchar('"');
	ptr = key;
	while ( ptr[0] != '\0' )
	{
		if ( ( ptr[0] == '\\' ) || ( ptr[0] == '"' ) )
			putchar('\\');

		putchar(ptr[0]);
		ptr++;
	}
	putchar('"');
	putchar('\n');

	free(key);
}



/**
 ** FUNCTION: parse_command_line
 **
 **  PURPOSE: Parse the command line arguments given to the program.
 **
 ** ARGUMENTS:
 **	argc	- The number of arguments given to the program.
 **	argv	- The table of argument strings as given to the program.
 **	opts	- A pointer to the option structure to fill in with option
 **		  information.
 **/

static void	parse_command_line (int argc, char **argv,
		                    ovlmount_opt_t *opts, char ***mounts)
{
	int	opt;

	while ( ( opt = getopt(argc, argv, "i:K:Mmno:PpS:sU:uvxz") ) != -1 )
	{
		switch (opt)
		{
			case 'i':	/* use the specified tab file. */
				opts->tab_file = optarg;
				break;

			case 'K':
				opts->determine_key = 1;
				opts->selector      = optarg;
				break;

			case 'M':	/* use mount function */
				opts->use_mount_prog = FALSE;
				break;

			case 'm':	/* use mount program */
				opts->use_mount_prog = TRUE;
				break;

			case 'o':	/* use the specified mount program. */
				opts->use_mount_prog = TRUE;
				opts->mount_prog = optarg;
				break;

			case 'n':
				opts->show_cmd = TRUE;
				opts->show_sel_key = TRUE;
				opts->do_mount = FALSE;
				break;

			case 'p':	/* run prefilter and display output. */
				opts->prefilter_only = TRUE;
				break;

			case 'P':
				opts->print_tab = TRUE;
				break;

			case 'S':	/* use the named selector */
				opts->selector = optarg;
				break;

			case 's':	/* read tab file from standard input. */
				opts->tab_file = NULL;
				break;

			case 'U':
				opts->upd_mtab = TRUE;
				opts->mtab_file = optarg;
				break;

			case 'u':
				opts->upd_mtab = TRUE;
				break;

			case 'v':
				opts->show_cmd = TRUE;
				opts->show_sel_key = TRUE;
				break;

			case 'x':
				opts->show_cmd = TRUE;
				break;

			case 'z':
				opts->upd_mtab = FALSE;
				break;

			default:
				usage(argv[0]);
				exit(1);
				break;
		}
	}

	mounts[0] = argv + optind;
}



/******************************************************************************
 ******************************************************************************
 **
 ** MAIN PROGRAM
 **/

main (int argc, char **argv)
{
	ovlmount_opt_t	ProgOpts;
	int		in_fd;
	char		**mounts;
	char		*key;
	char		*base;
	char		KeyProg;
	int		cur;
	int		ret;


		/* Initialize the defaults */

	ProgOpts.prefilter_only = FALSE;
	ProgOpts.tab_file       = DEFAULT_TAB_FILE;
	ProgOpts.print_tab      = FALSE;
	ProgOpts.show_cmd       = FALSE;
	ProgOpts.show_sel_key   = FALSE;
	ProgOpts.do_mount       = TRUE;
	ProgOpts.use_mount_prog = DEFAULT_USE_MOUNT_PROG;
	ProgOpts.determine_key  = FALSE;
	ProgOpts.mount_prog     = DEFAULT_MOUNT_PROG;
	ProgOpts.selector       = (char *) NULL;
	ProgOpts.upd_mtab       = DEFAULT_UPD_MTAB_FILE;
	ProgOpts.mtab_file      = DEFAULT_MTAB_FILE;

		/* Override defaults based on the program's name, as */
		/*  appropriate.                                     */

	base = strrchr(argv[0], '/');

	if ( base == (char *) NULL )
		base = argv[0];
	else
		base++;

	if ( ( strcmp(base, "ovlmount_key") == 0 ) ||
	     ( strcmp(base, "ovl_mount_key") == 0 ) )
	{
		KeyProg = TRUE;
		ProgOpts.determine_key = TRUE;
	}


		/* Parse the command line options */

	parse_command_line(argc, argv, &ProgOpts, &mounts);


		/* Open the input file */

	if ( ProgOpts.tab_file != (char *) NULL )
	{
		in_fd = open(ProgOpts.tab_file, O_RDONLY);

		if ( in_fd < 0 )
		{
			fatal("unable to open tab file, %s: %s",
			      ProgOpts.tab_file, strerror(errno));
		}

		if ( dup2(in_fd, 0) == -1 )
		{
			fatal("unable to dup tab file to stdin: %s",
			      strerror(errno));
		}
	}


		/* Turn off buffering for input so that pre-filtering works */
		/*  as implemented.  Otherwise, temporary files would be    */
		/*  needed.                                                 */

	setvbuf(stdin, NULL, _IONBF, 0);


		/* If the user requested to prefilter the tab file only, */
		/*  run the lexer in prefilter-only mode.                */

	if ( ProgOpts.prefilter_only )
	{
		prefilter_only_mode();

		while ( yylex() > 0 );

		return 0;
	}


		/* Parse the input tab file */

	ret = yyparse();

	if ( ret != 0 )
		return	1;


		/* If the user requested to determine the key for a */
		/*  selector, do that now and exit.                 */

	if ( ProgOpts.determine_key )
	{
		if ( KeyProg )
		{
			if ( ProgOpts.selector != (char *) NULL )
				perform_key_calc(&ProgOpts, ProgOpts.selector);

			while ( mounts[0] != (char *) NULL )
			{
				perform_key_calc(&ProgOpts, mounts[0]);
				mounts++;
			}
		}
		else
			perform_key_calc(&ProgOpts, ProgOpts.selector);

		return 0;
	}


		/* If the user requested to print the table, just do that */
		/*  now and do not execute any commands.                  */

	if ( ProgOpts.print_tab )
	{
		if ( FS_list != (List_t) NULL )
			dump_fs_list(FS_list, mounts);

		if ( FS_sel_list != (List_t) NULL )
			dump_sel_list(FS_sel_list);
	}
	else
	{
		mount_requested(FS_list, mounts, &ProgOpts);
	}

	return ret;
}
