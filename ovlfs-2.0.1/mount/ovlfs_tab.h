/******************************************************************************
 ******************************************************************************
 **
 ** FILE: ovl_debug.h
 **
 ** DESCRIPTION:
 **	Definitions used by the Overlay Filesystem mount program.
 **
 **
 ** REVISION HISTORY
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-06-08	ARTHUR N.	Initial Release
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifndef ovlfs_tab_STDC_INCLUDE
#define ovlfs_tab_STDC_INCLUDE	1

# ident "@(#)ovlfs_tab.h	1.1	[03/07/27 22:20:37]"

# include "list.h"


/******************************************************************************
 ******************************************************************************
 **
 ** CONSTANT DEFINITIONS
 **/

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

#define DEFAULT_USE_MOUNT_PROG	FALSE
#define DEFAULT_UPD_MTAB_FILE	FALSE

#define DEFAULT_TAB_DIR		"/etc"
#define DEFAULT_TAB_FILE	DEFAULT_TAB_DIR "/ovlfstab"
#define DEFAULT_MTAB_FILE	DEFAULT_TAB_DIR "/mtab"

#define DEFAULT_MOUNT_PROG	"/sbin/mount"	/* sometimes, /etc/mount */

#define CK_SUM_CMD		"(md5sum || cksum) 2>&1"

#define CD_SELECTOR_CMD		"/bin/sh", "-c", \
				"set -- $(cdinfo | " CK_SUM_CMD "); echo $1", \
				NULL

#define CD_SELECTOR		1
#define SPECIAL_SELECTOR	2


/******************************************************************************
 ******************************************************************************
 **
 ** TYPE DEFINITIONS
 **/

/**
 ** STRUCTURE: fs_opt_struct
 **
 **   PURPOSE: Maintain all of the options and related information for a single
 **            filesystem.
 **/

struct fs_opt_struct
{
	char	*fs_name;
	char	*dev_file;	/* only for some storage systems */
	char	*mnt_pt;
	char	*base_root;
	char	*stg_root;
	char	*stg_method;
	char	*stg_file;
	char	*options;
} ;

typedef struct fs_opt_struct	fs_opt_t;




/**
 ** STRUCTURE: fs_sel_tbl_ele_struct
 **
 **   PURPOSE: Maintain the information on a single filesystem selector table
 **            element.
 **/

struct fs_sel_tbl_ele_struct
{
	char		*key;
	char		*fs_name;
	fs_opt_t	more_opt;
} ;

typedef struct fs_sel_tbl_ele_struct	fs_sel_tbl_ele_t;



/**
 ** STRUCTURE: fs_sel_struct
 **
 **   PURPOSE: Maintain the information on a single filesystem selector.
 **/

struct fs_sel_struct
{
	int	type;
	char	*name;
	List_t	cmd;

	List_t	list;
} ;

typedef struct fs_sel_struct	fs_sel_t;



/**
 ** STRUCTURE: ovlmount_opt_struct
 **
 **   PURPOSE: Maintain the options for the ovlmount program.
 **/

struct ovlmount_opt_struct {
	char	prefilter_only;
	char	print_tab;
	char	show_cmd;
	char	do_mount;
	char	use_mount_prog;
	char	show_sel_key;		/* Show selector key result. */
	char	determine_key;
	char	upd_mtab;		/* Update mtab file? */

	char	*mount_prog;
	char	*tab_file;
	char	*selector;
	char	*mtab_file;
} ;

typedef struct ovlmount_opt_struct	ovlmount_opt_t;



/**
 ** PROTOTYPES
 **/

extern int	comp_fs_opt(fs_opt_t *, fs_opt_t *);
extern int	comp_fs_sel(fs_sel_t *, fs_sel_t *);
extern int	comp_fs_sel_tbl_ele(fs_sel_tbl_ele_t *, fs_sel_tbl_ele_t *);

#endif
