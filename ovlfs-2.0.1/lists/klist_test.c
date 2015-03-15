/******************************************************************************
 ******************************************************************************
 **
 ** FILE: klist_test.c
 **
 ** DESCRIPTION:
 **	Test the kernel list module.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 2003-03-26	ARTHUR N	Initial Release.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)klist_test.c	1.1	[03/07/27 22:20:37]"

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

#include <linux/proc_fs.h>
#include <asm/semaphore.h>

#include <linux/stddef.h>
#if defined(KERNEL_VERSION) && ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) )
# include <linux/slab.h>
#else
# include <linux/malloc.h>
#endif

#include "kernel_lists.h"

#ifndef macro2str
# define macro2str0(m)	#m
# define macro2str(m)	macro2str0(m)
#endif


struct test_ele_struct {
	int	id1;
	int	id2;
} ;

typedef struct test_ele_struct	test_ele_t;

#define MAX_ELEMENTS	1024
test_ele_t	Test_elements[MAX_ELEMENTS];

#define VALID_TEST_ELE(p)						\
		( ( (test_ele_t *) (p) >= Test_elements ) &&		\
		  ( (test_ele_t *) (p) < Test_elements + MAX_ELEMENTS ) )


                      /****                          ****/
                      /****  TEST SUPPORT FUNCTIONS  ****/
                      /****                          ****/

/**
 **
 **/

static void	failure_msg (int line, const char *msg)
{
	printk("KLIST_TEST FAILURE at line %d: %s\n", line, msg);
}



                       /****                        ****/
                       /****  COMPARISON FUNCTIONS  ****/
                       /****                        ****/

/**
 ** FUNCTION: comp_func1
 **/

static int	comp_func1 (const void *ptr1, const void *ptr2)
{
	test_ele_t	*ele1;
	test_ele_t	*ele2;

	ele1 = (test_ele_t *) ptr1;
	ele2 = (test_ele_t *) ptr2;

	return	ele2->id1 - ele1->id1;
}



/**
 ** FUNCTION: comp_func2
 **/

static int	comp_func2 (const void *ptr1, const void *ptr2)
{
	test_ele_t	*ele1;
	test_ele_t	*ele2;

	ele1 = (test_ele_t *) ptr1;
	ele2 = (test_ele_t *) ptr2;

	return	ele1->id2 - ele2->id2;
}



/**
 ** FUNCTION: comp_func3
 **/

static int	comp_func3 (const void *ptr1, const void *ptr2)
{
	test_ele_t	*ele1;
	test_ele_t	*ele2;

	ele1 = (test_ele_t *) ptr1;
	ele2 = (test_ele_t *) ptr2;

	return	( ele1->id2 % 3 ) - ( ele2->id2 % 3 );
}



                          /****                  ****/
                          /****  TEST EXECUTION  ****/
                          /****                  ****/

#define FAILURE(x)	failure_msg(__LINE__, #x)
#define VERIFY(x)	do { if ( ! (x) ) { FAILURE(x); return; } } while (0)
#define VERIFY2(x)	do { if ( ! (x) ) { FAILURE(x); goto func__out; } } while (0)

/**
 ** FUNCTION: add_elements
 **/

static int	Ele_last = 0;

static int	add_elements (list_t a_list, int num_ele)
{
	void		*rc;
	int		cur;

	if ( num_ele == 0 )
		return;

	if ( ( Ele_last + num_ele ) >= MAX_ELEMENTS )
		printk("KLIST TEST OUCH: too many elements; %d requested, %d "
		       "used, %d max\n", num_ele, Ele_last, MAX_ELEMENTS);

	cur = 0;

	while ( cur < num_ele )
	{
		Test_elements[cur].id1 = Ele_last;
		Test_elements[cur].id2 = num_ele - ( Ele_last % num_ele );

		rc = klist_func(insert_into_list)(a_list, Test_elements + cur);

		VERIFY2( rc == ( Test_elements + cur ) );

		cur++;
		Ele_last++;
	}

	return	0;

func__out:
	return	1;
}



/**
 ** FUNCTION: check_index_ele
 **
 **  PURPOSE: Verify the index on the given elements.
 **/

static int	check_index_ele (list_t a_list, klist_index_id_t index_id,
		                 int mode)
{
	int		cur;
	int		num_ele;
	test_ele_t	*one_ele;
	void		*ptr;
	int		last;
	int		rc;

	num_ele = klist_func(list_length)(a_list);

	last = -1;
	cur  = 1;

	while ( cur <= num_ele )
	{
		rc = klist_func(read_index)(a_list, index_id, cur, &ptr);

		VERIFY2(rc == 0);
		VERIFY2(VALID_TEST_ELE(ptr));

		one_ele = (test_ele_t *) ptr;

		switch ( mode )
		{
		    case 1:
			VERIFY(one_ele->id1 > last);
			last = one_ele->id1;
			break;

		    case 2:
			VERIFY(one_ele->id2 > last);
			last = one_ele->id2;
			break;

		    case 3:
			VERIFY(( one_ele->id2 % 3 ) >= ( last % 3 ));
			last = one_ele->id2;
			break;
		}

		cur++;
	}

	return	0;

func__out:
	return	1;
}



/**
 ** FUNCTION: test_search
 **/

static int	test_search (list_t a_list, klist_index_id_t index_id,
		             int t_id, int mode)
{
	test_ele_t	find_ele;
	void		*ptr;
	int		pos;
	int		rc;

	find_ele.id1 = -1;
	find_ele.id2 = -1;

	switch ( mode )
	{
	    case 1:
		find_ele.id1 = t_id;
		break;

	    case 2:
		find_ele.id2 = t_id;
		break;

	    case 3:
		find_ele.id2 = t_id;
		break;
	}


	rc = klist_func(search_index)(a_list, index_id, &find_ele, &pos);
	VERIFY2(rc == 0);

	rc = klist_func(read_index)(a_list, index_id, pos, &ptr);
	VERIFY2(rc == 0);
	VERIFY2(VALID_TEST_ELE(ptr));

	switch ( mode )
	{
	    case 1:
		VERIFY2( (((test_ele_t *) ptr)->id1) == t_id );
		break;

	    case 2:
		VERIFY2( (((test_ele_t *) ptr)->id2) == t_id );
		break;

	    case 3:
		VERIFY2( (((test_ele_t *) ptr)->id2) == ( t_id % 3 ));
		break;
	}

	return	0;

func__out:
	return	1;
}



/**
 ** FUNCTION: run_test_group1
 **/

static int	run_test_group1 ()
{
	list_t			a_list;
	klist_index_id_t	index_id;
	klist_index_id_t	index_id2;
	klist_index_id_t	index_id3;
	void			*ptr;
	int			pos;
	int			rc;

	a_list = klist_func(create_list)(NULL, comp_func1, comp_func1, NULL);
	VERIFY(a_list != NULL);


		/* 1. Create an index */
		/* Add an index */

	rc = klist_func(add_index)(a_list, comp_func2, 0, &index_id);
	VERIFY2( ( rc == 0 ) && ( index_id == 1 ) );

		/* 2. Attempt to drop an index that doesn't exist */
		/* Attempt to remove non-existent indexes */

	rc = klist_func(drop_index)(a_list, 2);
	VERIFY2(rc == -EINVAL);

	rc = klist_func(drop_index)(a_list, 0);
	VERIFY2(rc == -EINVAL);

	rc = klist_func(drop_index)(a_list, 3);
	VERIFY2(rc == -EINVAL);

	rc = klist_func(drop_index)(a_list, -1);
	VERIFY2(rc == -EINVAL);


		/* 3. Attempt to read from an index that doesn't exist */
	rc = klist_func(read_index)(a_list, index_id + 1, 1, &ptr);
	VERIFY2(rc == -EINVAL);

		/* 4. Attempt to search an index that doesn't exist */
	rc = klist_func(search_index)(a_list, index_id + 1, NULL, &pos);
	VERIFY2(rc == -EINVAL);

		/* 5. Attempt to change the sort on an index that doesn't */
		/*    exist.                                              */
	rc = klist_func(change_index_sort)(a_list, index_id + 1, comp_func1);
	VERIFY2(rc == -EINVAL);


		/* 6. Drop an existing index */
		/* Remove the existing index. */
	rc = klist_func(drop_index)(a_list, index_id);
	VERIFY2(rc == 0);

		/* 31. Create a new index after dropping an older one */

		/* Add the index again. */
	rc = klist_func(add_index)(a_list, comp_func2, 0, &index_id);
	VERIFY2( ( rc == 0 ) && ( index_id == 1 ) );


		/* Add elements into the list. */
	rc = add_elements(a_list, 5);
	VERIFY2( rc == 0 );

		/* 7. Read from an existing index */
		/* 13. Read from an index with lazy update off */
		/* Check the order of the indexed elements. */
	rc = check_index_ele(a_list, index_id, 2);
	VERIFY2( rc == 0 );


		/* 8. Search an existing index */
		/* 16. Search an index with lazy update off */
		/* 20. Search for an element that is in the index */
	rc = test_search(a_list, index_id, 1, 2);
	VERIFY2( rc == 0 );
	rc = test_search(a_list, index_id, 3, 2);
	VERIFY2( rc == 0 );


		/* Remove an element from the list. */
	ptr = klist_func(remove_nth)(a_list, 2);
	VERIFY2(ptr != NULL);


		/* Check the order of the indexed elements again. */
	rc = check_index_ele(a_list, index_id, 2);
	VERIFY2( rc == 0 );

		/* 9. Change the sort on an existing index */
		/* 19. Change the sort of an index with lazy update off */
	rc = klist_func(change_index_sort)(a_list, index_id, comp_func3);
	VERIFY2(rc == 0);

		/* Check the order of the indexed elements again, using the */
		/*  new order.                                              */
	rc = check_index_ele(a_list, index_id, 3);
	VERIFY2( rc == 0 );


		/* Add an index with lazy update on. */
	rc = klist_func(add_index)(a_list, comp_func2, KLIST_INDEX__LAZY_UPD,
	                           &index_id2);
	VERIFY2( ( rc == 0 ) && ( index_id2 == 2 ) );

		/* 10. Read from an index with lazy update on, and index not */
		/*     up-to-date.                                           */
		/* 12. Read from an index with lazy update on, and index */
		/*     up-to-date                                        */
	rc = check_index_ele(a_list, index_id2, 2);
	VERIFY2( rc == 0 );


		/* Add another index with lazy update on. */
	rc = klist_func(add_index)(a_list, comp_func1, KLIST_INDEX__LAZY_UPD,
	                           &index_id3);
	VERIFY2( ( rc == 0 ) && ( index_id3 == 3 ) );

		/* 14. Search an index with lazy update on, and index not */
		/*     up-to-date                                         */
	rc = test_search(a_list, index_id3, 1, 1);
	VERIFY2( rc == 0 );

		/* 15. Search an index with lazy update on, and index     */
		/*     up-to-date                                         */
	rc = test_search(a_list, index_id3, 4, 1);
	VERIFY2( rc == 0 );

		/* 18. Change the sort of an index with lazy update on, and */
		/*     index up-to-date */
	rc = klist_func(change_index_sort)(a_list, index_id3, comp_func2);
	VERIFY2( rc == 0 );

		/* Verify the order of elements */
	rc = check_index_ele(a_list, index_id3, 2);
	VERIFY2( rc == 0 );


		/* 17. Change the sort of an index with lazy update on, and */
		/*     index not up-to-date */
		/* Add some more items to the list; this will cause the lazy */
		/*  indexes to be "out-of-date".                             */
	rc = add_elements(a_list, 2);
	VERIFY2( rc == 0 );

	rc = klist_func(change_index_sort)(a_list, index_id3, comp_func3);
	VERIFY2( rc == 0 );

		/* Verify the order of elements */
	rc = check_index_ele(a_list, index_id3, 3);
	VERIFY2( rc == 0 );


		/* 21. Search for an element that is not in the index */
	printk("KLIST_TEST: expect a failure\n");
	rc = test_search(a_list, index_id2, -1, 2);
	VERIFY2( rc == 1 );
	printk("KLIST_TEST: good\n");


		/* Cleanup. */
	klist_func(destroy_list)(a_list);

	return 0;

func__out:
	klist_func(destroy_list)(a_list);
	return 1;
}



/**
 ** FUNCTION: run_test_group2
 **/

static int	run_test_group2 ()
{
	list_t			a_list;
	klist_index_id_t	index_id;
	klist_index_id_t	index_id2;
	klist_index_id_t	index_id3;
	void			*ptr;
	int			pos;
	int			rc;

	a_list = klist_func(create_list)(NULL, comp_func1, comp_func1, NULL);
	VERIFY(a_list != NULL);

		/* Restart element numbering from 0 */
	Ele_last = 0;

		/* Add an index without lazy update. */

	rc = klist_func(add_index)(a_list, comp_func2, 0, &index_id);
	VERIFY2( ( rc == 0 ) && ( index_id == 1 ) );


		/* Add an index with lazy update. */

	rc = klist_func(add_index)(a_list, comp_func2, KLIST_INDEX__LAZY_UPD,
	                           &index_id2);
	VERIFY2( ( rc == 0 ) && ( index_id == 1 ) );


		/* 26. Insert an element into a list with lazy update on */
		/* 27. Insert an element into a list with lazy update off */
		/* Add elements. */

	rc = add_elements(a_list, 25);
	VERIFY2( rc == 0 );

		/* Verify the order of elements just inserted */
	rc = check_index_ele(a_list, index_id, 2);
	VERIFY2( rc == 0 );

	rc = check_index_ele(a_list, index_id2, 2);
	VERIFY2( rc == 0 );

		/* Search for specific elements; the id2 value range is 1-25 */

		/* 22. Search for the first element in the index */
	rc = test_search(a_list, index_id, 1, 2);
	VERIFY2( rc == 0 );

		/* 23. Search for the last element in the index */
	rc = test_search(a_list, index_id2, 25, 2);
	VERIFY2( rc == 0 );

		/* 24. Search for the second element in the index */
	rc = test_search(a_list, index_id2, 2, 2);
	VERIFY2( rc == 0 );

		/* 25. Search for the second-to-last element in the index */
	rc = test_search(a_list, index_id2, 24, 2);
	VERIFY2( rc == 0 );

		/* 28. Delete an element from a list with lazy update on */
		/* 29. Delete an element from a list with lazy update off */
	ptr = klist_func(remove_nth)(a_list, 2);
	VERIFY2(ptr != NULL);

		/* Check the indexes */
	rc = check_index_ele(a_list, index_id, 2);
	VERIFY2( rc == 0 );
	rc = check_index_ele(a_list, index_id2, 2);
	VERIFY2( rc == 0 );

		/* 30. Access a newer index after dropping an older one */
		/* Drop the first index and access the second one. */
	rc = klist_func(drop_index)(a_list, index_id);
	VERIFY2(rc == 0);

	rc = check_index_ele(a_list, index_id2, 2);
	VERIFY2( rc == 0 );

		/* 32. Read a non-existent element from an existing index */
		/* Attempt to read past the end of the index. */
	rc = klist_func(read_index)(a_list, index_id2, 26, &ptr);
	VERIFY2(rc == -EINVAL);

		/* Cleanup. */
	klist_func(destroy_list)(a_list);

	return 0;

func__out:
	klist_func(destroy_list)(a_list);
	return 1;
}



/**
 ** FUNCTION: run_tests
 **/

static int	run_tests ()
{
	int	rc;

	rc = run_test_group1();

	if ( rc != 0 )
		goto	func__out;


	rc = run_test_group2();

	if ( rc != 0 )
		goto	func__out;

	printk("KLIST TEST: SUCCESS\n");
	return 0;

func__out:
	return 1;
}



                         /****                   ****/
                         /****  /proc INTERFACE  ****/
                         /****                   ****/

#define PROC_NODE_NAME	"klist_test"


/**
 ** FUNCTION: test_read_proc
 **/

static int	test_read_proc (char *page, char **start, off_t off, int count,
		                int *eof, void *data)
{
	int	rc;
	int	ret_code;

	if ( off != 0 )
	{
		eof[0] = 1;
		return	0;
	}

		/* Restart the numbering. */
	Ele_last = 0;

	rc = run_tests();

	if ( rc == 0 )
	{
		strcpy(page, "SUCCESS\n");
		ret_code = 8;
	}
	else
	{
		strcpy(page, "FAILURE\n");
		ret_code = 8;
	}

	eof[0] = 1;
	return	ret_code;
}



/**
 ** FUNCTION: init_proc_if
 ** FUNCTION: remove_proc_if
 **/

static void	init_proc_if ()
{
	struct proc_dir_entry	*new_ent;

	new_ent = create_proc_entry(PROC_NODE_NAME, 0666, NULL);

	if ( new_ent != NULL )
		new_ent->read_proc = test_read_proc;
}

static void	remove_proc_if ()
{
	remove_proc_entry(PROC_NODE_NAME, NULL);
}



			/**********************/
			/** MODULE INTERFACE **/
			/**********************/


#ifdef MODULE

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arthur Naseef");
MODULE_DESCRIPTION("Test of the generic Link List support for Linux");

/**
 ** FUNCTION: init_module
 **
 **  PURPOSE: Initialize the module.
 **/

int	init_module ()
{
	printk("starting kernel list test module (compiled %s)\n",
	       macro2str(DATE_STAMP));

	init_proc_if();

	return	0;
}

void	cleanup_module()
{
	printk("removing kernel list test module\n");
	remove_proc_if();
}

#endif
