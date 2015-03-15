/******************************************************************************
 ******************************************************************************
 **
 ** FILE: abs_lists.c
 **
 ** PURPOSE: This file holds the functions which support the list data type.
 **
 **
 ** NOTES:
 **	- The use of semaphores here to protect the lists should really be
 **	  changed to spinlocks since the protected sections are short.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 08/23/1997	ARTHUR N	Copied from abs_lists.c version 1.4.
 ** 02/20/1998	ARTHUR N	Added locking of lists.
 ** 02/27/1998	ARTHUR N	Added the kernel_lists symbol to the kernel
 **				 symbol table, added the set_list_autolock()
 **				 operation, and corrected a bug in
 **				 foreach_node() which caused the function
 **				 to attempt to lock the list after it was
 **				 already locked by the same process.
 ** 03/05/1998	ARTHUR N	Added the define_malloc() and define_free()
 **				 list functions.
 ** 03/08/1998	ARTHUR N	Only call do_free() throughout instead of
 **				 calling kfree() directly.
 ** 02/15/2003	ARTHUR N	Updated to support 2.4 kernels.
 ** 03/26/2003	ARTHUR N	Added alternate index support.  Also, added
 **				 module license, author, and description.
 ** 05/24/2003	ARTHUR N	Corrected location of alt_index_resize's
 **				 memset() destination.  Also, added index
 **				 checking debug code.
 ** 05/24/2003	ARTHUR N	Removed index checking debug code.
 ** 06/09/2003	ARTHUR N	Corrected symbol versioning support.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)kernel_lists.c	1.9	[03/07/27 22:20:37]"

#include "kernel_vers_spec.h"

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "klists.ver"
# endif
#endif

#ifdef MODULE
# if ! defined(__GENKSYMS__) || ! defined(MODVERSIONS)
#  include <linux/module.h>
# endif
#endif

#include <asm/semaphore.h>

#include <linux/stddef.h>

#if defined(KERNEL_VERSION) && ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) )
# include <linux/slab.h>
#else
# include <linux/malloc.h>
#endif

#include "kernel_lists.h"

#define ABS_NODE_DATA(n)	( ((n) == NULL) ? \
				  NULL : ((link_list_ptr) (n))->element )
#define LIST_NODE_DATA(l,n)	( ( (l)->list_type == ABSTRACT ) ? \
					ABS_NODE_DATA(n) : (n) )

/**
 ** STATIC PROTOTYPES
 **/

static list_t	create_list(next_node_func_t, comp_func_t, comp_func_t,
		            add_node_func_t);

static void	**make_index(list_t);
static int	__index_list(list_t);
static int	index_list(list_t);
static int	sort_list(list_t);
static int	list_length(list_t);
static void	*__nth_element(list_t, int);
static void	*nth_element(list_t, int);
static void	*search_for_element(list_t, void *);
static void	*insert_into_list(list_t, void *);
static void	*insert_into_list_sorted(list_t, void *);
static void	*insert_at_end(list_t, void *);

static void	*first_element(list_t);
static void	destroy_list(list_t);
static void	foreach_node(list_t, foreach_func_t);
static void	clear_list(list_t);
static list_t	dup_list(list_t);
static int	merge_list(list_t, list_t);
static void	*remove_element(list_t, void *);
static void	*remove_nth(list_t, int);

static void	*next_abs_node(void *);
static void	*add_abs_node(void *, void *);
static void	*add_abs_node_at_end(void *, void *);
static void	set_compare_func(list_t, comp_func_t);
static int	lock_list(list_t);
static int	unlock_list(list_t);

static void	*do_kmalloc(size_t);
static void	*my_realloc(void *, int, int);

static void	ssort(void *, size_t, size_t, comp_func_t, int);
static void	*bsearch(void *, void *, size_t, size_t, comp_func_t, int);
static void	set_list_autolock(list_t, int);

static void	klist_define_malloc(void *(*)(size_t));
static void	klist_define_free(void (*)(const void *));

static void	__drop_index(list_t, klist_index_id_t);

static int	add_index(list_t, comp_func_t, int, klist_index_id_t *);
static int	drop_index(list_t, klist_index_id_t);

static int	read_index(list_t, klist_index_id_t, int, void **);
static int	search_index(list_t, klist_index_id_t, void *, int *);
static int	change_index_sort(list_t, klist_index_id_t, comp_func_t);


/**
 ** GLOBALS
 **/


kernel_list_jump_tbl_t	kernel_lists = {
	create_list,
	index_list,
	sort_list,
	list_length,
	nth_element,
	search_for_element,
	insert_into_list,
	insert_into_list_sorted,
	insert_at_end,
	first_element,
	destroy_list,
	foreach_node,
	clear_list,
	dup_list,
	merge_list,
	remove_element,
	remove_nth,
	next_abs_node,
	add_abs_node,
	add_abs_node_at_end,
	set_compare_func,
	lock_list,
	unlock_list,
	set_list_autolock,
	klist_define_malloc,
	klist_define_free,
	add_index,
	drop_index,
	read_index,
	search_index,
	change_index_sort
} ;



/**
 ** STATIC VARIABLES
 **/

static void	*(*do_malloc)(size_t) = do_kmalloc;
static void	(*do_free)(const void *) = kfree;



/**
 ** FUNCTION: klist_define_malloc
 **
 **  PURPOSE: Define the malloc function to use for all memory allocated by
 **           the kernel lists.
 **
 ** NOTES:
 **	- Pass in NULL to reset the function to its default.
 **/

static void	klist_define_malloc (void *(*malloc_func)(size_t))
{
	if ( malloc_func == NULL )
		do_malloc = do_kmalloc;
	else
		do_malloc = malloc_func;
}



/**
 ** FUNCTION: klist_define_free
 **
 **  PURPOSE: Define the free function to use for all memory allocated by
 **           the kernel lists.
 **
 ** NOTES:
 **	- Pass in NULL to reset the function to its default.
 **	- Be warned that all memory allocated up to the point this function is
 **	  called will be free'd with a call to this function.
 **/

static void	klist_define_free (void (*free_func)(const void *))
{
	if ( free_func == NULL )
		do_free = kfree;
	else
		do_free = free_func;
}



/**
 ** FUNCTION: do_kmalloc
 **/

static void	*do_kmalloc (size_t size)
{
			/* Allocate KERNEL priority pages */

	return	kmalloc(size, GFP_KERNEL);
}



			/****                      ****/
			/**** LOCAL USE FUNCTIONS: ****/
			/****                      ****/

/**
 ** FUNCTION: my_realloc
 **/

static void	*my_realloc (void *obj, int old_size, int new_size)
{
	void	*result;

	result = do_kmalloc(new_size);

	if ( ( result != NULL ) && ( obj != NULL ) )
	{
		memcpy(result, obj, (new_size < old_size) ? new_size:old_size);
		do_free(obj);
	}

	return	result;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: get_list_index ()
 **
 **  PURPOSE: Return a pointer to the beginnig of the given list's index.
 **
 ** ARGUMENTS:
 **	list_to_index - The list data structure from which to get the index.
 **
 ** RETURNS:
 **     NULL   - If the index is not created.
 **     void * - Pointer to the beginning of the index.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	New Function
 **
 ******************************************************************************
 ******************************************************************************
 **/

static void	*get_list_index (list_t list)
{
	if ( list != (list_t) NULL )
		return	list->element_list;
	else
		return	(void *) NULL;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: grow_index ()
 **
 ** PURPOSE:  This function will use realloc to increase the size of the
 **           index on a list in case the list is already indexed and the
 **           user decides to add more elements to the list anyway.
 **
 ** ARGUMENTS:
 **	list_to_index - The list data structure (object) whose index needs
 **	                to grow.
 **	old_size      - The original size of the index (number of elements).
 **
 **
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

static void	*grow_index (list_t list_to_index, int old_size)
{
	void	**element_tbl;

	if ( list_to_index == (list_t) NULL )
		return	(void *) NULL;

	element_tbl = list_to_index->element_list;
	element_tbl = (void **) my_realloc(element_tbl,
	                                   old_size * sizeof(void *),
	                                   index_size(list_to_index));

	if ( element_tbl )
	{
		list_to_index->element_list = element_tbl;
		list_to_index->index_mode   = FLAT; /* Not sorted if it used */
		                                    /* to be!                */
	}

	return((void *) element_tbl);
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: grow_ordered ()
 **
 ** PURPOSE:  This function will use realloc to increase the size of the
 **           index on a list in case the list is already indexed and the
 **           user decides to add more elements to the list anyway.
 **
 ** ARGUMENTS:
 **	list_to_index - The list data structure (object) whose index needs
 **	                to grow.
 **
 ** RETURNS:
 **	ALLOCATE_FAILED - If the malloc function was unable to obtain enough
 **	                   memory for the new table.
 **	SUCCESS         - If the table was created properly.
 **
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

static void	*grow_index_ordered (list_t list_to_index, void *new_data,
		                     int pos, int old_size)
{
	void	**element_tbl;

	if ( list_to_index == (list_t) NULL )
		return	(void *) NULL;

	element_tbl = list_to_index->element_list;
	element_tbl = (void **) my_realloc(element_tbl,
	                                   old_size * sizeof(void *),
	                                   index_size(list_to_index));

	if ( element_tbl )
	{
		list_to_index->element_list = element_tbl;

			/** Open a hole in the array at the given position **/


		memmove(element_tbl + pos + 1, element_tbl + pos,
		        (list_length(list_to_index) - pos - 1) * sizeof(void *));

			/** Now place the pointer to the new data in that **/
			/**  position in the index.                       **/

		element_tbl[pos] = new_data;
	}
	else
	{
		do_free(list_to_index->element_list);
		list_to_index->element_list = NULL;
		list_to_index->index_mode   = NOT_INDEXED;
	}

	return((void *) element_tbl);
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: cleanup_index ()
 **
 ** PURPOSE:  This function will free the memory used by the list's index if
 **           it is indexed, and it will initialize the index to empty.
 **
 ** ARGUMENTS:
 **	list_to_clean - The list data structure (object) whose index is to be
 **	                cleaned.
 **
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

static void	cleanup_index (list_t list_to_clean)
{
	if ( list_to_clean == (list_t) NULL )
		return;

	if ( is_indexed(list_to_clean) )
	{
		do_free(list_to_clean->element_list);
		list_to_clean->element_list = NULL;
	}

	list_to_clean->index_mode = NOT_INDEXED;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: check_index ()
 **
 ** PURPOSE:  Check the mode of the index on the current list.  If the
 **           list is not indexed, then index it.
 **
 ** ARGUMENTS:
 **	list_to_index - The list data structure (object) to index.
 **
 ** RETURNS:
 **	ALLOCATE_FAILED - If the malloc function was unable to obtain enough
 **	                   memory for the new table.
 **	SUCCESS         - If the table was created properly.
 **
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

static int	check_index (list_t list_to_index)
{
	int	index_ok = SUCCESS;

	if ( list_to_index == (list_t) NULL )
		return	-1;

		/* Check if the list is already indexed */

	if ( ! is_indexed(list_to_index) )
		index_ok = __index_list(list_to_index);
	else
		index_ok = SUCCESS;

	return (index_ok);
}



/**
 ** FUNCTION: update_alt_index
 **
 **  PURPOSE: Update an alternate index in the given list.
 **/

static int	update_alt_index (list_t list, klist_index_id_t index_id)
{
	klist_index_t	*index;
	int		new_size;
	int		ret_code;

	ret_code = 0;

	index = list->indexes[index_id - 1];

		/* If an index array exists, release it now. */

	if ( index->elements != NULL )
		do_free(index->elements);

	if ( list->number_of_elements == 0 )
	{
		index->elements = NULL;
	}
	else
	{
		index->elements = make_index(list);

		if ( index->elements == NULL )
		{
			ret_code = -ENOMEM;
		}
		else
		{
				/* Sort the index. */

			ssort(index->elements, list->number_of_elements,
			      sizeof(void *), index->comp_func, TRUE);

			index->state = KLIST_INDEX__UP_TO_DATE;
		}
	}

	return	ret_code;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: l_search ()
 **
 ** PURPOSE:  Perform a linear search on the given list searching for the
 **           value passed by the user.  The list is searched sequentially
 **           starting from the first element in the list and continuing one
 **           element at a time until the desired element is found.
 **
 ** ARGUMENTS:
 **	list_to_search - A list through which to search for the desired value.
 **	value          - The search value being look for.  This value is
 **	                  simply passed to the user defined search function.
 **
 ** RETURNS:
 **	A void pointer to the beginning of the element if it is found, and
 **	NULL if it is not found.
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

static int	element_count = 0;	/* So anyone can find out */

static void	*l_search (list_t list_to_search, void *value)
{
	void		*cur_element;
	int		ret;
	int		found;
	link_list_ptr	l_ptr;

	if ( list_to_search == (list_t) NULL )
		return	(void *) NULL;

	element_count = 0;
	found         = FALSE;
	cur_element   = list_to_search->first_element;

	while ( ( element_count < list_length(list_to_search) ) &&
	        ( ! found ) )
	{
		if ( list_to_search->list_type == ABSTRACT )
		{
			l_ptr = (link_list_ptr) cur_element;
			ret = list_to_search->search_func(value,l_ptr->element);
		}
		else
			ret = list_to_search->search_func(value, cur_element);

		found = ( ret == 0 );

		if ( ! found )
			cur_element = list_to_search->next_node(cur_element);

		element_count++;
	}


	if ( found )
		if ( list_to_search->list_type == ABSTRACT )
			return(l_ptr->element);
		else
			return(cur_element);
	else
		return(NULL);
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: find_insert_pos
 **
 ** PURPOSE:  Find the position in the given list at which to insert the given
 **           data.  This function assumes that the list is already sorted and
 **           that the search function is defined for the given list.
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

static int	__find_insert_pos (list_t insert_list, void *new_data,
		                   comp_func_t comp_cb, void **index_ptr)
{
	int	high;
	int	low;
	int	cur;
	int	not_found;

	if ( insert_list == (list_t) NULL )
		return	0;

	low = 0;
	high = insert_list->number_of_elements - 1;

	do
	{
			/** Look at the middle **/

		cur = (int) ( low + high ) / 2;

		not_found = comp_cb(new_data, index_ptr[cur]);

			/** If the element is before the current element, **/
			/**  or it is after the current element, then     **/
			/**  adjust the part of the list we still need to **/
			/**  look at.                                     **/

		if ( not_found < 0 )
			high = cur - 1;
		else
			if ( not_found > 0 )
				low = cur + 1;
	} while ( ( not_found ) && ( low <= high ) );

		/** Now, determine which position this element **/
		/**  actually belongs in.                      **/

		/** If the element matches one found in the list,   **/
		/**  then just return the position of that element. **/

	if ( not_found )	           /* (not) Element goes at cur. pos. */
		if ( not_found > 0 ) /* Element goes after cur. pos */
			cur = cur + 1;

	return(cur);
}

static int	find_insert_pos (list_t insert_list, void *new_data)
{
	int	high;
	int	low;
	int	cur;
	int	not_found;
	void	**index_ptr;

	if ( insert_list == (list_t) NULL )
		return	0;

	return	__find_insert_pos(insert_list, new_data,
	                          insert_list->element_compare,
		                  insert_list->element_list);
}



#ifndef KLIST_NEVER_AUTOLOCK

/**
 ** FUNCTION: klist_auto_lock
 **
 **  PURPOSE: Perform the lock operation for auto-locking of the given list,
 **           if the list uses auto-locking.
 **/

#if KLIST_DEBUG_AUTO_LOCK_F
# define klist_auto_lock(l)	\
		printk("lock from %s line %d\n", __FILE__, __LINE__),	\
		klist_auto_lock0(l)
#else
# define klist_auto_lock(l)	\
		klist_auto_lock0(l)
#endif

static inline int	klist_auto_lock0 (list_t l_list)
{
	if ( l_list == (list_t) NULL )
		return	-EINVAL;

	if ( l_list->auto_lock )
		return	lock_list(l_list);
	else
		return	0;
}



/**
 ** FUNCTION: klist_auto_unlock
 **
 **  PURPOSE: Perform the unlock operation for auto-locking of the given list,
 **           if the list uses auto-locking.
 **/

#if KLIST_DEBUG_AUTO_LOCK_F
# define klist_auto_unlock(l)	\
 		printk("unlock from %s line %d\n", __FILE__, __LINE__),	\
 		klist_auto_unlock0(l)
#else
# define klist_auto_unlock(l)	\
		klist_auto_unlock0(l)
#endif

static inline void	klist_auto_unlock0 (list_t l_list)
{
	if ( l_list == (list_t) NULL )
		return;

	if ( l_list->auto_lock )
		unlock_list(l_list);
}

#else
#define klist_auto_lock(l)	0
#define klist_auto_unlock(l)
#endif



			/****                 ****/
			/**** LIST FUNCTIONS: ****/
			/****                 ****/

/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: create_list ()
 **
 ** PURPOSE:  Initialize the given list to be an empty list whose elements are
 **           of the given size.
 **
 ** ARGUMENTS:
 **	next_node        - A user provided function which, when passed a
 **	                    pointer to an element, will provide the next element
 **	                    in the linked list containing that element.
 **	element_compare  - A user provided function which, when passed two
 **                         pointers to two elements, will compare those
 **	                    elements and return an integer less than, equal to,
 **	                    or greater than zero indicating whether the first
 **	                    element should come before, at the same location,
 **	                    or after the second when sorted.
 **	search_function  - A user provided function which, when passed two
 **	                    arguments, will determine if the first argument
 **	                    (the search value) matches the record referenced
 **	                    by the second argument.
 **	add_function     - A user provided function which, when passed two
 **	                    arguments, will add the first (a new record) into
 **	                    the linked list pointed to by the second.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added support for kernel locking.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static list_t	create_list (next_node_func_t next_node,
		             comp_func_t element_compare,
		             comp_func_t search_function,
		             add_node_func_t add_function)
{
	list_t	new_list;

	if ( new_list = (list_t) do_malloc(sizeof(struct list_struct)) )
	{
#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif

		new_list->number_of_elements = 0;
		new_list->first_element      = NULL;
		new_list->last_element       = NULL;
		new_list->element_list       = (void *) NULL;
		new_list->element_compare    = element_compare;
		new_list->search_func        = search_function;
		new_list->index_mode         = NOT_INDEXED;
#ifndef KLIST_NEVER_AUTOLOCK
# if KERNEL_LIST_AUTO_LOCK
		new_list->auto_lock          = 1;
# else
		new_list->auto_lock          = 0;
# endif
#endif

# ifdef __SEMAPHORE_INITIALIZER
		init_MUTEX(&(new_list->l_sem));
# else
		new_list->l_sem              = MUTEX ;
# endif

		new_list->indexes        = NULL;
		new_list->index_arr_size = 0;

		if ( ( next_node == NULL ) || ( add_function == NULL ) )
		{
			new_list->next_node  = next_abs_node;
			new_list->add_node   = add_abs_node;
			new_list->add_at_end = add_abs_node_at_end;
			new_list->list_type  = ABSTRACT;
		}
		else
		{
			new_list->next_node = next_node;
			new_list->add_node  = add_function;
			new_list->list_type = USER_LIST;
		}
	}

	return(new_list);
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: index_list ()
 **
 ** PURPOSE:  Traverse the linked list, represented in the list passed in, and
 **           create a table of pointers to the beginning of each node in the
 **           list for quick reference and sorting.  This table is stored in
 **           the list structure itself.
 **
 ** ARGUMENTS:
 **	list_to_index - The list data structure (object) to index.
 **
 ** RETURNS:
 **	ALLOCATE_FAILED - If the malloc function was unable to obtain enough
 **	                   memory for the new table.
 **	SUCCESS         - If the table was created properly.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static void	**make_index (list_t list_to_index)
{
	int	current_element;
	void	*node_ptr;
	void	**cur_element;
	void	**result;

		/* Create the element table */

	result =(void **) do_malloc(index_size(list_to_index));

	if ( ! result )
		return	NULL;

	current_element = 0;
	node_ptr        = list_to_index->first_element;
	cur_element     = result;

	while ( current_element < list_to_index->number_of_elements )
	{
			/* For abstract lists, index the data pointed to */
			/*  by each element in the list instead of the   */
			/*  elements of the linked list themselves.      */

		if ( list_to_index->list_type == ABSTRACT )
			*cur_element = (void *)
			               ((link_list_ptr) node_ptr)->element;
		else
			*cur_element = (void *) node_ptr;

		node_ptr = (void *) ( *(list_to_index->next_node) )(node_ptr);
		current_element++;
		cur_element++;
	}

	return	result;
}

static int	__index_list (list_t list_to_index)
{
	int	current_element;
	void	*node_ptr;
	void	**cur_element;

	if ( list_to_index == (list_t) NULL )
		return	-1;

		/* Create the element table */

	list_to_index->element_list = make_index(list_to_index);

	if ( ! list_to_index->element_list )
		return	ALLOCATE_FAILED;

	list_to_index->index_mode = FLAT;

	return(SUCCESS);
}

static int	index_list (list_t list_to_index)
{
	if ( list_to_index == (list_t) NULL )
		return	-1;

		/* Lock the list (if it is auto-locking) */
	if ( klist_auto_lock(list_to_index) < 0 )
		return	LOCK_INTERRUPTED;

	klist_auto_unlock(list_to_index);

	return(SUCCESS);
}



			/************************* 
			 *************************
			 ***                   ***
			 *** Sorting Functions ***
			 ***                   ***
			 *************************
			 *************************/


/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: set_compare_func
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static void	set_compare_func(list_t list, comp_func_t comp_func)
{
	if ( list != (list_t) NULL )
	{
		if ( klist_auto_lock(list) < 0 )
			return;
		list->element_compare = comp_func;
		klist_auto_unlock(list);
	}
}


/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: sort_list ()
 **
 ** PURPOSE:  Sort the given list using the qsort (quick sort) function and
 **           the user provided element comparison function (element_compare).
 **
 ** ARGUMENTS:
 **	list_to_sort - The list data structure (object) to sort.
 **
 ** RETURNS:
 **	-1	- If an error occurs.
 **	TRUE	- The sort was successful.
 **	FALSE	- The sort was not successful.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static int	__sort_list (list_t list_to_sort)
{
	int	ok_to_sort;
	int	list_is_sorted;

		/* Initialize: indicate list is not sorted */

	list_is_sorted = FALSE;


		/* Make sure the list is indexed, and index it if not */

	if ( check_index(list_to_sort) == SUCCESS )
		ok_to_sort = TRUE;
	else
		ok_to_sort = FALSE;



		/* Sort only if the list is indexed and the user */
		/*  has defined an element comparison function.  */

	if ( ( ok_to_sort ) && ( list_to_sort->element_compare ) )
	{
		ssort(list_to_sort->element_list,
		      list_to_sort->number_of_elements,
		      sizeof(void *), list_to_sort->element_compare, TRUE);

		list_to_sort->index_mode = SORTED;

		list_is_sorted = TRUE;
	}

	return(list_is_sorted);
}

static int	sort_list (list_t list_to_sort)
{
	int	ret;

	if ( list_to_sort == (list_t) NULL )
		return	-1;

		/* Lock the list, then do the actual sort */

	if ( klist_auto_lock(list_to_sort) < 0 )
		return	-1;

	ret = __sort_list(list_to_sort);

	klist_auto_unlock(list_to_sort);

	return	ret;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: sort_sublist ()
 **
 ** PURPOSE:  Sort the specified portion of the given list using the qsort
 **           (quick sort) function and the user provided element comparison
 **           function (element_compare).
 **
 ** ARGUMENTS:
 **	list_to_sort - The list data structure (object) to sort.
 **	first_el     - The position of the first element in the part of the
 **	               list to sort.
 **	num_el       - The total number of elements to sort.
 **
 **
 ** RETURNS:
 **	TRUE	- The sort was successful.
 **	FALSE	- The sort was not successful.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	New Function
 ** 02/20/1998	ARTHUR N	Added support for auto-locking.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static int	sort_sublist (list_t list_to_sort, int first_el,
		              unsigned int num_el)
{
	int	ok_to_sort;
	int	list_is_sorted;

	if ( list_to_sort == (list_t) NULL )
		return	FALSE;

	if ( klist_auto_lock(list_to_sort) < 0 )
		return	FALSE;

		/* Initialize: indicate list is not sorted */

	list_is_sorted = FALSE;


		/* Make sure the list is indexed, and index it if not */

	if ( check_index(list_to_sort) == SUCCESS )
		ok_to_sort = TRUE;
	else
		ok_to_sort = FALSE;



		/* Sort only if the list is indexed and the user  */
		/*  has defined an element comparison function.   */
		/*  Also, make certain that the sub-list that the */
		/*  user specified is valid.                      */

	if ( ( ok_to_sort ) && ( list_to_sort->element_compare ) &&
	     ( first_el > 0 )  &&
	     ( ( first_el + num_el - 1 ) <= list_to_sort->number_of_elements ) )
	{
			/* Start sorting at the first element specified   */
			/*  Adjust the first element by -1 since the user */
			/*  sees the start of the list as 1 instead of 0. */

		ssort(&(list_to_sort->element_list[first_el - 1]),
		      num_el, sizeof(void *), list_to_sort->element_compare,
		      TRUE);

		list_to_sort->index_mode = SORTED;

		list_is_sorted = TRUE;
	}

	klist_auto_unlock(list_to_sort);

	return(list_is_sorted);
}



			/*************************** 
			 ***************************
			 ***                     ***
			 *** Searching Functions ***
			 ***                     ***
			 ***************************
			 ***************************/



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: set_search_func
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static void	set_search_func (list_t list, comp_func_t search_func)
{
	if ( list != (list_t) NULL )
	{
		if ( klist_auto_lock(list) < 0 )
			return;
		list->search_func = search_func;
		klist_auto_unlock(list);
	}
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: nth_element ()
 **
 ** PURPOSE:  Find the nth element in the list and return that element.
 **
 ** ARGUMENTS:
 **	list_to_search	- The list from which to obtain the nth element.
 **	nth		- The position of the element to find.  This must be
 **			  within the range [1, Number_of_elements].
 **
 ** RETURNS:
 **	A void pointer to the beginning of the element in the list if it
 **	 exists.  NULL is returned if the element does not exist.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static void	*__nth_element (list_t list_to_search, int nth)
{
	int		ok_to_search;
	link_list_ptr	abs_ptr;

	if ( list_to_search == (list_t) NULL )
		return	(void *) NULL;

		/* Make sure that the list is indexed so that */
		/*  getting the nth element makes sense.      */

	if ( check_index(list_to_search) == SUCCESS )
		ok_to_search = TRUE;
	else
		ok_to_search = FALSE;

		/* If the list is indexed properly and the */
		/*  nth element exists, then return that   */
		/*  element.                               */

		/* Note that there is no need to check for      */
		/*  abstract lists since the index already      */
		/*  contains pointers directly to the user data */

	if ( ( ok_to_search ) && ( nth > 0 ) &&
	     ( nth <= list_to_search->number_of_elements ) )
	{
		void	*ret;

			/* Don't access the list after unlocking... */

		ret = list_to_search->element_list[nth - 1];

		return	ret;
	}
	else
		return	NULL;
}

static void	*nth_element (list_t list_to_search, int nth)
{
	int		ok_to_search;
	void		*result;

	if ( list_to_search == (list_t) NULL )
		return	(void *) NULL;

	if ( klist_auto_lock(list_to_search) < 0 )
		return	(void *) NULL;

	result = __nth_element(list_to_search, nth);

	klist_auto_unlock(list_to_search);

	return	result;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: search_element_pos ()
 **
 ** PURPOSE:  Find an element in the list using a binary search.  The element
 **           is found by calling the user specified search function with
 **           a pointer to the search value and a pointer to the element
 **           being compared.
 **
 ** ARGUMENTS:
 **	list_to_search	- The list from which to obtain the nth element.
 **	value		- The value of the element to find.  This value is
 **			  actually just passed into the user specified function
 **			  and is otherwise untouched.
 **
 ** RETURNS:
 **	The position of the element found in the list, if found, and 0 if the
 **	 element is not found.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static int	search_element_pos (list_t list_to_search, void *value)
{
	char	*answer;
	int	ok_to_b_search = FALSE;
	int	element_no;

	if ( list_to_search == (list_t) NULL )
		return	0;

	if ( klist_auto_lock(list_to_search) < 0 )
		return	-1;


		/* Make sure the list is already indexed, and */
		/*  if not, try to index it.                  */

	if ( check_index(list_to_search) == SUCCESS )
	{
			/* The list is indexed, but is it sorted?  */
			/*  If so, we can use a bsearch, otherwise */
			/*  an linear_search is required.          */

		if ( list_to_search->index_mode == SORTED )
			ok_to_b_search = TRUE;
		else
			ok_to_b_search = FALSE;
	}
	else
		ok_to_b_search = FALSE;


		/* We can only use bsearch if the list is sorted */

	if ( ok_to_b_search )
	{
		answer = bsearch(value,
				 list_to_search->element_list,
				 list_to_search->number_of_elements,
				 sizeof(void *),
				 list_to_search->search_func, TRUE);

		if ( answer )
			element_no = ((answer - (char *)
			                        list_to_search->element_list) /
			              sizeof(void *)) + 1;
		else
			element_no = 0;
	}
	else
	{
		answer = l_search(list_to_search, value);

		if ( answer )
			element_no = element_count;	/* from l_search */
		else
			element_no = 0;
	}

	klist_auto_unlock(list_to_search);

	return	element_no;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: search_for_element ()
 **
 ** PURPOSE:  Find an element in the list using a binary search.  The element
 **           is found by calling the user specified search function with
 **           a pointer to the search value and a pointer to the element
 **           being compared.
 **
 ** ARGUMENTS:
 **	list_to_search	- The list from which to obtain the nth element.
 **	value		- The value of the element to find.  This value is
 **			  actually just passed into the user specified function
 **			  and is otherwise untouched.
 **
 ** RETURNS:
 **	A void pointer to the beginning of the element in the list if found,
 **	 and NULL if the element is not found.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static void	*search_for_element (list_t list_to_search, void *value)
{
	int		ok_to_b_search;
	void		*answer;
	link_list_ptr	l_ptr;

	if ( list_to_search == (list_t) NULL )
		return	(void *) NULL;

	if ( klist_auto_lock(list_to_search) < 0 )
		return	(void *) NULL;


		/* Make sure the list is already indexed, and */
		/*  if not, try to index it.                  */

	if ( check_index(list_to_search) == SUCCESS )
	{
			/* The list is indexed, but is it sorted?  */
			/*  If so, we can use a bsearch, otherwise */
			/*  an linear_search is required.          */

		if ( list_to_search->index_mode == SORTED )
			ok_to_b_search = TRUE;
		else
			ok_to_b_search = FALSE;
	}
	else
		ok_to_b_search = FALSE;


		/* We can only use bsearch if the list is sorted */

	if ( ok_to_b_search )
	{
		answer = bsearch(value,
				 list_to_search->element_list,
				 list_to_search->number_of_elements,
				 sizeof(void *),
				 list_to_search->search_func, TRUE);

		if ( answer )
			answer = * (void **) answer;
	}
	else
		answer = l_search(list_to_search, value);

	klist_auto_unlock(list_to_search);

	return	answer;
}



			/*************************** 
			 ***************************
			 ***                     ***
			 *** Insertion Functions ***
			 ***                     ***
			 ***************************
			 ***************************/


/**
 ** FUNCTION: alt_index_insert
 **
 **  PURPOSE: Given a list, the identifier for an alternate index attached to
 **           the list, and a new entry for the index, add the entry to the
 **           index now.
 **
 ** RULES:
 **	- The index size must already have been adjusted, with unused positions
 **	  at the end of the index.
 **	- The list's number_of_elements attribute must NOT include the new
 **	  element.
 **
 ** NOTES:
 **	- Data will be placed in the index at an offset equal to the list's
 **	  number_of_elements, when the index is maintained.
 **/

static void	alt_index_insert (list_t i_list, klist_index_id_t id,
		                  void *ele)
{
	klist_index_t	*index;
	int		move_size;
	int		insert_pos;

		/* If the index is not in-use, skip the update.  Also, if  */
		/*  the index's data array is not up-to-date, don't bring  */
		/*  it up-to-date now as there may be more list updates    */
		/*  before the index is needed.                            */

	if ( i_list->indexes[id - 1] == NULL )
		goto	alt_index_insert__out;

	index = i_list->indexes[id - 1];

	if ( ( index->state    == KLIST_INDEX__NEEDS_UPDATE ) ||
	     ( index->elements == NULL ) )
		goto	alt_index_insert__out;


		/* If lazy update is set, just set the index state to "needs */
		/*  update".                                                 */

	if ( index->flags & KLIST_INDEX__LAZY_UPD )
	{
		index->state = KLIST_INDEX__NEEDS_UPDATE;
		goto	alt_index_insert__out;
	}

		/* Find the insertion point. */

	insert_pos = __find_insert_pos(i_list, ele, index->comp_func,
	                               index->elements);

#ifdef ASSERT
	ASSERT( ( insert_pos >= 0 ) &&
	        ( insert_pos <= i_list->number_of_elements ) );
#endif

		/* Shift the existing contents to make space for the new */
		/*  element.                                             */

	if ( insert_pos < i_list->number_of_elements )
	{
			/* Determine the amount of data to move to make */
			/*  space in the index.                         */

		move_size = ( i_list->number_of_elements - insert_pos ) *
		            sizeof (void *);

			/* Move it now; the target and destination are in */
			/*  the same memory space (i.e. overlapping).     */

		memmove(&(index->elements[insert_pos + 1]),
		        &(index->elements[insert_pos]), move_size);
	}

		/* Save the new element. */

	index->elements[insert_pos] = ele;

alt_index_insert__out:

	return;
}



/**
 ** FUNCTION: alt_index_resize
 **
 **  PURPOSE: Given a list, the identifier for an alternate index attached to
 **           the list, and the number of elements being added to, or removed
 **           from the index, resize the index now.
 **
 ** RULES:
 **	- The list's number_of_elements attribute must NOT include the change
 **	  in number of elements.
 **	- Elements added or removed will be at the end of the index.
 **	- Positive change values grow the index and negative values shrink it.
 **/

static void	alt_index_resize (list_t i_list, klist_index_id_t id,
		                  int change)
{
	klist_index_t	*index;
	void		**new_arr;
	int		move_size;
	int		old_size;
	int		new_size;
	int		insert_pos;

		/* If the index is not in-use, skip the update.  Also, if  */
		/*  the index's data array is not up-to-date, don't bring  */
		/*  it up-to-date now as there may be more list updates    */
		/*  before the index is needed.                            */

	if ( i_list->indexes[id - 1] == NULL )
		goto	alt_index_insert__out;

	index = i_list->indexes[id - 1];

	if ( ( index->state    == KLIST_INDEX__NEEDS_UPDATE ) ||
	     ( index->elements == NULL ) )
		goto	alt_index_insert__out;


		/* If lazy update is set, just set the index state to "needs */
		/*  update".                                                 */

	if ( index->flags & KLIST_INDEX__LAZY_UPD )
	{
		index->state = KLIST_INDEX__NEEDS_UPDATE;
		goto	alt_index_insert__out;
	}


		/* Resize the index now. */

	old_size = i_list->number_of_elements * sizeof (void *);
	new_size = ( i_list->number_of_elements + change ) * sizeof (void *);

#ifdef ASSERT
	ASSERT( new_size >= 0 );
#endif

	if ( new_size == 0 )
	{
		do_free(index->elements);
		index->elements = NULL;
	}
	else
	{
		new_arr = my_realloc(index->elements, old_size, new_size);

		if ( new_arr == NULL )
		{
			index->state = KLIST_INDEX__NEEDS_UPDATE;
			do_free(index->elements);
			index->elements = NULL;
		}
		else
		{
			index->elements = new_arr;

				/* Zero out new elements. */

			if ( new_size > old_size )
				memset(((char *)index->elements) + old_size,
				       '\0', new_size - old_size);
		}
	}

alt_index_insert__out:

	return;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: insert_into_list ()
 **
 ** PURPOSE:  Insert a new node into the given linked list and increment the
 **            count of nodes in the list.  The user specified function is used
 **            to perform the actual insert, and its return value is taken to
 **            be the new beginning of the linked list.
 **
 ** ARGUMENTS:
 **	ptr1	- A void pointer to the information to be passed to the add
 **		   function provided by the user.
 **	ptr2	- A void pointer to the beginning of the first node in the
 **		   linked list.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/


static void	*insert_into_list (list_t insert_list, void *ptr1)
{
	void			*ret;
	void			*new_first;
	void			**tbl_ptr;
	klist_index_id_t	cur_ind;

	if ( insert_list == (list_t) NULL )
		return	(void *) NULL;

	if ( klist_auto_lock(insert_list) < 0 )
		return	(void *) NULL;


		/* Add this new node to the list if the add_node */
		/*  function is defined.                         */

	if ( insert_list->add_node )
		new_first = (*insert_list->add_node)(ptr1,
		                                    insert_list->first_element);
	else
		new_first = NULL;


		/* If the add succeeded, then update the */
		/*  list's count and check the index.    */

	if ( new_first )
	{
		int	old_size;

		insert_list->first_element = new_first;
		old_size = insert_list->number_of_elements;

			/* Update the alternate indexes before updating the */
			/*  list length.                                    */

		cur_ind = 1;

		while ( cur_ind <= insert_list->index_arr_size )
		{
			alt_index_resize(insert_list, cur_ind, 1);
			alt_index_insert(insert_list, cur_ind, ptr1);
			cur_ind++;
		}

		insert_list->number_of_elements++;

			/* If the list is indexed, then we must either */
			/*  add the new element to the index or clean  */
			/*  up the index.                              */

		if ( is_indexed(insert_list) )
			if ( insert_list->list_type == ABSTRACT )
			{
					/* Get the new index start and  */
					/*  set the LAST element to the */
					/*  new one in the index if     */
					/*  successful.  If not, then   */
					/*  clean up the index.         */

				tbl_ptr = grow_index(insert_list, old_size);

				if ( tbl_ptr )
					tbl_ptr[list_length(insert_list) - 1] =
						ptr1;
				else
					cleanup_index(insert_list);
			}
			else
				cleanup_index(insert_list);
	}

		/* Don't access the list after unlocking it: */

	ret = LIST_NODE_DATA(insert_list, new_first);
	klist_auto_unlock(insert_list);

	return	ret;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: insert_many ()
 **
 ** PURPOSE:  Insert several new nodes into the given linked list and increment
 **            update the count of nodes in the list.  This function only works
 **            for abstract lists.  The return value is the pointer to the
 **            first element given.
 **
 ** ARGUMENTS:
 **     insert_list - The list to insert the new data in to.
 **     data        - A pointer to the first element in the array of new
 **                    elements to insert.
 **     el_size     - The size of each element in the array (i.e. the distance
 **                    between the beginning of one element and the next one).
 **     num_el      - Total number of elements in the array to add to the list.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	New Function
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/


static void	*insert_many (list_t insert_list, char *data, int el_size,
		              int num_el)
{
	klist_index_id_t	cur_ind;
	void			*ret;
	void			*new_first;
	void			**tbl_ptr;
	char			*element;
	link_list_ptr		abs_nodes;
	int			add_to_index;
	int			cur_el;

	if ( insert_list == (list_t) NULL )
		return	(void *) NULL;

	if ( klist_auto_lock(insert_list) < 0 )
		return	(void *) NULL;

	new_first = NULL;

		/* Only perform the work if the list is an abstract list. */
		/*  User lists are not allowed.                           */

	if ( insert_list->list_type == ABSTRACT )
	{
		add_to_index = FALSE;


			/* Allocate memory for the new abstract nodes */

		abs_nodes = (link_list_ptr)
		            do_malloc(sizeof(link_list_t) * num_el);

		new_first = abs_nodes;


		if ( abs_nodes != NULL )
		{
			int	old_size;

			old_size = insert_list->number_of_elements;

			cur_ind = 1;

			while ( cur_ind <= insert_list->index_arr_size )
			{
				alt_index_resize(insert_list, cur_ind, num_el);
				cur_ind++;
			}


				/* Update the alternate indexes before */
				/*  updating the list length.          */

			for ( element = data, cur_el = 0; ( cur_el < num_el );
			      cur_el++ )
			{
					/* Loop through all of the indexes */
					/*  and add this element to each.  */

				cur_ind = 1;

				while ( cur_ind <= insert_list->index_arr_size )
				{
					alt_index_insert(insert_list, cur_ind,
					                 element);
					cur_ind++;
				}

				insert_list->number_of_elements++;
				element += el_size;
			}


				/* If the list is indexed, then we must */
				/*  either add the new elements to the  */
				/*  index or clean up the index.        */

			if ( is_indexed(insert_list) )
			{
					/* Get the new index start and   */
					/*  set the new elements to the  */
					/*  index if successful.  If     */
					/*  not, then clean up the index */

				tbl_ptr = grow_index(insert_list, old_size);

				if ( tbl_ptr )
					add_to_index = TRUE;
				else
					cleanup_index(insert_list);
			}
			else
				cleanup_index(insert_list);
		

			for ( element = data, cur_el = 0; ( cur_el < num_el );
			      cur_el++ )
			{
				abs_nodes->next    = abs_nodes + 1;
				abs_nodes->element = (void *) element;
				abs_nodes++;

					/* If the list is (still) indexed, */
					/*  then we must add each element  */
					/*  to the index also              */

				if ( add_to_index )
					tbl_ptr[cur_el] = element;

				element += el_size;
			}

			(abs_nodes - 1)->next = insert_list->first_element;
			insert_list->first_element = new_first;
		}
	}

		/* Don't access the list after unlocking it: */
	ret = LIST_NODE_DATA(insert_list, new_first);
	klist_auto_unlock(insert_list);

	return	ret;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: insert_into_list_sorted ()
 **
 ** PURPOSE:  Insert a new node into the given linked list and increment the
 **            count of nodes in the list.  The user specified function is used
 **            to perform the actual insert, and its return value is taken to
 **            be the new beginning of the linked list.
 **
 ** ARGUMENTS:
 **	ptr1	- A void pointer to the information to be passed to the add
 **		   function provided by the user.
 **	ptr2	- A void pointer to the beginning of the first node in the
 **		   linked list.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/


static void	*insert_into_list_sorted (list_t insert_list, void *ptr1)
{
	klist_index_id_t	cur_ind;
	void			*ret;
	void			*new_first;
	void			**tbl_ptr;
	int			ins_pos;

	if ( insert_list == (list_t) NULL )
		return	(void *) NULL;

	if ( klist_auto_lock(insert_list) < 0 )
		return	(void *) NULL;


		/* Add this new node to the list if the add_node */
		/*  function is defined.                         */

	if ( insert_list->add_node )
		new_first = (*insert_list->add_node)(ptr1,
		                                    insert_list->first_element);
	else
		new_first = NULL;


		/* If the add succeeded, then update the */
		/*  list's count and check the index.    */

	if ( new_first )
	{
			/* Update the alternate indexes before updating the */
			/*  list length.                                    */

		cur_ind = 1;

		while ( cur_ind <= insert_list->index_arr_size )
		{
			alt_index_resize(insert_list, cur_ind, 1);
			alt_index_insert(insert_list, cur_ind, ptr1);

			cur_ind++;
		}

		insert_list->first_element = new_first;

			/* The user wants to keep a sorted index, so */
			/*  make sure the index exists and then make */
			/*  sure it is sorted.                       */

		if ( insert_list->index_mode != SORTED )
		{
			int	old_size;

			old_size = insert_list->number_of_elements;
			insert_list->number_of_elements++;

				/* If an index already exists, try */
				/*  to grow it.                    */

			if ( is_indexed(insert_list) )
			{
				tbl_ptr = grow_index(insert_list, old_size);

				if ( tbl_ptr )
					tbl_ptr[list_length(insert_list) - 1] =
						ptr1;
				else
					cleanup_index(insert_list);
			}
				
				/* Use "local" sort so it does not attempt */
				/*  to lock the list again.                */

			__sort_list(insert_list);    /* create index & sort */
		}
		else
		{
			int	old_size;

			ins_pos = find_insert_pos(insert_list, ptr1);

			old_size = insert_list->number_of_elements;
			insert_list->number_of_elements++;

			tbl_ptr = grow_index_ordered(insert_list, ptr1,
			                             ins_pos, old_size);

				/* If grow_index failed, then clean up */
				/*  the now meaningless index.         */

			if ( ! tbl_ptr )
				cleanup_index(insert_list);
		}
			
	}

	ret = LIST_NODE_DATA(insert_list, new_first);
	klist_auto_unlock(insert_list);

	return	ret;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: insert_at_end ()
 **
 ** PURPOSE:  Insert a new node into the given linked list and increment the
 **            count of nodes in the list.  This function currently only
 **            works for ABSTRACT lists.
 **
 ** ARGUMENTS:
 **	insert_list	- The list to insert in to.
 **	ptr1		- A void pointer to the information to be passed to the
 **			   add function provided by the user.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/


static void	*insert_at_end (list_t insert_list, void *ptr1)
{
	void	*ret;
	void	*new_first;
	void	*new_last;
	void	**tbl_ptr;

	if ( insert_list == (list_t) NULL )
		return	(void *) NULL;

	if ( klist_auto_lock(insert_list) < 0 )
		return	(void *) NULL;

		/* Add this new node to the list if the add_node */
		/*  function is defined. (currently only works   */
		/*  for ABSTRACT lists).                         */

	if ( ( insert_list->add_node ) &&
	     ( insert_list->list_type == ABSTRACT ) )
		new_last = (*insert_list->add_at_end)(ptr1,
		                                    insert_list->last_element);
	else
		new_last = NULL;


		/* If the add succeeded, then update the list's count */
		/*  and check the index.                              */

	if ( new_last )
	{
		int	old_size;

		insert_list->last_element = new_last;

		if ( insert_list->first_element == NULL )
			insert_list->first_element = new_last;

		old_size = insert_list->number_of_elements;
		insert_list->number_of_elements++;

			/* If the list is indexed, then we must either */
			/*  add the new element to the index or clean  */
			/*  up the index.                              */

		if ( is_indexed(insert_list) )
			if ( insert_list->list_type == ABSTRACT )
			{
					/* Get the new index start and  */
					/*  set the LAST element to the */
					/*  new one in the index if     */
					/*  successful.  If not, then   */
					/*  clean up the index.         */

				tbl_ptr = grow_index(insert_list, old_size);

				if ( tbl_ptr )
					tbl_ptr[list_length(insert_list) - 1] =
						ptr1;
				else
					cleanup_index(insert_list);
			}
			else
				cleanup_index(insert_list);
	}

		/* Don't access the list after unlocking it: */
	ret = LIST_NODE_DATA(insert_list, new_last);
	klist_auto_unlock(insert_list);

	return	ret;
}



			/***********************/
			/***********************/
			/***                 ***/
			/*** Other Functions ***/
			/***                 ***/
			/***********************/
			/***********************/

/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: foreach_node ()
 **
 ** PURPOSE:  Traverse each element in the given list, one at a time, and
 **           call the user specified function once for each element, in
 **           essence providing the user with a FOREACH on the elements of the
 **           list.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static void	foreach_node (list_t list_to_traverse, foreach_func_t user_func)
{
	void		*cur_node;
	int		cur_pos;

	if ( list_to_traverse == (list_t) NULL )
		return;

	if ( klist_auto_lock(list_to_traverse) < 0 )
		return;

	if ( ! is_indexed(list_to_traverse) )
	{
		cur_node = list_to_traverse->first_element;

		while ( cur_node )
		{
				/* If the list is an abstract list, then */
				/*  pass a pointer to the data element   */
				/*  instead of the list node.            */

			if ( list_to_traverse->list_type == ABSTRACT )
				(*user_func)
				 (((link_list_ptr)cur_node)->element);
			else
				(*user_func)(cur_node);

			cur_node = (*list_to_traverse->next_node)(cur_node);
		}
	}
	else
	{
		cur_pos = 1;

		while ( cur_pos <= list_to_traverse->number_of_elements )
		{
			cur_node = __nth_element(list_to_traverse, cur_pos);
			(*user_func)(cur_node);
			cur_pos++;
		}
	}

	klist_auto_unlock(list_to_traverse);
}



/******************************************************************************
 ******************************************************************************
 **
 **   CLASS: list
 **
 **  METHOD: clear_list
 **
 ** PURPOSE: Remove all elements from the given list.  This empties the list
 **          and frees the resources used by those elements in the list without
 **          destroying the list itself so that the list can continue to be
 **          used.
 **/

static void	clear_list (list_t c_list)
{
	if ( c_list == (list_t) NULL )
		return;

	if ( klist_auto_lock(c_list) < 0 )
		return;

	cleanup_index(c_list);

		/* For an abstract list, we need to free the memory */
		/*  for the abstract elements themselves.           */

	if ( c_list->list_type == ABSTRACT )
	{
			/* Change the type of list in order to keep */
			/*  the foreach function from passing the   */
			/*  pointer to the user data to free.       */
			/*  Also, indicate that the index is not    */
			/*  valid any more - this is important.     */

		c_list->list_type = USER_LIST;

		foreach_node(c_list, (foreach_func_t) do_free);

		c_list->list_type = ABSTRACT;
	}

	c_list->number_of_elements = 0;
	c_list->first_element      = NULL;
	c_list->last_element       = NULL;

	klist_auto_unlock(c_list);
}



/******************************************************************************
 ******************************************************************************
 **
 **   CLASS: list
 **
 **  METHOD: merge_list
 **
 ** PURPOSE: Merge the second list into the first list. All of the members of
 **          the second list are inserted into the first using insert().
 **
 ** NOTES:
 **	- (DEADLOCK WARNING!) For auto-locking, the two lists given must always
 **	  be locked, by all processes using the two lists, in the same order as
 **	  used here; otherwise, there is a potential for deadlock if the second
 **	  list is locked by another process that then attempts to lock the
 **	  first list after this function locks the first list.
 **
 ** RETURNS:
 **	SUCCESS	- If the elements of the given list are successfully added to
 **		  this list.
 **	-1	- If an error occurs merging the elements of the given list.
 **/

static int	 __merge_list (list_t list1, list_t list2)
{
	int	cur_el;
	int	total_el;
	void	*one_el;
	int	ret_code;

	ret_code = SUCCESS;

	if ( ( list2 != (list_t) NULL ) && ( list1 != (list_t) NULL ) )
	{
		total_el = list_length(list2);

		cur_el = 1;

		while ( cur_el <= total_el )
		{
			one_el = __nth_element(list2, cur_el);

			insert_at_end(list1, one_el);

			cur_el++;
		}
	}
	else
		ret_code = -1;

	return	ret_code;
}

static int	 merge_list (list_t list1, list_t list2)
{
	int	cur_el;
	int	total_el;
	void	*one_el;
	int	ret_code;

	if ( ( list1 == (list_t) NULL ) || ( list2 == (list_t) NULL ) )
		return	-1;

	if ( klist_auto_lock(list1) < 0 )
		return	-1;
	else
		if ( klist_auto_lock(list2) < 0 )	/* DEADLOCK WARNING! */
		{
			klist_auto_unlock(list1);
			return	-1;
		}

	ret_code = __merge_list(list1, list2);

	klist_auto_unlock(list1);
	klist_auto_unlock(list2);

	return(ret_code);
}



/******************************************************************************
 ******************************************************************************
 **
 **   CLASS: list
 **
 **  METHOD: dup_list
 **
 ** PURPOSE: Make a copy of the given list.  Elements are copied by their
 **          value; the caller must take care of conflicts.
 **
 ** NOTES:
 **	- The new list uses the same comparison and search functions as the
 **	  original.
 **	- If the given list is a user list, the new list also uses the same
 **	  insert and next functions as the original.
 **
 ** RETURNS:
 **	NULL	- If an error occurs making a copy of the given list.
 **	list_t	- The new list.
 **/

static list_t	dup_list (list_t d_list)
{
	list_t	result;

	if ( d_list == (list_t) NULL )
		return	(list_t) NULL;		/* Sorry */

	if ( klist_auto_lock(d_list) < 0 )
		return	(list_t) NULL;


		/* Make sure to use NULL for the add and next nodes if */
		/*  the original is an abstract list; otherwise, the   */
		/*  the new list will be treated as a user list which  */
		/*  limits some of its functionality (e.g. remove_el). */

	if ( d_list->list_type == ABSTRACT )
		result = create_list(NULL, d_list->element_compare,
		                     d_list->search_func, NULL);
	else
		result = create_list(d_list->next_node, d_list->element_compare,
		                     d_list->search_func, d_list->add_node);

	if ( result != (list_t) NULL )
	{
			/* OK, now copy all elements from the given list to */
			/*  the new list and make sure that is successful.  */

		if ( __merge_list(result, d_list) != SUCCESS )
		{
			destroy_list(result);
			result = (list_t) NULL;
		}
	}

	klist_auto_unlock(d_list);

	return	result;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: list_length ()
 **
 ** PURPOSE:  Determine the length of the list.
 **
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

static int	list_length (list_t list_to_count)
{
		/* Note: no need to lock this - only an integer sized value */
		/*  is being referenced.                                    */

	if ( list_to_count == (list_t) NULL )
		return	0;

	return(list_to_count->number_of_elements);
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: first_element ()
 **
 ** PURPOSE:  Return the first element in the given list.  Note that this
 **           is the first element in the list and NOT the first from the
 **           index, and therefere is not necessarily the first logical
 **           element after sorting.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static void	*first_element (list_t p_list)
{
	void		*ret;
	link_list_ptr	link_ptr;

	if ( p_list == (list_t) NULL )
		return	(void *) NULL;

	if ( klist_auto_lock(p_list) < 0 )
		return	(void *) NULL;


		/* If this is an abstract list, make sure to return a  */
		/*  pointer to the data element, and not the list node */

	if ( p_list->list_type == ABSTRACT )
	{
		link_ptr = (link_list_ptr) p_list->first_element;

			/* Make sure that the first node exists, */
			/*  otherwise return NULL.               */

		if ( link_ptr )
			ret = (void *) link_ptr->element;
		else
			ret = NULL;
	}
	else
		ret = (void *) p_list->first_element;

	klist_auto_unlock(p_list);

	return	ret;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: next_abs_node ()
 **
 ** PURPOSE:  Given a void pointer to the beginning of an abstract linked list
 **           node, return a pointer to the beginning of the next node in the
 **           list.
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

static void	*next_abs_node (void *a_ptr)
{
	link_list_ptr	abs_ptr;

	abs_ptr = (link_list_ptr) a_ptr;

	return(abs_ptr->next);
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: add_abs_node ()
 **
 ** PURPOSE:  Given a void pointer to the beginning of the first linked list
 **           node in an abstract linked list, and a pointer to user data, add
 **           a new node to the abstract linked list which points to the user
 **           data.  This record will be added at the front of the list.
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

static void	*add_abs_node (void *data_ptr, void *front_of_list)
{
	link_list_ptr	new_node;

		/* Allocate the memory for this element and make it */
		/*  point to the user's data.                       */

	new_node = (link_list_ptr) do_malloc(sizeof(struct link_list_struct));

	if ( new_node )
	{
		new_node->element = data_ptr;
		new_node->next    = (link_list_ptr) front_of_list;
	}

	return (new_node);
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: add_abs_node_at_end ()
 **
 ** PURPOSE:  Given a void pointer to the beginning of the last linked list
 **           node in an abstract linked list, and a pointer to user data, add
 **           a new node to the abstract linked list which points to the user
 **           data.  This record will be added at the end of the list.
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

static void	*add_abs_node_at_end (void *data_ptr, void *e_o_list)
{
	link_list_ptr	new_node;
	link_list_ptr	end_of_list;

	end_of_list = (link_list_ptr) e_o_list;

		/* Allocate the memory for this element and make it */
		/*  point to the user's data.                       */

	new_node = (link_list_ptr) do_malloc(sizeof(struct link_list_struct));



	if ( new_node )
	{
			/** Add this node at the end **/

		if ( end_of_list )
			end_of_list->next = new_node;

		new_node->element = data_ptr;

			/** Being the last node in the list, indicate **/
			/**  this by pointing to nothing              **/

		new_node->next    = (link_list_ptr) NULL;
	}

	return (new_node);
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: destroy_list ()
 **
 ** PURPOSE:  Free all of the memory held by the given list.  This will
 **           make the list completely unusable, and therefore it would
 **           be necessary to use the create function before using this
 **           list again.  Note, however, that this function does NOT
 **           free memory used by user data, even if that data was used
 **           in this list.
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

static void	destroy_list (list_t d_list)
{
	klist_index_id_t	cur_ind;
	int		cur_el;
	void		*one_node;

	if ( d_list == (list_t) NULL )
		return;

	if ( klist_auto_lock(d_list) < 0 )
		return;

	if ( d_list->indexes != NULL )
	{
		cur_ind = 1;

		while ( cur_ind <= d_list->index_arr_size )
		{
			__drop_index(d_list, cur_ind);
			cur_ind++;
		}

		do_free(d_list->indexes);
		d_list->indexes        = NULL;
		d_list->index_arr_size = 0;
	}

	if ( is_indexed(d_list) )
	{
		do_free(d_list->element_list);
		d_list->element_list = NULL;
	}

	if ( d_list->list_type == ABSTRACT )
	{
			/* Change the type of list in order to keep */
			/*  the foreach function from passing the   */
			/*  pointer to the user data to free.       */
			/*  Also, indicate that the index is not    */
			/*  valid any more - this is important.     */

		d_list->list_type  = USER_LIST;
		d_list->index_mode = NOT_INDEXED;

		while ( d_list->first_element != (void *) NULL )
		{
			one_node = d_list->first_element;

			d_list->first_element =
				(*d_list->next_node)(d_list->first_element);

			do_free(one_node);
		}
	}

#ifdef MODULE
        MOD_DEC_USE_COUNT;
#endif

		/* I am tempted to leave the list marked as being locked  */
		/*  since any further access after this function would be */
		/*  illegal, but that leaves other issues and would not   */
		/*  solve the actual problem.                             */

	klist_auto_unlock(d_list);

		/* Free the memory used by the list itself */
	do_free(d_list);
}



/**
 ** FUNCTION: __find_remove_pos
 **
 **  PURPOSE: Given a list, an element in the list, an index on the list, and
 **           the search function for the index, find the element in the index
 **           and return its position.
 **/

static int	__find_remove_pos (list_t r_list, void *ele,
		                   comp_func_t comp_cb, void **index_arr)
{
	void	**search_result;
	void	**cur;
	int	match_f;
	int	ret_code;


		/* Use the index search to find a match; note that this */
		/*  may not be the element requested since the sort may */
		/*  not key on a unique index.                          */

	search_result = (void **) bsearch(ele, index_arr,
	                                  r_list->number_of_elements,
	                                  sizeof (void *), comp_cb, TRUE);

	if ( search_result == NULL )
	{
		ret_code = 0;
		goto	__find_remove_pos__out;
	}

		/* Is this the correct element? */

	if ( search_result[0] == ele )
	{
			/* Return the offset into the index. */

		ret_code = search_result - index_arr;
		goto	__find_remove_pos__out;
	}

		/* Search toward the start of the index for the correct      */
		/*  element.  Stop at the start of the list, when the        */
		/*  element is found, or the index elements no longer match. */

	ret_code = 0;
	match_f  = TRUE;
	cur      = search_result - 1;

	while ( ( cur > index_arr ) && ( ret_code == 0 ) && ( match_f ) )
	{
		if ( cur[0] == ele )
			ret_code = cur - index_arr;
		else if ( comp_cb(ele, cur[0]) != 0 )
			match_f = FALSE;

		cur--;
	}

	if ( ret_code != 0 )
		goto	__find_remove_pos__out;


		/* Not found that direction; try toward the end of the */
		/*  index.                                             */

	match_f = TRUE;
	cur     = search_result + 1;

	while ( ( cur < ( index_arr + r_list->number_of_elements ) ) &&
	        ( ret_code == 0 ) && ( match_f ) )
	{
		if ( cur[0] == ele )
			ret_code = cur - index_arr;
		else if ( comp_cb(ele, cur[0]) != 0 )
			match_f = FALSE;

		cur++;
	}

__find_remove_pos__out:
	return	ret_code;
}



/**
 ** FUNCTION: alt_index_delete
 **
 **  PURPOSE: Given a list, the identifier for an alternate index attached to
 **           the list, and an entry to remove from the index, remove the entry
 **           from the index now.
 **
 ** RULES:
 **	- The index size must NOT already have been adjusted.
 **	- An unused index position will be left at the end of the index on
 **	  success.
 **	- The index will be set to "NEEDS_UPDATE" if the update fails.
 **	- The list's number_of_elements attribute must still include the
 **	  element being removed.
 **/

static void	alt_index_delete (list_t r_list, klist_index_id_t id,
		                  void *ele)
{
	klist_index_t	*index;
	int		move_size;
	int		remove_pos;

		/* If the index is not in-use, skip the update.  Also, if  */
		/*  the index's data array is not up-to-date, don't bring  */
		/*  it up-to-date now as there may be more list updates    */
		/*  before the index is needed.                            */

	if ( r_list->indexes[id - 1] == NULL )
		goto	alt_index_delete__out;

	index = r_list->indexes[id - 1];

	if ( ( index->state    == KLIST_INDEX__NEEDS_UPDATE ) ||
	     ( index->elements == NULL ) )
		goto	alt_index_delete__out;


		/* If lazy update is set, just set the index state to "needs */
		/*  update".                                                 */

	if ( index->flags & KLIST_INDEX__LAZY_UPD )
	{
		index->state = KLIST_INDEX__NEEDS_UPDATE;
		goto	alt_index_delete__out;
	}

		/* Find the removal point. */

	remove_pos = __find_remove_pos(r_list, ele, index->comp_func,
	                               index->elements);

		/* If no match was found, just set the index state to "needs */
		/*  update" and continue.                                    */

	if ( remove_pos < 1 )
	{
		index->state = KLIST_INDEX__NEEDS_UPDATE;
		goto	alt_index_delete__out;
	}


		/* Shift the remaining contents to cover the space for the */
		/*  removed element.  If the removed element is the last   */
		/*  in the index, nothing needs to be done.                */

	if ( remove_pos < ( r_list->number_of_elements - 1 ) )
	{
			/* Determine the amount of data to move to make */
			/*  space in the index.                         */

		move_size = ( r_list->number_of_elements - (remove_pos + 1) ) *
		            sizeof (void *);

			/* Move it now; the target and destination are in */
			/*  the same memory space (i.e. overlapping).     */

		memmove(&(index->elements[remove_pos]),
		        &(index->elements[remove_pos + 1]), move_size);
	}

	index->elements[r_list->number_of_elements - 1] = NULL;

alt_index_delete__out:

	return;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: remove_element ()
 **
 **  PURPOSE: Remove the element with the given search value from a list.  The
 **           element is looked for using the user supplied search_func and a
 **           linear search.
 **
 **     NOTE: Current this function only works for ABSTRACT lists.  Also, any
 **           index on the list is lost after removing a node.
 **
 ** RETURN VALUE:
 **	NULL	- If the element was not found in the list.
 **	< 0	- If an error occurred or the list type is not supported.
 **	> 0	- The address of the user's data element that was removed.
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

static void	*remove_element (list_t list, void *search_val)
{
	klist_index_id_t	cur_ind;
	link_list_ptr		last_node;
	link_list_ptr		cur_node;
	void			*removed_data;
	int			found;

	if ( list == (list_t) NULL )
		return	(void *) -1;

	if ( list->list_type != ABSTRACT )
		return	(void *) -1;

	if ( klist_auto_lock(list) < 0 )
		return	(void *) -1;

	last_node = NULL;
	cur_node  = (link_list_ptr) list->first_element;
	found     = FALSE;

	while ( ( ! found ) && ( cur_node ) )
	{
		if ( list->search_func(search_val, cur_node->element) )
		{
			last_node = cur_node;
			cur_node  = cur_node->next;
		}
		else
			found = TRUE;
	}


	if ( found )
	{
			/* Remember the user data of the removed node */
			/*  so that we may return it to the user      */

		removed_data = cur_node->element;


			/* If the very first element in the list is the */
			/*  one to remove, then we need to change the   */
			/*  pointer to the beginning of the list.       */

		if ( last_node == NULL )
			list->first_element = (void *) cur_node->next;
		else
			last_node->next = cur_node->next;

			/** If the very last element is being removed,  **/
			/**  then move the last element pointer back to **/
			/**  the immediately preceeding element.        **/

		if ( list->last_element == cur_node )
			list->last_element = last_node;


			/* Our data structure is cleaned, but not the user's */

		do_free(cur_node);

		cleanup_index(list);

		cur_ind = 1;

		while ( cur_ind <= list->index_arr_size )
		{
			alt_index_delete(list, cur_ind, removed_data);
			alt_index_resize(list, cur_ind, -1);

			cur_ind++;
		}

		list->number_of_elements--;
	}
	else
		removed_data = NULL;

	klist_auto_unlock(list);

	return	removed_data;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: remove_nth ()
 **
 **  PURPOSE: Remove the nth element from the given list.
 **
 **     NOTE: Current this function only works for ABSTRACT lists.  Also, any
 **           index on the list is lost after removing a node.
 **
 ** RETURN VALUE:
 **	NULL	- If the nth element was not in the list.
 **	< 0	- If an error occurred or the list type is not supported.
 **	> 0	- The address of the user's data element that was removed.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/17/1996	ARTHUR N	Initial Release
 **
 ******************************************************************************
 ******************************************************************************
 **/

static void	*remove_nth (list_t list, int nth)
{
	klist_index_id_t	cur_ind;
	link_list_ptr		last_node;
	link_list_ptr		cur_node;
	void			*removed_data;

	if ( list == (list_t) NULL )
		return	(void *) -1;

	if ( ( list->list_type != ABSTRACT ) || ( nth < 1 ) )
		return	(void *) -1;

	if ( klist_auto_lock(list) < 0 )
		return	(void *) -1;

/* TBD: this removes the nth linked-list element, which is NOT always the */
/*      same as that returned by the nth_element function!                */

/* *** USER WARNING - THIS WILL BE CHANGED ***                            */

	last_node = NULL;
	cur_node  = (link_list_ptr) list->first_element;

	while ( ( nth > 1 ) && ( cur_node != (link_list_ptr) NULL ) )
	{
		last_node = cur_node;
		cur_node  = cur_node->next;
		nth--;
	}


	if ( ( nth == 1 ) && ( cur_node != (link_list_ptr) NULL ) )
	{
			/* Remember the user data of the removed node */
			/*  so that we may return it to the user      */

		removed_data = cur_node->element;


			/* If the very first element in the list is the */
			/*  one to remove, then we need to change the   */
			/*  pointer to the beginning of the list.       */

		if ( last_node == NULL )
			list->first_element = (void *) cur_node->next;
		else
			last_node->next = cur_node->next;

			/** If the very last element is being removed,  **/
			/**  then move the last element pointer back to **/
			/**  the immediately preceeding element.        **/

		if ( list->last_element == cur_node )
			list->last_element = last_node;

		cur_ind = 1;

		while ( cur_ind <= list->index_arr_size )
		{
			alt_index_delete(list, cur_ind, removed_data);
			alt_index_resize(list, cur_ind, -1);

			cur_ind++;
		}


			/* Our data structure is cleaned, but not the user's */

		do_free(cur_node);

		cleanup_index(list);

		list->number_of_elements--;
	}
	else
		removed_data = NULL;

	klist_auto_unlock(list);

	return	removed_data;
}



/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION: remove_node ()
 **
 **  PURPOSE: Remove a node from the given list which is pointed to by the
 **           given node pointer.
 **
 **     NOTE: Current this function only works for ABSTRACT lists.  Also, any
 **           index on the list is lost after removing a node.
 **
 ** RETURN VALUE:
 **	NULL	- If the node was not found in the list.
 **	< 0	- If an error occurred or the list type is not supported.
 **	> 0	- The address of the user's data element that was removed.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

static void	*remove_node (list_t list, void *node_ptr)
{
	klist_index_id_t	cur_ind;
	link_list_ptr		last_node;
	link_list_ptr		cur_node;
	void			*removed_data;
	int			found;

	if ( list == (list_t) NULL )
		return	(void *) -1;

	if ( list->list_type != ABSTRACT )
		return	(void *) -1;

	if ( klist_auto_lock(list) < 0 )
		return	(void *) -1;

	last_node = NULL;
	cur_node  = (link_list_ptr) list->first_element;
	found     = FALSE;

	while ( ( ! found ) && ( cur_node ) )
	{
			/* Is this the user element we are looking for? */

		if ( cur_node->element != node_ptr )
		{
			last_node = cur_node;
			cur_node  = cur_node->next;
		}
		else
			found = TRUE;
	}


	if ( found )
	{
			/* Remember the user data of the removed node */
			/*  so that we may return it to the user      */

		removed_data = cur_node->element;


			/* If the very first element in the list is the */
			/*  one to remove, then we need to change the   */
			/*  pointer to the beginning of the list.       */

		if ( last_node == NULL )
			list->first_element = (void *) cur_node->next;
		else
			last_node->next = cur_node->next;


			/** If the very last element is being removed,  **/
			/**  then move the last element pointer back to **/
			/**  the immediately preceeding element.        **/

		if ( list->last_element == cur_node )
			list->last_element = last_node;

		cur_ind = 1;

		while ( cur_ind <= list->index_arr_size )
		{
			alt_index_delete(list, cur_ind, removed_data);
			alt_index_resize(list, cur_ind, -1);

			cur_ind++;
		}

			/* Our data structure is cleaned, but not the user's */

		do_free(cur_node);

		cleanup_index(list);

		list->number_of_elements--;
	}
	else
		removed_data = NULL;

	klist_auto_unlock(list);

	return	removed_data;
}



/******************************************************************************
 ******************************************************************************
 **
 **				DEBUGGING PURPOSES ONLY
 **
 ** FUNCTION: debug_list ()
 **
 **  PURPOSE: Produce a diagnostic output to standard error which can be used
 **           to aid in debugging a list management problem.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 06/11/1996	ARTHUR N	Initial Release
 ** 02/20/1998	ARTHUR N	Added auto-locking of the list.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifdef DEBUG_LISTS

static void	debug_list (list_t d_list)
{
	void	*ptr;
	int	val, loop;

	if ( klist_auto_lock(d_list) < 0 )
		return;

	printk("NUM ELS: %d\n\r",  d_list->number_of_elements);
	printk("FIRST EL: %x\n\r", d_list->first_element);
	printk("EL LIST: %x\n\r",  d_list->element_list);

	val = d_list->number_of_elements;

	for (ptr = d_list->first_element, loop = 0; loop < val; loop++ )
	{
		printk(" (%x, %x) --> ", ptr, d_list->element_list[loop]);

		ptr = (*d_list->next_node)(ptr);
	}

	printk("\n\n");

	klist_auto_unlock(d_list);
}

#endif

		/*********************************/
		/** SORTING/SEARCHING FUNCTIONS **/
		/*********************************/

/**
 ** FUNCTION: swap_bytes
 **
 **  PURPOSE: Swap two blocks of data of the specified size.
 **/

static void inline	swap_bytes (void *one, void *two, size_t num)
{
	char	buf[256];

	while ( num > 0 )
	{
		if ( num > sizeof(buf) )
		{
			memcpy(buf, one, sizeof(buf));
			memcpy(one, two, sizeof(buf));
			memcpy(two, buf, sizeof(buf));
			num -= sizeof(buf);
		}
		else
		{
			memcpy(buf, one, num);
			memcpy(one, two, num);
			memcpy(two, buf, num);
			num = 0;
		}
	}
}



/**
 ** FUNCTION: ssort
 **
 **  PURPOSE: Sort the given array of elements using a selection sort.
 **
 ** NOTES:
 **	- This is my replacement for qsort(); I am using a selection sort
 **	  for two reasons: 1) I have only written a couple of qsorts before;
 **	  2) I am trying to get this done as quickly as possible.
 **/

#define ss_nth(b,n,s)	( (void *) ( ((char *)(b)) + ((n) * (s)) ) )

static void	ssort (void *base, size_t num_el, size_t size_el,
		       comp_func_t compar, int de_ref_f)
{
	void	*ele1;
	void	*ele2;
	size_t	pos;
	size_t	cur;
	size_t	smallest;

#if ! FASTER_AND_MORE_DANGEROUS
	if ( ( base == NULL ) || ( num_el <= 0 ) || ( size_el <= 0 ) ||
	     ( compar == NULL ) )
		return;
#endif

	if ( de_ref_f && ( size_el != (sizeof (void **)) ) )
	{
		printk("%s: de_ref_f set and size is %d, not %d!\n",
		       __FUNCTION__, size_el, sizeof (void **));
		return;
	}

	for ( pos = 0; pos < num_el - 1; pos++ )
	{
		smallest = pos;

		for ( cur = pos + 1; cur < num_el; cur++ )
		{
			ele1 = ss_nth(base, smallest, size_el);
			ele2 = ss_nth(base, cur, size_el);

			if ( de_ref_f )
			{
				if ( ele1 != NULL )
					ele1 = ((void **) ele1)[0];

				if ( ele2 != NULL )
					ele2 = ((void **) ele2)[0];
			}

			if ( compar(ele1, ele2) > 0 )
				smallest = cur;
		}

		if ( smallest != pos )
			swap_bytes(ss_nth(base, pos, size_el),
			           ss_nth(base, smallest, size_el), size_el);
	}
}



/**
 ** FUNCTION: bsearch
 **/

#define bs_nth(b,n,s)	( (void *) ( ((char *)(b)) + ((n) * (s)) ) )

static void	*bsearch (void *value, void *base, size_t num_el,
		          size_t size_el, comp_func_t compar, int de_ref_f)
{
	void	*ele;
	int	high;
	int	low;
	int	cur;
	int	not_found;

	if ( ( base == NULL ) || ( compar == (comp_func_t) NULL ) ||
	     ( num_el <= 0 ) )
		return	NULL;

	if ( de_ref_f && ( size_el != sizeof (void **) ) )
		return	NULL;

	low = 0;
	high = num_el - 1;

	do
	{
			/** Look at the middle **/

		cur = (int) ( low + high ) / 2;

		ele = bs_nth(base, cur, size_el);

		if ( de_ref_f && ( ele != NULL ) )
			ele = ((void **) ele)[0];

		not_found = compar(value, ele);

			/** If the element is before the current element, **/
			/**  or it is after the current element, then     **/
			/**  adjust the part of the list we still need to **/
			/**  look at.                                     **/

		if ( not_found < 0 )
			high = cur - 1;
		else
			if ( not_found > 0 )
				low = cur + 1;
	} while ( ( not_found ) && ( low <= high ) );

	if ( ! not_found )
		return	bs_nth(base, cur, size_el);
	else
		return	NULL;
}



			/***************************/
			/** KERNEL ACCESS CONTROL **/
			/***************************/

/**
 ** FUNCTION: lock_list
 **
 **  PURPOSE: Lock the given list in order to prevent simultaneous access by
 **           another process to the same list.
 **
 ** RETURNS:
 **	0	- On success.
 **	-EINTR	- If the process was interrupted while attempting to lock the
 **		  list.
 **/

static int	lock_list (list_t l_list)
{
	if ( l_list != (list_t) NULL )
		if ( down_interruptible(&(l_list->l_sem)) < 0 )
			return	-EINTR;

	return	0;
}



/**
 ** FUNCTION: unlock_list
 **
 **  PURPOSE: Unlock the given list that was locked through lock_list.
 **
 ** NOTES:
 **	- This function must only be used on the list after the list was
 **	  locked by the lock_list function.
 **/

static int	unlock_list (list_t l_list)
{
	if ( l_list != (list_t) NULL )
		up(&(l_list->l_sem));

	return	0;
}



/**
 ** FUNCTION: set_list_autolock
 **
 **  PURPOSE: Set the auto lock status for the given list to the specified
 **           value.
 **/

static void	set_list_autolock (list_t l_list, int do_locking)
{
	if ( l_list != (list_t) NULL )
		if ( do_locking )
			l_list->auto_lock = 1;
		else
			l_list->auto_lock = 0;
}



                            /*********************/
                            /**  INDEX SUPPORT  **/
                            /*********************/


/**
 ** FUNCTION: add_index
 **
 **  PURPOSE: Add a new index to the list.
 **/

static int	add_index (list_t list, comp_func_t cb_func, int flags,
		           klist_index_id_t *r_index_id)
{
	klist_index_t		**new_indexes;
	klist_index_t		*new_index;
	klist_index_id_t	result;
	int			old_size;
	int			new_size;
	int			ret_code;

		/* Validate the arguments given. */

	if ( ( list == (list_t) NULL ) || ( cb_func == (comp_func_t) NULL ) ||
	     ( r_index_id == NULL ) )
	{
		ret_code = -EINVAL;
		goto	add_index__out;
	}

		/* Lock the list, if necessary */

	ret_code = klist_auto_lock(list);
	if ( ret_code != 0 )
		goto	add_index__out;

	if ( list->index_arr_size == 0 )
	{
		list->indexes = do_malloc(sizeof list->indexes[0]);

		if ( list->indexes == NULL )
		{
			ret_code = -ENOMEM;
			goto	add_index__out_locked;
		}

		result               = 1;
		list->index_arr_size = 1;
		list->indexes[0]     = NULL;
	}
	else
	{
			/* Scan the array for an unused slot. */
		result = 1;
		while ( ( result <= list->index_arr_size ) &&
		        ( list->indexes[result - 1] != NULL ) )
		{
			result++;
		}

		if ( result <= list->index_arr_size )
			goto	add_index__have_slot;


			/* No available slot found; create one. */

		result = list->index_arr_size + 1;
		old_size = ( sizeof list->indexes[0] ) * list->index_arr_size;
		new_size = ( sizeof list->indexes[0] ) * result;

		new_indexes = my_realloc(list->indexes, old_size, new_size);

		if ( new_indexes == NULL )
		{
			ret_code = -ENOMEM;
			goto	add_index__out_locked;
		}

		new_indexes[result - 1] = NULL;
		list->indexes           = new_indexes;
		list->index_arr_size    = result;
	}

add_index__have_slot:

		/* Allocate memory for the index information structure. */

	new_index = do_malloc(sizeof new_index[0]);

	if ( new_index == NULL )
	{
		ret_code = -ENOMEM;
		goto	add_index__out_locked;
	}

		/* Setup the index information. */

	new_index->comp_func = cb_func;
	new_index->flags     = flags;
	new_index->state     = KLIST_INDEX__NEEDS_UPDATE;
	new_index->elements  = NULL;


	ret_code                  = 0;
	r_index_id[0]             = result;
	list->indexes[result - 1] = new_index;

		/* If immediate indexing is requested, attempt to index now. */
		/*  Don't return the error now since the index was created.  */

	(void) update_alt_index(list, result);

add_index__out_locked:

	klist_auto_unlock(list);

add_index__out:

	return	ret_code;
}



/**
 ** FUNCTION: drop_index
 **
 **  PUROPSE: Drop the specified index from the given list.
 **/

static void	__drop_index (list_t list, klist_index_id_t index_id)
{
	klist_index_t	*index;
	int		ret_code;

	if ( list->indexes[index_id - 1 ] == NULL )
		return;


		/* Remove the index from the indexes for this list, then */
		/*  release it.                                          */

	index = list->indexes[index_id - 1];
	list->indexes[index_id - 1] = NULL;

	if ( index->elements != NULL )
	{
		do_free(index->elements);
		index->elements = NULL;
	}

	index->comp_func = NULL;

	do_free(index);
}

static int	drop_index (list_t list, klist_index_id_t index_id)
{
	klist_index_t	*index;
	int		ret_code;

		/* Validate the arguments given. */

	if ( list == (list_t) NULL )
	{
		ret_code = -EINVAL;
		goto	drop_index__out;
	}

		/* Lock the list, if necessary */

	ret_code = klist_auto_lock(list);
	if ( ret_code != 0 )
		goto	drop_index__out;

	if ( ( index_id < 1 ) || ( index_id > list->index_arr_size ) ||
	     ( list->indexes[index_id - 1 ] == NULL ) )
	{
		ret_code = -EINVAL;
		goto	drop_index__out_locked;
	}

		/* Use the "local" version to do the real work. */

	__drop_index(list, index_id);

drop_index__out_locked:
	klist_auto_unlock(list);

drop_index__out:
	return	ret_code;
}



/**
 ** FUNCTION: read_index
 **
 **  PURPOSE: Obtain the data element at the specified location in an index
 **           attached to the given list.
 **/

static int	read_index (list_t list, klist_index_id_t index_id, int nth,
		            void **r_ele)
{
	klist_index_t	*index;
	int		ret_code;

		/* Validate the list argument given. */

	if ( list == (list_t) NULL )
	{
		ret_code = -EINVAL;
		goto	read_index__out;
	}

		/* Lock the list, if necessary */

	ret_code = klist_auto_lock(list);
	if ( ret_code != 0 )
		goto	read_index__out;

		/* Validate the remaining arguments based on the list's */
		/*  attributes.                                         */

	if ( ( index_id < 1 ) || ( index_id > list->index_arr_size ) ||
	     ( nth < 1 ) || ( nth > list->number_of_elements ) ||
	     ( list->indexes[index_id - 1] == NULL ) )
	{
		ret_code = -EINVAL;
		goto	read_index__out_locked;
	}


		/* Remember the index. */

	index = list->indexes[index_id - 1];

		/* If the index is not up-to-date, update it now. */

	if ( index->state != KLIST_INDEX__UP_TO_DATE )
	{
		ret_code = update_alt_index(list, index_id);

		if ( ret_code != 0 )
			goto	read_index__out_locked;
	}


		/* Return the element to the caller. */

	ret_code = 0;
	r_ele[0] = index->elements[nth - 1];

read_index__out_locked:
	klist_auto_unlock(list);

read_index__out:
	return	ret_code;
}



/**
 ** FUNCTION: search_index
 **
 **  PURPOSE: Given a list, index identifier, and data element, find the
 **           element in the index and return its position.
 **/

static int	search_index (list_t list, klist_index_id_t index_id,
		              void *data, int *r_pos)
{
	void		*answer;
	klist_index_t	*index;
	int		ret_code;

		/* Validate the list argument given. */

	if ( list == (list_t) NULL )
	{
		ret_code = -EINVAL;
		goto	search_index__out;
	}

		/* Lock the list, if necessary */

	ret_code = klist_auto_lock(list);
	if ( ret_code != 0 )
		goto	search_index__out;

		/* Validate the remaining arguments based on the list's */
		/*  attributes.                                         */

	if ( ( index_id < 1 ) || ( index_id > list->index_arr_size ) ||
	     ( list->indexes[index_id - 1] == NULL ) )
	{
		ret_code = -EINVAL;
		goto	search_index__out_locked;
	}


		/* Remember the index. */
	index = list->indexes[index_id - 1];

		/* If the index is not up-to-date, update it now. */

	if ( index->state != KLIST_INDEX__UP_TO_DATE )
	{
		ret_code = update_alt_index(list, index_id);

		if ( ret_code != 0 )
			goto	search_index__out_locked;
	}


		/* Search the index for the element given. */

	answer = bsearch(data, index->elements, list->number_of_elements,
		         sizeof (void *), index->comp_func, TRUE);

	if ( answer == NULL )
	{
		ret_code = -ENOENT;
		goto	search_index__out_locked;
	}

		/* Return the position of the element. */
	ret_code = 0;
	r_pos[0] = (((char *) answer - (char *) index->elements) /
		    sizeof(void *)) + 1;

search_index__out_locked:
	klist_auto_unlock(list);

search_index__out:
	return	ret_code;
}



/**
 ** FUNCTION: change_index_sort
 **
 **  PURPOSE: Change the current sort of the specified index to the one
 **           specified by the given sort/search callback.
 **/

static int	change_index_sort (list_t list, klist_index_id_t index_id,
		                   comp_func_t new_cb)
{
	void		*answer;
	klist_index_t	*index;
	int		ret_code;

		/* Validate the list argument given. */

	if ( list == (list_t) NULL )
	{
		ret_code = -EINVAL;
		goto	change_index_sort__out;
	}

		/* Lock the list, if necessary */

	ret_code = klist_auto_lock(list);
	if ( ret_code != 0 )
		goto	change_index_sort__out;


		/* Validate the remaining arguments based on the list's */
		/*  attributes.                                         */

	if ( ( index_id < 1 ) || ( index_id > list->index_arr_size ) ||
	     ( list->indexes[index_id - 1] == NULL ) )
	{
		ret_code = -EINVAL;
		goto	change_index_sort__out_locked;
	}


		/* Remember the index. */

	index = list->indexes[index_id - 1];

		/* Mark the index as out of date and set its sort function. */

	index->state     = KLIST_INDEX__NEEDS_UPDATE;
	index->comp_func = new_cb;


		/* If lazy index update is not set, update it now. */

	if ( ! ( index->flags & KLIST_INDEX__LAZY_UPD ) )
		(void) update_alt_index(list, index_id);

change_index_sort__out_locked:
	klist_auto_unlock(list);

change_index_sort__out:
	return	ret_code;
}



			/**********************/
			/** MODULE INTERFACE **/
			/**********************/


#ifdef MODULE

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arthur Naseef");
MODULE_DESCRIPTION("Generic Link List support for Linux");

/* #ifdef EXPORT_SYMBOL	-- would be nice. For want of a better solution! */

#if POST_20_KERNEL_F

EXPORT_SYMBOL(kernel_lists);

# else

static struct symbol_table	klists_symtab = {
#  include <linux/symtab_begin.h>
	X(kernel_lists),
#  include <linux/symtab_end.h>
} ;

# endif


/**
 ** FUNCTION: init_module
 **
 **  PURPOSE: Initialize the module.
 **/

int	init_module ()
{
#if KDEBUG || KLIST_BANNER
	printk("starting kernel list module (compiled %s)\n",
	       macro2str(DATE_STAMP));
#endif

#ifndef EXPORT_SYMBOL
	register_symtab(&klists_symtab);
#endif

	return	0;
}

void	cleanup_module()
{
#if KDEBUG
	printk("removing kernel list module\n");
#endif
}

#endif
