/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998-2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 ** FILE: list.h
 **
 ** PURPOSE: This file contains definitions used by the list object.
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	=============================================
 ** 2003-05-25	ARTHUR N.	Initial Release (as part of ovlfs)
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)list.h	1.1	[03/07/27 22:20:36]"

#include <sys/types.h>

#include "libdefines.h"

	/* Protect the standard C section separately from the C++ section */
	/*  The standard C section is included in ALL files.              */

#if ! defined(_list_STD_C)
#define _list_STD_C


/******************************************************************************
 ******************************************************************************
 **
 ** CONSTANT DEFINITIONS
 **/

#define NULL_ELEMENT	(List_el_t) NULL

/** list errors: **/
#define _list_BASE_ERROR		0
#define _list_ERROR(n)			( _list_BASE_ERROR - (n) )
#define _list_MEMORY_ALLOC_FAIL		_list_ERROR(1)
#define _list_BAD_PROGRAMMING		_list_ERROR(2)
#define _list_BAD_ARGUMENT		_list_ERROR(3)
#define _list_NOT_FOUND			_list_ERROR(4)

/* index modes */
#define _list_NO_INDEX			0
#define _list_IS_INDEXED		1
#define _list_IS_SORTED			2

#define _list_ind_IS_SORTED		0
#define _list_ind_NOT_SORTED		1

/******************************************************************************
 ******************************************************************************
 **
 ** TYPE DEFINITIONS
 **/

typedef char	*List_el_t;
typedef int	(*Comp_func_t)(List_el_t, List_el_t);
typedef int	(*Foreach_func_t)(List_el_t);	/* Must return 0 on success */
typedef void	(*Forall_func_t)(List_el_t);

typedef int	(*Foreach_func2_t)(List_el_t, void *);
typedef void	(*Forall_func2_t)(List_el_t, void *);

# ifdef __cplusplus
typedef class list	*List_t;
# else
typedef void		*List_t;
# endif




/******************************************************************************
 ******************************************************************************
 **
 ** FUNCTION PROTOTYPES
 **/

# ifdef __cplusplus
extern "C" {
# endif

List_t		List_create (Comp_func_t);
List_t		List_create_abs ();
void		List_destroy (List_t);
int		List_insert (List_t, List_el_t);
int		List_insert_ordered (List_t, List_el_t);
int		List_remove (List_t, List_el_t);
int		List_mark_for_removal (List_t, List_el_t);
int		List_mark_nth_for_removal (List_t, int);
int		List_remove_marked (List_t);
List_el_t	List_first (List_t);
List_el_t	List_nth (List_t, int);
uint		List_length (List_t);
uint		List_find_element (List_t, List_el_t);
uint		List_find_element_with_func (List_t, List_el_t, Comp_func_t);
int		List_foreach (List_t, Foreach_func_t);
int		List_forall (List_t, Forall_func_t);
int		List_foreach2 (List_t, Foreach_func2_t, void *);
int		List_forall2 (List_t, Forall_func2_t, void *);
int		List_clear (List_t);

List_el_t	*List_convert_to_array(List_t, int, int *);
int		List_store_to_array(List_t, List_el_t *, int, int);

# ifdef __cplusplus
}
# endif

#endif


	/* Protect the C++ section */

#if defined(__cplusplus) && ! defined(_list_PLUS_PLUS)
#define _list_PLUS_PLUS		1

class list_element {
	public:
		list_element();
		~list_element();
		List_el_t	element();
		list_element	*next();
		int	set_element(List_el_t);
		int	set_next(list_element *Next);
		void	print();
		int	compare_el(list_element *, Comp_func_t = NULL);
		int	compare_el(List_el_t, Comp_func_t = NULL);
		void	mark_for_delete();
		int	is_marked_for_delete();

	protected:
		List_el_t	my_element;
		char		delete_flag;
		list_element	*Next;
} ;



//
// CLASS: list_index
//

class list_index {
	public:
		list_index(Comp_func_t = (Comp_func_t) NULL);
		~list_index();

		list_element	*nth(uint);
		List_el_t	nth_value(uint);

		uint		find_element(List_el_t, Comp_func_t = NULL);

		int	sort(Comp_func_t = NULL);
		int	set_nth(uint, list_element *);
		int	add(list_element *);
		int	expand(uint);
		int	resize(uint);
		int	insert(uint, list_element *, int = FALSE);
		int	insert_ordered(list_element *);
		int	remove(uint);
		int	clear();

		Comp_func_t	get_comp_func()	{ return this->compare; }

	private:
		int	do_compare(List_el_t el1, List_el_t el2)
		{
			if ( this->compare == (Comp_func_t) NULL )
				return	el1 - el2;
			else
				return	this->compare(el1, el2);
		}

		uint	find_element_binary(List_el_t, Comp_func_t = NULL);
		uint	find_element_linear(List_el_t, Comp_func_t = NULL);

	private:
		Comp_func_t	compare;

		list_element	**index;
		uint		index_size;
		uint		num_in_index;

		int		sort_ind;	// Just a boolean
} ;



//
// CLASS: list
//

class list {
	public:
		list(Comp_func_t comp_func = NULL);
		~list();

		int		insert(List_el_t);
		int		insert_at_front(List_el_t);
		int		insert_ordered(List_el_t);

		int		remove(List_el_t);
		int		remove_nth(int);
		int		mark_for_removal(List_el_t);
		int		mark_nth_for_removal(int);
		int		remove_marked();

		int		clear();	/* Remove all */

		List_el_t	nth(int);
		List_el_t	nth_ele(int);
		List_el_t	set_nth(int, List_el_t);
		List_el_t	set_nth_ele(int, List_el_t);
		List_el_t	first();
		uint		length();

		uint		find_element(List_el_t,
				             Comp_func_t comp_func = NULL);
		int		find_group(List_el_t, uint *, uint *);
		int		find_group(int, List_el_t, uint *, uint *);

			// Print elements as integers
		void		print(int offset = 16);
		void		print_index(int, int offset = 16);
		int		sort(int = TRUE);
		int		sort(Comp_func_t comp_func, int = TRUE);
		void		set_compare_func(Comp_func_t comp_func);

		int		foreach(Foreach_func_t);
		int		forall(Forall_func_t);
		int		foreach2(Foreach_func2_t, void *);
		int		forall2(Forall_func2_t, void *);
		list		*dup();
		int		merge(list *);

		int		compare(list *);	// Which is first?

			// Additional Index maintainance

		int		add_index(int *, Comp_func_t = NULL);
		int		drop_index(int);
		List_el_t	index_nth(int, int);
		int		index_sort(int, Comp_func_t = NULL);
		uint		index_find_element(int, List_el_t,
				                   Comp_func_t = NULL);


			// Convert list to an array of the elements stored
			//  add number of elements specified to end of array
		List_el_t	*convert_to_array(int extra = 0,
				                  int *size = NULL);
		int		store_to_array(List_el_t *, int,
				               int start = 1);

	protected:
		list_element	*first_node();
		list_element	*nth_node(int, int = -1);
		list_element	*find_node(List_el_t value,
				           list_element **last = NULL);
		uint		find_element_linear(List_el_t,
				                 Comp_func_t comp_func = NULL);
		uint		find_element_binary(List_el_t,
				                 Comp_func_t comp_func = NULL);
		uint		find_group_end(int, List_el_t, uint, int);

		int		(*compare_func)(List_el_t, List_el_t);
		int		insert_node(List_el_t);
		int		insert_node_at_front(List_el_t);
		int		insert_node_ordered(List_el_t);
		uint		find_insert_pos(List_el_t, int *);
		int		create_index(int = -1);
		int		reindex(uint);

			// Additional Index maintainance

		int		prepare_index(list_index *);
		int		reprepare_indices();
		int		sort_indices();
		int		add_to_indices(list_element *);
		int		insert_into_indices(list_element *, uint);
		int		add_to_indices_ordered(list_element *);


		uint		number_el;
		list_element	**index;
		list_element	**phys_index;
		list_index	**more_indices;	// currently unused
		int		num_indices;
		list_element	*First;
		list_element	*Last;
		int		index_mode;
		int		phys_index_mode;
} ;



//
// MACROS
//

#define grab_value_macro(e)	( (e) == (list_element *) NULL ? \
				  NULL_ELEMENT : (e)->element() )

#endif
