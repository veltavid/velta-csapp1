/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

 

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define init_size 0

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define set_4(p,v) (*(unsigned int*)(p)=(v))
#define get_4(p) (*(unsigned int*)(p))
#define set(p,v) (*(unsigned long long*)(p)=(v))
#define get(p) (*(unsigned long long*)(p))
#define pack(size,if_use) ((size) | (if_use))
#define set_size(p,v) (set_4(p+4,v))
#define get_size(p) (get_4(p+4))
#define is_inuse(p) (get_size(p)&1)

/* 
 * mm_init - initialize the malloc package.
 */
static char *mem_start_brk;
static char *mem_brk;
static char *mem_max_addr;

int mm_init(void)
{
	mem_init();
	mem_start_brk=mem_heap_lo();
	mem_sbrk(init_size);
	set_size(mem_start_brk,init_size);
	mem_brk=mem_start_brk;
	mem_max_addr=mem_start_brk+init_size;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(8+size+SIZE_T_SIZE);
    int sign=0;
    void *result;
	for(result=mem_brk;;)
	{
		if(!is_inuse(result) && get_size(result)>=newsize)
		{
			if(get_size(result)-newsize<=8)
			{
				newsize=get_size(result);
		 		set_size(result,pack(newsize,1));
				//set_4(result,0);
				set_4(result+newsize,newsize);
			}
			else
			{
				set_4(result+get_size(result),get_size(result)-newsize);
				set_size(result+newsize,get_size(result)-newsize);
				set_4(result+newsize,newsize);
				//set_4(result,0);
				set_size(result,pack(newsize,1));
			}
			break;
		}
		if(result==mem_brk && sign)//extend heap
		{
			result=mem_sbrk(newsize);
			//set_4(result,0);
			set_size(result,pack(newsize,1));
			set_4(result+newsize,newsize);
			mem_max_addr+=newsize;
			return result+8;
		}
		result=result+(get_size(result)&(~0-1));
		if(result>=mem_max_addr)
		{
			result=mem_start_brk;
			sign=1;
		}
	}
	mem_brk=result+newsize;
	if(mem_brk>=mem_max_addr)
	mem_brk=mem_start_brk;
   	return result+8;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	char *p=ptr-8;
	int pre_inuse,post_inuse,size; 
	size=get_size(p)-1;
	set_size(p,size&(~0-1));
	set_4(p+size,size);
	pre_inuse=is_inuse(p-get_4(p)) || (get_4(p)==0);
	post_inuse=is_inuse(p+get_size(p)) || (p+size==mem_max_addr);
	if(!pre_inuse && !post_inuse)
	{
		size=get_size(p)+get_size(p-get_4(p))+get_size(p+get_size(p));
		p=p-get_4(p);
		set_size(p,size);
		set_4(p+size,size);
	}
	else if(!pre_inuse)
	{
		size=get_size(p)+get_size(p-get_4(p));
		p=p-get_4(p);
		set_size(p,size);
		set_4(p+size,size);
	}
	else if(!post_inuse)
	{
		size=get_size(p)+get_size(p+get_size(p));
		set_size(p,size);
		set_4(p+size,size);
	}
	if(mem_brk>p && mem_brk<(p+size))
	mem_brk=p;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    	void *p = ptr-8;
    	void *newptr;
	int newsize = ALIGN(8+size+SIZE_T_SIZE);
    	size_t copySize;
    	int pre_inuse,post_inuse,p_size;
    	p_size=get_size(p)-1;
	copySize=p_size-8;
	if(newsize>p_size)
    	{
    		pre_inuse=is_inuse(p-get_4(p)) || (get_4(p)==0);
    		post_inuse=is_inuse(p+p_size);
		if(!pre_inuse)
		{
			p_size+=get_size(p-get_4(p));
			p=p-get_4(p);
			if(p_size>=newsize)
			{
				memcpy(p+8, ptr, copySize);
				if(p_size-newsize<=8)
				{
					newsize=p_size;
			 		set_size(p,pack(p_size,1));
					set_4(p+p_size,p_size);
				}
				else
				{
					set_4(p+p_size,p_size-newsize);
					set_size(p+newsize,p_size-newsize);
					set_4(p+newsize,newsize);
					set_size(p,pack(newsize,1));
				}
				if(mem_brk>p && mem_brk<p+newsize)
				mem_brk=p;
				return p+8;
			}
		}
		if(!post_inuse)
		{
			if(p+p_size==mem_max_addr)
			{
				if(!pre_inuse)
				memcpy(p+8, ptr, copySize);
				mem_sbrk(newsize-p_size);
				set_size(p,pack(newsize,1));
				set_4(p+newsize,newsize);
				mem_max_addr+=newsize-p_size;
				return p+8;
			}
			else
			{
				p_size+=get_size(p+p_size);
				if(p_size>=newsize)
				{
					memcpy(p+8, ptr, copySize);
					if(p_size-newsize<=8)
					{
						newsize=p_size;
				 		set_size(p,pack(p_size,1));
						set_4(p+p_size,p_size);
					}
					else
					{
						set_4(p+p_size,p_size-newsize);
						set_size(p+newsize,p_size-newsize);
						set_4(p+newsize,newsize);
						set_size(p,pack(newsize,1));
					}
					if(mem_brk>p && mem_brk<p+newsize)
					mem_brk=p;
					return p+8;
				}
			}
		}
		p = mm_malloc(size);
		memcpy(p, ptr, copySize);
		mm_free(ptr);
		return p;
	}
	else if(newsize<p_size-8)
	{
		set_4(p+p_size,p_size-newsize);
		set_size(p+newsize,p_size-newsize);
		set_4(p+newsize,newsize);
		set_size(p,pack(newsize,1));  		
	}
	return p+8;
}
