//////////////////////////////////////////////////////////////////////////////
//
// COPYRIGHT (C) 1998-2003 By Arthur Naseef
//
// This file is covered under the GNU General Public License Version 2.  For
//  more information, see the file COPYING.
//
// FILE: list_index.C
//
// DESCRIPTION:
//	This file contains the source code for the List Index class.
//
//
// REVISION HISTORY:
//
// DATE		AUTHOR		DESCRIPTION
// ==========	===============	=============================================
// 2003-05-25	ARTHUR N.	Initial Release (as part of ovlfs).
//
//////////////////////////////////////////////////////////////////////////////

#ident "@(#)list_index.C	1.1	[03/07/27 22:20:36]"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "list.h"

#if 0
 Comp_func_t	compare;
 list_element	**index;
 uint		index_size;
 uint		num_in_index;
 int		sort_ind;
#endif


//
// FUNCTION: find_insert_pos
//
//  PURPOSE: Find the insert position of the given element in the given array
//           of elements.
//
// NOTES:
//	- Returns 0 for the first position in the index.
//

static uint	find_insert_pos (list_element **index, int num,
		                 list_element *new_ele,
		                 Comp_func_t comp_func = NULL)
{
	unsigned int	low;
	unsigned int	high;
	unsigned int	mid_point;
	int		found;
	int		done;
	int		ret_code;

	low   = 0;
	high  = num - 1;
	found = FALSE;
	done  = FALSE;

		// Search until a matching element is found, or there are no
		//  more elements to look at.

	while ( ( low <= high ) && ( ! found ) && ( ! done ) )
	{
		mid_point = ( low + high ) / 2;

		ret_code = index[mid_point]->compare_el(new_ele, comp_func);

			// mid_point comes before element

		if ( ret_code < 0 )
			low = mid_point + 1;
		else
			if ( ret_code > 0 )
			{
				if ( mid_point == 0 )
					done = TRUE;

				high = mid_point - 1;
			}
			else
				found = TRUE;
	}

		// If a matching element is found, use its position as the
		//  insert position; otherwise, determine which side of the
		//  last element compared the new element falls on.

	if ( ! found )
		if ( ret_code < 0 )
			mid_point++;		// existing one comes first
		else
			mid_point--;		// new one comes first

	return	mid_point;
}



		////                    ////
		////  LIST INDEX CLASS  ////
		////                    ////

//
//  METHOD: list_index
//
// PURPOSE: This is the constructor for the List Index class.
//

list_index :: list_index (Comp_func_t comp_func)
{
	this->compare      = comp_func;
	this->index        = (list_element **) NULL;
	this->num_in_index = 0;
	this->index_size   = 0;
	this->sort_ind     = FALSE;
}



//
//  METHOD: ~list_index
//
// PURPOSE: This is the destructor for the List Index class.
//

list_index :: ~list_index ()
{
	if ( this->index != (list_element **) NULL )
	{
		free(this->index);
		this->index = (list_element **) NULL;
	}
}



//
//  METHOD: nth
//
// PURPOSE: Obtain the nth element in the index.
//

list_element	*list_index :: nth (uint ele_no)
{
	list_element	*result;

	if ( ( ele_no > this->num_in_index ) || ( ele_no < 1 ) )
		result = (list_element *) NULL;
	else
		result = this->index[ele_no - 1];

	return	result;
}



//
//  METHOD: nth_value
//
// PUPROSE: Obtain the value of the nth element in the index.
//

List_el_t	list_index :: nth_value (uint ele_no)
{
	List_el_t	result;

	if ( ( ele_no > this->num_in_index ) || ( ele_no < 1 ) )
		result = NULL_ELEMENT;
	else
	{
		if ( this->index[ele_no - 1] == (list_element *) NULL )
			result = NULL_ELEMENT;
		else
			result = this->index[ele_no - 1]->element();
	}

	return	result;
}



//
//  METHOD: find_element
//
// PURPOSE: Given an element that is in the index, find its position in the
//          index.
//

uint		list_index :: find_element (List_el_t ele,
		                            Comp_func_t comp_func)
{
	unsigned int	result;

	if ( comp_func == (Comp_func_t) NULL )
		comp_func = this->compare;


		// If the index is not sorted then use a linear search;
		//  otherwise, use a binary search

	if ( this->sort_ind )
		result = this->find_element_binary(ele, comp_func);
	else
		result = this->find_element_linear(ele, comp_func);

	return	result;
}



//
//  METHOD: sort
//
// PURPOSE: Sort the index.
//

static inline int	do_comp (Comp_func_t comp_func, list_element *ele1,
			         list_element *ele2)
{
	if ( ele1 == (list_element *) NULL )
		return	1;
	else
		return	ele1->compare_el(ele2, comp_func);
}

int	list_index :: sort (Comp_func_t comp_func)
{
	uint		cur_ele;
	uint		smallest;
	uint		look;
	list_element	*smallest_ele;

	if ( comp_func == (Comp_func_t) NULL )
		comp_func = this->compare;

	for ( cur_ele = 0; cur_ele < ( this->num_in_index - 1 ); cur_ele++ )
	{
		smallest = cur_ele;
		smallest_ele = this->index[smallest];

		for ( look = cur_ele + 1; look < this->num_in_index; look++ )
		{
			if ( do_comp(comp_func, smallest_ele,
			             this->index[look]) > 0 )
			{
				smallest = look;
				smallest_ele = this->index[smallest];
			}
		}

		if ( smallest != cur_ele )
		{
				// Swap this element with the "smallest" one

			this->index[smallest] = this->index[cur_ele];
			this->index[cur_ele] = smallest_ele;
		}
	}

	this->sort_ind = TRUE;

	return	SUCCESS;
}



//
//  METHOD: set_nth
//
// PURPOSE: Set the nth element in the index to the given element.  Note that
//          the element that already exists at this position is simply dropped
//          from the index; the caller is responsible for making sure the
//          memory it uses is freed.
//

int	list_index :: set_nth (uint ele_no, list_element *new_ele)
{
	uint	size;
	int	ret_code;

		// If this element's position is past the end of the existing
		//  index, resize the index to allow this element to be stored

	if ( ele_no > this->index_size )
		ret_code = this->resize(ele_no);
	else
		ret_code = SUCCESS;

	if ( ret_code == SUCCESS )
	{
		this->index[ele_no - 1] = new_ele;
		this->sort_ind = FALSE;

			// If this element's position is past the last element
			//  currently in use in the index, update the number of
			//  elements in the index and initialize all elements
			//  in between.

		if ( ele_no > this->num_in_index )
		{
			size = ( ele_no - this->num_in_index ) - 1;

			if ( size > 0 )
				memset(this->index + num_in_index, '\0',
				       size * sizeof(list_element *));

			this->num_in_index = ele_no;
		}
	}

	return	ret_code;
}



//
//  METHOD: add
//
// PURPOSE: Add the given element to the end of the index.
//

int	list_index :: add (list_element *new_ele)
{
	int	ret_code;
	uint	position;

		// Increment the count of elements in the index and remember
		//  the position for this element in the index.

	position = this->num_in_index;
	this->num_in_index++;

	if ( this->num_in_index >= this->index_size )
		ret_code = this->resize(this->num_in_index);
	else
		ret_code = SUCCESS;

	if ( ret_code == SUCCESS )
	{
		this->index[position] = new_ele;
		this->sort_ind = FALSE;
	}
	else
		this->num_in_index--;

	return	ret_code;
}



//
//  METHOD: expand
//
// PURPOSE: Expand the index by the specified number of elements.
//

int	list_index :: expand (uint num_ele)
{
	return	this->resize(this->index_size + num_ele);
}



//
//  METHOD: resize
//
// PURPOSE: Set the size of the index to the specified size.  Note that any
//          elements cut off of the index are simply dropped.
//

int	list_index :: resize (uint num_ele)
{
	list_element	**new_ind;
	int		ret_code = SUCCESS;

		// Make sure the request does not attepmt to erase the index

	if ( num_ele < 1 )
		return	_list_BAD_ARGUMENT;


		// Allocate the memory for the new index with realloc()

	new_ind = (list_element **) realloc(this->index,
	                                    num_ele * sizeof(list_element *));


		// If the allocation failed, keep the existing index, but
		//  return an error.

	if ( new_ind == (list_element **) NULL )
		ret_code = _list_MEMORY_ALLOC_FAIL;
	else
	{
		this->index = new_ind;
		this->index_size = num_ele;
	}

	return	ret_code;
}



//
//  METHOD: insert
//
// PURPOSE: Insert the given element into the index at the specified position.
//
// NOTES:
//	- If the index is already sorted, and the third argument is not zero,
//	  the index will maintain its sorted status.  Therefore, it is up to
//	  the caller, in that case, to make certain the element is being
//	  inserted at the correct position in the index.
//

int	list_index :: insert (uint ele_no, list_element *new_ele,
	                      int is_in_order_ind)
{
	uint	shift;
	uint	new_size;
	int	ret_code;

	if ( ele_no > this->num_in_index )
		new_size = ele_no;
	else
		new_size = this->num_in_index + 1;

	if ( new_size > this->index_size )
		ret_code = this->resize(new_size);
	else
		ret_code = SUCCESS;

	if ( ret_code == SUCCESS )
	{
			// Shift the elements to make space, unless this
			//  element is going at the end of the index.

		if ( ele_no < this->num_in_index )
		{
			shift = ( this->num_in_index - ele_no ) + 1;

			memmove(this->index + ele_no, this->index + ele_no - 1,
			        shift * sizeof(list_element *));
		}
		else if ( ( ele_no - 1 ) > this->num_in_index )
		{
				// Initialize the space in between

			shift = ( ele_no - this->num_in_index ) - 1;

			memset(this->index + this->num_in_index, '\0',
			       shift * sizeof(list_element *));
		}

		this->index[ele_no - 1] = new_ele;
		this->num_in_index = new_size;


			// If the element is not inserted in sorted order,
			//  indicate the index is not sorted

		if ( ! is_in_order_ind )
			this->sort_ind = FALSE;
	}

	return	ret_code;
}



//
//  METHOD: insert_ordered
//
// PURPOSE: Insert the given element into this index in sorted position.  If
//          the index is not already sorted, it will be sorted.
//

int	list_index :: insert_ordered (list_element *new_ele)
{
	uint	pos;
	uint	new_size;
	uint	shift;
	int	ret_code = SUCCESS;

	if ( this->sort_ind )
	{

			// Strecth out the index, if it is not large enough

		new_size = this->num_in_index + 1;

		if ( new_size > this->index_size )
			ret_code = this->resize(new_size);

		if ( ret_code == SUCCESS )
		{
				// Find the position in the list to insert the
				//  element.

			pos = find_insert_pos(this->index, this->num_in_index,
			                      new_ele, this->compare);

				// If the insert position is the end of the
				//  list, just add this element to the end.

			if ( pos == this->num_in_index )
				this->index[pos] = new_ele;
			else
			{
					// Shift all of the elements from this
					//  position over to make space for the
					//  new element.

				shift = this->num_in_index - (pos + 1);

				memmove(this->index + pos + 1,
				        this->index + pos,
				        shift * sizeof(list_element *));

				this->index[pos] = new_ele;
			}

			this->num_in_index = new_size;
		}
	}
	else
	{
			// this index is not yet sorted; just add this element
			//  and then sort the index :)

		ret_code = this->add(new_ele);

		if ( ret_code == SUCCESS )
			ret_code = this->sort();
	}

	return	ret_code;
}



//
//  METHOD: remove
//
// PURPOSE: Remove the specified element from the index.  Note that this does
//          affect the position of all elements after the specified one in
//          the index.
//
// NOTES:
//	- Removing elements from the index does not affect the sorted status
//	  of the index.
//

int	list_index :: remove (uint ele_no)
{
	uint	shift;
	uint	new_size;
	int	ret_code;

	if ( ( ele_no > this->num_in_index ) || ( ele_no < 1 ) )
		return	_list_BAD_ARGUMENT;

	new_size = this->num_in_index - 1;

		// Shift the elements to fill in the space, unless this
		//  element is at the end of the index.

	if ( ele_no < this->num_in_index )
	{
		shift = ( this->num_in_index - ele_no );

		memmove(this->index + ele_no - 1, this->index + ele_no,
		        shift * sizeof(list_element *));
	}

	this->num_in_index = new_size;

	return	ret_code;
}



//
//  METHOD: clear
//
// PURPOSE: Remove all elements from the index, but leave the index usable.
//

int	list_index :: clear ()
{
	this->num_in_index = 0;
	this->index_size = 0;

	if ( this->index != (list_element **) NULL )
	{
		free(this->index);
		this->index = (list_element **) NULL;
	}

	return	SUCCESS;
}



		////  PRIVATE METHODS  ////


//
//  METHOD: find_element_binary
//
// PURPOSE: Use a binary search to find the specified element in this index.
//

uint	list_index :: find_element_binary (List_el_t ele, Comp_func_t comp_func)
{
	unsigned int	low;
	unsigned int	high;
	unsigned int	mid_point;
	int		found;
	int		done;
	int		ret_code;

	low   = 0;
	high  = this->num_in_index - 1;
	found = FALSE;
	done  = FALSE;

		// Search until the element is found, or there are no more
		//  elements to look at.

	while ( ( low <= high ) && ( ! found ) && ( ! done ) )
	{
		mid_point = ( low + high ) / 2;

		ret_code = this->index[mid_point]->compare_el(ele, comp_func);

			// mid_point comes before element

		if ( ret_code < 0 )
			low = mid_point + 1;
		else
			if ( ret_code > 0 )
			{
				if ( mid_point == 0 )
					done = TRUE;

				high = mid_point - 1;
			}
			else
				found = TRUE;
	}

		// If the element we are looking for is not found, then
		//  indicate this by returning 0.  Otherwise, return the
		//  position of the matching element.

	if ( ! found )
		mid_point = 0;
	else
		mid_point++;	// Add 1 for the user; user #'s start at 1

	return(mid_point);
}



//
//  METHOD: find_element_linear
//
// PURPOSE: Use a linear search to find the specified element in this index.
//

uint	list_index :: find_element_linear (List_el_t ele, Comp_func_t comp_func)
{
	uint	pos;
	int	found;

	found = FALSE;
	pos = 0;

	while ( ( ! found ) && ( pos < this->num_in_index ) )
	{
		if ( this->index[pos]->compare_el(ele, comp_func) == 0 )
			found = TRUE;
		else
			pos++;
	}

	if ( ! found )
		pos = 0;
	else
		pos++;

	return	pos;
}
