#ifndef _LINKEDLIST_H_
#define _LINKEDLIST_H_

#include <stdbool.h>  // for bool type (true, false)

#include "CP.h"   // for CPSize_t

typedef void* LinkedListPayload;

// A LinkedList is a doubly-linked list.
struct ll_head;
typedef struct ll_head *LinkedList;

// When a customer frees a linked list, they need to pass a pointer to a
// function that does any necessary free-ing of the payload of a linked
// list node. We invoke the pointed-to function once for each node in
// the linked list, before the LinkedListPtr itself is freed.
typedef void(*LLPayloadFreeFn)(LinkedListPayload payload);

// When sorting a linked list or comparing two elements of a linked list,
// customers must pass in a comparator function.  The function accepts two
// payload pointers as arguments and returns an integer that is:
//
//    -1  if payload_a < payload_b
//     0  if payload_a == payload_b
//    +1  if payload_a > payload_b
typedef int(*LLPayloadCompareFn)(LinkedListPayload payload_a, LinkedListPayload payload_b);

// Allocate and return a new linked list.  The caller takes responsibility for
// eventually calling FreeLinkedList to free memory associated with the list.
//
// Arguments: none.
//
// Returns: NULL on error, non-NULL on success.
LinkedList MakeLinkedList(void);

// Free a linked list after use (not safe after invoking this)
//
// Arguments:
//
// - list: the linked list to free.  It is unsafe to use "list" after this
//   function returns.
//
// - free_payload: a pointer to a payload freeing function; see above
//   for details on what this is.
void FreeLinkedList(LinkedList list,
                    LLPayloadFreeFn free_payload);

// Return the number of elements in list.
//
// Arguments:
//
// - list:  the list to query
//
// Returns:
//
// - list length
CPSize_t LLSize(LinkedList list);

// Adds a new element to the head of the linked list.
//
// Arguments:
//
// - list: the LinkedList to push onto
//
// - payload: the payload to push; it's up to the caller to interpret and
//   manage the memory of the payload.
//
// Returns false on failure, true on success.
bool LLPush(LinkedList list, LinkedListPayload payload);

// Pop an element from the head of the linked list.
//
// Arguments:
//
// - list: the LinkedList to pop from
//
// - payload_ptr: a return parameter that is set by the callee; on success,
//   the payload is returned through this parameter.
//
// Returns false on failure, true on success.
bool LLPop(LinkedList list, LinkedListPayload *payload_ptr);

// Adds a new element to the tail of the linked list.
//
// Arguments:
//
// - list: the LinkedList to push onto
//
// - payload: the payload to push; it's up to the caller to interpret and
//   manage the memory of the payload.
//
// Returns false on failure, true on success.
bool LLAppend(LinkedList list, LinkedListPayload payload);

// Pop an element from the tail of the linked list.
//
// Arguments:
//
// - list: the LinkedList to pop from
//
// - payload_ptr: a return parameter that is set by the callee; on success,
//   the payload is returned through this parameter.
//
// Returns false on failure, true on success.
bool LLSlice(LinkedList list, LinkedListPayload *payload_ptr);

// Sorts a LinkedList in place.
//
// Arguments:
//
// - list: the list to sort
//
// - ascending: if 0, sorts descending, else sorts ascending.
//
// - comparator_function:  this argument is a pointer to a payload comparator
//   function; see above.
void LLSort(LinkedList list, unsigned int ascending,
                    LLPayloadCompareFn comparator_function);

struct ll_iter;
typedef struct ll_iter *LLIter;  // same trick to hide implementation.

// Manufacture an iterator for the list.  Customers can use the iterator to
// advance forwards or backwards through the list, kind of like a cursor.
// Caller is responsible for eventually calling LLIterFree to free memory
// associated with the iterator.
//
// Arguments:
//
// - list: the list from which we'll return an iterator
//
// - pos: where to start (0 = head, 1 = tail)
//
// Returns NULL on failure (out of memory, empty list) or non-NULL on success.
LLIter LLGetIter(LinkedList list, int pos);

// When you're done with an iterator, you must free it by calling this
// function.
//
// Arguments:
//
// - iter: the iterator to free. Don't use it after freeing it.
void LLIterFree(LLIter iter);

// Test whether the iterator can advance forward.
//
// Arguments:
//
// - iter: the iterator
//
// Returns:
//
// - true if the iterator can advance (is not at tail)
//
// - false if the iterator cannot advance (is at the tail)
bool LLiterHasNext(LLIter iter);

// Advance the iterator, i.e. move to the next node in the
// list.
//
// Arguments:
//
// - iter: the iterator
//
// Returns:
//
// true: if the iterator has been advanced to the next node
//
// false: if the iterator is at the last node and cannot
// be advanced
bool LLIterAdvance(LLIter iter);

// Test whether the iterator can advance backwards.
//
// Arguments:
//
// - iter: the iterator
//
// Returns:
//
// - true if the iterator can advance (is not at head)
//
// - false if the iterator cannot advance (is at the head)
bool LLIterHasPrev(LLIter iter);

// Rewind the iterator, i.e. move to the previous node in the
// list.
//
// Arguments:
//
// - iter: the iterator
//
// Returns:
//
// true: if the iterator has been shifted to the previous node
//
// false: if the iterator is at the first node and cannot be
// shifted back any more
bool LLIterBack(LLIter iter);

// Returns the payload of the list node that the iterator currently
// points at.
//
// Arguments:
//
// - iter: the iterator to fetch the payload from.
//
// - payload: a "return parameter" through which the payload is returned.
void LLIterPayload(LLIter iter, LinkedListPayload *payload);

// Delete the node the iterator is pointing to.  After deletion, the iterator:
//
// - is invalid and cannot be used (but must be freed), if there was only one
//   element in the list
//
// - the successor of the deleted node, if there is one.
//
// - the predecessor of the deleted node, if the iterator was pointing at
//   the tail.
//
// Arguments:
//
// - iter:  the iterator to delete from
//
// - free_payload: invoked to free the payload
//
// Returns:
//
// - false if the deletion succeeded, but the list is now empty
//
// - true if the deletion succeeded, and the list is still non-empty
bool LLiterDel(LLIter iter,
                      LLPayloadFreeFn free_payload);

// Drops an element into the list right before the node
// which the iterator is currently pointing at.
//
// Arguments:
//
// - iter: the iterator to insert through
//
// - payload: the payload to insert
//
// Returns:
//
// - false on failure (out of memory)
//
// - true on success; the iterator still points to the same node,
//   not to the inserted node.
bool LLIteratorInsertBefore(LLIter iter, LinkedListPayload payload);

#endif  // _LINKEDLIST_H_
