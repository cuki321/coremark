/*
Author : Shay Gal-On, EEMBC

This file is part of  EEMBC(R) and CoreMark(TM), which are Copyright (C) 2009 
All rights reserved.                            

EEMBC CoreMark Software is a product of EEMBC and is provided under the terms of the
CoreMark License that is distributed with the official EEMBC COREMARK Software release. 
If you received this EEMBC CoreMark Software without the accompanying CoreMark License, 
you must discontinue use and download the official release from www.coremark.org.  

Also, if you are publicly displaying scores generated from the EEMBC CoreMark software, 
make sure that you are in compliance with Run and Reporting rules specified in the accompanying readme.txt file.

EEMBC 
4354 Town Center Blvd. Suite 114-200
El Dorado Hills, CA, 95762 
*/ 
/* File: core_main.c
	This file contains the framework to acquire a block of memory, seed initial parameters, tun t he benchmark and report the results.
*/
#include "coremark.h"

//--------------------------------------------------------core_state.c---------------------------------------------------------//
enum CORE_STATE core_state_transition( ee_u8 **instr , ee_u32 *transition_count);

/*
Topic: Description
	Simple state machines like this one are used in many embedded products.
	
	For more complex state machines, sometimes a state transition table implementation is used instead, 
	trading speed of direct coding for ease of maintenance.
	
	Since the main goal of using a state machine in CoreMark is to excercise the switch/if behaviour,
	we are using a small moore machine. 
	
	In particular, this machine tests type of string input,
	trying to determine whether the input is a number or something else.
	(see core_state.png).
*/

/* Function: core_bench_state
	Benchmark function

	Go over the input twice, once direct, and once after introducing some corruption. 
*/
ee_u16 core_bench_state(ee_u32 blksize, ee_u8 *memblock, 
		ee_s16 seed1, ee_s16 seed2, ee_s16 step, ee_u16 crc) 
{
	ee_u32 final_counts[NUM_CORE_STATES];
	ee_u32 track_counts[NUM_CORE_STATES];
	ee_u8 *p=memblock;
	ee_u32 i;


#if CORE_DEBUG
	ee_printf("State Bench: %d,%d,%d,%04x\n",seed1,seed2,step,crc);
#endif
	for (i=0; i<NUM_CORE_STATES; i++) {
		final_counts[i]=track_counts[i]=0;
	}
	/* run the state machine over the input */
	while (*p!=0) {
		enum CORE_STATE fstate=core_state_transition(&p,track_counts);
		final_counts[fstate]++;
#if CORE_DEBUG
	ee_printf("%d,",fstate);
	}
	ee_printf("\n");
#else
	}
#endif
	p=memblock;
	while (p < (memblock+blksize)) { /* insert some corruption */
		if (*p!=',')
			*p^=(ee_u8)seed1;
		p+=step;
	}
	p=memblock;
	/* run the state machine over the input again */
	while (*p!=0) {
		enum CORE_STATE fstate=core_state_transition(&p,track_counts);
		final_counts[fstate]++;
#if CORE_DEBUG
	ee_printf("%d,",fstate);
	}
	ee_printf("\n");
#else
	}
#endif
	p=memblock;
	while (p < (memblock+blksize)) { /* undo corruption is seed1 and seed2 are equal */
		if (*p!=',')
			*p^=(ee_u8)seed2;
		p+=step;
	}
	/* end timing */
	for (i=0; i<NUM_CORE_STATES; i++) {
		crc=crcu32(final_counts[i],crc);
		crc=crcu32(track_counts[i],crc);
	}
	return crc;
}

/* Default initialization patterns */
static ee_u8 *intpat[4]  ={(ee_u8 *)"5012",(ee_u8 *)"1234",(ee_u8 *)"-874",(ee_u8 *)"+122"};
static ee_u8 *floatpat[4]={(ee_u8 *)"35.54400",(ee_u8 *)".1234500",(ee_u8 *)"-110.700",(ee_u8 *)"+0.64400"};
static ee_u8 *scipat[4]  ={(ee_u8 *)"5.500e+3",(ee_u8 *)"-.123e-2",(ee_u8 *)"-87e+832",(ee_u8 *)"+0.6e-12"};
static ee_u8 *errpat[4]  ={(ee_u8 *)"T0.3e-1F",(ee_u8 *)"-T.T++Tq",(ee_u8 *)"1T3.4e4z",(ee_u8 *)"34.0e-T^"};

/* Function: core_init_state
	Initialize the input data for the state machine.

	Populate the input with several predetermined strings, interspersed.
	Actual patterns chosen depend on the seed parameter.
	
	Note:
	The seed parameter MUST be supplied from a source that cannot be determined at compile time
*/
void core_init_state(ee_u32 size, ee_s16 seed, ee_u8 *p) {
	ee_u32 total=0,next=0,i;
	ee_u8 *buf=0;
#if CORE_DEBUG
	ee_u8 *start=p;
	ee_printf("State: %d,%d\n",size,seed);
#endif
	size--;
	next=0;
	while ((total+next+1)<size) {
		if (next>0) {
			for(i=0;i<next;i++)
				*(p+total+i)=buf[i];
			*(p+total+i)=',';
			total+=next+1;
		}
		seed++;
		switch (seed & 0x7) {
			case 0: /* int */
			case 1: /* int */
			case 2: /* int */
				buf=intpat[(seed>>3) & 0x3];
				next=4;
			break;
			case 3: /* float */
			case 4: /* float */
				buf=floatpat[(seed>>3) & 0x3];
				next=8;
			break;
			case 5: /* scientific */
			case 6: /* scientific */
				buf=scipat[(seed>>3) & 0x3];
				next=8;
			break;
			case 7: /* invalid */
				buf=errpat[(seed>>3) & 0x3];
				next=8;
			break;
			default: /* Never happen, just to make some compilers happy */
			break;
		}
	}
	size++;
	while (total<size) { /* fill the rest with 0 */
		*(p+total)=0;
		total++;
	}
#if CORE_DEBUG
	ee_printf("State Input: %s\n",start);
#endif
}

static ee_u8 ee_isdigit(ee_u8 c) {
	ee_u8 retval;
	retval = ((c>='0') & (c<='9')) ? 1 : 0;
	return retval;
}

/* Function: core_state_transition
	Actual state machine.

	The state machine will continue scanning until either:
	1 - an invalid input is detcted.
	2 - a valid number has been detected.
	
	The input pointer is updated to point to the end of the token, and the end state is returned (either specific format determined or invalid).
*/

enum CORE_STATE core_state_transition( ee_u8 **instr , ee_u32 *transition_count) {
	ee_u8 *str=*instr;
	ee_u8 NEXT_SYMBOL;
	enum CORE_STATE state=CORE_START;
	for( ; *str && state != CORE_INVALID; str++ ) {
		NEXT_SYMBOL = *str;
		if (NEXT_SYMBOL==',') /* end of this input */ {
			str++;
			break;
		}
		switch(state) {
		case CORE_START:
			if(ee_isdigit(NEXT_SYMBOL)) {
				state = CORE_INT;
			}
			else if( NEXT_SYMBOL == '+' || NEXT_SYMBOL == '-' ) {
				state = CORE_S1;
			}
			else if( NEXT_SYMBOL == '.' ) {
				state = CORE_FLOAT;
			}
			else {
				state = CORE_INVALID;
				transition_count[CORE_INVALID]++;
			}
			transition_count[CORE_START]++;
			break;
		case CORE_S1:
			if(ee_isdigit(NEXT_SYMBOL)) {
				state = CORE_INT;
				transition_count[CORE_S1]++;
			}
			else if( NEXT_SYMBOL == '.' ) {
				state = CORE_FLOAT;
				transition_count[CORE_S1]++;
			}
			else {
				state = CORE_INVALID;
				transition_count[CORE_S1]++;
			}
			break;
		case CORE_INT:
			if( NEXT_SYMBOL == '.' ) {
				state = CORE_FLOAT;
				transition_count[CORE_INT]++;
			}
			else if(!ee_isdigit(NEXT_SYMBOL)) {
				state = CORE_INVALID;
				transition_count[CORE_INT]++;
			}
			break;
		case CORE_FLOAT:
			if( NEXT_SYMBOL == 'E' || NEXT_SYMBOL == 'e' ) {
				state = CORE_S2;
				transition_count[CORE_FLOAT]++;
			}
			else if(!ee_isdigit(NEXT_SYMBOL)) {
				state = CORE_INVALID;
				transition_count[CORE_FLOAT]++;
			}
			break;
		case CORE_S2:
			if( NEXT_SYMBOL == '+' || NEXT_SYMBOL == '-' ) {
				state = CORE_EXPONENT;
				transition_count[CORE_S2]++;
			}
			else {
				state = CORE_INVALID;
				transition_count[CORE_S2]++;
			}
			break;
		case CORE_EXPONENT:
			if(ee_isdigit(NEXT_SYMBOL)) {
				state = CORE_SCIENTIFIC;
				transition_count[CORE_EXPONENT]++;
			}
			else {
				state = CORE_INVALID;
				transition_count[CORE_EXPONENT]++;
			}
			break;
		case CORE_SCIENTIFIC:
			if(!ee_isdigit(NEXT_SYMBOL)) {
				state = CORE_INVALID;
				transition_count[CORE_INVALID]++;
			}
			break;
		default:
			break;
		}
	}
	*instr=str;
	return state;
}
//--------------------------------------------------------core_state.c---------------------------------------------------------//

//------------------------------------------------------------core_list_join.c-------------------------------------------------//
//#include "Arduino.h"
//extern ee_u16 core_bench_list(core_results *res, ee_s16 finder_idx);
list_head *core_list_find(list_head *list,list_data *info);
list_head *core_list_reverse(list_head *list);
list_head *core_list_remove(list_head *item);
list_head *core_list_undo_remove(list_head *item_removed, list_head *item_modified);
list_head *core_list_insert_new(list_head *insert_point
	, list_data *info, list_head **memblock, list_data **datablock
	, list_head *memblock_end, list_data *datablock_end);
typedef ee_s32(*list_cmp)(list_data *a, list_data *b, core_results *res);
list_head *core_list_mergesort(list_head *list, list_cmp cmp, core_results *res);

ee_s16 calc_func(ee_s16 *pdata, core_results *res) {
	ee_s16 data=*pdata;
	ee_s16 retval;
	ee_u8 optype=(data>>7) & 1; /* bit 7 indicates if the function result has been cached */
	if (optype) /* if cached, use cache */
		return (data & 0x007f);
	else { /* otherwise calculate and cache the result */
		ee_s16 flag=data & 0x7; /* bits 0-2 is type of function to perform */
		ee_s16 dtype=((data>>3) & 0xf); /* bits 3-6 is specific data for the operation */
		dtype |= dtype << 4; /* replicate the lower 4 bits to get an 8b value */
		switch (flag) {
			case 0:
				if (dtype<0x22) /* set min period for bit corruption */
					dtype=0x22;
				retval=core_bench_state(res->size,res->memblock[3],res->seed1,res->seed2,dtype,res->crc);
				if (res->crcstate==0)
					res->crcstate=retval;
				break;
			case 1:
				retval=core_bench_matrix(&(res->mat),dtype,res->crc);
				if (res->crcmatrix==0)
					res->crcmatrix=retval;
				break;
			default:
				retval=data;
				break;
		}
		res->crc=crcu16(retval,res->crc);
		retval &= 0x007f; 
		*pdata = (data & 0xff00) | 0x0080 | retval; /* cache the result */
		return retval;
	}
}
/* Function: cmp_complex
	Compare the data item in a list cell.

	Can be used by mergesort.
*/
ee_s32 cmp_complex(list_data *a, list_data *b, core_results *res) {
	ee_s16 val1=calc_func(&(a->data16),res);
	ee_s16 val2=calc_func(&(b->data16),res);
	return val1 - val2;
}

/* Function: cmp_idx
	Compare the idx item in a list cell, and regen the data.

	Can be used by mergesort.
*/
ee_s32 cmp_idx(list_data *a, list_data *b, core_results *res) {
	if (res==NULL) {
		a->data16 = (a->data16 & 0xff00) | (0x00ff & (a->data16>>8));
		b->data16 = (b->data16 & 0xff00) | (0x00ff & (b->data16>>8));
	}
	return a->idx - b->idx;
}

void copy_info(list_data *to,list_data *from) {
	to->data16=from->data16;
	to->idx=from->idx;
}

/* Benchmark for linked list:
	- Try to find multiple data items.
	- List sort
	- Operate on data from list (crc)
	- Single remove/reinsert
	* At the end of this function, the list is back to original state
*/
ee_u16 core_bench_list(core_results *res, ee_s16 finder_idx) {
	ee_u16 retval=0;
	ee_u16 found=0,missed=0;
	list_head *list=res->list;
	ee_s16 find_num=res->seed3;
	list_head *this_find;
	list_head *finder, *remover;
	list_data info;
	ee_s16 i;

	info.idx=finder_idx;
	/* find <find_num> values in the list, and change the list each time (reverse and cache if value found) */
	for (i=0; i<find_num; i++) {
		info.data16= (i & 0xff) ;
		this_find=core_list_find(list,&info);
		list=core_list_reverse(list);
		if (this_find==NULL) {
			missed++;
			retval+=(list->next->info->data16 >> 8) & 1;
		}
		else {
			found++;
			if (this_find->info->data16 & 0x1) /* use found value */
				retval+=(this_find->info->data16 >> 9) & 1;
			/* and cache next item at the head of the list (if any) */
			if (this_find->next != NULL) {
				finder = this_find->next;
				this_find->next = finder->next;
				finder->next=list->next;
				list->next=finder;
			}
		}
		if (info.idx>=0)
			info.idx++;
#if CORE_DEBUG
	ee_printf("List find %d: [%d,%d,%d]\n",i,retval,missed,found);
#endif
	}
	retval+=found*4-missed;
	/* sort the list by data content and remove one item*/
	if (finder_idx>0)
		list=core_list_mergesort(list,cmp_complex,res);
	remover=core_list_remove(list->next);
	/* CRC data content of list from location of index N forward, and then undo remove */
	finder=core_list_find(list,&info);
	if (!finder)
		finder=list->next;
	while (finder) {
		retval=crc16(list->info->data16,retval);
		finder=finder->next;
	}
#if CORE_DEBUG
	ee_printf("List sort 1: %04x\n",retval);
#endif
	remover=core_list_undo_remove(remover,list->next);
	/* sort the list by index, in effect returning the list to original state */
	list=core_list_mergesort(list,cmp_idx,NULL);
	/* CRC data content of list */
	finder=list->next;
	while (finder) {
		retval=crc16(list->info->data16,retval);
		finder=finder->next;
	}
#if CORE_DEBUG
	ee_printf("List sort 2: %04x\n",retval);
#endif
	return retval;
}
/* Function: core_list_init
	Initialize list with data.

	Parameters:
	blksize - Size of memory to be initialized.
	memblock - Pointer to memory block.
	seed - 	Actual values chosen depend on the seed parameter.
		The seed parameter MUST be supplied from a source that cannot be determined at compile time

	Returns:
	Pointer to the head of the list.

*/
list_head *core_list_init(ee_u32 blksize, list_head *memblock, ee_s16 seed) {
	/* calculated pointers for the list */
	ee_u32 per_item=16+sizeof(struct list_data_s);
	ee_u32 size=(blksize/per_item)-2; /* to accomodate systems with 64b pointers, and make sure same code is executed, set max list elements */
	list_head *memblock_end=memblock+size;
	list_data *datablock=(list_data *)(memblock_end);
	list_data *datablock_end=datablock+size;
	/* some useful variables */
	ee_u32 i;
	list_head *finder,*list=memblock;
	list_data info;

	/* create a fake items for the list head and tail */
	list->next=NULL;
	list->info=datablock;
	list->info->idx=0x0000;
	list->info->data16=(ee_s16)0x8080;
	memblock++;
	datablock++;
	info.idx=0x7fff;
	info.data16=(ee_s16)0xffff;
	core_list_insert_new(list,&info,&memblock,&datablock,memblock_end,datablock_end);
	
	/* then insert size items */
	for (i=0; i<size; i++) {
		ee_u16 datpat=((ee_u16)(seed^i) & 0xf);
		ee_u16 dat=(datpat<<3) | (i&0x7); /* alternate between algorithms */
		info.data16=(dat<<8) | dat;		/* fill the data with actual data and upper bits with rebuild value */
		core_list_insert_new(list,&info,&memblock,&datablock,memblock_end,datablock_end);
	}
	/* and now index the list so we know initial seed order of the list */
	finder=list->next;
	i=1;
	while (finder->next!=NULL) {
		if (i<size/5) /* first 20% of the list in order */
			finder->info->idx=i++;
		else { 
			ee_u16 pat=(ee_u16)(i++ ^ seed); /* get a pseudo random number */
			finder->info->idx=0x3fff & (((i & 0x07) << 8) | pat); /* make sure the mixed items end up after the ones in sequence */
		}
		finder=finder->next;
	}
	list = core_list_mergesort(list,cmp_idx,NULL);
#if CORE_DEBUG
	ee_printf("Initialized list:\n");
	finder=list;
	while (finder) {
		ee_printf("[%04x,%04x]",finder->info->idx,(ee_u16)finder->info->data16);
		finder=finder->next;
	}
	ee_printf("\n");
#endif
	return list;
}

/* Function: core_list_insert
	Insert an item to the list

	Parameters:
	insert_point - where to insert the item.
	info - data for the cell.
	memblock - pointer for the list header
	datablock - pointer for the list data
	memblock_end - end of region for list headers
	datablock_end - end of region for list data

	Returns:
	Pointer to new item.
*/
list_head *core_list_insert_new(list_head *insert_point, list_data *info, list_head **memblock, list_data **datablock
	, list_head *memblock_end, list_data *datablock_end) {
	list_head *newitem;
	
	if ((*memblock+1) >= memblock_end)
		return NULL;
	if ((*datablock+1) >= datablock_end)
		return NULL;
		
	newitem=*memblock;
	(*memblock)++;
	newitem->next=insert_point->next;
	insert_point->next=newitem;
	
	newitem->info=*datablock;
	(*datablock)++;
	copy_info(newitem->info,info);
	
	return newitem;
}

/* Function: core_list_remove
	Remove an item from the list.

	Operation:
	For a singly linked list, remove by copying the data from the next item 
	over to the current cell, and unlinking the next item.

	Note: 
	since there is always a fake item at the end of the list, no need to check for NULL.

	Returns:
	Removed item.
*/
list_head *core_list_remove(list_head *item) {
	list_data *tmp;
	list_head *ret=item->next;
	/* swap data pointers */
	tmp=item->info;
	item->info=ret->info;
	ret->info=tmp;
	/* and eliminate item */
	item->next=item->next->next;
	ret->next=NULL;
	return ret;
}

/* Function: core_list_undo_remove
	Undo a remove operation.

	Operation:
	Since we want each iteration of the benchmark to be exactly the same,
	we need to be able to undo a remove. 
	Link the removed item back into the list, and switch the info items.

	Parameters:
	item_removed - Return value from the <core_list_remove>
	item_modified - List item that was modified during <core_list_remove>

	Returns:
	The item that was linked back to the list.
	
*/
list_head *core_list_undo_remove(list_head *item_removed, list_head *item_modified) {
	list_data *tmp;
	/* swap data pointers */
	tmp=item_removed->info;
	item_removed->info=item_modified->info;
	item_modified->info=tmp;
	/* and insert item */
	item_removed->next=item_modified->next;
	item_modified->next=item_removed;
	return item_removed;
}

/* Function: core_list_find
	Find an item in the list

	Operation:
	Find an item by idx (if not 0) or specific data value

	Parameters:
	list - list head
	info - idx or data to find

	Returns:
	Found item, or NULL if not found.
*/
list_head *core_list_find(list_head *list,list_data *info) {
	if (info->idx>=0) {
		while (list && (list->info->idx != info->idx))
			list=list->next;
		return list;
	} else {
		while (list && ((list->info->data16 & 0xff) != info->data16))
			list=list->next;
		return list;
	}
}
/* Function: core_list_reverse
	Reverse a list

	Operation:
	Rearrange the pointers so the list is reversed.

	Parameters:
	list - list head
	info - idx or data to find

	Returns:
	Found item, or NULL if not found.
*/

list_head *core_list_reverse(list_head *list) {
	list_head *next=NULL, *tmp;
	while (list) {
		tmp=list->next;
		list->next=next;
		next=list;
		list=tmp;
	}
	return next;
}
/* Function: core_list_mergesort
	Sort the list in place without recursion.

	Description:
	Use mergesort, as for linked list this is a realistic solution. 
	Also, since this is aimed at embedded, care was taken to use iterative rather then recursive algorithm.
	The sort can either return the list to original order (by idx) ,
	or use the data item to invoke other other algorithms and change the order of the list.

	Parameters:
	list - list to be sorted.
	cmp - cmp function to use

	Returns:
	New head of the list.

	Note: 
	We have a special header for the list that will always be first,
	but the algorithm could theoretically modify where the list starts.

 */
list_head *core_list_mergesort(list_head *list, list_cmp cmp, core_results *res) {
    list_head *p, *q, *e, *tail;
    ee_s32 insize, nmerges, psize, qsize, i;

    insize = 1;

    while (1) {
        p = list;
        list = NULL;
        tail = NULL;

        nmerges = 0;  /* count number of merges we do in this pass */

        while (p) {
            nmerges++;  /* there exists a merge to be done */
            /* step `insize' places along from p */
            q = p;
            psize = 0;
            for (i = 0; i < insize; i++) {
                psize++;
			    q = q->next;
                if (!q) break;
            }

            /* if q hasn't fallen off end, we have two lists to merge */
            qsize = insize;

            /* now we have two lists; merge them */
            while (psize > 0 || (qsize > 0 && q)) {

				/* decide whether next element of merge comes from p or q */
				if (psize == 0) {
				    /* p is empty; e must come from q. */
				    e = q; q = q->next; qsize--;
				} else if (qsize == 0 || !q) {
				    /* q is empty; e must come from p. */
				    e = p; p = p->next; psize--;
				} else if (cmp(p->info,q->info,res) <= 0) {
				    /* First element of p is lower (or same); e must come from p. */
				    e = p; p = p->next; psize--;
				} else {
				    /* First element of q is lower; e must come from q. */
				    e = q; q = q->next; qsize--;
				}

		        /* add the next element to the merged list */
				if (tail) {
				    tail->next = e;
				} else {
				    list = e;
				}
				tail = e;
	        }

			/* now p has stepped `insize' places along, and q has too */
			p = q;
        }
		
	    tail->next = NULL;

        /* If we have done only one merge, we're finished. */
        if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
            return list;

        /* Otherwise repeat, merging lists twice the size */
        insize *= 2;
    }
#if COMPILER_REQUIRES_SORT_RETURN
	return list;
#endif
}
//------------------------------------------------------------core_list_join.c-------------------------------------------------//

//--------------------------------------------------------core_util.c---------------------------------------------------------//
#if (SEED_METHOD==SEED_VOLATILE)
	extern volatile ee_s32 seed1_volatile;
	extern volatile ee_s32 seed2_volatile;
	extern volatile ee_s32 seed3_volatile;
	extern volatile ee_s32 seed4_volatile;
	extern volatile ee_s32 seed5_volatile;
	ee_s32 get_seed_32(int i) {
		ee_s32 retval;
		switch (i) {
			case 1:
				retval=seed1_volatile;
				break;
			case 2:
				retval=seed2_volatile;
				break;
			case 3:
				retval=seed3_volatile;
				break;
			case 4:
				retval=seed4_volatile;
				break;
			case 5:
				retval=seed5_volatile;
				break;
			default:
				retval=0;
				break;
		}
		return retval;
	}
#elif (SEED_METHOD==SEED_ARG)
ee_s32 parseval(char *valstring) {
	ee_s32 retval=0;
	ee_s32 neg=1;
	int hexmode=0;
	if (*valstring == '-') {
		neg=-1;
		valstring++;
	}
	if ((valstring[0] == '0') && (valstring[1] == 'x')) {
		hexmode=1;
		valstring+=2;
	}
		/* first look for digits */
	if (hexmode) {
		while (((*valstring >= '0') && (*valstring <= '9')) || ((*valstring >= 'a') && (*valstring <= 'f'))) {
			ee_s32 digit=*valstring-'0';
			if (digit>9)
				digit=10+*valstring-'a';
			retval*=16;
			retval+=digit;
			valstring++;
		}
	} else {
		while ((*valstring >= '0') && (*valstring <= '9')) {
			ee_s32 digit=*valstring-'0';
			retval*=10;
			retval+=digit;
			valstring++;
		}
	}
	/* now add qualifiers */
	if (*valstring=='K')
		retval*=1024;
	if (*valstring=='M')
		retval*=1024*1024;

	retval*=neg;
	return retval;
}

ee_s32 get_seed_args(int i, int argc, char *argv[]) {
	if (argc>i)
		return parseval(argv[i]);
	return 0;
}

#elif (SEED_METHOD==SEED_FUNC)
/* If using OS based function, you must define and implement the functions below in core_portme.h and core_portme.c ! */
ee_s32 get_seed_32(int i) {
	ee_s32 retval;
	switch (i) {
		case 1:
			retval=portme_sys1();
			break;
		case 2:
			retval=portme_sys2();
			break;
		case 3:
			retval=portme_sys3();
			break;
		case 4:
			retval=portme_sys4();
			break;
		case 5:
			retval=portme_sys5();
			break;
		default:
			retval=0;
			break;
	}
	return retval;
}
#endif

/* Function: crc*
	Service functions to calculate 16b CRC code.

*/
ee_u16 crcu8(ee_u8 data, ee_u16 crc )
{
	ee_u8 i=0,x16=0,carry=0;

	for (i = 0; i < 8; i++)
    {
		x16 = (ee_u8)((data & 1) ^ ((ee_u8)crc & 1));
		data >>= 1;

		if (x16 == 1)
		{
		   crc ^= 0x4002;
		   carry = 1;
		}
		else 
			carry = 0;
		crc >>= 1;
		if (carry)
		   crc |= 0x8000;
		else
		   crc &= 0x7fff;
    }
	return crc;
} 
ee_u16 crcu16(ee_u16 newval, ee_u16 crc) {
	crc=crcu8( (ee_u8) (newval)				,crc);
	crc=crcu8( (ee_u8) ((newval)>>8)	,crc);
	return crc;
}
ee_u16 crcu32(ee_u32 newval, ee_u16 crc) {
	crc=crc16((ee_s16) newval		,crc);
	crc=crc16((ee_s16) (newval>>16)	,crc);
	return crc;
}
ee_u16 crc16(ee_s16 newval, ee_u16 crc) {
	return crcu16((ee_u16)newval, crc);
}

ee_u8 check_data_types() {
	ee_u8 retval=0;
	if (sizeof(ee_u8) != 1) {
		ee_printf("ERROR: ee_u8 is not an 8b datatype!\n");
		retval++;
	}
	if (sizeof(ee_u16) != 2) {
		ee_printf("ERROR: ee_u16 is not a 16b datatype!\n");
		retval++;
	}
	if (sizeof(ee_s16) != 2) {
		ee_printf("ERROR: ee_s16 is not a 16b datatype!\n");
		retval++;
	}
	if (sizeof(ee_s32) != 4) {
		ee_printf("ERROR: ee_s32 is not a 32b datatype!\n");
		retval++;
	}
	if (sizeof(ee_u32) != 4) {
		ee_printf("ERROR: ee_u32 is not a 32b datatype!\n");
		retval++;
	}
	if (sizeof(ee_ptr_int) != sizeof(int *)) {
		ee_printf("ERROR: ee_ptr_int is not a datatype that holds an int pointer!\n");
		retval++;
	}
	if (retval>0) {
		ee_printf("ERROR: Please modify the datatypes in core_portme.h!\n");
	}
	return retval;
}

//------------------------------------------------------------core_util.c-------------------------------------------------//

/* Function: iterate
	Run the benchmark for a specified number of iterations.

	Operation:
	For each type of benchmarked algorithm:
		a - Initialize the data block for the algorithm.
		b - Execute the algorithm N times.

	Returns:
	NULL.
*/
static ee_u16 list_known_crc[]   =      {(ee_u16)0xd4b0,(ee_u16)0x3340,(ee_u16)0x6a79,(ee_u16)0xe714,(ee_u16)0xe3c1};
static ee_u16 matrix_known_crc[] =      {(ee_u16)0xbe52,(ee_u16)0x1199,(ee_u16)0x5608,(ee_u16)0x1fd7,(ee_u16)0x0747};
static ee_u16 state_known_crc[]  =      {(ee_u16)0x5e47,(ee_u16)0x39bf,(ee_u16)0xe5a4,(ee_u16)0x8e3a,(ee_u16)0x8d84};
void *iterate(void *pres) {
	ee_u32 i;
	ee_u16 crc;
	core_results *res=(core_results *)pres;
	ee_u32 iterations=res->iterations;
	res->crc=0;
	res->crclist=0;
	res->crcmatrix=0;
	res->crcstate=0;

	for (i=0; i<iterations; i++) {
		crc=core_bench_list(res,1);
		res->crc=crcu16(crc,res->crc);
		crc=core_bench_list(res,-1);
		res->crc=crcu16(crc,res->crc);
		if (i==0) res->crclist=res->crc;
	}
	return NULL;
}

#if (SEED_METHOD==SEED_ARG)
ee_s32 get_seed_args(int i, int argc, char *argv[]);
#define get_seed(x) (ee_s16)get_seed_args(x,argc,argv)
#define get_seed_32(x) get_seed_args(x,argc,argv)
#else /* via function or volatile */
ee_s32 get_seed_32(int i);
#define get_seed(x) (ee_s16)get_seed_32(x)
#endif

#if (MEM_METHOD==MEM_STATIC)
ee_u8 static_memblk[TOTAL_DATA_SIZE];
#endif
char *mem_name[3] = {"Static","Heap","Stack"};
/* Function: main
	Main entry routine for the benchmark.
	This function is responsible for the following steps:

	1 - Initialize input seeds from a source that cannot be determined at compile time.
	2 - Initialize memory block for use.
	3 - Run and time the benchmark.
	4 - Report results, testing the validity of the output if the seeds are known.

	Arguments:
	1 - first seed  : Any value
	2 - second seed : Must be identical to first for iterations to be identical
	3 - third seed  : Any value, should be at least an order of magnitude less then the input size, but bigger then 32.
	4 - Iterations  : Special, if set to 0, iterations will be automatically determined such that the benchmark will run between 10 to 100 secs

*/

/*#if MAIN_HAS_NOARGC
MAIN_RETURN_TYPE coremark_main(void) {
	int argc=0;
	char *argv[1];
#else
MAIN_RETURN_TYPE coremark_main(int argc, char *argv[]) {
#endif*/

//----------------------------LT--------------------------------//
int coremark_main(void) {
	int argc=0;
	char *argv[1];
	ee_u16 i,j=0,num_algorithms=0;
	ee_s16 known_id=-1,total_errors=0;
	ee_u16 seedcrc=0;
	CORE_TICKS total_time;
	core_results results[MULTITHREAD];
#if (MEM_METHOD==MEM_STACK)
	ee_u8 stack_memblock[TOTAL_DATA_SIZE*MULTITHREAD];
#endif

	/* first call any initializations needed */
	//--LT--// portable_init(&(results[0].port), &argc, argv);
	/* First some checks to make sure benchmark will run ok */
	if (sizeof(struct list_head_s)>128) {
		ee_printf("list_head structure too big for comparable data!\n");
		return MAIN_RETURN_VAL;
	}
	/*results[0].seed1=get_seed(1);
	results[0].seed2=get_seed(2);
	results[0].seed3=get_seed(3);
	results[0].iterations=get_seed_32(4);*/
#if CORE_DEBUG
	results[0].iterations=1;
#endif
	//results[0].execs=get_seed_32(5);
	if (results[0].execs==0) { /* if not supplied, execute all algorithms */
		results[0].execs=ALL_ALGORITHMS_MASK;
	}
	/* put in some default values based on one seed only for easy testing */
	if ((results[0].seed1==0) && (results[0].seed2==0) && (results[0].seed3==0)) { /* validation run */
		results[0].seed1=0;
		results[0].seed2=0;
		results[0].seed3=0x66;
	}
	if ((results[0].seed1==1) && (results[0].seed2==0) && (results[0].seed3==0)) { /* perfromance run */
		results[0].seed1=0x3415;
		results[0].seed2=0x3415;
		results[0].seed3=0x66;
	}
#if (MEM_METHOD==MEM_STATIC)
	results[0].memblock[0]=(void *)static_memblk;
	results[0].size=TOTAL_DATA_SIZE;
	results[0].err=0;
#if (MULTITHREAD>1)
#error "Cannot use a static data area with multiple contexts!"
#endif
#elif (MEM_METHOD==MEM_MALLOC)
	for (i=0 ; i<MULTITHREAD; i++) {
		ee_s32 malloc_override=get_seed(7);
		if (malloc_override != 0)
		results[i].size=malloc_override;
		else
		results[i].size=TOTAL_DATA_SIZE;
		results[i].memblock[0]=portable_malloc(results[i].size);
		results[i].seed1=results[0].seed1;
		results[i].seed2=results[0].seed2;
		results[i].seed3=results[0].seed3;
		results[i].err=0;
		results[i].execs=results[0].execs;
	}
#elif (MEM_METHOD==MEM_STACK)
	for (i=0 ; i<MULTITHREAD; i++) {
		results[i].memblock[0]=stack_memblock+i*TOTAL_DATA_SIZE;
		results[i].size=TOTAL_DATA_SIZE;
		results[i].seed1=results[0].seed1;
		results[i].seed2=results[0].seed2;
		results[i].seed3=results[0].seed3;
		results[i].err=0;
		results[i].execs=results[0].execs;
	}
#else
#error "Please define a way to initialize a memory block."
#endif
	/* Data init */
	/* Find out how space much we have based on number of algorithms */
	for (i=0; i<NUM_ALGORITHMS; i++) {
		if ((1<<(ee_u32)i) & results[0].execs)
		num_algorithms++;
	}
	for (i=0 ; i<MULTITHREAD; i++)
	results[i].size=results[i].size/num_algorithms;
	/* Assign pointers */
	for (i=0; i<NUM_ALGORITHMS; i++) {
		ee_u32 ctx;
		if ((1<<(ee_u32)i) & results[0].execs) {
			for (ctx=0 ; ctx<MULTITHREAD; ctx++)
			//results[ctx].memblock[i+1]=(char *)(results[ctx].memblock[0])+results[0].size*j; //--LT--//
			results[ctx].memblock[i+1]=(ee_u8 *)(results[ctx].memblock[0])+results[0].size*j;
			j++;
		}
	}
	/* call inits */
	/*for (i=0 ; i<MULTITHREAD; i++) {
		if (results[i].execs & ID_LIST) {
			results[i].list=core_list_init(results[0].size,results[i].memblock[1],results[i].seed1);
		}
		if (results[i].execs & ID_MATRIX) {
			core_init_matrix(results[0].size, results[i].memblock[2], (ee_s32)results[i].seed1 | (((ee_s32)results[i].seed2) << 16), &(results[i].mat) );
		}
		if (results[i].execs & ID_STATE) {
			core_init_state(results[0].size,results[i].seed1,results[i].memblock[3]);
		}
	}*/
	/* automatically determine number of iterations if not set */
	if (results[0].iterations==0) {
		secs_ret secs_passed=0;
		ee_u32 divisor;
		results[0].iterations=1;
		while (secs_passed < (secs_ret)1) {
			results[0].iterations*=10;
			//--LT--// start_time();
			iterate(&results[0]);
			//--LT--// stop_time();
			//--LT--// secs_passed=time_in_secs(get_time());
		}
		/* now we know it executes for at least 1 sec, set actual run time at about 10 secs */
		divisor=(ee_u32)secs_passed;
		if (divisor==0) /* some machines cast float to int as 0 since this conversion is not defined by ANSI, but we know at least one second passed */
		divisor=1;
		results[0].iterations*=1+10/divisor;
	}

	/* perform actual benchmark */
	//--LT--// start_time();
	int cm_start;
	//cm_start = millis();

#if (MULTITHREAD>1)
	if (default_num_contexts>MULTITHREAD) {
		default_num_contexts=MULTITHREAD;
	}
	for (i=0 ; i<default_num_contexts; i++) {
		results[i].iterations=results[0].iterations;
		results[i].execs=results[0].execs;
		core_start_parallel(&results[i]);
	}
	for (i=0 ; i<default_num_contexts; i++) {
		core_stop_parallel(&results[i]);
	}
#else
	iterate(&results[0]);
#endif
	//--LT--// stop_time();
	int cm_stop;
	//cm_stop = millis();
	int cm_total_time;
	//cm_total_time = cm_stop - cm_start;
	//--LT--// total_time=get_time();
	/* get a function of the input to report */
	/*seedcrc=crc16(results[0].seed1,seedcrc);
	seedcrc=crc16(results[0].seed2,seedcrc);
	seedcrc=crc16(results[0].seed3,seedcrc);
	seedcrc=crc16(results[0].size,seedcrc);*/

	switch (seedcrc) { /* test known output for common seeds */
		case 0x8a02: /* seed1=0, seed2=0, seed3=0x66, size 2000 per algorithm */
		known_id=0;
		ee_printf("6k performance run parameters for coremark.\n");
		break;
		case 0x7b05: /*  seed1=0x3415, seed2=0x3415, seed3=0x66, size 2000 per algorithm */
		known_id=1;
		ee_printf("6k validation run parameters for coremark.\n");
		break;
		case 0x4eaf: /* seed1=0x8, seed2=0x8, seed3=0x8, size 400 per algorithm */
		known_id=2;
		ee_printf("Profile generation run parameters for coremark.\n");
		break;
		case 0xe9f5: /* seed1=0, seed2=0, seed3=0x66, size 666 per algorithm */
		known_id=3;
		ee_printf("2K performance run parameters for coremark.\n");
		break;
		case 0x18f2: /*  seed1=0x3415, seed2=0x3415, seed3=0x66, size 666 per algorithm */
		known_id=4;
		ee_printf("2K validation run parameters for coremark.\n");
		break;
		default:
		total_errors=-1;
		break;
	}
	if (known_id>=0) {
		for (i=0 ; i<default_num_contexts; i++) {
			results[i].err=0;
			if ((results[i].execs & ID_LIST) &&
			(results[i].crclist!=list_known_crc[known_id])) {
				ee_printf("[%u]ERROR! list crc 0x%04x - should be 0x%04x\n",i,results[i].crclist,list_known_crc[known_id]);
				results[i].err++;
			}
			if ((results[i].execs & ID_MATRIX) &&
			(results[i].crcmatrix!=matrix_known_crc[known_id])) {
				ee_printf("[%u]ERROR! matrix crc 0x%04x - should be 0x%04x\n",i,results[i].crcmatrix,matrix_known_crc[known_id]);
				results[i].err++;
			}
			if ((results[i].execs & ID_STATE) &&
			(results[i].crcstate!=state_known_crc[known_id])) {
				ee_printf("[%u]ERROR! state crc 0x%04x - should be 0x%04x\n",i,results[i].crcstate,state_known_crc[known_id]);
				results[i].err++;
			}
			total_errors+=results[i].err;
		}
	}
	//total_errors+=check_data_types();
	/* and report results */
/*********************************** LT *********************************************
	ee_printf("CoreMark Size    : %lu\n",(ee_u32)results[0].size);
	ee_printf("Total ticks      : %lu\n",(ee_u32)total_time);
	#if HAS_FLOAT
	ee_printf("Total time (secs): %f\n",time_in_secs(total_time));
	if (time_in_secs(total_time) > 0)
	ee_printf("Iterations/Sec   : %f\n",default_num_contexts*results[0].iterations/time_in_secs(total_time));
	#else
	ee_printf("Total time (secs): %d\n",time_in_secs(total_time));
	if (time_in_secs(total_time) > 0)
	ee_printf("Iterations/Sec   : %d\n",default_num_contexts*results[0].iterations/time_in_secs(total_time));
	#endif
	if (time_in_secs(total_time) < 10) {
		ee_printf("ERROR! Must execute for at least 10 secs for a valid result!\n");
		total_errors++;
	}
	ee_printf("Iterations       : %lu\n",(ee_u32)default_num_contexts*results[0].iterations);
	ee_printf("Compiler version : %s\n",COMPILER_VERSION);
	ee_printf("Compiler flags   : %s\n",COMPILER_FLAGS);
#if (MULTITHREAD>1)
	ee_printf("Parallel %s : %d\n",PARALLEL_METHOD,default_num_contexts);
#endif
	ee_printf("Memory location  : %s\n",MEM_LOCATION);
	// output for verification
	ee_printf("seedcrc          : 0x%04x\n",seedcrc);
	if (results[0].execs & ID_LIST)
	for (i=0 ; i<default_num_contexts; i++)
	ee_printf("[%d]crclist       : 0x%04x\n",i,results[i].crclist);
	if (results[0].execs & ID_MATRIX)
	for (i=0 ; i<default_num_contexts; i++)
	ee_printf("[%d]crcmatrix     : 0x%04x\n",i,results[i].crcmatrix);
	if (results[0].execs & ID_STATE)
	for (i=0 ; i<default_num_contexts; i++)
	ee_printf("[%d]crcstate      : 0x%04x\n",i,results[i].crcstate);
	for (i=0 ; i<default_num_contexts; i++)
	ee_printf("[%d]crcfinal      : 0x%04x\n",i,results[i].crc);
*********************************************************************************/
	if (total_errors==0) {
		ee_printf("Correct operation validated. See readme.txt for run and reporting rules.\n");
#if HAS_FLOAT
		if (known_id==3) {
			//--LT--//ee_printf("CoreMark 1.0 : %f / %s %s",default_num_contexts*results[0].iterations/time_in_secs(total_time),COMPILER_VERSION,COMPILER_FLAGS);
#if defined(MEM_LOCATION) && !defined(MEM_LOCATION_UNSPEC)
			ee_printf(" / %s",MEM_LOCATION);
#else
			ee_printf(" / %s",mem_name[MEM_METHOD]);
#endif

#if (MULTITHREAD>1)
			ee_printf(" / %d:%s",default_num_contexts,PARALLEL_METHOD);
#endif
			ee_printf("\n");
		}
#endif
	}
	if (total_errors>0)
	ee_printf("Errors detected\n");
	if (total_errors<0)
	ee_printf("Cannot validate operation for these seed values, please compare with results on a known platform.\n");

	#if (MEM_METHOD==MEM_MALLOC)
	for (i=0 ; i<MULTITHREAD; i++)
	portable_free(results[i].memblock[0]);
	#endif
	/* And last call any target specific code for finalizing */
	//--LT--//portable_fini(&(results[0].port));

	return cm_total_time;	
}



