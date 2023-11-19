#pragma once

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>

#include "list.h"

// External: memory allocation functions
void *malloc(size_t size);
void *calloc(size_t nitems, size_t nsize);
void free(void *ptr);
void *realloc(void *ptr, size_t size);

// Some useful macros
#define ALIGNMENT 8		   // Linux alignment is 8
#define MIN_ALLOC_SIZE 8   // Fit the alignment
#define MIN_BRK_INCREASE 8 // min alloc size is 2

// do not put used bit to size for now
// #define SET_USED(ptr)
// #define SET_UNUSED(ptr)
// #define IS_USED(ptr)
// #define IS_UNUSED(ptr)
// #define GET_SIZE_FROM_USED(ptr)

// Metadata block
typedef struct metablock
{
	char used;		  // 1 for allocated
	int size;		  // Block size of this chunk (for alloc...) max is 4096 so DIDNT USE SIZE_T
	int prev_size;	  // Block size of prev chunk (merge)
	node_t free_list; // Owner
} metablock_t;

// Align(target size) to 8
#define PAD_SIZE (ALIGNMENT - ((sizeof(metablock_t) - 1) % ALIGNMENT + 1))
#define ALIGN_CHUNK_SIZE(size) (size + (ALIGNMENT - ((size - 1) % ALIGNMENT + 1)) + sizeof(metablock_t) + PAD_SIZE)
#define MIN_CHUNK_SIZE ALIGN_CHUNK_SIZE(MIN_ALLOC_SIZE)

// mem <-> metablk
#define chunk2mem(chunk) (void *)(((char *)chunk) + sizeof(metablock_t) + PAD_SIZE)
#define mem2chunk(mem) (metablock_t *)(((char *)mem) - sizeof(metablock_t) - PAD_SIZE)

// global vars hold heap range, free list, and lock of the free doubly linked list
static metablock_t *list_head = NULL;
static metablock_t *list_tail = NULL;
static node_t free_list = {&(free_list), &(free_list)};
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sbrk_lock = PTHREAD_MUTEX_INITIALIZER;
// static pthread_mutex_t slave_lock = PTHREAD_MUTEX_INITIALIZER;

// Internal: helper function
static void split_chunk(metablock_t *target_chunk, size_t size);
static void *get_alloc_from_target_chunk(metablock_t *target_chunk, size_t size);
static void *my_sbrk(size_t size);
static metablock_t *find_best_fit(size_t size);
static void merge(metablock_t *target_chunk);

// split the target block to (chunk of size) && (new chunk remained)
// return the remain block to global free list, invoked by get_alloc_from_target_chunk()
static void split_chunk(metablock_t *target_chunk, size_t size)
{
	size_t new_chunk_size;
	metablock_t *succ_new_chunk;
	metablock_t *new_chunk;

	// Set chunk size and get the block ptr
	new_chunk_size = target_chunk->size - ALIGN_CHUNK_SIZE(size);
	target_chunk->size = ALIGN_CHUNK_SIZE(size);
	new_chunk = (metablock_t *)(((char *)target_chunk) + target_chunk->size);

	// Return the new blk to global free list
	if (target_chunk == list_tail)
		list_tail = new_chunk;

	new_chunk->prev_size = target_chunk->size;
	new_chunk->size = new_chunk_size;
	new_chunk->used = 0;
	insert(&(new_chunk->free_list), &free_list);

	if (new_chunk != list_tail)
	{
		succ_new_chunk = (metablock_t *)((char *)new_chunk + new_chunk->size);
		succ_new_chunk->prev_size = new_chunk->size;
	}
	return;
}

// split the target block, get the requiested block, return the new block to free list (locked required)
static void *get_alloc_from_target_chunk(metablock_t *target_chunk, size_t size)
{
	// no target block
	if (target_chunk == NULL)
		return NULL;

	size_t new_chunk_size;

	// smaller target block than requested
	if (target_chunk->size < (ALIGN_CHUNK_SIZE(size)))
		return NULL;

	new_chunk_size = target_chunk->size - ALIGN_CHUNK_SIZE(size);
	// New block has 8 bytes, split and return it to free list
	if (new_chunk_size >= MIN_CHUNK_SIZE)
		split_chunk(target_chunk, size);
	target_chunk->used = 1;
	return chunk2mem(target_chunk);
}

// sbrk() wrapper to get more mem space when there is no qualified block in the free list
static void *my_sbrk(size_t size)
{
	size_t chunk_size;
	metablock_t *new_chunk;
	size_t incr_size;

	// requested chunk size && sbrk size
	chunk_size = ALIGN_CHUNK_SIZE(size);
	incr_size = chunk_size <= MIN_BRK_INCREASE ? MIN_BRK_INCREASE : chunk_size;

	// call sbrk()
	if ((new_chunk = sbrk(incr_size)) == (void *)-1)
	{
		if (errno == ENOMEM)
			fprintf(stderr, "3. sbrk size %d with chunk ptr %ld data segment size limit reached \n", incr_size, new_chunk);
		else if (errno == EAGAIN)
			fprintf(stderr, "3. sbrk size %d with chunk ptr %ld private pages is temporarily insufficient \n", incr_size, new_chunk);
		else
			fprintf(stderr, "3. sbrk size %d with chunk ptr %ld for unknown reason \n", incr_size, new_chunk);
		// return NULL;
		new_chunk = mmap(NULL, sizeof(char) * incr_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
		if (new_chunk == NULL)
		{
			fprintf(stderr, "4. mmap fail with size %d with chunk ptr %x for unknown reason \n", incr_size, new_chunk);
			return NULL;
		}
		else
			fprintf(stderr, "4. mmap size %d with chunk ptr %x \n", incr_size, new_chunk);
	}
	new_chunk->used = 0;
	new_chunk->size = incr_size;
	new_chunk->prev_size = 0;

	// insert to tail
	if (list_head != NULL)
	{
		new_chunk->prev_size = list_tail->size;
		list_tail = new_chunk;
	}
	else
	{
		list_head = new_chunk;
		new_chunk->prev_size = 0;
		list_tail = new_chunk;
	}
	return (void *)get_alloc_from_target_chunk(new_chunk, size);
	// return new_chunk;
}

// change to first fit algo since best fit doesn't work ...
static metablock_t *find_best_fit(size_t size)
{
	size_t chunk_size = ALIGN_CHUNK_SIZE(size);
	size_t best_fit_size = 0;
	metablock_t *bese_fit_chunk = NULL;
	metablock_t *cur_chunk = NULL;

	// Find largest chunk that can service request, if it exists
	list_for_each_entry(cur_chunk, &free_list, free_list)
	{
		// if (cur_chunk->size >= chunk_size && cur_chunk->size ?? best_fit_size)
		if (cur_chunk->size >= chunk_size)
		{
			best_fit_size = cur_chunk->size;
			bese_fit_chunk = cur_chunk;
		}
	}

	// If we found a suitable chunk, remove from free_list and return it
	if (bese_fit_chunk != NULL)
	{
		delete (&(bese_fit_chunk->free_list));
		bese_fit_chunk->used = 1;
	}
	return bese_fit_chunk;
}

// merge the just freed block for coalesec
static void merge(metablock_t *target_chunk)
{
	if (target_chunk == NULL)
		return;

	pthread_mutex_lock(&list_lock);
	if ((target_chunk == list_head) && (target_chunk == list_tail))
	{
		pthread_mutex_unlock(&list_lock);
		return;
	}

	metablock_t *prev;
	metablock_t *succ;
	metablock_t *next_succ;

	// check the succ and merge them
	if (target_chunk != list_tail)
	{
		succ = (metablock_t *)((char *)target_chunk + target_chunk->size);
		if (!succ->used)
		{
			delete (&(succ->free_list));
			target_chunk->size += succ->size;
			if (succ == list_tail)
				list_tail = target_chunk;
			else
			{
				next_succ = (metablock_t *)((char *)target_chunk + target_chunk->size);
				next_succ->prev_size = target_chunk->size;
			}
		}
	}
	// check the prev and merge them
	if (target_chunk != list_head)
	{
		prev = (metablock_t *)(((char *)target_chunk) - target_chunk->prev_size);
		if (!prev->used)
		{
			delete (&(target_chunk->free_list));
			prev->size += target_chunk->size;
			if (target_chunk == list_tail)
				list_tail = prev;
			else
			{
				next_succ = (metablock_t *)(((char *)prev) + prev->size);
				next_succ->prev_size = prev->size;
			}
		}
	}
	pthread_mutex_unlock(&list_lock);
	return;
}
