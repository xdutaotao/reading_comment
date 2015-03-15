/******************************************************************************
 ******************************************************************************
 **
 ** FILE: truncate.c
 **
 ** DESCRIPTION:
 **	Sources for the truncate program which calls the truncate() system
 **	call on a file.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-02-15	ARTHUR N.	Initial Release
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#) truncate.c 1.1"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#ifndef TRUE
# define TRUE	1
#endif

#ifndef FALSE
# define FALSE	0
#endif


/***
 ***
 ***/

static off_t	Trunc_len = 0;
static char	*Trunc_file = NULL;



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



                           /****                ****/
                           /****  MAIN PROGRAM  ****/
                           /****                ****/

/**
 ** FUNCTION: usage
 **/

static void	usage (FILE *fptr, const char *progname)
{
	fprintf(fptr, "Usage: %s [-h] [-f path] [-l len]\n", progname);
	fprintf(fptr, "\n");
	fprintf(fptr, "    -h\t\tDisplay this help and exit.\n");
	fprintf(fptr, "    -f\tpath\tPath to the file to use.\n");
	fprintf(fptr, "    -l\tlength\tResulting file length.\n");
	fprintf(fptr, "\n");
}



/**
 ** FUNCTION: parse_cmdline
 **/

#define do_getopt(c,v)	getopt((c), (v), "hf:l:")

static void	parse_cmdline (int argc, char **argv)
{
	int	opt;

	opt = do_getopt(argc, argv);

	while ( opt != -1 )
	{
		switch ( opt )
		{
		    case 'h':
			usage(stderr, argv[0]);
			exit(0);
			break;

		    case 'f':
			Trunc_file = optarg;
			break;

		    case 'l':
			Trunc_len = str_to_int(optarg);
			break;

		    default:
			usage(stderr, argv[0]);
			exit(1);
			break;
		}

		opt = do_getopt(argc, argv);
	}
}



/***
 *** MAIN PROGRAM
 ***/

int	main (int argc, char **argv)
{
	int	rc;
	int	exit_cd;

	exit_cd = 1;

	parse_cmdline(argc, argv);

	if ( Trunc_file != NULL )
	{
		printf("Truncating %s to length %ld\n", Trunc_file, Trunc_len);
		rc = truncate(Trunc_file, Trunc_len);

		if ( rc != 0 )
			perror(Trunc_file);
		else
			exit_cd = 0;
	}
	else
	{
		usage(stderr, argv[0]);
		fprintf(stderr, "\nPlease specify the file to truncate.\n");
	}

	return	0;
}
