/******************************************************************************
 ******************************************************************************
 **
 ** FILE: kernel_lists.h
 **
 ** PURPOSE: Maintain program constants and data structures which are used in
 **          list management.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 08/23/1997	ARTHUR N.	Copied from abs_lists.h version 1.2.
 ** 03/05/1998	ARTHUR N.	Added the define_malloc() and define_free()
 **				 list functions (use with care).
 ** 02/15/2003	ARTHUR N.	Updated to support kernel version 2.4.
 ** 03/26/2003	ARTHUR N.	Added alternate index support.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "klists.ver"
# endif
#endif

#include <linux/wait.h>

#ifndef KERNEL_LISTS_HEADER	/* Make sure to include this file only ONCE */
#define KERNEL_LISTS_HEADER

#ident "@(#)kernel_lists.h	1.4	[03/07/27 22:20:35]"

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#define ALLOCATE_FAILED		20
#define LOCK_INTERRUPTED	21
#define SUCCESS			0

#define USER_LIST	1
#define ABSTRACT	2

#define NOT_INDEXED	1
#define FLAT		2
#define SORTED		3

/** KERNEL_LIST_AUTO_LOCK - if non-zero, the kernel list functions will do **/
/**  their own locking by default, which means that the caller must not    **/
/**  lock the list on their own.                                           **/

#define KERNEL_LIST_AUTO_LOCK	1

/** KLIST_NEVER_AUTOLOCK - define (no value needed) to compile out all **/
/**  support for auto-locking.  This could improve performance...      **/

/* #define KLIST_NEVER_AUTOLOCK - off by default */


	/* Index Flags */

#define KLIST_INDEX__LAZY_UPD	1

	/* Index State */

#define KLIST_INDEX__UP_TO_DATE		1
#define KLIST_INDEX__NEEDS_UPDATE	2


/** foreach_func_t **/
typedef void	(*foreach_func_t)(void *);
typedef int	(*comp_func_t)(const void *, const void *);
typedef void	*(*next_node_func_t)(void *);
typedef void	*(*add_node_func_t)(void *, void *);


/**
 ** STRUCTURE: klist_index_struct
 **
 **   PURPOSE: Maintain the information for a single index added to the list.
 **
 ** NOTES:
 **	- This structure is not related to the "primary" index already used by
 **	  the list.
 **/

struct klist_index_struct {
	comp_func_t	comp_func;	/* Sort/search callback */
	int		flags;
	int		state;
	void		**elements;
} ;

typedef struct klist_index_struct	klist_index_t;
typedef int				klist_index_id_t;



/******************************************************************************
 ******************************************************************************
 **
 ** STRUCTURE: list_struct
 **
 ** PURPOSE:   Maintains information about an abstract list.  Used to make
 **             linked list operations more generalized.
 **
 ** STRUCTURE ELEMENTS:
 **	number_of_elements - The count of nodes in the linked list.
 **	first_element      - A void pointer to the beginning of the first
 **	                      element in the linked list.
 **	last_element       - A void pointer to the last element in the linked
 **	                      list.  This is only used for inserting at the
 **	                      end of the list.
 **	element_list       - A table of pointers used to index the linked list
 **	                      in order to provide quick access and easy
 **	                      sorting.
 **	next_node          - A user supplied function which, when given a node
 **	                      in the linked list, provides the next node in the
 **	                      list.
 **	element_compare    - A user supplied function which, when given two
 **	                      nodes in the linked list, determines which node
 **	                      should appear first in the list (for sorting).
 **	search_func        - A user supplied function which, when given a value
 **	                      to find and a pointer to an element in the list,
 **	                      determines if the value is found or comes before/
 **	                      after the given node (used for binary search).
 **	add_node           - A user supplied function which, when given a
 **	                      pointer to a new record and the beginning of the
 **	                      list, will add that record in to the list and
 **	                      return a pointer to the new beginning of the
 **	                      list.
 **	list_type          - Indicates if this list is an abstract list, and
 **	                      therefore uses the abstract linked list
 **	                      management, or if it is a user defined list.
 **		o ABSTRACT  abstract list management is used.  The user
 **		             does not need to define a next_node and an
 **		             add_node function.
 **
 **		o USER_LIST user defined list used.  The user MUST define
 **		             both the next_node and add_node functions.
 **
 **	index_mode         - Indicates the way in which the list is currently
 **	                      indexed.
 **		o NOT_INDEXED the list has not been indexed yet.
 **		o FLAT        the list has been indexed, but not sorted.
 **		o SORTED      the list has been indexed and sorted.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 **
 ******************************************************************************
 ******************************************************************************
 **/

typedef	struct list_struct	*list_t;

struct	list_struct	{
	int	number_of_elements;
	int	list_type;	/* Either user list or abstract list */
	int	index_mode;	/* Either no index, flat, or sorted  */

	void	*first_element;
	void	*last_element;
	void	**element_list;

			/** Linked list operations - supplied by user **/

	void	*(*next_node)(void *);			/* Get next node      */
	int	(*element_compare)(const void *, const void *);
	int	(*search_func)(const void *, const void *);
	void	*(*add_node)(void *, void *);		/* Add node to list   */
	void	*(*add_at_end)(void *, void *);		/* Add node to end    */

# ifndef KLIST_NEVER_AUTOLOCK
	int	auto_lock;		/* should I do my own locking? */
# endif
	struct semaphore	l_sem;		/* prevent simul. access */


		/* Additional Index Support */

	int		index_arr_size;
	klist_index_t	**indexes;
} ;



/******************************************************************************
 ******************************************************************************
 **
 ** STRUCTURE: link_list_struct
 **
 ** PURPOSE:   This structure gives the user a more generalized method of
 **            working with linked lists because the user does not need to
 **            define the next_node and add_node functions above.  This list
 **            is used when the type_of_list in the list_struct is ABSTRACT.
 **
 ** STRUCTURE ELEMENTS:
 **	element	- A pointer to a single record in the linked list.
 **	next	- A pointer to the next node in the abstract linked list.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 **
 ******************************************************************************
 ******************************************************************************
 **/

typedef struct link_list_struct	*link_list_ptr;
typedef struct link_list_struct	link_list_t;

struct link_list_struct	{
	void		*element;

	link_list_ptr	next;
} ;



/**
 ** STRUCTURE: kernel_jump_table_struct
 **
 **   PURPOSE: This is the data structure used as the single contact point for
 **            all of the list functions.  The idea is that one structure of
 **            this type will be declared external (i.e. extern) and all other
 **            functions/global data structures will be defined statically
 **            (i.e. static).
 **/

struct kernel_jump_table_struct {
	list_t	(*create_list)(next_node_func_t, comp_func_t, comp_func_t,
		               add_node_func_t);

	int	(*index_list)(list_t);
	int	(*sort_list)(list_t);
	int	(*list_length)(list_t);
	void	*(*nth_element)(list_t, int);
	void	*(*search_for_element)(list_t, void *);
	void	*(*insert_into_list)(list_t, void *);
	void	*(*insert_into_list_sorted)(list_t, void *);
	void	*(*insert_at_end)(list_t, void *);

	void	*(*first_element)(list_t);
	void	(*destroy_list)(list_t);
	void	(*foreach_node)(list_t, foreach_func_t);
	void	(*clear_list)(list_t);
	list_t	(*dup_list)(list_t);
	int	(*merge_list)(list_t, list_t);
	void	*(*remove_element)(list_t, void *);
	void	*(*remove_nth)(list_t, int);

	void	*(*next_abs_node)(void *);
	void	*(*add_abs_node)(void *, void *);
	void	*(*add_abs_node_at_end)(void *, void *);
	void	(*set_compare_func)(list_t, comp_func_t);
	int	(*lock)(list_t);
	int	(*unlock)(list_t);
	void	(*set_list_autolock)(list_t, int);
	void	(*define_malloc)(void *(*)(size_t));
	void	(*define_free)(void (*)(const void *));

	int	(*add_index)(list_t, comp_func_t, int, klist_index_id_t *);
	int	(*drop_index)(list_t, klist_index_id_t);

	int	(*read_index)(list_t, klist_index_id_t, int, void **);
	int	(*search_index)(list_t, klist_index_id_t, void *, int *);
	int	(*change_index_sort)(list_t, klist_index_id_t, comp_func_t);
} ;

typedef struct kernel_jump_table_struct	kernel_list_jump_tbl_t;



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTIONS:		(PROTOTYPED)
 **
 **	create_list        - Creates a new abstract linked list and initializes
 **	                      the list, which includes setting the user defined
 **	                      functions for next_node, element_compare,
 **	                      search_func, & add_node.
 **
 **	index_list         - Index the list by looping through the linked list
 **	                      and storing pointers to the beginning of each
 **	                      element into an array.
 **
 **	sort_list          - Sort (and index first if necessary) the linked
 **	                      list.
 **
 **	nth_element        - Retrieve the nth element from the linked list.
 **	                      The list will be indexed if it is not already.
 **
 **	search_for_element - Find the element with the given value in the
 **	                      linked list, and return a pointer to it.
 **
 **	insert_into_list   - Add the new record into the linked list and return
 **	                      a pointer to the new beginning of the list.
 **
 **	list_length        - Determine the number of elements in the list.
 **
 ** MACROS:
 **
 **	index_size - Determine the size of the table needed to index a list.
 **
 **	is_indexed - Return 0 if the list is NOT indexed, and non-zero if it is
 **	              indexed.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 **
 ******************************************************************************
 ******************************************************************************
 **/

#define index_size(l)	( (l)->number_of_elements * sizeof(void *) )
#define is_indexed(l)	( (l)->index_mode != NOT_INDEXED )


/**
 **
 **/

extern kernel_list_jump_tbl_t	kernel_lists;

#define	klist_func(f)		kernel_lists.f


#endif	/* KERNEL_LISTS_HEADER */
