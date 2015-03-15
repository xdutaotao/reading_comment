/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998-2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovlfs_tab.l
 **
 ** DESCRIPTION:
 **	Lexical analyzer definition for the ovlfs mount-tab file format.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-06-04	ARTHUR N.	Initial Release.
 ** 2003-06-09	ARTHUR N.	Support different implementations of flex.
 **				 Boy, I miss lex.  When did simple stop being
 **				 good?
 **
 ******************************************************************************
 ******************************************************************************
 **/

%{
#ident "@(#)ovlfs_tab.l	1.2	[03/07/27 22:20:37]"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "y.tab.h"
#include "ovlfs_tab.h"


/**
 ** FORWARD DECLARATIONS
 **/

static void	parse_spec_string(int);
static void	parse_string(char, int);



#if DEBUG_RETURN
# define do_return(x)	{ int y = (x); printf("returning %d\n", y); return y; }
#else
# define do_return(x)	return(x)
#endif

static char		*CurStr = (char *) NULL;

#ifdef YYSTYPE
extern YYSTYPE		yylval;
#else
extern long		yylval;
#endif

#ifdef FLEX

	/* Redefine the YY_INPUT() routine to read one character at a time. */
	/*  The person that thought flex needed its own buffering did not   */
	/*  appear to know that FILE POINTERS ARE ALREADY BUFFERRED. What a */
	/*  waste and hassle.                                               */

# define YY_INPUT(buf,result,max_size) \
        { \
               int c = getchar(); \
               result = (c == EOF) ? YY_NULL : (buf[0] = c, 1); \
        }
#endif



/**
 ** FUNCTION: fatal
 **
 **  PURPOSE: Display an error message and terminate the program.
 **
 ** ARGUMENTS:
 **	msg	- A format string to pass to vfprintf for the error message.
 **	...	- Arguments for the given format string.
 **/

static void	fatal (char *msg, ...)
{
	va_list	arg_list;

	va_start(arg_list, msg);

		/* Print a standard error message prefix followed by the */
		/*  given error message.                                 */

	fprintf(stderr, "Fatal error, line %d: ", yylineno);
	vfprintf(stderr, msg, arg_list);

	va_end(arg_list);

	exit(1);
}



/**
 ** FUNCTION: str_merge
 **
 **  PURPOSE: Given two strings, concatenate them into a new string and release
 **           the memory used by both arguments.
 **
 ** NOTES:
 **	- The returned string is allocated; the caller is responsible for
 **	  freeing the result.
 **/

static char	*str_merge (char *first, char *second)
{
	char	*tmp;

	tmp = (char *) malloc(strlen(first) + strlen(second) + 1);

	if ( tmp != (char *) NULL )
	{
		sprintf(tmp, "%s%s", first, second);
	}
	else
		fatal("Unable to allocate needed memory");

	free(first);
	free(second);

	return	tmp;
}

%}

%start STRINGS
%start PREFILTER_ONLY

filesystem	[fF][iI][lL][eE][sS][yY][sS][tT][eE][mM]
mount_point	[mM][oO][uU][nN][tT]-[pP][oO][iI][nN][tT]
mount_pt	[mM][oO][uU][nN][tT]-[pP][tT]
base_root	[bB][aA][sS][eE]_[rR][oO][oO][tT]
storage		[sS][tT][oO][rR][aA][gG][eE]
stg_method	[sS][tT][gG]_[mM][eE][tT][hH][oO][dD]
stg_file	[sS][tT][gG]_[fF][iI][lL][eE]
options		[oO][pP][tT][iI][oO][nN][sS]
end		[eE][nN][dD]
cd		[cC][dD]
command		[cC][oO][mM][mM][aA][nN][dD]
key		[kK][eE][yY]
special		[sS][pP][eE][cC][iI][aA][lL]
selector	[sS][eE][lL][eE][cC][tT][oO][rR]

WHITESPACE	[ \t]

LBRACE		\{
RBRACE		\}

DQUOTE		\"
SQUOTE		\'

PREFILTER_IND	\#\!\!

%%

{PREFILTER_IND}.*		{ prefilter(yytext + 3); }

<PREFILTER_ONLY>.*		ECHO;
<PREFILTER_ONLY>\n		printf("\n");

<STRINGS>{LBRACE}[0-9]+{RBRACE}	parse_spec_string(1);
<STRINGS>[^ \n\t{"']+		{
					CurStr = str_merge(CurStr,
					                   strdup(yytext));
				}

<STRINGS>{DQUOTE}		parse_string('"', 1);
<STRINGS>{SQUOTE}		parse_string('\'', 1);

<STRINGS>{WHITESPACE}+	{
				BEGIN 0;
				yylval = (long) CurStr;
				CurStr = NULL;
				do_return(v_string);
			}
<STRINGS>[\n]		{
				BEGIN 0;
				yylval = (long) CurStr;
				CurStr = NULL;
				do_return(v_string);
			}

^{WHITESPACE}*\#.*	/* eat all comment lines */ ;

{WHITESPACE}+		/* eat all whitespace */ ;

{filesystem}		do_return(t_fs);
{mount_point}		do_return(t_mnt_pt);
{mount_pt}		do_return(t_mnt_pt);
{base_root}		do_return(t_base_root);
{storage}		do_return(t_stg_root);
{stg_method}		do_return(t_stg_method);
{stg_file}		do_return(t_stg_file);
{options}		do_return(t_options);
{end}			do_return(t_end);
{cd}			do_return(t_cd);
{command}		do_return(t_command);
{key}			do_return(t_key);
{special}		do_return(t_special);
{selector}		do_return(t_selector);

{LBRACE}[0-9]+{RBRACE}	{ BEGIN STRINGS; parse_spec_string(0); }
{LBRACE}		fatal("Invalid string specifier");

[^ \n\t{"']+		{ BEGIN STRINGS; CurStr = strdup(yytext); }

{DQUOTE}		{ BEGIN STRINGS; parse_string('"', 0); }
{SQUOTE}		{ BEGIN STRINGS; parse_string('\'', 0); }

\n			;

.			fatal("Parser internal error");

%%


/**
 ** NOTES:
 **	- parse_string and parse_spec_string are placed here with forward
 **	  declarations above to prevent warnings regarding input's type.
 **/

/**
 ** FUNCTION: parse_string
 **
 **  PURPOSE: Given that the specified quote was found on input as the start of
 **           a string, parse the entire string and store in memory.
 **
 ** ARGUMENTS:
 **	quote	- The quote character to search for as the end of the string.
 **		  This character can be placed in the string by escaping it
 **		  with the backslash character.
 **
 **	merge	- Specify whether this string must be merged with the existing
 **		  value or this is a new string result.
 **/

static void	parse_string (char quote, int merge_ind)
{
	char	quoted_str_buf[4096];
	int	pos;
	int	one_char;
	int	start_line;
	char	*result;
	int	escaped;

	start_line = yylineno;

	pos = -1;
	escaped = 0;
	result = (char *) NULL;

		/* Continue reading input until the end of string is found. */

	do
	{
		pos++;

		one_char = input();

		if ( one_char == 0 )
		{
			fatal("file ends within the string constant starting "
			      "on line %d", start_line);
		}

			/* If the character is the backslash, take the next */
			/*  character as is.                                */

		if ( one_char == '\\' )
		{
				/* Read over the backslash. */

			one_char = input();

			if ( one_char == 0 )
			{
				fatal("file ends within the string constant "
				      "starting on line %d", start_line);
			}

			quoted_str_buf[pos] = one_char;

			escaped = 1;
		}
		else
		{
				/* If the character is the quote, close the */
				/*  string; otherwise, just add it.         */

			if ( one_char == quote )
				quoted_str_buf[pos] = '\0';
			else
				quoted_str_buf[pos] = one_char;

			escaped = 0;
		}

			/* If the input buffer has filled up, copy it into   */
			/*  the result and reset our position in the buffer. */

		if ( pos == ( sizeof(quoted_str_buf) - 2 ) )
		{
			quoted_str_buf[sizeof(quoted_str_buf) - 1] = '\0';

			if ( result == (char *) NULL )
				result = strdup(quoted_str_buf);
			else
				result = str_merge(result, quoted_str_buf);

			if ( result == (char *) NULL )
				fatal("unable to allocate needed memory");

			pos = -1;
		}
	}
	while ( ( one_char != quote ) || escaped );


		/* Add the contents of the buffer to the result, unless the */
		/*  buffer is currently empty.                              */

	if ( pos > 0 )
	{
		if ( result == (char *) NULL )
			result = strdup(quoted_str_buf);
		else
			result = str_merge(result, quoted_str_buf);

		if ( result == (char *) NULL )
			fatal("unable to allocate needed memory");
	}

	if ( result == (char *) NULL )
	{
			/* Empty string on input. */

		if ( merge_ind )
			;		/* Just leave it alone :) */
		else
		{
			CurStr = malloc(1);

			if ( CurStr != (char *) NULL )
				CurStr[0] = '\0';
		}
	}
	else
	{
		if ( merge_ind )
			CurStr = str_merge(CurStr, result);
		else
			CurStr = result;
	}

	if ( CurStr == (char *) NULL )
		fatal("unable to allocate needed memory");
}



/**
 ** FUNCTION: parse_spec_string
 **
 **  PURPOSE: Given that a special string's size specifier has been found on
 **           input, read the string into memory.
 **
 ** ARGUMENTS:
 **	merge	- Indicate whether this string is a new result string or must
 **		  be merged with the existing result string.
 **/

static void	parse_spec_string (int merge_ind)
{
	int	start_line = yylineno;
	int	cur;
	int	start_len;
	int	len;
	int	one_char;
	char	*ptr;
	char	*result;


		/* Use sscanf() to extract the length. */

	if ( sscanf(yytext, "{%d}", &len) != 1 )
	{
		fatal("Internal parser error - unable to determine string "
		      "length");
	}

	if ( len <= 0 )		/* should never be < 0, but be safe */
	{
			/* The string is the empty string.  Don't try to */
			/*  use strdup("") because many strdup's fail on */
			/*  the empty string.                            */

		result = (char *) malloc(1);

		if ( result != (char *) NULL )
			result[0] = '\0';
		else
			fatal("Unable to allocate needed memory");
	}
	else
	{
			/* Allocate the memory to hold the string and the */
			/*  terminating null byte.                        */

		result = (char *) malloc(len + 1);

		if ( result == (char *) NULL )
			fatal("Unable to allocate needed memory");


		ptr = result;
		start_len = len;	/* for error message, if needed */

			/* Read into the result buffer until the entire */
			/*  string has been read.                       */

		while ( len-- )
		{
			one_char = input();

			if ( one_char == 0 )
			{
				fatal("End of file in string starting on "
				      "line %d, length %d", start_line, len);
			}

			ptr[0] = one_char;
			ptr++;
		}

		result[len] = '\0';	/* Make sure it is null terminated */
	}

	if ( merge_ind )
		CurStr = str_merge(CurStr, result);
	else
		CurStr = result;
}



/**
 ** FUNCTION: prefilter_only_mode
 **/

void	prefilter_only_mode ()
{
	BEGIN PREFILTER_ONLY;
}
