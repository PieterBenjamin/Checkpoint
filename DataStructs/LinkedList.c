#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "CP.h"
#include "LinkedList.h"
#include "LinkedList_priv.h"

LinkedList MakeLinkedList(void) {
  // allocate the linked list record
  LinkedList ll = (LinkedList) malloc(sizeof(LinkedListHead));
  if (ll == NULL) {
    // out of memory
    return (LinkedList) NULL;
  }

  ll->head = NULL;
  ll->tail = NULL;
  ll->ht_size = 0;

  // return our newly minted linked list
  return ll;
}

void FreeLinkedList(LinkedList list,
                    LLPayloadFreeFn free_payload) {
  // defensive programming: check arguments for sanity.
  assert(list != NULL);
  assert(free_payload != NULL);

  LinkedListNodePtr old_node;
  while (list->head != NULL) {
    (*free_payload)(list->head->payload);  // free the payload
    old_node = list->head;
    list->head = list->head->next;
    free(old_node);  // free the node
  }

  // free the list record
  free(list);
}

CPSize_t LLSize(LinkedList list) {
  // defensive programming: check argument for safety.
  assert(list != NULL);
  return list->ht_size;
}

bool LLPush(LinkedList list, LinkedListPayload payload) {
  // defensive programming: check argument for safety. The user-supplied
  // argument can be anything, of course, so we need to make sure it's
  // reasonable (e.g., not NULL).
  assert(list != NULL);

  // allocate space for the new node.
  LinkedListNodePtr ln =
    (LinkedListNodePtr) malloc(sizeof(LinkedListNode));
  if (ln == NULL) {
    // out of memory
    return false;
  }

  // set the payload
  ln->payload = payload;

  if (list->ht_size == 0) {
    // degenerate case; list is currently empty
    assert(list->head == NULL);  // debugging aid
    assert(list->tail == NULL);  // debugging aid
    ln->next = ln->prev = NULL;
    list->head = list->tail = ln;
    list->ht_size = 1U;
    return true;
  }

  ln->next = list->head;
  ln->prev = NULL;
  list->head->prev = ln;
  list->head = ln;
  list->ht_size = list->ht_size + 1;

  // return success
  return true;
}

bool LLPop(LinkedList list, LinkedListPayload *payload_ptr) {
  // defensive programming.
  assert(payload_ptr != NULL);
  assert(list != NULL);

  if (list->ht_size == 0) {
    return false;
  }

  *payload_ptr = list->head->payload;
  LinkedListNodePtr old_node = list->head;

  if (list->ht_size == 1) {  // case a
    list->head = NULL;
    list->tail = NULL;
  } else {  // case b (>=2 elements)
    list->head = list->head->next;
    list->head->prev = NULL;
  }

  list->ht_size = list->ht_size - 1;
  free(old_node);

  return true;
}

bool LLAppend(LinkedList list, LinkedListPayload payload) {
  // defensive programming: check argument for safety.
  assert(list != NULL);

  LinkedListNodePtr ln =
    (LinkedListNodePtr) malloc(sizeof(LinkedListNode));
  if (ln == NULL) {
    // out of memory
    return false;
  }

  // set the fields of new node
  ln->payload = payload;
  ln->next = NULL;
  if (list->ht_size < 1) {
    ln->prev = NULL;
    list->head = ln;
  } else {
    ln->prev = list->tail;
    list->tail->next = ln;
  }

  list->tail = ln;
  list->ht_size = list->ht_size + 1;

  return true;
}

bool LLSlice(LinkedList list, LinkedListPayload *payload_ptr) {
  // defensive programming.
  assert(payload_ptr != NULL);
  assert(list != NULL);

  if (list->ht_size < 1) {
    return false;
  }

  *payload_ptr = list->tail->payload;
  LinkedListNodePtr old_node = list->tail;

  if (list->ht_size == 1) {
    list->head = list->tail = NULL;
    list->ht_size = 0;
  } else {
    list->tail = old_node->prev;
    list->tail->next = NULL;
    list->ht_size = list->ht_size - 1;
  }

  free(old_node);

  return true;
}

void LLSort(LinkedList list, unsigned int ascending,
                    LLPayloadCompareFn comparator_function) {
  assert(list != NULL);  // defensive programming
  if (list->ht_size < 2) {
    // no sorting needed
    return;
  }

  // we'll implement bubblesort! nice and easy, and nice and slow :)
  int swapped;
  do {
    LinkedListNodePtr curnode;

    swapped = 0;
    curnode = list->head;
    while (curnode->next != NULL) {
      int compare_result = comparator_function(curnode->payload,
                                               curnode->next->payload);
      if (ascending) {
        compare_result *= -1;
      }
      if (compare_result < 0) {
        // bubble-swap payloads
        LinkedListPayload tmp;
        tmp = curnode->payload;
        curnode->payload = curnode->next->payload;
        curnode->next->payload = tmp;
        swapped = 1;
      }
      curnode = curnode->next;
    }
  } while (swapped);
}

LLIter LLGetIter(LinkedList list, int pos) {
  // defensive programming
  assert(list != NULL);
  assert((pos == 0) || (pos == 1));

  // if the list is empty, return failure.
  if (LLSize(list) == 0U)
    return NULL;

  // OK, let's manufacture an iterator.
  LLIter li = (LLIter) malloc(sizeof(LLIterSt));
  if (li == NULL) {
    // out of memory!
    return NULL;
  }

  // set up the iterator.
  li->list = list;
  if (pos == 0) {
    li->node = list->head;
  } else {
    li->node = list->tail;
  }

  // return the new iterator
  return li;
}

void LLIterFree(LLIter iter) {
  // defensive programming
  assert(iter != NULL);
  free(iter);
}

bool LLiterHasNext(LLIter iter) {
  // defensive programming
  assert(iter != NULL);
  assert(iter->list != NULL);
  assert(iter->node != NULL);

  // Is there another node beyond the iterator?
  if (iter->node->next == NULL)
    return false;  // no

  return true;  // yes
}

bool LLIterAdvance(LLIter iter) {
  // defensive programming
  assert(iter != NULL);
  assert(iter->list != NULL);
  assert(iter->node != NULL);

  if (iter->node->next != NULL) {
    iter->node = iter->node->next;
    return true;
  }

  // Nope, there isn't another node, so return failure.
  return false;
}

bool LLIterHasPrev(LLIter iter) {
  // defensive programming
  assert(iter != NULL);
  assert(iter->list != NULL);
  assert(iter->node != NULL);

  // Is there another node beyond the iterator?
  if (iter->node->prev == NULL)
    return false;  // no

  return true;  // yes
}

bool LLIterBack(LLIter iter) {
  // defensive programming
  assert(iter != NULL);
  assert(iter->list != NULL);
  assert(iter->node != NULL);

  if (iter->node->prev != NULL) {
    iter->node = iter->node->prev;
    return true;
  }

  // nope, so return failure.
  return false;
}

void LLIterPayload(LLIter iter, LinkedListPayload *payload) {
  // defensive programming
  assert(iter != NULL);
  assert(iter->list != NULL);
  assert(iter->node != NULL);

  // set the return parameter.
  *payload = iter->node->payload;
}

bool LLiterDel(LLIter iter,
                      LLPayloadFreeFn free_payload) {
  // defensive programming
  assert(iter != NULL);
  assert(iter->list != NULL);
  assert(iter->node != NULL);

  LinkedListNodePtr old_node = iter->node;  // Used to free old node.
  LinkedList list = iter->list;
  // Regardless of where the node is, we want to use the freeing function
  // on its payload.
  (*free_payload)(iter->node->payload);

  if (list->ht_size == 1) {  // Only one element.
    list->head = list->tail = NULL;
    iter->node = NULL;
  } else if (old_node == list->head) {  // Iter is at the head.
    list->head = list->head->next;
    list->head->prev = NULL;
    iter->node = old_node->next;
  } else if (old_node == list->tail) {  // Iter is at the tail
    list->tail = list->tail->prev;
    list->tail->next = NULL;
    iter->node = old_node->prev;
  } else {  // generic case
    old_node->prev->next = old_node->next;
    old_node->next->prev = old_node->prev;
    iter->node = old_node->next;
  }

  list->ht_size = list->ht_size - 1;

  free(old_node);

  return list->ht_size == 0 ? false : true;
}

bool LLIteratorInsertBefore(LLIter iter, LinkedListPayload payload) {
  // defensive programming
  assert(iter != NULL);
  assert(iter->list != NULL);
  assert(iter->node != NULL);

  // If the cursor is pointing at the head, use our
  // LLPush function.
  if (iter->node == iter->list->head) {
    return LLPush(iter->list, payload);
  }

  // General case: we have to do some splicing.
  LinkedListNodePtr newnode =
    (LinkedListNodePtr) malloc(sizeof(LinkedListNode));
  if (newnode == NULL)
    return false;  // out of memory

  newnode->payload = payload;
  newnode->next = iter->node;
  newnode->prev = iter->node->prev;
  newnode->prev->next = newnode;
  newnode->next->prev = newnode;
  iter->list->ht_size += 1;
  return true;
}
