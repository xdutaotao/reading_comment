#ifndef lint
/*static char yysccsid[] = "from: @(#)yaccpar	1.9 (Berkeley) 02/21/93";*/
static char yyrcsid[] = "$Id: skeleton.c,v 1.4 1993/12/21 18:45:32 jtc Exp $";
#endif
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#define YYPREFIX "yy"
#line 27 "ovlfs_tab.y"
#ident "@(#)ovlfs_tab.y	1.1	[03/07/27 22:20:37]"
#line 50 "ovlfs_tab.y"
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


#line 186 "y.tab.c"
#define t_base_root 257
#define t_devfile 258
#define t_end 259
#define t_fs 260
#define t_mnt_pt 261
#define t_options 262
#define t_space 263
#define t_stg_file 264
#define t_stg_method 265
#define t_stg_root 266
#define t_cd 267
#define t_command 268
#define t_key 269
#define t_special 270
#define t_selector 271
#define v_string 272
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    2,    0,    3,    3,    1,    1,    4,    9,    7,    8,
    8,   10,   10,   10,   10,   10,   10,   10,    5,    5,
   12,   12,   13,   13,   15,   16,   16,   14,   14,   17,
    6,   11,
};
short yylen[] = {                                         2,
    0,    3,    2,    0,    1,    1,    4,    0,    2,    2,
    0,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    3,    4,    4,    5,    2,    2,    1,    2,    1,    4,
    1,    1,
};
short yydefred[] = {                                      1,
    0,    0,    0,    0,    0,    4,    5,    6,    0,    0,
   32,    8,   31,    0,    0,    0,   19,   20,    0,    0,
    0,    0,    3,    7,    0,    0,    0,    0,    0,    0,
    0,    9,    0,    0,    0,   29,    0,    0,   13,   12,
   14,   15,   16,   17,   18,   10,    0,   28,   27,    0,
    0,    8,   26,   30,
};
short yydgoto[] = {                                       1,
    6,    2,   16,    7,    8,   12,   19,   32,   20,   33,
   13,    9,   10,   35,   38,   50,   36,
};
short yysindex[] = {                                      0,
    0, -252, -256, -254, -247,    0,    0,    0, -240, -230,
    0,    0,    0, -256, -256, -252,    0,    0, -227, -231,
 -241, -232,    0,    0, -256, -256, -256, -256, -256, -256,
 -256,    0, -231, -256, -241,    0, -256, -241,    0,    0,
    0,    0,    0,    0,    0,    0, -256,    0,    0, -256,
 -241,    0,    0,    0,
};
short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   37,    0,    0,    0, -249,
 -221,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0, -249,    0, -220,    0,    0, -219,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0, -248,
 -227,    0,    0,    0,
};
short yygindex[] = {                                      0,
   25,    0,    0,    0,    0,   -1,  -10,   10,    0,    0,
  -25,    0,    0,    6,    0,    0,  -28,
};
#define YYTABLESIZE 44
short yytable[] = {                                      39,
   40,   41,   42,   43,   44,   45,   48,    3,   47,   11,
   25,   49,   21,   22,    4,   11,   14,    5,   17,   11,
   25,   52,   48,   15,   53,   25,   26,   34,   18,   27,
   28,   24,   29,   30,   31,   37,    2,   21,   22,   23,
   23,   54,   46,   51,
};
short yycheck[] = {                                      25,
   26,   27,   28,   29,   30,   31,   35,  260,   34,  259,
  259,   37,   14,   15,  267,  272,  271,  270,  259,  269,
  269,   47,   51,  271,   50,  257,  258,  269,  259,  261,
  262,  259,  264,  265,  266,  268,    0,  259,  259,  259,
   16,   52,   33,   38,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 272
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"t_base_root","t_devfile","t_end",
"t_fs","t_mnt_pt","t_options","t_space","t_stg_file","t_stg_method",
"t_stg_root","t_cd","t_command","t_key","t_special","t_selector","v_string",
};
char *yyrule[] = {
"$accept : TABFILE",
"$$1 :",
"TABFILE : $$1 TABFILE_ELE MORE_TABFILE_ELE",
"MORE_TABFILE_ELE : MORE_TABFILE_ELE TABFILE_ELE",
"MORE_TABFILE_ELE :",
"TABFILE_ELE : FS_DECL",
"TABFILE_ELE : FS_SELECTOR",
"FS_DECL : t_fs NAME FS_OPTIONS t_end",
"$$2 :",
"FS_OPTIONS : $$2 FS_OPT_LIST",
"FS_OPT_LIST : FS_ONE_OPT FS_OPT_LIST",
"FS_OPT_LIST :",
"FS_ONE_OPT : t_devfile STRING",
"FS_ONE_OPT : t_base_root STRING",
"FS_ONE_OPT : t_mnt_pt STRING",
"FS_ONE_OPT : t_options STRING",
"FS_ONE_OPT : t_stg_file STRING",
"FS_ONE_OPT : t_stg_method STRING",
"FS_ONE_OPT : t_stg_root STRING",
"FS_SELECTOR : CD_SELECTOR t_end",
"FS_SELECTOR : SPECIAL_SELECTOR t_end",
"CD_SELECTOR : t_cd t_selector NAME",
"CD_SELECTOR : t_cd t_selector NAME SELECTOR_TABLE",
"SPECIAL_SELECTOR : t_special t_selector NAME SPEC_SEL_CMD",
"SPECIAL_SELECTOR : t_special t_selector NAME SPEC_SEL_CMD SELECTOR_TABLE",
"SPEC_SEL_CMD : t_command SPEC_SEL_CMD_ARGS",
"SPEC_SEL_CMD_ARGS : SPEC_SEL_CMD_ARGS STRING",
"SPEC_SEL_CMD_ARGS : STRING",
"SELECTOR_TABLE : SELECTOR_TABLE SELECTOR_TABLE_ELE",
"SELECTOR_TABLE : SELECTOR_TABLE_ELE",
"SELECTOR_TABLE_ELE : t_key STRING STRING FS_OPTIONS",
"NAME : STRING",
"STRING : v_string",
};
#endif
#ifndef YYSTYPE
typedef int YYSTYPE;
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 500
#define YYMAXDEPTH 500
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#define yystacksize YYSTACKSIZE
#line 392 "ovlfs_tab.y"

yyerror (char *msg)
{
	extern char	yytext[];
	extern int	yylineno;

	fprintf(stderr, "error at or near %s on line %d: %s\n",
	        yytext, yylineno, msg);
}
#line 351 "y.tab.c"
#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
#if defined(__STDC__)
yyparse(void)
#else
yyparse()
#endif
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1)
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1)
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 1:
#line 224 "ovlfs_tab.y"
{
				FS_list = List_create((Comp_func_t)
				                      comp_fs_opt);
				FS_sel_list = List_create((Comp_func_t)
				                          comp_fs_sel);
			}
break;
case 5:
#line 236 "ovlfs_tab.y"
{
				List_insert(FS_list, (List_el_t) yyvsp[0]);

				yyval = (long) FS_list;
			}
break;
case 6:
#line 241 "ovlfs_tab.y"
{
				List_insert(FS_sel_list, (List_el_t) yyvsp[0]);

				yyval = (long) FS_sel_list;
			}
break;
case 7:
#line 249 "ovlfs_tab.y"
{
			fs_opt_t	*result;

			start_new_fs(&result);

			result[0] = cur_fs_opt;
			result->fs_name = (char *) yyvsp[-2];

			yyval = (long) result;
		}
break;
case 8:
#line 260 "ovlfs_tab.y"
{ memset(&cur_fs_opt, '\0', sizeof(cur_fs_opt)); }
break;
case 12:
#line 267 "ovlfs_tab.y"
{ cur_fs_opt.dev_file = (char *) yyvsp[0]; }
break;
case 13:
#line 269 "ovlfs_tab.y"
{ cur_fs_opt.base_root = (char *) yyvsp[0]; }
break;
case 14:
#line 271 "ovlfs_tab.y"
{ cur_fs_opt.mnt_pt = (char *) yyvsp[0]; }
break;
case 15:
#line 273 "ovlfs_tab.y"
{ cur_fs_opt.options = (char *) yyvsp[0]; }
break;
case 16:
#line 275 "ovlfs_tab.y"
{ cur_fs_opt.stg_file = (char *) yyvsp[0]; }
break;
case 17:
#line 277 "ovlfs_tab.y"
{ cur_fs_opt.stg_method = (char *) yyvsp[0]; }
break;
case 18:
#line 279 "ovlfs_tab.y"
{ cur_fs_opt.stg_root = (char *) yyvsp[0]; }
break;
case 19:
#line 282 "ovlfs_tab.y"
{ yyval = yyvsp[-1]; }
break;
case 20:
#line 283 "ovlfs_tab.y"
{ yyval = yyvsp[-1]; }
break;
case 21:
#line 286 "ovlfs_tab.y"
{
				fs_sel_t	*tmp;
				tmp = create_selector(CD_SELECTOR, (char *) yyvsp[0]);

				yyval = (YYSTYPE) tmp;
			}
break;
case 22:
#line 293 "ovlfs_tab.y"
{
				fs_sel_t	*tmp;
				tmp = create_selector(CD_SELECTOR,
				                      (char *) yyvsp[-1]);

				tmp->list = (List_t) yyvsp[0];

				yyval = (YYSTYPE) tmp;
			}
break;
case 23:
#line 304 "ovlfs_tab.y"
{
				fs_sel_t	*tmp;

				tmp = create_selector(SPECIAL_SELECTOR,
				                      (char *) yyvsp[-1]);

				tmp->cmd = (List_t) yyvsp[0];

				yyval = (YYSTYPE) tmp;
			}
break;
case 24:
#line 315 "ovlfs_tab.y"
{
				fs_sel_t	*tmp;

				tmp = create_selector(SPECIAL_SELECTOR,
				                      (char *) yyvsp[-2]);

				tmp->cmd = (List_t) yyvsp[-1];
				tmp->list = (List_t) yyvsp[0];

				yyval = (YYSTYPE) tmp;
			}
break;
case 25:
#line 327 "ovlfs_tab.y"
{ yyval = yyvsp[0]; }
break;
case 26:
#line 330 "ovlfs_tab.y"
{
				List_insert((List_t) yyvsp[-1], (List_el_t) yyvsp[0]);

				yyval = yyvsp[-1];
			}
break;
case 27:
#line 336 "ovlfs_tab.y"
{
				List_t	tmp;

				tmp = List_create_abs();

				if ( tmp == (List_t) NULL )
					fatal("unable to allocate needed "
					      "memory");

				List_insert(tmp, (List_el_t) yyvsp[0]);

				yyval = (YYSTYPE) tmp;
			}
break;
case 28:
#line 352 "ovlfs_tab.y"
{
				List_insert((List_t) yyvsp[-1], (List_el_t) yyvsp[0]);

				yyval = yyvsp[-1];
			}
break;
case 29:
#line 358 "ovlfs_tab.y"
{
				List_t	tmp;

				tmp = List_create((Comp_func_t)
				                  comp_fs_sel_tbl_ele);

				if ( tmp == (List_t) NULL )
					fatal("unable to allocate needed "
					      "memory");

				List_insert(tmp, (List_el_t) yyvsp[0]);

				yyval = (YYSTYPE) tmp;
			}
break;
case 30:
#line 374 "ovlfs_tab.y"
{
				fs_sel_tbl_ele_t	*tmp;

				tmp = create_fs_sel_tbl_ele((char *) yyvsp[-2],
				                            (char *) yyvsp[-1]);

					/* set fs opt struct */

				tmp->more_opt = cur_fs_opt;

				yyval = (long) tmp;
			}
break;
case 31:
#line 387 "ovlfs_tab.y"
{ yyval = yyvsp[0]; }
break;
case 32:
#line 389 "ovlfs_tab.y"
{ yyval = yyvsp[0]; }
break;
#line 694 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1)
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
