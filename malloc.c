#include "malloc.h"

void *malloc(size_t size)
{
	metablock_t *target_chunk;
	size = size < MIN_ALLOC_SIZE ? MIN_ALLOC_SIZE : size;

	if ((target_chunk = find_best_fit(size)) == NULL)
	{
		void *ret;
		// pthread_mutex_lock(&list_lock);
		ret = my_sbrk(size);
		// pthread_mutex_unlock(&list_lock);
		// fprintf(stderr, "1. malloc size of %d in ptr %ld\n", size, ret);
		// return get_alloc_from_target_chunk(ret, size);
		return ret;
	}
	else
	{
		void *ret;
		// pthread_mutex_lock(&list_lock);
		ret = get_alloc_from_target_chunk(target_chunk, size);
		// pthread_mutex_unlock(&list_lock);
		// fprintf(stderr, "2. malloc size of %d in ptr %ld\n", size, ret);
		return ret;
	}
}

void *calloc(size_t nitems, size_t nsize)
{
	size_t size = nitems * nsize;
	size = size < MIN_ALLOC_SIZE ? MIN_ALLOC_SIZE : size;
	void *ret = malloc(size);
	if (ret != NULL)
		memset(ret, '\0', size);
	return ret;
}

void free(void *ptr)
{
	if (ptr == NULL)
		return;
	metablock_t *target_chunk = mem2chunk(ptr);
	target_chunk->used = 0;
	// pthread_mutex_lock(&list_lock);
	insert(&(target_chunk->free_list), &free_list);
	merge(target_chunk);
	// pthread_mutex_unlock(&list_lock);
	return;
}

void *realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		return malloc(size);

	if (size == 0)
	{
		free(ptr);
		return NULL;
	}
	size = size < MIN_ALLOC_SIZE ? MIN_ALLOC_SIZE : size;
	size_t new_chunk_size = ALIGN_CHUNK_SIZE(size);
	metablock_t *cur_chunk = mem2chunk(ptr);

	// equal: return
	if (cur_chunk->size == new_chunk_size)
		return ptr;
	// larger: split or not
	else if (cur_chunk->size > new_chunk_size)
	{
		if (cur_chunk->size < (new_chunk_size + MIN_CHUNK_SIZE))
			return ptr;
		else
		{
			// pthread_mutex_lock(&list_lock);
			split_chunk(cur_chunk, size);
			void *ret = chunk2mem(cur_chunk);
			// pthread_mutex_unlock(&list_lock);
			return ret;
		}
	}
	// smaller: malloc a new block
	else
	{
		void *ret = malloc(size);
		if (ret == NULL)
			return ret;
		// pthread_mutex_lock(&list_lock);
		memcpy(ret, ptr, cur_chunk->size - sizeof(metablock_t) - PAD_SIZE);
		// pthread_mutex_unlock(&list_lock);
		free(ptr);
		return ret;
	}
}
