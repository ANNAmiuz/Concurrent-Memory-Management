// #ifndef LIST_H
// #define LIST_H
#pragma once
#include <stddef.h>

// doubly linked list node
typedef struct node
{
	struct node *next, *prev;
} node_t;

// insert to list head
void insert(node_t *new, node_t *head)
{
	head->next->prev = new;
	new->next = head->next;
	new->prev = head;
	head->next = new;
}

// remove a node from list
void delete (node_t *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
}

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &node_t pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) ({					\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
	(type *)( (char *)__mptr - offsetof(type,member) ); })

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)                 \
	for (pos = list_entry((head)->next, typeof(*pos), member); \
		 &pos->member != (head);                               \
		 pos = list_entry(pos->member.next, typeof(*pos), member))

// #endif
