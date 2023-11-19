
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "mm.h"


/*
 * extend_heap_size:
 */

// void *extend_heap_size(size_t size)
// {
//   void *current_base = sbrk(0);
//   void *extended = sbrk(size);
//   assert(extended != (void *)-1); // Note: not thread-safe

//   /*
//    * Modify this function according to your needs
//    */

//   return extended;
// }

void *malloc(size_t size)
{
  /*
   * Insert malloc implementation here
   */
  static int heap_inited = 0;
  if (!heap_inited)
  {
    heap_inited = 1;
    init_heap();
  }

  // init_heap stuff from before goes here
  size_t *header;
  int blk_size = ALIGN(size + SIZE_T_SIZE);
  blk_size = (blk_size < MIN_BLK_SIZE) ? MIN_BLK_SIZE : blk_size;
  header = find_fit(blk_size);
  if (header)
  {
    *header = ((free_blk_header_t *)header)->size | 1L;
    // *header = *header | 1; <-- also works (why?)
    // FIXME: split if possible
    // fprintf(stderr, "find fit\n");
  }
  else
  {
    header = (size_t *)sbrk(blk_size);
    *header = blk_size | 1L;
    // fprintf(stderr, "sbrk\n");
  }
  // fprintf(stderr, "the malloc size is %ld -> %ld -> %ld\n", size, blk_size, *(size_t *)header & ~1L);
  return (char *)header + SIZE_T_SIZE;
}

void *calloc(size_t nitems, size_t nsize)
{

  /*
   * Insert calloc implementation here
   */

  size_t size = nitems * nsize; // TODO: check for overflow.
  void *ptr = malloc(size);
  // memset(ptr, 0, size);
  return ptr;
}

void free(void *ptr)
{

  /*
   * Insert free implementation here
   */
  free_blk_header_t *header = (char *)ptr - SIZE_T_SIZE,
                    *free_list_head = base;
  // add freed block to free list after head
  header->size = *(size_t *)header & ~1L;
  header->next = free_list_head->next;
  header->prior = free_list_head;
  free_list_head->next = free_list_head->next->prior = header;
}

void *realloc(void *ptr, size_t size)
{

  /*
   * Insert realloc implementation here
   */

  if (!ptr)
  {
    // NULL ptr. realloc should act like malloc.
    return malloc(size);
  }

  // We have enough space. Could free some once we implement split.
  char *header = (char *)ptr - SIZE_T_SIZE;
  size_t blk_size = *(size_t *)header & ~1L;
  // fprintf(stderr, "the size is %ld\n", blk_size);
  if (blk_size >= size)
  {
    return ptr;
  }

  // Need to really realloc. Malloc new space and free old space.
  // Then copy old data to new space.
  void *new_ptr;
  new_ptr = malloc(size);
  if (!new_ptr)
  {
    return NULL; // TODO: set errno on failure.
  }
  memcpy(new_ptr, ptr, blk_size);
  free(ptr);
  return new_ptr;
}