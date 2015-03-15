/******************************************************************************
 ******************************************************************************
 **
 ** FILE: file_test.c
 **
 ** DESCRIPTION:
 **	Sources for the file_test program which performs specific file-system
 **	interface functions as requested by the user without special handling
 **	or fancy features that could result in additional file-system
 **	interface calls.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-05-25	ARTHUR N.	Initial Release
 ** 2003-07-05	ARTHUR N.	Added the getdent command.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)file_test.c	1.2	[03/07/27 22:20:38]"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <dirent.h>

#include "file_test.h"
 
#ifndef TRUE
# define TRUE	1
#endif

#ifndef FALSE
# define FALSE	0
#endif

#define OP_NONE		0
#define OP_LISTDIR	1
#define OP_OPENFILE	2
#define OP_STAT		3	/* perform stat() */
#define OP_RENAME	4	/* perform rename() */
#define OP_UNLINK	5	/* perform unlink() */
#define OP_RMDIR	6	/* perform rmdir() */
#define OP_SYMLINK	7	/* perform symlink() */
#define OP_LSTAT	8	/* perform lstat() */
#define OP_GETDENT	9	/* perform getents() syscall */



/***
 *** FILE-LOCAL VARIABLES
 ***/

static char	*Op_arg1 = NULL;
static char	*Op_arg2 = NULL;
static int	Op = OP_NONE;

static int	Chroot_f = FALSE;
static char	*new_root = NULL;

static const char	*Op_names[] =
{
	"none",
	"listdir",
	"open",
	"stat",
	"rename",
	"unlink",
	"rmdir",
	"symlink",
	"lstat",
	"getdent",
} ;

#define NUM_CMD_NAME	( ( sizeof Op_names ) / ( sizeof Op_names[0] ) )



                          /****                  ****/
                          /****  ERROR HANDLING  ****/
                          /****                  ****/


#define FATAL(args...)	fatal2(__FILE__, __FUNCTION__, __LINE__, args)
#define WARN(args...)	warn2(__FILE__, __FUNCTION__, __LINE__, args)



/**
 ** FUNCTION: fatal2
 **/

#ifdef __GNU__
__attribute__(( noreturn ))
#endif
static void	fatal2 (const char *file_name, const char *func_name,
		        int line_num, const char *msg, ...)
{
	va_list	arg_list;

	fprintf(stderr, "fatal [%s, %s:%d]: ", func_name, file_name, line_num);

	va_start(arg_list, msg);

	vfprintf(stderr, msg, arg_list);

	va_end(arg_list);

	fputc('\n', stderr);

	exit(1);
}



/**
 ** FUNCTION: warn2
 **/

#if __GNUC__
__attribute__((unused))
#endif
static void	warn2 (const char *file_name, const char *func_name,
		       int line_num, const char *msg, ...)
{
	va_list	arg_list;

	fprintf(stderr, "fatal [%s, %s:%d]: ", func_name, file_name, line_num);

	va_start(arg_list, msg);

	vfprintf(stderr, msg, arg_list);

	va_end(arg_list);

	fputc('\n', stderr);
}



                         /****                   ****/
                         /****  GENERAL-PURPOSE  ****/
                         /****                   ****/

/**
 ** FUNCTION: str_to_int
 **/

#if __GNUC__
__attribute__(( unused ))
#endif
static int	str_to_int (const char *str)
{
	char	*ptr;
	int	ret_code;

	if ( str[0] == '\0' )
		FATAL("invalid number: value is empty");

	ret_code = strtol(str, &ptr, 0);

	if ( ptr[0] != '\0' )
		FATAL("invalid number, '%s'", str);

	return	ret_code;
}



/**
 ** FUNCTION: fmt_time
 **
 **  PURPOSE: Format the specified time into a standard format and return a
 **           pointer to the buffer containing the result.
 **
 ** NOTES:
 **	- This is similar to ctime() except that the format may be different,
 **	  and no newline is included in the result.
 **/

static char	*fmt_time (time_t when)
{
	static char	buffer[256];
	int		len;

	len = strftime(buffer, sizeof buffer, "%c", localtime(&when));

	if ( len == 0 )
		buffer[0] = '\0';
	else
		buffer[(sizeof buffer) - 1] = '\0';

	return	buffer;
}



/**
 ** FUNCTION: dump_stat_info
 **
 **  PURPOSE: Display the given file stat information.
 **/

static void	dump_stat_info (const struct stat *info)
{
	printf("device:\t\t0x%04x (%d)\n", (uint32_t) info->st_dev,
	       (uint32_t) info->st_dev);
	printf("inode:\t\t%lu\n", info->st_ino);
	printf("mode:\t\t0%o\n", info->st_mode);
	printf("nlink:\t\t%d\n", (uint32_t) info->st_nlink);
	printf("uid:\t\t%d\n", info->st_uid);
	printf("gid:\t\t%d\n", info->st_gid);
	printf("rdev:\t\t0x%04x (%d)\n", (uint32_t) info->st_rdev,
	       (uint32_t) info->st_rdev);
	printf("size:\t\t%lu\n", info->st_size);
	printf("blksize:\t%lu\n", info->st_blksize);
	printf("blocks:\t\t%lu\n", info->st_blocks);
	printf("atime:\t\t%lu (%s)\n", info->st_atime,
	       fmt_time(info->st_atime));
	printf("mtime:\t\t%lu (%s)\n", info->st_mtime,
	       fmt_time(info->st_mtime));
	printf("ctime:\t\t%lu (%s)\n", info->st_ctime,
	       fmt_time(info->st_ctime));
}



                          /****                  ****/
                          /****  TEST FUNCTIONS  ****/
                          /****                  ****/

/**
 ** FUNCTION: do_getents
 **
 **  PURPOSE: Perform the getents operation which opens a directory, reads its
 **           entries, and closes the directory.
 **
 ** NOTES:
 **	- This function only uses the following filesystem system calls:
 **
 **		- open()
 **		- readdir()
 **		- close()
 **/

#define DIRENT_CACHE_SIZE	256

static void	do_getdent ()
{
	uint8_t	dirent_cache[DIRENT_CACHE_SIZE];
	uint8_t	*cur;
	uint8_t	*end;
	int	dir_fd;
	int	rc;

	if ( Op_arg1 == NULL )
		Op_arg1 = ".";

	dir_fd = open(Op_arg1, O_RDONLY, 0);

	if ( dir_fd == -1 )
	{
		FATAL("failed to open directory, %s: %s", Op_arg1,
		      strerror(errno));
	}

	do
	{
		rc = getents(dir_fd, dirent_cache, sizeof dirent_cache);

		if ( rc == -1 )
		{
			FATAL("failed to read directory, %s: %s", Op_arg1,
			      strerror(errno));
		}

		end = dirent_cache + rc;

		cur = dirent_cache;

		while ( cur < end )
		{
			printf("dir entry: %s\n", getent_name(cur));

			cur = getent_next(cur);
		}
	} while ( rc > 0 );

	close(dir_fd);
}



/**
 ** FUNCTION: do_listdir
 **
 **  PURPOSE: Perform the listdir operation which opens a directory, reads its
 **           entries, and closes the directory.
 **
 ** NOTES:
 **	- This function only uses the following filesystem system calls:
 **
 **		- open()
 **		- readdir()
 **		- close()
 **/

static void	do_listdir ()
{
	DIR		*dir_ptr;
	struct dirent	*one_ent;

	if ( Op_arg1 == NULL )
		Op_arg1 = ".";

	dir_ptr = opendir(Op_arg1);

	if ( dir_ptr == NULL )
	{
		FATAL("failed to open directory, %s: %s", Op_arg1,
		      strerror(errno));
	}

	one_ent = readdir(dir_ptr);

	while ( one_ent != NULL )
	{
		printf("dir entry: %s\n", one_ent->d_name);
		one_ent = readdir(dir_ptr);
	}

	closedir(dir_ptr);
}



/**
 ** FUNCTION: do_openfile
 **
 **  PURPOSE: Perform the open file operation, which only opens and closes a
 **           file.
 **
 ** NOTES:
 **	- Only uses the following filesystem system calls:
 **		- open()
 **		- close()
 **/

static void	do_openfile ()
{
	int	fd;

	if ( Op_arg1 == NULL )
		Op_arg1 = "/dev/null";

	fd = open(Op_arg1, O_RDONLY);

	if ( fd == -1 )
	{
		FATAL("failed to open file, %s: %s", Op_arg1, strerror(errno));
	}

	close(fd);
}



/**
 ** FUNCTION: do_stat
 **
 **  PURPOSE: Perform the stat operation which only stats the specified file.
 **
 ** NOTES:
 **	- This operation only uses the following filesystem system calls:
 **		- stat()
 **/

static void	do_stat ()
{
	struct stat	info;
	int		rc;

	if ( Op_arg1 == NULL )
		Op_arg1 = ".";

	rc = stat(Op_arg1, &info);

	if ( rc == -1 )
		FATAL("failed to stat file, %s: %s", Op_arg1, strerror(errno));

	dump_stat_info(&info);
}



/**
 ** FUNCTION: do_rename
 **
 **  PURPOSE: Perform the rename operation with the requested arguments.
 **
 ** NOTES:
 **	- Only the rename() filesystem system call is used here.
 **/

static void	do_rename ()
{
	int	rc;

	if ( ( Op_arg1 == NULL ) || ( Op_arg2 == NULL ) )
		FATAL("rename command requires two path arguments");

	rc = rename(Op_arg1, Op_arg2);

	if ( rc == -1 )
		FATAL("failed to rename file %s to %s: %s", Op_arg1, Op_arg2,
		      strerror(errno));
}



/**
 ** FUNCTION: do_unlink
 **
 **  PURPOSE: Perform the unlink operation.
 **
 ** NOTES:
 **	- Only the unlink() filesystem system call is used here.
 **/

static void	do_unlink ()
{
	int	rc;

	if ( Op_arg1 == NULL )
		FATAL("unlink command requires a path argument");

	rc = unlink(Op_arg1);

	if ( rc == -1 )
		FATAL("failed to unlink file, %s: %s", Op_arg1,
		      strerror(errno));
}



/**
 ** FUNCTION: do_rmdir
 **
 **  PURPOSE: Perofrm the rmdir operation.
 **
 ** NOTES:
 **	- Only the rmdir() filesystem system call is used here.
 **/

static void	do_rmdir ()
{
	int	rc;

	if ( Op_arg1 == NULL )
		FATAL("rmdir command requires a path argument");

	rc = rmdir(Op_arg1);

	if ( rc == -1 )
		FATAL("failed to rmdir path, %s: %s", Op_arg1,
		      strerror(errno));
}


/**
 ** FUNCTION: do_symlink
 **
 **  PURPOSE: Perform the symlink operation.
 **
 ** NOTES:
 **	- Only the symlink() filesystem system call is used here.
 **/

static void	do_symlink ()
{
	int	rc;

	if ( ( Op_arg1 == NULL ) || ( Op_arg2 == NULL ) )
		FATAL("symlink command requires two path arguments");

	rc = symlink(Op_arg1, Op_arg2);

	if ( rc == -1 )
		FATAL("failed to create symlink %s -> %s: %s", Op_arg2,
		      Op_arg1, strerror(errno));
}



/**
 ** FUNCTION: do_lstat
 **
 **  PURPOSE: Perform the lstat operation.
 **
 ** NOTES:
 **	- Only the lstat() filesystem system call is used here.
 **/

static void	do_lstat ()
{
	struct stat	info;
	int		rc;

	if ( Op_arg1 == NULL )
		Op_arg1 = ".";

	rc = lstat(Op_arg1, &info);

	if ( rc == -1 )
		FATAL("failed to lstat file, %s: %s", Op_arg1, strerror(errno));

	dump_stat_info(&info);
}



/**
 ** FUNCTION: run_file_test
 **
 **  PURPOSE: Run the test as requested by the user.
 **/

static void	run_file_test ()
{
	int	rc;

		/* If the user requested a chroot first, do that now. */

	if ( Chroot_f )
	{
		rc = chroot(new_root);

		if ( rc == -1 )
			FATAL("failed to chroot to %s: %s", new_root,
			      strerror(errno));

		rc = chdir("/");

		if ( rc == -1 )
			FATAL("failed to chdir to / after chroot to %s: %s",
			      new_root, strerror(errno));
	}

		/* Now perform the requested operation. */

	switch ( Op )
	{
	    case OP_LISTDIR:
		do_listdir();
		break;

	    case OP_OPENFILE:
		do_openfile();
		break;

	    case OP_STAT:
		do_stat();
		break;

	    case OP_RENAME:
		do_rename();
		break;

	    case OP_UNLINK:
		do_unlink();
		break;

	    case OP_RMDIR:
		do_rmdir();
		break;

	    case OP_SYMLINK:
		do_symlink();
		break;

	    case OP_LSTAT:
		do_lstat();
		break;

	    case OP_GETDENT:
		do_getdent();
		break;
	}
}



                           /****                ****/
                           /****  MAIN PROGRAM  ****/
                           /****                ****/

/**
 ** FUNCTION: usage
 **
 **  PURPOSE: Display program command-line usage information.
 **/

static void	usage (FILE *fptr, const char *progname)
{
	int	cur;

	fprintf(fptr, "Usage: %s [-h] [-C cmd] [-l dir] [-o file] [-r root] "
	        "file ...\n", progname);
	fprintf(fptr, "\n");
	fprintf(fptr, "    -C\tcmd\tExecute the command specified.\n");
	fprintf(fptr, "    -h\t\tDisplay this help and exit.\n");
	fprintf(fptr, "    -l\tdir\tList directory, dir.\n");
	fprintf(fptr, "    -o\tfile\tOpen file, file.\n");
	fprintf(fptr, "    -r\troot\tChange root before performing op.\n");
	fprintf(fptr, "\n");
	fprintf(fptr, "Command Names:\n");

	cur = 0;

	while ( cur < NUM_CMD_NAME )
	{
		fprintf(fptr, "\t%s\n", Op_names[cur]);
		cur++;
	}

	fprintf(fptr, "\n");
}



/**
 ** FUNCTION: parse_cmdname
 **
 **  PURPOSE: Given a string, convert it from a command name into an operation.
 **/

static void	parse_cmdname (const char *name)
{
	int	cur;
	int	found_f;

		/* Loop until a match is found or there are no more names. */

	found_f = FALSE;
	cur     = 0;

	while ( ( cur < NUM_CMD_NAME ) && ( ! found_f ) )
	{
		if ( strcasecmp(Op_names[cur], name) == 0 )
		{
			Op = cur;
			found_f = TRUE;
		}
		cur++;
	}

	if ( ! found_f )
		FATAL("invalid command name, %s", name);
}



/**
 ** FUNCTION: stat_cmd_opts
 **
 **  PURPOSE: Command-line argument handling for the stat operation.
 **/

static void	stat_cmd_opts (int argc, char **argv)
{
	if ( argc > optind )
	{
		Op_arg1 = argv[optind];
		optind++;
	}
}



/**
 ** FUNCTION: rename_cmd_opts
 **
 **  PURPOSE: Command-line argument handling for the rename operation.
 **/

static void	rename_cmd_opts (int argc, char **argv)
{
	if ( argc > optind )
	{
		Op_arg1 = argv[optind];
		optind++;
	}

	if ( argc > optind )
	{
		Op_arg2 = argv[optind];
		optind++;
	}
}



/**
 ** FUNCTION: unlink_cmd_opts
 **
 **  PURPOSE: Command-line argument handling for the unlink operation.
 **/

static void	unlink_cmd_opts (int argc, char **argv)
{
	if ( argc > optind )
	{
		Op_arg1 = argv[optind];
		optind++;
	}
}



/**
 ** FUNCTION: rmdir_cmd_opts
 **
 **  PURPOSE: Command-line argument handling for the rmdir operation.
 **/

static void	rmdir_cmd_opts (int argc, char **argv)
{
	if ( argc > optind )
	{
		Op_arg1 = argv[optind];
		optind++;
	}
}



/**
 ** FUNCTION: symlink_cmd_opts
 **
 **  PURPOSE: Command-line argument handling for the symlink operation.
 **/

static void	symlink_cmd_opts (int argc, char **argv)
{
	if ( argc > optind )
	{
		Op_arg1 = argv[optind];
		optind++;
	}

	if ( argc > optind )
	{
		Op_arg2 = argv[optind];
		optind++;
	}
}



/**
 ** FUNCTION: getdent_cmd_opts
 **
 **  PURPOSE: Command-line argument handling for the getdent operation.
 **/

static void	getdent_cmd_opts (int argc, char **argv)
{
	if ( argc > optind )
	{
		Op_arg1 = argv[optind];
		optind++;
	}
}



/**
 ** FUNCTION: parse_cmdline
 **
 **  PURPOSE: Parse the arguments given on the command line.
 **/

#define do_getopt(c,v)	getopt((c), (v), "C:hl:o:r:")

static void	parse_cmdline (int argc, char **argv)
{
	int	opt;

		/* Search for options. */

	opt = do_getopt(argc, argv);

	while ( opt != -1 )
	{
		switch ( opt )
		{
		    case 'h':
			usage(stderr, argv[0]);
			exit(0);
			break;

		    case 'C':
			parse_cmdname(optarg);
			break;

		    case 'l':
			Op = OP_LISTDIR;
			Op_arg1 = optarg;
			break;

		    case 'o':
			Op = OP_OPENFILE;
			Op_arg1 = optarg;
			break;

		    case 'r':
			Chroot_f = TRUE;
			new_root = optarg;
			break;

		    default:
			usage(stderr, argv[0]);
			exit(1);
			break;
		}

		opt = do_getopt(argc, argv);
	}


		/* Process non-option arguments. */

	switch ( Op )
	{
	    case OP_STAT:
		stat_cmd_opts(argc, argv);
		break;

	    case OP_RENAME:
		rename_cmd_opts(argc, argv);
		break;

	    case OP_UNLINK:
		unlink_cmd_opts(argc, argv);
		break;

	    case OP_RMDIR:
		rmdir_cmd_opts(argc, argv);
		break;

	    case OP_SYMLINK:
		symlink_cmd_opts(argc, argv);
		break;

	    case OP_LSTAT:
		stat_cmd_opts(argc, argv);
		break;

	    case OP_GETDENT:
		getdent_cmd_opts(argc, argv);
		break;
	}
}



/***
 *** MAIN PROGRAM
 ***/

int	main (int argc, char **argv)
{
		/* Parse the command-line arguments. */

	parse_cmdline(argc, argv);


		/* Make sure an operation was specified */

	if ( Op == OP_NONE )
	{
		usage(stderr, argv[0]);
		FATAL("you must specify an operation");
	}


		/* Run the "test" now. */

	run_file_test();

	return	0;
}
