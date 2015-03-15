//////////////////////////////////////////////////////////////////////////////
//
// COPYRIGHT (C) 1998-2003 By Arthur Naseef
//
// This file is covered under the GNU General Public License Version 2.  For
//  more information, see the file COPYING.
//
// FILE: list.C
//
// PURPOSE: This file contains the definition of the list object.
//
// PROGRAMMER NOTES:
//	FOR FURTHER DEVELOPMENT:
//		- maintain index through deletions.
//		- add ability to group records.
//		- add multiple-indexing.
//
// REVISION HISTORY:
//
// DATE		AUTHOR		DESCRIPTION
// ==========	===============	=============================================
// 2003-05-25	ARTHUR N.	Initial Release (as part of ovlfs).
//
//////////////////////////////////////////////////////////////////////////////

#ident "@(#)list.C	1.1	[03/07/27 22:20:36]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

			////////////////////
			//     CLASS      //
			//  list_element  //
			////////////////////

			////////////////////
			//                //
			// PUBLIC METHODS //
			//                //
			////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list_element
//
//  METHOD: list_element
//
// PURPOSE: Initialize the list_element object.
//

list_element :: list_element()
{
	Next        = NULL;
	my_element  = NULL_ELEMENT;
	delete_flag = FALSE;
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list_element
//
//  METHOD: ~list_element
//
// PURPOSE: Free the resources used by this list_element object.
//

list_element :: ~list_element()
{
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list_element
//
//  METHOD: element
//
// PURPOSE: Return the value stored in this list element.
//

List_el_t	list_element :: element()
{
	return(my_element);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list_element
//
//  METHOD: next
//
// PURPOSE: Obtain the element which follows this one in the list.
//

list_element	*list_element :: next()
{
	return(Next);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list_element
//
//  METHOD: print
//
// PURPOSE: Print the element, as an integer, to standard output.
//

void	list_element :: print ()
{
	printf(" %d", (int) my_element);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list_element
//
//  METHOD: set_element
//
// PURPOSE: Set the element's value to the given value.
//
// RETURNS:
//	SUCCESS	- always.
//

int	list_element :: set_element (List_el_t new_el)
{
	my_element = new_el;

	return(SUCCESS);
}



////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list_element
//
//  METHOD: set_next
//
// PURPOSE: Set the pointer to the next element to the value given.
//
// RETURNS:
//	SUCCESS	- always.
//

int	list_element :: set_next (list_element *new_next)
{
	Next = new_next;

	return(SUCCESS);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list_element
//
//  METHOD: compare_el
//
// PURPOSE: Compare this element with the one given in order to determine
//          which should come first in sorted order.
//
// RETURN VALUE:
//	< 0	- If this element comes before the other one.
//	> 0	- If this element comes after the other one.
//	  0	- If these two elements are equal.
//

// compare given pointer to other element

int	list_element :: compare_el (list_element *other_el,
	                            Comp_func_t comp_func)
{
		// Return the comparison of the two elements; if the comparison
		//  function passed is not defined, then just compare the
		//  values of the two elements themselves.

	if ( comp_func != NULL )
		return(comp_func(element(), other_el->element()));
	else
		return(element() - other_el->element());
}


// compare given value of other element

int	list_element :: compare_el (List_el_t other_el, Comp_func_t comp_func)
{
	int	ret_code;

		// Return the comparison of the two elements; if the comparison
		//  function passed is not defined, then just compare the
		//  values of the two elements themselves.

	if ( comp_func != (Comp_func_t) NULL )
		ret_code = comp_func(element(), other_el);
	else
		ret_code = element() - other_el;

	return(ret_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list_element
//
//  METHOD: mark_for_delete
//
// PURPOSE: Mark this list element for later deletion.
//

void	list_element :: mark_for_delete()
{
	delete_flag = TRUE;
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list_element
//
//  METHOD: is_marked_for_delete
//
// PURPOSE: Determine if this list element is marked for deletion.
//
//
// RETURNS:
//	TRUE	- If the element is marked to be deleted.
//	FALSE	- If the element is not marked to be deleted.
//

int	list_element :: is_marked_for_delete()
{
	return(delete_flag);
}




			////////////////////
			//     CLASS      //
			//      list      //
			////////////////////

			////////////////////
			//                //
			// PUBLIC METHODS //
			//                //
			////////////////////


//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: list
//
// PURPOSE: Initialize the list object.
//

list :: list (Comp_func_t comp_func)
{
	number_el       = 0;
	num_indices     = 0;
	more_indices    = (list_index **) NULL;
	index           = (list_element **) NULL;
	phys_index      = (list_element **) NULL;
	First           = (list_element *) NULL;
	Last            = (list_element *) NULL;
	index_mode      = _list_NO_INDEX;
	phys_index_mode = _list_NO_INDEX;
	compare_func    = comp_func;
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: ~list
//
// PURPOSE: Free the resources used by this list object.
//

list :: ~list ()
{
	list_element	*cur_el;
	list_element	*next;
	int		cur_ind;

		// Destroy the index, if it was created

	if ( this->index != (list_element **) NULL )
		delete this->index;


		// Destroy any additional indices created

	if ( this->more_indices != (list_index **) NULL )
	{
		for ( cur_ind = 0; cur_ind < this->num_indices; cur_ind++ )
		{
			if ( this->more_indices[cur_ind] != (list_index *)
			                                    NULL )
			{
				delete	this->more_indices[cur_ind];
				this->more_indices[cur_ind] = (list_index *)
				                              NULL;
			}
		}

		free(this->more_indices);
		this->more_indices = (list_index **) NULL;
	}


		// Loop through all of the nodes in the list and destroy
		//  all of them.

	cur_el = this->First;

	while ( cur_el != (list_element *) NULL )
	{
			// Remember the next node before destroying the
			//  current one (otherwise the next one will be lost).

		next = cur_el->next();

		delete cur_el;

		cur_el = next;
	}
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: print
//
// PURPOSE: Print the given list to standard output, with a left margin
//          of the specified number of spaces.
//

void	list :: print (int offset)
{
	list_element	*CurNode;
	int		node_count;
	char		off_buf[81];

	if ( offset > 80 ) offset = 80;
	sprintf(off_buf, "%*s", offset, " ");
	
	node_count = 0;

	CurNode = first_node();

	if ( CurNode == (list_element *) NULL )
		printf("%sEmpty list\n", off_buf);
	else
		while ( CurNode != (list_element *) NULL )
		{
			if ( ( node_count % 10 ) == 0 )
				printf("%s", off_buf);

			CurNode->print();

			if ( ( ++node_count ) % 10 == 0 )
				printf("\n");

			CurNode = CurNode->next();
		}

	if ( ( node_count % 10 ) != 0 )
		printf("\n");
}

void	list :: print_index (int ind_no, int offset)
{
	int		cur_ele;
	list_element	*one_el;
	list_index	*a_index;
	char		off_buf[81];

	if ( ( ind_no > ( this->num_indices - 1 ) ) || ( ind_no < 0 ) )
		return;

	a_index = this->more_indices[ind_no];

	if ( offset > 80 ) offset = 80;
	sprintf(off_buf, "%*s", offset, " ");
	
	cur_ele = 1;

	while ( cur_ele <= this->number_el )
	{
		if ( ( ( cur_ele - 1 ) % 10 ) == 0 )
			printf("%s", off_buf);

		one_el = a_index->nth(cur_ele);

		if ( one_el != (list_element *) NULL )
			one_el->print();
		else
			printf("NULL");

		if ( ( cur_ele ) % 10 == 0 )
			printf("\n");

		cur_ele++;
	}

	if ( ( ( cur_ele - 1 ) % 10 ) != 0 )
		printf("\n");
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: set_compare_func
//
// PURPOSE: Set the function used for comparing elements to the one given.
//

void	list :: set_compare_func (Comp_func_t comp_func)
{
	compare_func = comp_func;
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: insert
//
// PURPOSE: Insert the new element into the list.
//
// RETURNS:
//	SUCCESS			- If the ned element is added successfully.
//	_list_BAD_PROGRAMMING	- If the internal data structures are
//				  incorrect.
//	_list_MEMORY_ALLOC_FAIL	- If an error occurs allocating memory for the
//				  new node.
//

int	list :: insert (List_el_t new_element)
{
	return(insert_node(new_element));
}



//
//   CLASS: list
//
//  METHOD: insert_at_front
//
// PURPOSE: Insert the given element at the front of the list.
//

int	list :: insert_at_front (List_el_t new_element)
{
	return	insert_node_at_front(new_element);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: insert_ordered
//
// PURPOSE: Insert the new element into the list, and maintain the sorted
//          order of the list.
//
// RETURNS:
//	SUCCESS			- If the new element is added successfully.
//	_list_BAD_PROGRAMMING	- If the internal data structures are
//				  incorrect.
//	_list_MEMORY_ALLOC_FAIL	- If an error occurs allocating memory for the
//				  new node or the index.
//

int	list :: insert_ordered (List_el_t new_element)
{
	return(insert_node_ordered(new_element));
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: remove
//
// PURPOSE: Remove the given element from the list. The first node whose
//          value matches the one given is removed.
//
// NOTES:
//	This method of removal is inefficient because it requires a linear
//	search for each removal and it does not maintain an existing index.
//	For better efficiency with multiple removals, use the
//	mark_for_removal and remove_marked methods.
//

int	list :: remove (List_el_t rm_element)
{
	list_element	*ListNode;
	list_element	*PrevNode;
	int		error_code;

	error_code = SUCCESS;

	ListNode = find_node(rm_element, &PrevNode);


		// Now remove the node, if it was found

	if ( ListNode != (list_element *) NULL )
	{
			// Check if the node found is the last node in the
			//  list, in which case we must update the pointer
			//  to the last node

		if ( ListNode == Last )
			Last = PrevNode;


			// Check if this is the first element in the list

		if ( PrevNode == (list_element *) NULL )
		{
				// Yes, this is the first element.  Lets
				//  change the pointer to the first element
				//  to point to the next one in the list

			First = First->next();
		}
		else
		{
				// Ok, remove this node from the list by
				//  setting his predecessor to point to his
				//  successor.

			PrevNode->set_next(ListNode->next());
		}

			//
			// If removing the last element, move the
			//  last element back to the previous one
			//

		if ( ListNode == Last )
			Last = PrevNode;

			// Now free the memory used by the removed node

		delete ListNode;

			// Right now, we can not maintain the index after
			//  deletion;  however, this will just cause the
			//  next use of the index to call create_index to
			//  start the index over again.

		if ( index != (list_element **) NULL )
		{
			free(index);
			index = (list_element **) NULL;
			index_mode = _list_NO_INDEX;
		}

		if ( phys_index != (list_element **) NULL )
		{
			free(phys_index);
			phys_index = (list_element **) NULL;
			phys_index_mode = _list_NO_INDEX;
		}

			// Cleanup the additional indices

		error_code = this->reprepare_indices();

			// Decrement the element count

		number_el--;
	}

	return(error_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: remove_nth
//
// PURPOSE: Remove the specified element from the list.
//
//
// NOTES:
//	This method of removal is inefficient because it requires a linear
//	search for each removal and it does not maintain an existing index.
//	For better efficiency with multiple removals, use the
//	mark_for_removal and remove_marked methods.
//

int	list :: remove_nth (int rm_ele)
{
	list_element	*ListNode;
	list_element	*PrevNode;
	int		error_code;
	int		cur_ele;

	error_code = SUCCESS;

	PrevNode = (list_element *) NULL;
	ListNode = this->First;
	cur_ele = 1;

		/* Find the node before the one to remove.  Remember that */
		/*  the number given by the user starts at 1.             */

	while ( ( cur_ele < rm_ele ) &&
	        ( ListNode != (list_element *) NULL ) )
	{
		PrevNode = ListNode;
		ListNode = ListNode->next();
		cur_ele++;
	}


		// Now remove the node, if it was found

	if ( ListNode != (list_element *) NULL )
	{
			// Check if the node found is the last node in the
			//  list, in which case we must update the pointer
			//  to the last node

		if ( ListNode == Last )
			Last = PrevNode;


			// Check if this is the first element in the list

		if ( PrevNode == (list_element *) NULL )
		{
				// Yes, this is the first element.  Lets
				//  change the pointer to the first element
				//  to point to the next one in the list

			First = First->next();
		}
		else
		{
				// Ok, remove this node from the list by
				//  setting his predecessor to point to his
				//  successor.

			PrevNode->set_next(ListNode->next());
		}
			//
			// If removing the last element, move the
			//  last element back to the previous one
			//

		if ( ListNode == Last )
			Last = PrevNode;

			// Now free the memory used by the removed node

		delete ListNode;

			// Right now, we can not maintain the index after
			//  deletion;  however, this will just cause the
			//  next use of the index to call create_index to
			//  start the index over again.  

		if ( index != (list_element **) NULL )
		{
			free(index);
			index = (list_element **) NULL;
			index_mode = _list_NO_INDEX;
		}

		if ( phys_index != (list_element **) NULL )
		{
			free(phys_index);
			phys_index = (list_element **) NULL;
			phys_index_mode = _list_NO_INDEX;
		}

			// Cleanup the additional indices

		error_code = this->reprepare_indices();

			// Decrement the element count

		number_el--;
	}
	else
		error_code = FAILURE;

	return(error_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: mark_for_removal
//
// PURPOSE: Mark the specified element to be deleted by the remove_marked
//          method.
//
// RETURNS:
//	SUCCESS	- If the element is found and marked for removal.
//	FAILURE	- If the element is not found in the list.
//

int	list :: mark_for_removal (List_el_t rm_element)
{
	list_element	*ListNode;
	int		error_code;
	int		node_num;

	error_code = SUCCESS;

	node_num = find_element(rm_element, NULL);

		// Now mark the node, if it was found

	if ( node_num > 0 )
	{
		ListNode = nth_node(node_num);

			// Check to make sure it is not null, but this should
			//  NEVER happen since we just found the element

		if ( ListNode != (list_element *) NULL )
			ListNode->mark_for_delete();
		else
			error_code = FAILURE;
	}
	else
		error_code = FAILURE;

	return(error_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: mark_nth_for_removal
//
// PURPOSE: Mark the specified element to be deleted by the remove_marked
//          method.
//
// RETURNS:
//	SUCCESS	- If the element is found and marked for removal.
//	FAILURE	- If the element is not found in the list.
//

int	list :: mark_nth_for_removal (int nth_element)
{
	list_element	*ListNode;
	int		error_code;
	int		node_num;

	error_code = SUCCESS;

		// Now mark the node, if it was found

	if ( nth_element > 0 )
	{
		ListNode = nth_node(nth_element);

			// Check to make sure it is not null, but this should
			//  NEVER happen since we just found the element

		if ( ListNode != (list_element *) NULL )
			ListNode->mark_for_delete();
		else
			error_code = FAILURE;
	}
	else
		error_code = FAILURE;

	return(error_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: remove_marked
//
// PURPOSE: Remove all elements from the list which were previously marked
//          to be removed.
//
// RETURNS:
//	int	- The number of nodes removed from the list.
//

int	list :: remove_marked ()
{
	int		ret_code;
	list_element	*CurNode;
	list_element	*RemNode;
	list_element	*PrevNode;

	ret_code = 0;

		//
		// Start with the first node, and indicate that the previous
		//  node is not defined since this is the first node.
		//

	CurNode  = first_node();
	PrevNode = (list_element *) NULL;

		//
		// Loop until there are no more nodes to check.
		//
	while ( CurNode != (list_element *) NULL )
	{
		if ( CurNode->is_marked_for_delete() )
		{
			RemNode = CurNode;

			CurNode = CurNode->next();

				//
				// If the current node is the first in the
				//  list, then reset the first node.
				//
			if ( PrevNode == (list_element *) NULL )
				First = CurNode;
			else
				PrevNode->set_next(CurNode);

				//
				// If removing the last element, move the
				//  last element back to the previous one
				//

			if ( RemNode == Last )
				Last = PrevNode;

				// Now free the memory used by the removed node

			delete RemNode;

			number_el--;
			ret_code++;
		}
		else
		{
			PrevNode = CurNode;
			CurNode = CurNode->next();
		}
	}

		// If any nodes were removed, take care of the index
			// Right now, we can not maintain the index after
			//  deletion;  however, this will just cause the
			//  next use of the index to call create_index to
			//  re-create the index over again.

	if ( ret_code > 0 )
	{
		if ( index != (list_element **) NULL )
		{
			free(index);
			index = (list_element **) NULL;
			index_mode = _list_NO_INDEX;
		}

		if ( phys_index != (list_element **) NULL )
		{
			free(phys_index);
			phys_index = (list_element **) NULL;
			phys_index_mode = _list_NO_INDEX;
		}

			// Cleanup the additional indices

		ret_code = this->reprepare_indices();
	}

	return(ret_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: clear
//
// PURPOSE: Remove all elements from the list.  This is the fastest way to
//          clean up a list without destroying it.
//

int	list :: clear ()
{
	int		ret_code;
	list_element	*CurNode;
	list_element	*RemNode;
	list_element	*PrevNode;
	int		cur_ind;
	list_index	*a_index;

	ret_code = 0;

	while ( this->First != (list_element *) NULL )
	{
		RemNode = this->First;
		this->First = this->First->next();

		delete RemNode;
	}

	if ( this->index != (list_element **) NULL )
	{
		free(this->index);
		this->index = (list_element **) NULL;
		index_mode = _list_NO_INDEX;
	}

	if ( phys_index != (list_element **) NULL )
	{
		free(phys_index);
		phys_index = (list_element **) NULL;
		phys_index_mode = _list_NO_INDEX;
	}

		// Clear all of the additional indices also

	if ( this->more_indices != (list_index **) NULL )
	{
		for ( cur_ind = 0; cur_ind < this->num_indices; cur_ind++ )
		{
			a_index = this->more_indices[cur_ind];

				// Clear the list; should we check the return
				//  code?  Probably...

			if ( a_index != (list_index *) NULL )
				a_index->clear();
		}
	}

	this->number_el = 0;
	this->Last = (list_element *) NULL;

	return(ret_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: first
//
// PURPOSE: Obtain the first element in the list.
//
// RETURNS:
//	NULL_ELEMENT	- If the list is empty.
//	List_el_t	- The value of the first element in the list.
//

List_el_t	list :: first ()
{
	List_el_t	answer;

	if ( First != (list_element *) NULL )
		answer = First->element();
	else
		answer = NULL_ELEMENT;


	return(answer);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: nth
//
// PURPOSE: Return the nth element in the list. For a sorted list, elements
//          are returned in sorted order from (n = 1) to (n = element_count).
//

List_el_t	list :: nth (int n)
{
	List_el_t	answer;
	list_element	*NthNode;

	NthNode = nth_node(n);

	if ( NthNode != (list_element *) NULL )
		answer = NthNode->element();
	else
		answer = NULL_ELEMENT;


	return(answer);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: nth_ele
//
// PURPOSE: Return the nth element in the list.  Elements are always returned
//          based on their physical order.
//

List_el_t	list :: nth_ele (int n)
{
	List_el_t	answer;
	list_element	*NthNode;
	int		error;

	error = SUCCESS;

	NthNode = nth_node(n, -2);

	if ( NthNode != (list_element *) NULL )
		answer = NthNode->element();
	else
		answer = NULL_ELEMENT;


	return(answer);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: set_nth
//
// PURPOSE: Set the value of the nth element in the list to the value given.
//          Elements are numbered in the same manner returned by the nth()
//          method.
//

List_el_t	list :: set_nth (int n, List_el_t new_val)
{
	List_el_t	answer;
	list_element	*NthNode;

	NthNode = nth_node(n);

	if ( NthNode != (list_element *) NULL )
	{
			// Get the old value to return it to the caller

		answer = NthNode->element();
		NthNode->set_element(new_val);
	}
	else
		answer = NULL_ELEMENT;


	return(answer);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: set_nth_ele
//
// PURPOSE: Set the value of the nth element in the list to the value given.
//          Elements are numbered in the same manner returned by the nth_ele()
//          method.
//

List_el_t	list :: set_nth_ele (int n, List_el_t new_val)
{
	List_el_t	answer;
	list_element	*NthNode;

	NthNode = nth_node(n, -2);

	if ( NthNode != (list_element *) NULL )
	{
			// Get the old value to return it to the caller

		answer = NthNode->element();
		NthNode->set_element(new_val);
	}
	else
		answer = NULL_ELEMENT;


	return(answer);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: length
//
// PURPOSE: Obtain the length of the list.
//
// RETURNS:
//	int	- The number of elements stored in the list.
//

unsigned int	list :: length ()
{
	return(number_el);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: find_element
//
// PURPOSE: Find the element with the given value in the list. Use the given
//          comparison method, if it is defined; otherwise, use the default
//          comparison method.
//
// NOTES:
//	This method uses a binary search if the list is sorted. Otherwise,
//	it uses a linear search. Therefore, it is important that the same
//	algorithm that was used to sort the list be used here.
//

unsigned int	list :: find_element (List_el_t find_el, Comp_func_t comp_func)
{
	unsigned int	result;

	if ( comp_func == NULL )
		comp_func = compare_func;

		// If the list is not sorted then use a linear search;
		//  otherwise, use a binary search

	if ( index_mode != _list_IS_SORTED )
		result = find_element_linear(find_el, comp_func);
	else
		result = find_element_binary(find_el, comp_func);

	return(result);
}



//
//  METHOD: find_group
//
// PURPOSE: Given an element to compare, find the first and last elements in
//          the list that match the given element assuming that the specified
//          list, or index, is sorted.
//
// NOTES:
//	- If the list is not sorted, the results will not be as expected.
//

int	list :: find_group (List_el_t ele, uint *first, uint *last)
{
	uint	cur;
	uint	r_first;
	uint	r_last;
	int	ret_code = SUCCESS;


		// Validate the arguments given

	if ( ( first == (uint *) NULL ) || ( last == (uint *) NULL ) )
		return	_list_BAD_ARGUMENT;

		// Just use our own lookup method to find any matching element

	cur = this->find_element(ele);

	if ( cur != 0 )
	{
			// Found one element, now search towards the front
			//  of the list for a match

		r_first = this->find_group_end(-1, ele, cur, -1);
		r_last = this->find_group_end(-1, ele, cur, 1);

		first[0] = r_first;
		last[0] = r_last;
	}
	else
		ret_code = _list_NOT_FOUND;

	return	ret_code;;
}

int	list :: find_group (int index_no, List_el_t ele, uint *first,
	                    uint *last)
{
	uint	cur;
	uint	r_first;
	uint	r_last;
	int	ret_code = SUCCESS;


		// Validate the arguments given

	if ( ( first == (uint *) NULL ) || ( last == (uint *) NULL ) )
		return	_list_BAD_ARGUMENT;

	if ( ( index_no > this->num_indices ) ||
	     ( ( index_no < 0 ) && ( index_no != -1 ) ) )
		return	_list_BAD_ARGUMENT;


		// Just use our own lookup method to find any matching element

	if ( index_no == -1 )
		return	this->find_group(ele, first, last);


		// Just use the index's lookup method to find any matching ele

	cur = this->index_find_element(index_no, ele);

	if ( cur != 0 )
	{
			// Found one element, now search towards the front
			//  of the list for a match

		r_first = this->find_group_end(index_no, ele, cur, -1);
		r_last = this->find_group_end(index_no, ele, cur, 1);

		first[0] = r_first;
		last[0] = r_last;
	}
	else
		ret_code = _list_NOT_FOUND;

	return	ret_code;
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: foreach
//
// PURPOSE: Call the given callback function once for each element in the
//          list, passing each element, one at a time.  If the callback
//          function returns a non-zero value, processing stops and this
//          method returns that value.
//
//
// RETURNS:
//	> 0			- The number of elements processed if
//				  successful.
//	_list_BAD_PROGRAMMING	- If an internal error occurs.
//	int			- The value returned from the callback
//				  function, if non-zero.
//

int	list :: foreach (Foreach_func_t callback_func)
{
	int		ret_code;
	int		num_processed;
	int		cur_element;
	list_element	*CurNode;

	ret_code = 0;
	num_processed = 0;

	cur_element = 1;

	while ( ( cur_element <= this->number_el ) && ( ret_code == 0 ) )
	{
		CurNode = this->nth_node(cur_element);

			// This should not happen:

		if ( CurNode != (list_element *) NULL )
		{
			num_processed++;
			ret_code = (*callback_func)(CurNode->element());
		}
		else
			ret_code = _list_BAD_PROGRAMMING;

		cur_element++;
	}

	if ( ret_code == 0 )
		return(num_processed);
	else
		return(ret_code);
}


//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: foreach2
//
// PURPOSE: This method is the same as the original foreach() method above
//          except that it will pass a user-defined value through to the call-
//          back function given for improved modularity.
//
//
// RETURNS:
//	> 0			- The number of elements processed if
//				  successful.
//	_list_BAD_PROGRAMMING	- If an internal error occurs.
//	int			- The value returned from the callback
//				  function, if non-zero.
//

int	list :: foreach2 (Foreach_func2_t callback_func, void *u_data)
{
	int		ret_code;
	int		num_processed;
	int		cur_element;
	list_element	*CurNode;

	ret_code = 0;
	num_processed = 0;

	cur_element = 1;

	while ( ( cur_element <= this->number_el ) && ( ret_code == 0 ) )
	{
		CurNode = this->nth_node(cur_element);

			// This should not happen:

		if ( CurNode != (list_element *) NULL )
		{
			num_processed++;
			ret_code = (*callback_func)(CurNode->element(), u_data);
		}
		else
			ret_code = _list_BAD_PROGRAMMING;

		cur_element++;
	}

	if ( ret_code == 0 )
		return(num_processed);
	else
		return(ret_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: forall
//
// PURPOSE: Call the given callback function once for each element in the
//          list, passing each element, one at a time.  All elements are
//          processed unless an internal error is found.
//
//
// RETURNS:
//	> 0			- The number of elements processed if
//				  successful.
//	_list_BAD_PROGRAMMING	- If an internal error occurs.
//

int	list :: forall (Forall_func_t callback_func)
{
	int		ret_code;
	int		num_processed;
	int		cur_element;
	list_element	*CurNode;

	ret_code = 0;
	num_processed = 0;

	cur_element = 1;

	while ( ( cur_element <= this->number_el ) && ( ret_code == 0 ) )
	{
		CurNode = this->nth_node(cur_element);

			// This should not happen:

		if ( CurNode != (list_element *) NULL )
		{
			num_processed++;
			(*callback_func)(CurNode->element());
		}
		else
			ret_code = _list_BAD_PROGRAMMING;

		cur_element++;
	}

	if ( ret_code == 0 )
		return(num_processed);
	else
		return(ret_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: forall2
//
// PURPOSE: Call the given callback function once for each element in the
//          list, passing each element, one at a time.  All elements are
//          processed unless an internal error is found.
//
//
// RETURNS:
//	> 0			- The number of elements processed if
//				  successful.
//	_list_BAD_PROGRAMMING	- If an internal error occurs.
//

int	list :: forall2 (Forall_func2_t callback_func, void *u_data)
{
	int		ret_code;
	int		num_processed;
	int		cur_element;
	list_element	*CurNode;

	ret_code = 0;
	num_processed = 0;

	cur_element = 1;

	while ( ( cur_element <= this->number_el ) && ( ret_code == 0 ) )
	{
		CurNode = this->nth_node(cur_element);

			// This should not happen:

		if ( CurNode != (list_element *) NULL )
		{
			num_processed++;
			(*callback_func)(CurNode->element(), u_data);
		}
		else
			ret_code = _list_BAD_PROGRAMMING;

		cur_element++;
	}

	if ( ret_code == 0 )
		return(num_processed);
	else
		return(ret_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: dup
//
// PURPOSE: Duplicate the list and return a pointer to the new list.
//
//
// RETURNS:
//	NULL	- If an error occurs.
//	list *	- If the list is successfully copied.
//

list	*list :: dup ()
{
	list		*new_list;
	list_element	*cur_el;

	new_list = new list(this->compare_func);

	if ( new_list != (list *) NULL )
	{
		cur_el = this->first_node();

		while ( cur_el != (list_element *) NULL )
		{
			new_list->insert(cur_el->element());

			cur_el = cur_el->next();
		}
	}

	return(new_list);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: merge
//
// PURPOSE: Merge the given list into this list. All of the members of the
//          given list are inserted into the list using insert().
//
//
// RETURNS:
//	SUCCESS	- If the elements of the given list are successfully added to
//		  this list.
//	FAILURE	- If an error occurs merging the elements of the given list.
//

int	list :: merge (list *list2)
{
	int		cur_el;
	int		total_el;
	List_el_t	one_el;
	int		ret_code;

	ret_code = SUCCESS;

	if ( list2 != (list *) NULL )
	{
		total_el = list2->length();

		cur_el = 1;

		while ( ( cur_el <= total_el ) && ( ret_code == SUCCESS ) )
		{
			one_el = list2->nth(cur_el);

			ret_code = this->insert(one_el);

			cur_el++;
		}
	}
	else
		ret_code = FAILURE;

	return(ret_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: compare
//
// PURPOSE: Compare the given list to this list to determine which appears
//          first in sorted order.
//
// NOTES:
//	- Shorter lists are first in sorted order when all of the elements of
//	  the two lists are the same.
//
//	- Elements are compared in the order they appear in the two lists; if
//	  the ordering of elements change in either list, the sorted order of
//	  the two lists may also change.
//
//	- The comparison function of the first list given is used to compare
//	  elements.  It is important that the contents of the two lists are
//	  of types that are supported by the comparison function.
//
//
// RETURNS:
//	0	- If the two lists appear to be identical.
//	< 0	- If this list appears first in sorted order.
//	> 0	- If the other list appears first in sorted order.
//

int	list :: compare (list *list2)
{
	int		cur_el;
	int		total_el;
	list_element	*node1;
	list_element	*node2;
	int		ret_code;

	if ( list2 != (list *) NULL )
	{
			// Determine the length of the shorter list

		total_el = list2->length();

		if ( total_el > this->length() )
			total_el = this->length();


			// Compare the elements of the two lists until a
			//  difference is found or then end of the shorter
			//  list is reached.

		cur_el = 1;
		ret_code = 0;

		while ( ( cur_el <= total_el ) && ( ret_code == 0 ) )
		{
			node1 = this->nth_node(cur_el);
			node2 = list2->nth_node(cur_el);

			if ( node1 == (list_element *) NULL )
				ret_code = 1;
			else if ( node2 == (list_element *) NULL )
				ret_code = -1;
			else
				ret_code = node1->compare_el(node2);

			cur_el++;
		}

			// If all of the elements appear to be the same,
			//  compare the list lengths

		if ( ret_code == 0 )
			ret_code = this->length() - list2->length();
	}
	else
		ret_code = -1;

	return	ret_code;
}



//////////////////////////////////////////////////////////////////////////////
//
//  METHOD: add_index
//
// PURPOSE: Add a new index to this list and return the number of the index.
//
// NOTES:
//	- The current implementation of this method does not re-use indices
//	  that were droped with the drop_index method.
//

int	list :: add_index (int *ind_no, Comp_func_t comp_func)
{
	list_index	*new_ind;
	list_index	**ele;
	list_index	**tmp;
	unsigned int	new_size;
	int		ret_code = SUCCESS;

		// Verify the arguments given.

	if ( ind_no == (int *) NULL )
		return	_list_BAD_ARGUMENT;


		// Create the new index object

	new_ind = new list_index(comp_func);

	if ( new_ind == (list_index *) NULL )
		return	_list_MEMORY_ALLOC_FAIL;
	else
	{
		ret_code = this->prepare_index(new_ind);

		if ( ret_code != SUCCESS )
		{
			delete	new_ind;

			return	ret_code;
		}
	}


		// Now add this index to the table of indices.  Check if the
		//  table of indices has been allocated yet.

	if ( this->more_indices == (list_index **) NULL )
	{
			// Allocate memory for the array

		this->more_indices = (list_index **)
		                     malloc(sizeof(list_index *));

		if ( this->more_indices == (list_index **) NULL )
			ret_code = _list_MEMORY_ALLOC_FAIL;
		else
		{
				// Save the new List Index

			this->more_indices[0] = new_ind;
			this->num_indices = 1;

			ind_no[0] = 0;	// the first index is number 0
		}
	}
	else
	{
		new_size = this->num_indices + 1;

			// Allocate memory for the new array

		tmp = (list_index **) realloc(this->more_indices,
		                              new_size * sizeof(list_index *));


			// If the allocation failed, return the error and
			//  leave the existing array as is.

		if ( tmp == (list_index **) NULL )
			ret_code = _list_MEMORY_ALLOC_FAIL;
		else
		{
			this->more_indices = tmp;

				// Save the new List Index

			this->more_indices[this->num_indices] = new_ind;
			ind_no[0] = this->num_indices;

			this->num_indices = new_size;
		}
	}

	return	ret_code;
}



//////////////////////////////////////////////////////////////////////////////
//
//  METHOD: drop_index
//
// PURPOSE: Drop the specified index from the list.
//
// NOTES:
//	- It is not necessary, and probably is not correct, to free the
//	  elements stored in the index before freeing the index itself.
//
//	- The current implementation of this method does not free the memory
//	  used by the index table for the dropped index, even if it is the
//	  last one in the table.
//

int	list :: drop_index (int ind_no)
{
	list_index	*ptr;
	int		ret_code = SUCCESS;


		// Verify the argument given.

	if ( ( ind_no > ( this->num_indices - 1 ) ) || ( ind_no < 0 ) )
		return	_list_BAD_ARGUMENT;


		// Check if the index is still valid and free it if so.

	ptr = this->more_indices[ind_no];

	if ( ptr != (list_index *) NULL )
	{
		this->more_indices[ind_no] = (list_index *) NULL;

		delete	ptr;
	}

	return	ret_code;
}



//////////////////////////////////////////////////////////////////////////////
//
//  METHOD: index_nth
//
// PURPOSE: Obtain the nth element from the specified index.
//

List_el_t	list :: index_nth (int ind_no, int ele_no)
{
	list_index	*a_index;
	List_el_t	ret_code;

	if ( ( ind_no > ( this->num_indices - 1 ) ) || ( ind_no < 0 ) )
		return	NULL_ELEMENT;

	a_index = this->more_indices[ind_no];

	if ( a_index == (list_index *) NULL )
		ret_code = NULL_ELEMENT;
	else
		ret_code = a_index->nth_value(ele_no);

	return	ret_code;
}



//////////////////////////////////////////////////////////////////////////////
//
//  METHOD: index_sort
//
// PURPOSE: Sort the specified index for this list.
//

int	list :: index_sort (int ind_no, Comp_func_t comp_func)
{
	list_index	*a_index;
	int		ret_code;

	if ( ( ind_no > ( this->num_indices - 1 ) ) || ( ind_no < 0 ) )
		return	_list_BAD_ARGUMENT;

	a_index = this->more_indices[ind_no];

	if ( a_index == (list_index *) NULL )
		ret_code = _list_BAD_ARGUMENT;
	else
		ret_code = a_index->sort(comp_func);

	return	ret_code;
}



//////////////////////////////////////////////////////////////////////////////
//
//  METHOD: index_find_element
//
// PURPOSE: Find an element by searching the specified index.
//

uint	list :: index_find_element (int ind_no, List_el_t find_el,
	                            Comp_func_t comp_func)
{
	list_index	*a_index;
	int		ret_code;

	if ( ( ind_no > ( this->num_indices - 1 ) ) || ( ind_no < 0 ) )
		return	0;	// _list_BAD_ARGUMENT

	a_index = this->more_indices[ind_no];

	if ( a_index == (list_index *) NULL )
		ret_code = 0;	// _list_BAD_ARGUMENT
	else
		ret_code = a_index->find_element(find_el, comp_func);

	return	ret_code;
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: convert_to_array
//
// PURPOSE: Convert this list into an array of its elements.  The list
//          remains unaffected.
//
// ARGUMENTS:
//	extra	- The number of extra elements to add to the total length of
//		  the array.
//	size	- The address of an integer to store the total number of
//		  elements in the array.  If it is null, it is ignored.
//
// NOTES:
//	- In order to determine the resulting size of the array, the caller
//	  can use the length() method and add the value passed in to extra,
//	  or the caller can pass the address of the integer to store the total
//	  size of the array in the size argument.
//	- The extra elements in the array are NOT initialized.
//
// RETURNS:
//	NULL		- If the total number of elements for the array is 0,
//			  or there is an error allocating memory.
//	List_el_t *	- A pointer to the new array.

List_el_t	*list :: convert_to_array (int extra, int *size)
{
	List_el_t	*ret_code;
	int		tot_ele;
	int		cur_ele;
	list_element	*CurNode;

	tot_ele = this->number_el + extra;

	if ( tot_ele > 0 )
	{
		ret_code = (List_el_t *) malloc(tot_ele * sizeof(List_el_t));

		if ( ret_code != (List_el_t *) NULL )
		{
			for ( cur_ele = 1; cur_ele <= this->number_el;
			      cur_ele++ )
			{
				CurNode = this->nth_node(cur_ele);

				if ( CurNode != (list_element *) NULL )
					ret_code[cur_ele - 1] =
						CurNode->element();
				else
					ret_code[cur_ele - 1] = 0;   // error
			}
		}
	}
	else
		ret_code = (List_el_t *) NULL;


		// Save the resulting array's size in the space given, if it
		//  was given.  Store 0 if, for any reason, the array was not
		//  created.

	if ( size != (int *) NULL )
		if ( ret_code != (List_el_t *) NULL )
			size[0] = tot_ele;
		else
			size[0] = 0;

	return	ret_code;
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: store_to_array
//
// PURPOSE: Store the elements of this list into the given array starting with
//          the specified element in the list and continuing up to the
//          specified length of the array.
//
// ARGUMENTS:
//	List_el_t *	- A pointer to the array to store elements in.  Each
//			  element must have the size of a (List_el_t) value.
//	int		- The size of the array given.  This is the maximum
//			  number of elements the caller wants filled in the
//			  array.
//	int		- (Optional) The first element in the list to store in
//			  the array.  If this is not specified, the first
//			  element in the list is stored as the first element in
//			  the array.
//
// RETURNS:
//	< 0	- If an error
//

int	list :: store_to_array (List_el_t *array, int size, int start)
{
	int		cur_ele;
	int		max;
	list_element	*CurNode;
	int		ret_code;

	if ( array == (List_el_t *) NULL )
		return	_list_BAD_ARGUMENT;		// Sorry

	if ( start < 1 )
		start = 1;

	max = start + size - 1;

	if ( max > this->number_el )
		max = this->number_el;

	ret_code = 0;

	for ( cur_ele = start; cur_ele <= max; cur_ele++ )
	{
		CurNode = this->nth_node(cur_ele);

		if ( CurNode != (list_element *) NULL )
			array[cur_ele - start] = CurNode->element();
		else
			array[cur_ele - start] = 0;	// error

		ret_code++;
	}

	return	ret_code;
}



			/////////////////////
			//                 //
			// PRIVATE METHODS //
			//                 //
			/////////////////////


//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: find_element_linear
//
// PURPOSE: Finds the specified element in the list using the given comparison
//          method. The search is performed as a linear search on the list.
//

unsigned int	list :: find_element_linear (List_el_t find_el,
		                             Comp_func_t comp_func)
{
	list_element	*CurNode;
	int		found;
	unsigned int	cur_element;

	CurNode     = first_node();
	cur_element = 0;
	found       = FALSE;

	while ( ( CurNode != (list_element *) NULL ) && ( ! found ) )
	{
		cur_element++;

		if ( CurNode->compare_el(find_el, comp_func) == 0 )
			found = TRUE;
		else
			CurNode = CurNode->next();
	}

	if ( ! found )
		cur_element = 0;

	return(cur_element);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: find_element_binary
//
// PURPOSE: Finds the specified element in the list using the given comparison
//          method. The search is performed as a binary search on the list.
//

unsigned int	list :: find_element_binary (List_el_t find_el,
		                             Comp_func_t comp_func)
{
	unsigned int	low;
	unsigned int	high;
	unsigned int	mid_point;
	int		found;
	int		done;
	int		ret_code;

	low   = 0;
	high  = number_el - 1;
	found = FALSE;
	done  = FALSE;

	while ( ( low <= high ) && ( ! found ) && ( ! done ) )
	{
		mid_point = ( low + high ) / 2;

		ret_code = index[mid_point]->compare_el(find_el, comp_func);

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
//  METHOD: find_group_end
//
// PURPOSE: Given an element to match, and a starting position in the list,
//          find the end of the grouping of elements that match the given one
//          by searching in the specified direction.
//

uint	list :: find_group_end (int ind_no, List_el_t ele, uint start, int dir)
{
	uint		ret;
	uint		mult;
	int		cur_pos;
	list_index	*a_index;
	list_element	*other;
	Comp_func_t	comp_func;
	int		found;

	comp_func = this->compare_func;

	if ( ind_no != -1 )
	{
		if ( ( ind_no < 0 ) || ( ind_no >= this->num_indices ) )
			return	_list_BAD_ARGUMENT;
		else
		{
			a_index = this->more_indices[ind_no];

			if ( a_index == (list_index *) NULL )
				return	_list_BAD_ARGUMENT;
			else
				comp_func = a_index->get_comp_func();
		}
	}

	cur_pos = start + dir;
	mult = 1;
	found = TRUE;

		// Search outward until a non-match is found, doubling the
		//  distance of each successive search.

	while ( ( cur_pos > 0 ) && ( cur_pos <= this->number_el ) && ( found ) )
	{
			// See if the current element matches

		if ( ind_no == -1 )
			other = this->index[cur_pos - 1];
		else
			other = a_index->nth(cur_pos);

		if ( other->compare_el(ele, comp_func) != 0)
			found = FALSE;
		else
		{
				// Double the multiplier and add it

			mult = mult << 1;
			cur_pos += dir * mult;
		}
	}

		// If found is still TRUE, then a limit of the list was passed

	if ( found )
	{
			// If we were searching forward, set the current point
			//  to the end of the list; otherwise, set it to the
			//  beginning of the list.

		if ( dir > 0 )
			cur_pos = this->number_el;
		else
			cur_pos = 1;
	}


		// Now, just perform a linear search between the last position
		//  last position checked and the start point until a match is
		//  found

	found = FALSE;
	dir *= -1;	// going in the other direction now

	while ( ( ! found ) && ( cur_pos != start ) )
	{
			// See if the current element matches

		if ( ind_no == -1 )
			other = this->index[cur_pos - 1];
		else
			other = a_index->nth(cur_pos);

		if ( other->compare_el(ele, comp_func) == 0)
			found = TRUE;
		else
			cur_pos += dir;
	}

	return	cur_pos;
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: first_node
//
// PURPOSE: Obtains the pointer to the first node in the list.
//
// RETURNS:
//	NULL		- If the list contains no elements.
//	list_element *	- A pointer to the first element in the list.
//

list_element	*list :: first_node ()
{
	return(First);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: find_node
//
// PURPOSE: Determines the location of the node with the given value and
//          returns a pointer to that node and the node preceding it in
//          the list.
//

list_element	*list :: find_node (List_el_t value, list_element **last)
{
	list_element	*cur_element;
	int		found;

	cur_element = first_node();

	if ( last != (list_element **) NULL )
		*last = (list_element *) NULL;

	found = FALSE;

	while ( ( cur_element != (list_element *) NULL ) && ( ! found ) )
	{
		if ( cur_element->element() == value )
			found = TRUE;
		else
		{
			if ( last != (list_element **) NULL )
				*last = cur_element;

			cur_element = cur_element->next();
		}
	}

	return(cur_element);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: create_index
//
// PURPOSE: Creates an index into the list and initializes the index to
//          point to the elements in the order they appear in the list.
//
// RETURNS:
//	SUCCESS			- If the index is created successfully.
//	_list_MEMORY_ALLOC_FAIL	- If an error occurs allocating memory for the
//				  index.
//

int	list :: create_index (int index_no)
{
	int		error_code;
	unsigned int	cur_el;
	list_element	*OneEl;
	list_element	**new_ind;

	if ( ( index_no != -1 ) && ( index_no != -2 ) )
		return	_list_BAD_ARGUMENT;

	error_code = SUCCESS;

	if ( number_el > 0 )
	{
		new_ind = (list_element **)
		        calloc(number_el, sizeof(list_element *));

		if ( new_ind == NULL )
			error_code = _list_MEMORY_ALLOC_FAIL;
		else
		{
			OneEl = first_node();

			for ( cur_el = 1; cur_el <= number_el; cur_el++ )
			{
				if ( OneEl == (list_element *) NULL )
					new_ind[cur_el-1] = (list_element *)
					                    NULL;
				else
				{
					new_ind[cur_el-1] = OneEl;

					OneEl = OneEl->next();
				}
			}
		}

		if ( error_code == SUCCESS )
		{
			if ( index_no == -1 )
			{
				this->index = new_ind;
				this->index_mode = _list_IS_INDEXED;
			}
			else if ( index_no == -2 )
			{
				this->phys_index = new_ind;
				this->phys_index_mode = _list_IS_INDEXED;
			}
		}
	}

	return(error_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: reindex
//
// PURPOSE: Adjust the size of the index to include more or less elements
//          as specified. All new elements are initialized to be undefined.
//
// RETURNS:
//	SUCCESS			- If the index is adjusted to its new size.
//	_list_MEMORY_ALLOC_FAIL	- If the memory allocation failed.
//
// NOTE:
//	If the new size is less than or equal to zero, then the index is
//	freed.
//

int	list :: reindex (unsigned int new_size)
{
	int		error_code;
	unsigned int	cur_el;
	unsigned int	arr_size;

	error_code = SUCCESS;

	if ( new_size > 0 )
	{
		if ( index != NULL )
		{
			arr_size = new_size * sizeof(list_element **);

			index = (list_element **) realloc(index, arr_size);

			if ( index == (list_element **) NULL )
				error_code = _list_MEMORY_ALLOC_FAIL;
			else
			{
				cur_el = number_el + 1;

					// Initialize new members of the index
					//  to NULL

				while ( cur_el <= new_size )
				{
					index[cur_el-1] = (list_element *)NULL;
					cur_el++;
				}
			}
		}

		if ( ( this->phys_index != NULL ) &&
		     ( error_code == SUCCESS ) )
		{
			arr_size = new_size * sizeof(list_element **);

			phys_index = (list_element **)
			             realloc(phys_index, arr_size);

			if ( phys_index == (list_element **) NULL )
				error_code = _list_MEMORY_ALLOC_FAIL;
			else
			{
				cur_el = number_el + 1;

					// Initialize new members of the index
					//  to NULL

				while ( cur_el <= new_size )
				{
					phys_index[cur_el-1] = (list_element *)
					                       NULL;
					cur_el++;
				}
			}
		}
	}
	else
	{
		if ( index != (list_element **) NULL )
		{
			free(index);
			index = (list_element **) NULL;
			index_mode = _list_NO_INDEX;
		}

		if ( this->phys_index != (list_element **) NULL )
		{
			free(phys_index);
			phys_index = (list_element **) NULL;
			phys_index_mode = _list_NO_INDEX;
		}
	}


	return(error_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: nth_node
//
// PURPOSE: Obtains the nth node in the list. If the list has not yet been
//          indexed, then an index is created for it. If the index can not
//          be created, then a linear search is used to find the nth element.
//
// RETURNS:
//	NULL		- If the value of n is too large, too small, or the
//			  list is empty.
//	list_element *	- The pointer to the nth node.
//

list_element	*list :: nth_node (int n, int from_ind)
{
	list_element	**the_index;
	list_element	*cur_el;

		// Find the index we are to use

	if ( from_ind == -1 )
		the_index = this->index;
	else if ( from_ind = -2 )
		the_index = this->phys_index;
	else
		return	(list_element *) NULL;


	if ( ( First != (list_element *) NULL ) && ( n > 0 ) &&
	     ( n <= number_el ) )
	{
			// If this list is not index, then index it
		if ( the_index == (list_element **) NULL )
			create_index(from_ind);

			// Well, if for some reason we still do not have an
			//  index, we can get by with a linear lookup

		if ( the_index == (list_element **) NULL ) 
		{
			cur_el = first_node();

				// Loop until we have found the nth element
				//  or there are no more elements left

			while ( ( --n > 0 ) &&
			        ( cur_el != (list_element *) NULL ) )
			{
				cur_el = cur_el->next();
			}
		}
		else
			cur_el = the_index[n - 1];  // Great! use the index.
	}
	else
		cur_el = (list_element *) NULL;      // Return the null value

	return(cur_el);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: insert_node
//
// PURPOSE: Adds the given element to the end of the list.
//
// RETURNS:
//	SUCCESS			- If the new element is added successfully.
//	_list_BAD_PROGRAMMING	- If the internal data structures are
//				  incorrect.
//	_list_MEMORY_ALLOC_FAIL	- If an error occurs allocating memory for the
//				  new node.
//

int	list :: insert_node (List_el_t new_element)
{
	list_element	*NewListEl;
	int		error_code;

	error_code = SUCCESS;

	if ( ( NewListEl = new list_element ) != (list_element *) NULL )
	{
		NewListEl->set_element(new_element);
		NewListEl->set_next((list_element *)NULL);

		if ( number_el == 0 )
		{
			First = NewListEl;
			Last  = NewListEl;
		}
		else
			if ( Last == (list_element *) NULL )
				error_code = _list_BAD_PROGRAMMING;
			else
			{
					// Add the new element as the last

				Last->set_next(NewListEl);
				Last = NewListEl;
			}
	}
	else
		error_code = _list_MEMORY_ALLOC_FAIL;

	if ( error_code == SUCCESS )
	{
		error_code = reindex(number_el + 1);

			// If the re-index succeeded, then set the last (new)
			//  element of the index to the new element.

		if ( error_code == SUCCESS )
		{
			if ( index != (list_element **) NULL )
				index[number_el] = Last;

			if ( phys_index != (list_element **) NULL )
				phys_index[number_el] = Last;
		}

		error_code = this->add_to_indices(Last);

		if ( index_mode == _list_IS_SORTED )
			index_mode = _list_IS_INDEXED;

		number_el++;
	}

	return(error_code);
}



//
//   CLASS: List
//
//  METHOD: insert_node_at_front
//
// PURPOSE: Insert a new node at the front of the list with the given value.
//

int	list :: insert_node_at_front (List_el_t new_element)
{
	list_element	*NewListEl;
	int		error_code;

	error_code = SUCCESS;

	if ( ( NewListEl = new list_element ) != (list_element *) NULL )
	{
		NewListEl->set_element(new_element);
		NewListEl->set_next(this->First);

		if ( number_el == 0 )
		{
			First = NewListEl;
			Last  = NewListEl;
		}
		else
			if ( this->First == (list_element *) NULL )
				error_code = _list_BAD_PROGRAMMING;
			else
				this->First = NewListEl;
	}
	else
		error_code = _list_MEMORY_ALLOC_FAIL;

	if ( error_code == SUCCESS )
	{
		error_code = reindex(number_el + 1);

			// If the re-index succeeded, then set the first
			//  element of the index to the new element.  This
			//  requires the entire index to be shifted.

		if ( error_code == SUCCESS )
		{
			if ( this->index != (list_element **) NULL )
			{
				memmove(this->index + 1, this->index,
					sizeof(list_element *) *
				        this->number_el);

				this->index[0] = this->First;
			}

			if ( this->phys_index != (list_element **) NULL )
			{
				memmove(this->phys_index + 1, this->phys_index,
					sizeof(list_element *) *
				        this->number_el);

				this->phys_index[0] = this->First;
			}
		}

		error_code = this->insert_into_indices(this->First, 1);

		if ( index_mode == _list_IS_SORTED )
			index_mode = _list_IS_INDEXED;

		number_el++;
	}

	return	error_code;
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: insert_node_ordered
//
// PURPOSE: Insert the new element into the list and maintain the sorted
//          order of the list.
//
// RETURNS:
//	SUCCESS			- If the new element is added successfully.
//	_list_BAD_PROGRAMMING	- If the internal data structures are
//				  incorrect.
//	_list_MEMORY_ALLOC_FAIL	- If an error occurs allocating memory for the
//				  new node or the index.
//
// NOTES:
//	If the list is not already sorted, it will be sorted automatically.
//

int	list :: insert_node_ordered (List_el_t new_element)
{
	list_element	*NewListEl;
	int		error_code;
	unsigned int	insert_pos;
	int		comparison;

	error_code = SUCCESS;

	if ( ( NewListEl = new list_element ) != (list_element *) NULL )
	{
		NewListEl->set_element(new_element);
		NewListEl->set_next((list_element *)NULL);

		if ( number_el == 0 )
		{
			First = NewListEl;
			Last  = NewListEl;
		}
		else
		{
				// Make sure this list is already sorted!

			if ( index_mode != _list_IS_SORTED )
				sort();


				// Find the location to insert this node at

			insert_pos = find_insert_pos(new_element, &comparison);


				// If the insertion point was not found, then
				//  there must be a bug in the algorithm.

			if ( insert_pos == 0 )
				error_code = _list_BAD_PROGRAMMING;
			else
			{
					// Add the new element as the last in
					//  the linked list

				Last->set_next(NewListEl);
				Last = NewListEl;
			}
		}
	}
	else
		error_code = _list_MEMORY_ALLOC_FAIL;


	if ( error_code == SUCCESS )
	{
			// If this is the first element in the list then
			//  just use create_index to get the index

		if ( number_el == 0 )
		{
			number_el++;
			error_code = create_index();
			index_mode = _list_IS_SORTED;
		}
		else
		{
			error_code = reindex(number_el + 1);

				// If the re-index succeeded, then shift the
				//  existing elements of the index to make
				//  room for the new element; this will keep
				//  the index in sorted order.

			if ( ( error_code == SUCCESS ) &&
			     ( index != (list_element **) NULL ) )
			{
					// Check if we are inserting before
					//  the element at the given position;
					//  the insert_pos is in user terms
					//  which means that it is 1 greater
					//  than the position in the index.

				if ( comparison > 0 )
					insert_pos--;

				if ( insert_pos < number_el )
					memmove(index + insert_pos + 1,
					        index + insert_pos,
					        (number_el - insert_pos) *
						sizeof(*index));

				index[insert_pos] = NewListEl;
			}

			if ( ( error_code == SUCCESS ) &&
			     ( phys_index != (list_element **) NULL ) )
				this->phys_index[number_el] = NewListEl;

			number_el++;
		}

		error_code = this->add_to_indices_ordered(NewListEl);
	}

	return(error_code);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: find_insert_pos
//
// PURPOSE: Determine the location in the sorted list to insert the given
//          element so that the sorted order is maintained.
//
// RETURNS:
//	int	- The position in the list to insert the given element.
//		  Positions are numbered from 1 to the length of the list.
//

unsigned int	list :: find_insert_pos (List_el_t find_el, int *comparison)
{
	unsigned int	low;
	unsigned int	high;
	unsigned int	mid_point;
	int		found;
	int		done;
	int		ret_code;

	low       = 0;
	high      = number_el - 1;
	mid_point = 0;
	found     = FALSE;
	done      = FALSE;


		// The list must be sorted in order to use this function

	if ( index_mode != _list_IS_SORTED )
	{
		*comparison = 0;
		return(0);
	}

	while ( ( low <= high ) && ( ! found ) && ( ! done ) )
	{
		mid_point = ( low + high ) / 2;

		ret_code = index[mid_point]->compare_el(find_el, compare_func);

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

		// Now the midpoint is either the location where the
		//  element belongs or where another element which matches
		//  has been found.

	mid_point++;	// Add 1 for the user; user #'s start at 1
	*comparison = ret_code;    // Save the value of the last comparison

	return(mid_point);
}



//////////////////////////////////////////////////////////////////////////////
//
//   CLASS: list
//
//  METHOD: sort
//
// PURPOSE: Sort the list using the comparison function specified, if given,
//          or the default comparison function for this list.
//
// RETURNS:
//	SUCCESS	- Always.
//

// Using the default comparison function:

int	list :: sort (int sort_ind)
{
	unsigned int	smallest;
	unsigned int	i;
	unsigned int	j;
	list_element	*Element1;
	list_element	*Element2;
	int		ret_code = SUCCESS;

		// Make sure there is at least one element before continuing
	if ( this->number_el <= 0 )
		return	SUCCESS;	// this is not an error

	if ( index_mode == _list_NO_INDEX )
		create_index();

	if ( index == (list_element **) NULL )
		return(FAILURE);

		// Start from 0 even though to the user numbering starts
		//  with 1 because the user does not need to see this index,
		//  and this is more efficient then always subtracting 1.

	for ( i = 0; i < ( number_el - 1 ); i++ )
	{
			// Assume the element at this position is the
			//  smallest already.

		smallest = i;
		Element1 = index[i];

		for ( j = i + 1; j < number_el; j++ )
		{
			Element2 = index[j];

				// If element 2 is LESS than element 1,
				//  then set element 2 as the smallest so far

			if ( Element2->compare_el(Element1, compare_func) < 0 )
			{
				smallest = j;
				Element1 = Element2;
			}
		}

			// If the smallest value is not the one which is
			//  already here, then swap it here.

		if ( smallest != i )
		{
			index[smallest] = index[i];
			index[i] = Element1;
		}
	}

	index_mode = _list_IS_SORTED;

	if ( sort_ind )
		ret_code = this->sort_indices();

	return	ret_code;
}


// Given the comparison function:

int	list :: sort (Comp_func_t comp_func, int sort_ind)
{
	unsigned int	smallest;
	unsigned int	i;
	unsigned int	j;
	list_element	*Element1;
	list_element	*Element2;
	int		ret_code = SUCCESS;

	if ( index_mode == _list_NO_INDEX )
		create_index();

	if ( index == (list_element **) NULL )
		return(FAILURE);

		// Start from 0 even though to the user numbering starts
		//  with 1 because the user does not need to see this index,
		//  and this is more efficient then always subtracting 1.

	for ( i = 0; i < ( number_el - 1 ); i++ )
	{
			// Assume the element at this position is the
			//  smallest already.

		smallest = i;
		Element1 = index[i];

		for ( j = i + 1; j < number_el; j++ )
		{
			Element2 = index[j];

				// If element 2 is LESS than element 1,
				//  then set element 2 as the smallest so far

			if ( Element2->compare_el(Element1, comp_func) < 0 )
			{
				smallest = j;
				Element1 = Element2;
			}
		}

			// If the smallest value is not the one which is
			//  already here, then swap it here.

		if ( smallest != i )
		{
			index[smallest] = index[i];
			index[i] = Element1;
		}
	}

	index_mode = _list_IS_SORTED;

	if ( sort_ind )
		ret_code = this->sort_indices();

	return	ret_code;
}



//////////////////////////////////////////////////////////////////////////////
//
//  METHOD: prepare_index
//
// PURPOSE: Given an index, add all of the elements of this list to the index.
//
// NOTES:
//	- Any elements in the index when this method is called will be lost.
//

int	list :: prepare_index (list_index *ind)
{
	list_element	*OneEl;
	uint		ele_no;
	int		ret_code = SUCCESS;


		// there's nothing to do if there are no elements in this list.

	if ( this->number_el < 1 )
		return	SUCCESS;

		// Prepare the index for the number of elements coming in order
		//  to save time expanding the index.

	ind->resize(this->number_el);

	OneEl = this->First;
	ele_no = 0;

		// Loop through the list of elements and add each one to the
		//  index

	while ( ( OneEl != (list_element *) NULL ) && ( ret_code == SUCCESS ) )
	{
		ele_no++;
		ret_code = ind->set_nth(ele_no, OneEl);

		OneEl = OneEl->next();
	}

	return	ret_code;
}



//
//  METHOD: reprepare_indices
//
// PURPOSE: Reprepare the additional indices for this list.  Any sorted order
//          for the lists will be lost.
//

int	list :: reprepare_indices ()
{
	int		cur_ind;
	list_index	*a_index;
	int		ret_code;

	if ( this->more_indices == (list_index **) NULL )
		return	SUCCESS;

	cur_ind = 0;
	ret_code = SUCCESS;

	while ( ( cur_ind < this->num_indices ) && ( ret_code == SUCCESS ) )
	{
		a_index = this->more_indices[cur_ind];

		if ( a_index != (list_index *) NULL )
			ret_code = this->prepare_index(a_index);

		cur_ind++;
	}

	return	ret_code;
}



//
//  METHOD: sort_indices
//
// PURPOSE: Sort all of the additional indices for this list.
//

int	list :: sort_indices ()
{
	int	cur_ind;
	int	ret_code;

	if ( this->more_indices == (list_index **) NULL )
		return	SUCCESS;

	cur_ind = 0;
	ret_code = SUCCESS;

	while ( ( cur_ind < this->num_indices ) && ( ret_code == SUCCESS ) )
	{
		if ( this->more_indices[cur_ind] != (list_index *) NULL )
			ret_code = this->more_indices[cur_ind]->sort();

		cur_ind++;
	}

	return	ret_code;
}



//
//  METHOD: add_to_indices
//
// PURPOSE: Add the given element to the end of all additional indices for
//          this list.
//

int	list :: add_to_indices (list_element *new_ele)
{
	int		cur_ind;
	list_index	*a_index;
	int		ret_code = SUCCESS;

		// Loop through all of the additional indices for this list
		//  and add the new element to each one.

	for ( cur_ind = 0; cur_ind < this->num_indices; cur_ind++ )
	{
		a_index = this->more_indices[cur_ind];

		if ( a_index != (list_index *) NULL )
			ret_code = a_index->add(new_ele);
	}

	return	ret_code;
}



//
//  METHOD: insert_into_indices
//
// PURPOSE: Insert the given element into all of the additional indices for
//          this list at the specified position.
//

int	list :: insert_into_indices (list_element *new_ele, uint pos)
{
	int		cur_ind;
	list_index	*a_index;
	int		ret_code = SUCCESS;

		// Loop through all of the additional indices for this list
		//  and add the new element to each one.

	for ( cur_ind = 0; cur_ind < this->num_indices; cur_ind++ )
	{
		a_index = this->more_indices[cur_ind];

		if ( a_index != (list_index *) NULL )
			ret_code = a_index->insert(pos, new_ele);
	}

	return	ret_code;
}



//
//  METHOD: add_to_indices_ordered
//
// PURPOSE: Insert the given element into all of the additional indices for
//          this list while maintaining the sorted order of those indices.
//

int	list :: add_to_indices_ordered (list_element *new_ele)
{
	int		cur_ind;
	list_index	*a_index;
	int		ret_code = SUCCESS;

		// Loop through all of the additional indices for this list
		//  and add the new element to each one.

	for ( cur_ind = 0; cur_ind < this->num_indices; cur_ind++ )
	{
		a_index = this->more_indices[cur_ind];

		if ( a_index != (list_index *) NULL )
			ret_code = a_index->insert_ordered(new_ele);
	}

	return	ret_code;
}
