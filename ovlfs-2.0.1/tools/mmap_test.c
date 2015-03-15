/******************************************************************************
 ******************************************************************************
 **
 ** FILE: mmap_test.c
 **
 ** DESCRIPTION:
 **	Sources for the mmap_test program which mmap's a file and performs
 **	various operations on it.
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

#ident "@(#) mmap_test.c 1.2"

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

#define DEFAULT_MAP_FILE "/dev/null"


/***
 ***
 ***/

static int	Write_test_f = FALSE;
static int	Read_test_f = TRUE;
static char	*Map_file = DEFAULT_MAP_FILE;
static uint32_t	Map_length = 4096;
static off_t	Map_offset = 0;



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



                          /****                  ****/
                          /****  TEST FUNCTIONS  ****/
                          /****                  ****/

/**
 ** FUNCTION: write_data
 **/

static void	write_data (uint8_t *loc, uint32_t len)
{
	char		buffer[256];
	char		*ptr;
	uint32_t	date_len;
	uint32_t	total_out;
	time_t		now;

	now = time(NULL);
	
	ctime_r(&now, buffer);
	ptr = strchr(buffer, '\n');

	if ( ptr != NULL )
		ptr[0] = '\0';

	date_len = strlen(buffer);

	if ( date_len > len )
		date_len = len;


	printf("Writing date, \"%.*s\"\n", (int) date_len, buffer);

	memcpy(loc, buffer, date_len);

	printf("Filling to %u bytes\n", len);

	total_out = date_len;

	while ( total_out < len )
	{
		loc[total_out] = (uint8_t) total_out;
		total_out++;
	}
}



/**
 ** FUNCTION: write_test
 **/

static void	write_test ()
{
	int	fd;
	int	rc;
	uint8_t	*ptr;

	fd = open(Map_file, O_RDWR | O_CREAT, 0666);

	if ( fd == -1 )
	{
		FATAL("failed to open map file, %s: %s", Map_file,
		      strerror(errno));
	}

	ptr = mmap(NULL, Map_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	           Map_offset);

	if ( ptr == MAP_FAILED )
		FATAL("failed to map %s: %s", Map_file, strerror(errno));

	write_data(ptr, Map_length);

	rc = munmap(ptr, Map_length);

	if ( rc == -1 )
		FATAL("failed to unmap %s: %s", Map_file, strerror(errno));
}



/**
 ** FUNCTION: dump_data
 **/

static void	dump_data (uint8_t *data, uint32_t len)
{
	uint32_t	cur;

	cur = 0;

	while ( cur < len )
	{
		printf("%02x ", data[cur]);
		cur++;
	}

	printf(" ");

	cur = 0;

	while ( cur < len )
	{
		if ( isprint(data[cur]) )
			printf("%c", data[cur]);
		else
			printf(".");
		cur++;
	}

	printf("\n");
}



/**
 ** FUNCTION: read_data
 **/

static void	read_data (uint8_t *data, uint32_t len)
{
	if ( len > 40 )
	{
		printf("Data start:\n");
		dump_data(data, 20);

		printf("Data end:\n");
		dump_data(data + ( len - 20 ), 20);
	}
	else
	{
		printf("Data:\n");
		dump_data(data, len);
	}
}



/**
 ** FUNCTION: read_test
 **/

static void	read_test ()
{
	int	fd;
	int	rc;
	uint8_t	*ptr;

	fd = open(Map_file, O_RDONLY);

	if ( fd == -1 )
	{
		FATAL("failed to open map file, %s: %s", Map_file,
		      strerror(errno));
	}

	ptr = mmap(NULL, Map_length, PROT_READ, MAP_SHARED, fd, Map_offset);

	if ( ptr == MAP_FAILED )
		FATAL("failed to map %s: %s", Map_file, strerror(errno));

	read_data(ptr, Map_length);

	rc = munmap(ptr, Map_length);

	if ( rc == -1 )
		FATAL("failed to unmap %s: %s", Map_file, strerror(errno));
}



/**
 ** FUNCTION: run_all_tests
 **/

static void	run_all_tests ()
{
	if ( Read_test_f )
		read_test();

	if ( Write_test_f )
		write_test();
}



                           /****                ****/
                           /****  MAIN PROGRAM  ****/
                           /****                ****/

/**
 ** FUNCTION: usage
 **/

static void	usage (FILE *fptr, const char *progname)
{
	fprintf(fptr, "Usage: %s [-hnw]\n", progname);
	fprintf(fptr, "\n");
	fprintf(fptr, "    -h\t\tDisplay this help and exit.\n");
	fprintf(fptr, "    -f\tpath\tPath to the file to use.\n");
	fprintf(fptr, "    -l\tlength\tLength to map.\n");
	fprintf(fptr, "    -n\t\tDo Not perform the read test.\n");
	fprintf(fptr, "    -o\toffset\tOffset into file to map.\n");
	fprintf(fptr, "    -w\t\tPerform write test.\n");
	fprintf(fptr, "\n");
}



/**
 ** FUNCTION: parse_cmdline
 **/

#define do_getopt(c,v)	getopt((c), (v), "hf:l:no:w")

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
			Map_file = optarg;
			break;

		    case 'l':
			Map_length = str_to_int(optarg);
			break;

		    case 'n':
			Read_test_f = FALSE;
			break;

		    case 'o':
			Map_offset = str_to_int(optarg);
			break;

		    case 'w':
			Write_test_f = TRUE;
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
	parse_cmdline(argc, argv);

	run_all_tests();

	return	0;
}
