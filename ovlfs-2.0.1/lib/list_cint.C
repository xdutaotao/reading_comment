//////////////////////////////////////////////////////////////////////////////
//
// COPYRIGHT (C) 1998-2003 By Arthur Naseef
//
// This file is covered under the GNU General Public License Version 2.  For
//  more information, see the file COPYING.
//
//
// FILE: list_cint.C
//
// PURPOSE: This file contains the C interface functions to the list class.
//
//
// REVISION HISTORY:
//
// DATE		AUTHOR		DESCRIPTION
// ==========	===============	=============================================
// 2003-05-25	ARTHUR N.	Initial Release (as part of ovlfs).
//
//////////////////////////////////////////////////////////////////////////////

#ident "@(#)list_cint.C	1.1	[03/07/27 22:20:36]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

extern "C" {

//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION: List_create
//
// PURPOSE:
//

List_t	List_create (Comp_func_t comp_func)
{
	list	*new_list;

	new_list = new list(comp_func);

	return((List_t) new_list);
}

List_t	List_create_abs ()
{
	list	*new_list;

	new_list = new list;

	return((List_t) new_list);
}

void	List_destroy (List_t list_ptr)
{
	list	*aList;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		delete aList;
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION: list_insert
//
//  PURPOSE: Insert the new element into the list.
//

int	List_insert (List_t list_ptr, List_el_t new_element)
{
	list	*aList;
	int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->insert(new_element);
	}
	else
		ret_code = -1;

	return(ret_code);
}

int	List_insert_ordered (List_t list_ptr, List_el_t new_element)
{
	list	*aList;
	int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->insert_ordered(new_element);
	}
	else
		ret_code = -1;

	return(ret_code);
}

int	List_remove (List_t list_ptr, List_el_t rm_element)
{
	list	*aList;
	int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->remove(rm_element);
	}
	else
		ret_code = -1;

	return(ret_code);
}

int	List_mark_for_removal (List_t list_ptr, List_el_t rm_element)
{
	list	*aList;
	int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->mark_for_removal(rm_element);
	}
	else
		ret_code = -1;

	return(ret_code);
}

int	List_mark_nth_for_removal (List_t list_ptr, int rm_element)
{
	list	*aList;
	int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->mark_nth_for_removal(rm_element);
	}
	else
		ret_code = -1;

	return(ret_code);
}

int	List_remove_marked (List_t list_ptr)
{
	list	*aList;
	int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->remove_marked();
	}
	else
		ret_code = -1;

	return(ret_code);
}


List_el_t	List_first (List_t list_ptr)
{
	list		*aList;
	List_el_t	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->first();
	}
	else
		ret_code = (List_el_t) -1;

	return(ret_code);
}

List_el_t	List_nth (List_t list_ptr, int n)
{
	list		*aList;
	List_el_t	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->nth(n);
	}
	else
		ret_code = (List_el_t) -1;

	return(ret_code);
}

unsigned int	List_length (List_t list_ptr)
{
	list		*aList;
	unsigned int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->length();
	}
	else
		ret_code = (unsigned int) -1;

	return(ret_code);
}


unsigned int	List_find_element (List_t list_ptr, List_el_t find_el)
{
	list		*aList;
	unsigned int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->find_element(find_el);
	}
	else
		ret_code = (unsigned int) -1;

	return(ret_code);
}

unsigned int	List_find_element_with_func (List_t list_ptr, List_el_t find_el,
		                             Comp_func_t comp_func)
{
	list		*aList;
	unsigned int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->find_element(find_el, comp_func);
	}
	else
		ret_code = (unsigned int) -1;

	return(ret_code);
}


int	List_foreach (List_t list_ptr, Foreach_func_t callback_func)
{
	list	*aList;
	int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->foreach(callback_func);
	}
	else
		ret_code = (int) -1;

	return(ret_code);
}


int	List_forall (List_t list_ptr, Forall_func_t callback_func)
{
	list	*aList;
	int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->forall(callback_func);
	}
	else
		ret_code = (int) -1;

	return(ret_code);
}


List_el_t	*List_convert_to_array(List_t list_ptr, int size, int *tot_len)
{
	list		*aList;
	List_el_t	*ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->convert_to_array(size, tot_len);
	}
	else
		ret_code = (List_el_t *) NULL;

	return(ret_code);
}


int	List_store_to_array (List_t list_ptr, List_el_t *array, int tot,
	                     int start)
{
	list	*aList;
	int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->store_to_array(array, tot, start);
	}
	else
		ret_code = -1;

	return	ret_code;
}


int	List_clear (List_t list_ptr)
{
	list	*aList;
	int	ret_code;

	if ( list_ptr != (List_t) NULL )
	{
		aList = (list *) list_ptr;

		ret_code = aList->clear();
	}
	else
		ret_code = -1;

	return	ret_code;
}

}	// End of extern "C" section
