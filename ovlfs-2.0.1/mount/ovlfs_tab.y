/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998-2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovlfs_tab.y
 **
 ** DESCRIPTION:
 **	Parser definition for the ovlfs mount-tab file format.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-06-04	ARTHUR N.	Initial Release.
 **
 ******************************************************************************
 ******************************************************************************
 **/

%{
#ident "@(#)ovlfs_tab.y	1.1	[03/07/27 22:20:37]"
%}

%token t_base_root
%token t_devfile
%token t_end
%token t_fs
%token t_mnt_pt
%token t_options
%token t_space
%token t_stg_file
%token t_stg_method
%token t_stg_root

%token t_cd
%token t_command
%token t_key
%token t_special
%token t_selector

%token v_string

%{
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "ovlfs_tab.h"


/**
 ** GLOBALS:
 **/

List_t	FS_list = (List_t) NULL;		/* Result list */
List_t	FS_sel_list = (List_t) NULL;		/* Selector list */

static fs_opt_t	cur_fs_opt = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

extern int	yylineno;


	/* The following is needed for 64 bit machines with 32 bit ints and */
	/*  64 bit pointers since pointers are being used as values here.   */

#define YYSTYPE	long


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
	fprintf(stderr, ".\n");

	va_end(arg_list);

	exit(1);
}



/**
 ** FUNCTION: start_new_fs
 **
 **  PURPOSE: create the structure for a new filesystem.
 **/

long	start_new_fs (fs_opt_t **result)
{
	fs_opt_t	*ptr;

	ptr = (fs_opt_t *) malloc(sizeof(fs_opt_t));

	if ( ptr == (fs_opt_t *) NULL )
	{
		fatal("unable to allocate needed memory");
	}

	memset(ptr, '\0', sizeof(fs_opt_t));

	result[0] = ptr;

	return (long)	ptr;
}



/**
 ** FUNCTION: create_fs_sel_tbl_ele
 **
 **  PURPOSE: Create a new FS selector table element structure with the
 **           specified key value and filesystem name.
 **/

static fs_sel_tbl_ele_t	*create_fs_sel_tbl_ele (char *key, char *fs_name)
{
	fs_sel_tbl_ele_t	*result;

	result = (fs_sel_tbl_ele_t *) malloc(sizeof(fs_sel_tbl_ele_t));

	if ( result == (fs_sel_tbl_ele_t *) NULL )
	{
		fatal("unable to allocate needed memory");
	}

	result->key     = key;
	result->fs_name = fs_name;
	memset(&(result->more_opt), '\0', sizeof(result->more_opt));

	return result;
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

	tmp = (char *) malloc(strlen(first) + strlen(second));

	if ( tmp != (char *) NULL )
	{
		sprintf(tmp, "%s%s", first, second);
	}
	else
	{
		fatal("unable to allocate needed memory");
	}

	free(first);
	free(second);

	return	tmp;
}



/**
 ** FUNCTION: create_selector
 **
 **  PURPOSE: Create a new selector structure.
 **/

fs_sel_t	*create_selector (int type, char *name)
{
	fs_sel_t	*result;

	result = (fs_sel_t *) malloc(sizeof(fs_sel_t));

	if ( result == (fs_sel_t *) NULL )
		fatal("unable to allocate needed memory");

	result->type = type;
	result->name = name;
	result->cmd  = (List_t) NULL;

	result->list = List_create_abs();

	if ( result->list == (List_t) NULL )
		fatal("unable to allocate needed memory");
}


%}

%%

TABFILE:		{
				FS_list = List_create((Comp_func_t)
				                      comp_fs_opt);
				FS_sel_list = List_create((Comp_func_t)
				                          comp_fs_sel);
			}
			TABFILE_ELE MORE_TABFILE_ELE ;

MORE_TABFILE_ELE:	MORE_TABFILE_ELE TABFILE_ELE ;
MORE_TABFILE_ELE:	;


TABFILE_ELE:	FS_DECL {
				List_insert(FS_list, (List_el_t) $1);

				$$ = (long) FS_list;
			} ;
TABFILE_ELE:	FS_SELECTOR {
				List_insert(FS_sel_list, (List_el_t) $1);

				$$ = (long) FS_sel_list;
			} ;


FS_DECL:	t_fs NAME FS_OPTIONS t_end
		{
			fs_opt_t	*result;

			start_new_fs(&result);

			result[0] = cur_fs_opt;
			result->fs_name = (char *) $2;

			$$ = (long) result;
		} ;

FS_OPTIONS:	{ memset(&cur_fs_opt, '\0', sizeof(cur_fs_opt)); }
		FS_OPT_LIST;

FS_OPT_LIST:	FS_ONE_OPT FS_OPT_LIST ;
FS_OPT_LIST:	;

FS_ONE_OPT:	t_devfile STRING
		{ cur_fs_opt.dev_file = (char *) $2; } ;
FS_ONE_OPT:	t_base_root STRING
		{ cur_fs_opt.base_root = (char *) $2; } ;
FS_ONE_OPT:	t_mnt_pt STRING
		{ cur_fs_opt.mnt_pt = (char *) $2; } ;
FS_ONE_OPT:	t_options STRING
		{ cur_fs_opt.options = (char *) $2; } ;
FS_ONE_OPT:	t_stg_file STRING
		{ cur_fs_opt.stg_file = (char *) $2; } ;
FS_ONE_OPT:	t_stg_method STRING
		{ cur_fs_opt.stg_method = (char *) $2; } ;
FS_ONE_OPT:	t_stg_root STRING
		{ cur_fs_opt.stg_root = (char *) $2; } ;


FS_SELECTOR:		CD_SELECTOR t_end { $$ = $1; } ;
FS_SELECTOR:		SPECIAL_SELECTOR t_end { $$ = $1; } ;

CD_SELECTOR:		t_cd t_selector NAME
			{
				fs_sel_t	*tmp;
				tmp = create_selector(CD_SELECTOR, (char *) $3);

				$$ = (YYSTYPE) tmp;
			} ;
CD_SELECTOR:		t_cd t_selector NAME SELECTOR_TABLE
			{
				fs_sel_t	*tmp;
				tmp = create_selector(CD_SELECTOR,
				                      (char *) $3);

				tmp->list = (List_t) $4;

				$$ = (YYSTYPE) tmp;
			} ;

SPECIAL_SELECTOR:	t_special t_selector NAME SPEC_SEL_CMD
			{
				fs_sel_t	*tmp;

				tmp = create_selector(SPECIAL_SELECTOR,
				                      (char *) $3);

				tmp->cmd = (List_t) $4;

				$$ = (YYSTYPE) tmp;
			} ;
SPECIAL_SELECTOR:	t_special t_selector NAME SPEC_SEL_CMD SELECTOR_TABLE
			{
				fs_sel_t	*tmp;

				tmp = create_selector(SPECIAL_SELECTOR,
				                      (char *) $3);

				tmp->cmd = (List_t) $4;
				tmp->list = (List_t) $5;

				$$ = (YYSTYPE) tmp;
			} ;

SPEC_SEL_CMD:		t_command SPEC_SEL_CMD_ARGS { $$ = $2; };

SPEC_SEL_CMD_ARGS:	SPEC_SEL_CMD_ARGS STRING
			{
				List_insert((List_t) $1, (List_el_t) $2);

				$$ = $1;
			} ;
SPEC_SEL_CMD_ARGS:	STRING
			{
				List_t	tmp;

				tmp = List_create_abs();

				if ( tmp == (List_t) NULL )
					fatal("unable to allocate needed "
					      "memory");

				List_insert(tmp, (List_el_t) $1);

				$$ = (YYSTYPE) tmp;
			} ;


SELECTOR_TABLE:		SELECTOR_TABLE SELECTOR_TABLE_ELE
			{
				List_insert((List_t) $1, (List_el_t) $2);

				$$ = $1;
			} ;
SELECTOR_TABLE:		SELECTOR_TABLE_ELE
			{
				List_t	tmp;

				tmp = List_create((Comp_func_t)
				                  comp_fs_sel_tbl_ele);

				if ( tmp == (List_t) NULL )
					fatal("unable to allocate needed "
					      "memory");

				List_insert(tmp, (List_el_t) $1);

				$$ = (YYSTYPE) tmp;
			} ;

SELECTOR_TABLE_ELE:	t_key STRING STRING FS_OPTIONS
			{
				fs_sel_tbl_ele_t	*tmp;

				tmp = create_fs_sel_tbl_ele((char *) $2,
				                            (char *) $3);

					/* set fs opt struct */

				tmp->more_opt = cur_fs_opt;

				$$ = (long) tmp;
			} ;

NAME:		STRING { $$ = $1; } ;

STRING:		v_string { $$ = $1; } ;

%%

yyerror (char *msg)
{
	extern char	yytext[];
	extern int	yylineno;

	fprintf(stderr, "error at or near %s on line %d: %s\n",
	        yytext, yylineno, msg);
}
