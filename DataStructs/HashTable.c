/*
 * Copyright ©2019 Aaron Johnston.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Summer Quarter 2019 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "CSE333.h"
#include "HashTable.h"
#include "HashTable_priv.h"

// A private utility function to grow the hashtable (increase
// the number of buckets) if its load factor has become too high.
static void ResizeHashtable(HashTable ht);

// a free function that does nothing
static void LLNullFree(LLPayload_t freeme) { }
static void HTNullFree(HTValue_t freeme) { }

// Helper method which searches through a LL fora specific key.
//
// Arguments:
//
// - key: The key to search for (using == equality).
//
// - iter_ptr: A pointer to a (assumed not null) Linked List Iterator.
//
// Returns:
//
// - A ptr to the HTKeyValue which possesses the desired key (if it exists in
//   the LinkedList *iter_ptr is iterating through).
//
// - NULL otherwise.
static HTKeyValuePtr search(HTKey_t key,
                            LLIter *iter_ptr);


// Inserts a HTKeyValue into the specified Linked List.
//
// Arguments:
//
// - newkeyvalue: The HTKeyValue to insert.
//
// - insertchain_ptr: A pointer to the LL to insert the HTKeyValue into.
//
// Returns:
//
// - 0: If any memory error occurs.
//
// - 1: If insertion was successful.
static int InsertHTKVNodeIntoLL(HTKeyValue newkeyvalue,
                                LinkedList *insertchain_ptr);

HashTable AllocateHashTable(HWSize_t num_buckets) {
  HashTable ht;
  HWSize_t  i;

  // defensive programming
  if (num_buckets == 0) {
    return NULL;
  }

  // allocate the hash table record
  ht = (HashTable) malloc(sizeof(HashTableRecord));
  if (ht == NULL) {
    return NULL;
  }

  // initialize the record
  ht->num_buckets = num_buckets;
  ht->num_elements = 0;
  ht->buckets =
    (LinkedList *) malloc(num_buckets * sizeof(LinkedList));
  if (ht->buckets == NULL) {
    // make sure we don't leak!
    free(ht);
    return NULL;
  }
  for (i = 0; i < num_buckets; i++) {
    ht->buckets[i] = AllocateLinkedList();
    if (ht->buckets[i] == NULL) {
      // allocating one of our bucket chain lists failed,
      // so we need to free everything we allocated so far
      // before returning NULL to indicate failure.  Since
      // we know the chains are empty, we'll pass in a
      // free function pointer that does nothing; it should
      // never be called.
      HWSize_t j;
      for (j = 0; j < i; j++) {
        FreeLinkedList(ht->buckets[j], LLNullFree);
      }
      free(ht->buckets);
      free(ht);
      return NULL;
    }
  }

  return (HashTable) ht;
}

void FreeHashTable(HashTable table,
                   ValueFreeFnPtr value_free_function) {
  HWSize_t i;

  assert(table != NULL);  // be defensive

  // loop through and free the chains on each bucket
  for (i = 0; i < table->num_buckets; i++) {
    LinkedList  bl = table->buckets[i];
    HTKeyValue *nextKV;

    // pop elements off the the chain list, then free the list
    while (NumElementsInLinkedList(bl) > 0) {
      assert(PopLinkedList(bl, (LLPayload_t*)&nextKV));
      value_free_function(nextKV->value);
      free(nextKV);
    }
    // the chain list is empty, so we can pass in the
    // null free function to FreeLinkedList.
    FreeLinkedList(bl, LLNullFree);
  }

  // free the bucket array within the table record,
  // then free the table record itself.
  free(table->buckets);
  free(table);
}

HWSize_t NumElementsInHashTable(HashTable table) {
  assert(table != NULL);
  return table->num_elements;
}

HTKey_t FNVHash64(unsigned char *buffer, HWSize_t len) {
  // This code is adapted from code by Landon Curt Noll
  // and Bonelli Nicola:
  //
  // http://code.google.com/p/nicola-bonelli-repo/
  static const uint64_t FNV1_64_INIT = 0xcbf29ce484222325ULL;
  static const uint64_t FNV_64_PRIME = 0x100000001b3ULL;
  unsigned char *bp = (unsigned char *) buffer;
  unsigned char *be = bp + len;
  uint64_t hval = FNV1_64_INIT;

  /*
   * FNV-1a hash each octet of the buffer
   */
  while (bp < be) {
    /* xor the bottom with the current octet */
    hval ^= (uint64_t) * bp++;
    /* multiply by the 64 bit FNV magic prime mod 2^64 */
    hval *= FNV_64_PRIME;
  }
  /* return our new hash value */
  return hval;
}

HTKey_t FNVHashInt64(HTValue_t hashval) {
  unsigned char buf[8];
  int i;
  uint64_t hashme = (uint64_t)hashval;

  for (i = 0; i < 8; i++) {
    buf[i] = (unsigned char) (hashme & 0x00000000000000FFULL);
    hashme >>= 8;
  }
  return FNVHash64(buf, 8);
}

HWSize_t HashKeyToBucketNum(HashTable ht, HTKey_t key) {
  return key % ht->num_buckets;
}

static int InsertHTKVNodeIntoLL(HTKeyValue newkeyvalue,
                                LinkedList *insertchain_ptr) {
  LinkedList insertchain = *insertchain_ptr;
  HTKeyValuePtr newkeyvalue_heap;

  newkeyvalue_heap = (HTKeyValuePtr)(malloc(sizeof(HTKeyValue)));
  if (newkeyvalue_heap == NULL) {
    return 0;
  }

  newkeyvalue_heap->key  = newkeyvalue.key;
  newkeyvalue_heap->value = newkeyvalue.value;

  return PushLinkedList(insertchain, (LLPayload_t *)newkeyvalue_heap) ? 1 : 0;
}

static HTKeyValuePtr search(HTKey_t key,
                            LLIter *iter_ptr) {
  HTKeyValuePtr p;
  LLIter iter = *iter_ptr;

  while (LLIteratorHasNext(iter)) {
    // *p = iter->node->payload;
    LLIteratorGetPayload(iter, (LLPayload_t *)&p);

    if (p->key == key) {
      return p;
    }
    LLIteratorNext(iter);
  }

  // Edge case: last element.
  LLIteratorGetPayload(iter, (LLPayload_t *)&p);
  if (p->key == key) {
    return p;
  }

  return NULL;  // Key not found.
}

int InsertHashTable(HashTable table,
                    HTKeyValue newkeyvalue,
                    HTKeyValue *oldkeyvalue) {
  HWSize_t insertbucket;
  LinkedList insertchain;
  HTKeyValuePtr p;
  int insert_status;

  assert(table != NULL);
  ResizeHashtable(table);

  // calculate which bucket we're inserting into,
  // grab its linked list chain
  insertbucket = HashKeyToBucketNum(table, newkeyvalue.key);
  insertchain = table->buckets[insertbucket];

  // Step 1 -- finish the implementation of InsertHashTable.
  // This is a fairly complex task, so you might decide you want
  // to define/implement a helper function that helps you find
  // and optionally remove a key within a chain, rather than putting
  // all that logic inside here.  You might also find that your helper
  // can be reused in steps 2 and 3.

  if (NumElementsInLinkedList(insertchain) == 0) {  // No elems in list.
    insert_status = (int)InsertHTKVNodeIntoLL(newkeyvalue, &insertchain);
    if (insert_status == 1) {
      table->num_elements = table->num_elements + 1;
    }
    return insert_status;
  }

  LLIter iter = LLMakeIterator(insertchain, 0);

  if (iter == NULL) {  // Out of memory
    return 0;
  }

  p = search(newkeyvalue.key, &iter);

  if (p == NULL) {  // Elems in list, but none with the key we have.
    free(iter);
    insert_status = (int)InsertHTKVNodeIntoLL(newkeyvalue, &insertchain);
    if (insert_status == 1) {
      table->num_elements = table->num_elements + 1;
    }
    return insert_status;
  }

  // There were values in the list, and the list had our desired key.
  *oldkeyvalue = *p;
  p->value = newkeyvalue.value;
  free(iter);
  return 2;
}

int LookupHashTable(HashTable table,
                    HTKey_t key,
                    HTKeyValue *keyvalue) {
  assert(table != NULL);
  HTKeyValuePtr p;

  // Step 2 -- implement LookupHashTable.

  HWSize_t insertbucket;
  LinkedList insertchain;

  assert(table != NULL);

  // calculate which bucket we're inserting into,
  // grab its linked list chain
  insertbucket = HashKeyToBucketNum(table, key);
  insertchain = table->buckets[insertbucket];

  if (NumElementsInLinkedList(insertchain) == 0) {
    return 0;
  }

  LLIter iter = LLMakeIterator(insertchain, 0);

  if (iter == NULL) {  // Out of memory
    return -1;
  }

  p = search(key, &iter);

  if (p == NULL) {
    free(iter);
    return 0;
  }

  *keyvalue = *p;

  free(iter);
  return 1;
}

int RemoveFromHashTable(HashTable table,
                        HTKey_t key,
                        HTKeyValue *keyvalue) {
  assert(table != NULL);
  HTKeyValuePtr p;


  HWSize_t insertbucket;
  LinkedList insertchain;

  assert(table != NULL);

  // calculate which bucket we're inserting into,
  // grab its linked list chain
  insertbucket = HashKeyToBucketNum(table, key);
  insertchain = table->buckets[insertbucket];

  // Step 3 -- implement RemoveFromHashTable.

  if (NumElementsInLinkedList(insertchain) == 0) {
    return 0;
  }

  LLIter iter = LLMakeIterator(insertchain, 0);

  if (iter == NULL) {  // Out of memory
    return -1;
  }

  p = search(key, &iter);

  if (p == NULL) {
    free(iter);
    return 0;
  }

  *keyvalue = *p;
  LLIteratorDelete(iter, LLNullFree);
  table->num_elements = table->num_elements - 1;
  free(p);
  free(iter);
  return 1;
}

HTIter HashTableMakeIterator(HashTable table) {
  HTIterRecord *iter;
  HWSize_t      i;

  assert(table != NULL);  // be defensive

  // malloc the iterator
  iter = (HTIterRecord *) malloc(sizeof(HTIterRecord));
  if (iter == NULL) {
    return NULL;
  }

  // if the hash table is empty, the iterator is immediately invalid,
  // since it can't point to anything.
  if (table->num_elements == 0) {
    iter->is_valid = false;
    iter->ht = table;
    iter->bucket_it = NULL;
    return iter;
  }

  // initialize the iterator.  there is at least one element in the
  // table, so find the first element and point the iterator at it.
  iter->is_valid = true;
  iter->ht = table;
  for (i = 0; i < table->num_buckets; i++) {
    if (NumElementsInLinkedList(table->buckets[i]) > 0) {
      iter->bucket_num = i;
      break;
    }
  }
  assert(i < table->num_buckets);  // make sure we found it.
  iter->bucket_it = LLMakeIterator(table->buckets[iter->bucket_num], 0UL);
  if (iter->bucket_it == NULL) {
    // out of memory!
    free(iter);
    return NULL;
  }
  return iter;
}

void HTIteratorFree(HTIter iter) {
  assert(iter != NULL);
  if (iter->bucket_it != NULL) {
    LLIteratorFree(iter->bucket_it);
    iter->bucket_it = NULL;
  }
  iter->is_valid = false;
  free(iter);
}

int HTIteratorNext(HTIter iter) {
  assert(iter != NULL);

  // Step 4 -- implement HTIteratorNext.

  assert(iter->bucket_it != NULL);

  // There are 3 cases to consider
  // 1. There is a next node in the LL (trivial)
  // 2. There are no more nodes in the LL but there is another nonempty bucket
  // 3. There are no more nodes in the LL and there are no more nonempty buckets
  if (NumElementsInHashTable(iter->ht) == 0) {
    iter->is_valid = false;
    return 0;
  }

  // Case 1
  if (LLIteratorHasNext(iter->bucket_it)) {
    LLIteratorNext(iter->bucket_it);
    iter->is_valid = true;
    return 1;
  }

  // Case 2/3
  for (int i = iter->bucket_num + 1; i < iter->ht->num_buckets; i++) {
    if (NumElementsInLinkedList(iter->ht->buckets[i]) > 0) {
      free(iter->bucket_it);
      iter->bucket_it = LLMakeIterator(iter->ht->buckets[i], 0UL);

      assert(iter->bucket_it != NULL);

      iter->bucket_num = i;
      return 1;
    }
  }

  iter->bucket_num = iter->ht->num_buckets + 1;
  iter->is_valid = false;
  return 0;
}

int HTIteratorPastEnd(HTIter iter) {
  assert(iter != NULL);

  // Step 5 -- implement HTIteratorPastEnd.

  return iter->is_valid ? 0 : 1;
}

int HTIteratorGet(HTIter iter, HTKeyValue *keyvalue) {
  assert(iter != NULL);

  // Step 6 -- implement HTIteratorGet.

  HTKeyValuePtr p;

  if (!(iter->is_valid) | (NumElementsInHashTable(iter->ht) == 0)) {
    return 0;
  }

  LLIteratorGetPayload(iter->bucket_it, (LLPayload_t *)&p);
  *keyvalue = *p;

  return 1;
}

int HTIteratorDelete(HTIter iter, HTKeyValue *keyvalue) {
  HTKeyValue kv;
  int res, retval;

  assert(iter != NULL);

  // Try to get what the iterator is pointing to.
  res = HTIteratorGet(iter, &kv);
  if (res == 0)
    return 0;

  // Advance the iterator.
  res = HTIteratorNext(iter);
  if (res == 0) {
    retval = 2;
  } else {
    retval = 1;
  }
  res = RemoveFromHashTable(iter->ht, kv.key, keyvalue);
  assert(res == 1);
  assert(kv.key == keyvalue->key);
  assert(kv.value == keyvalue->value);

  return retval;
}

static void ResizeHashtable(HashTable ht) {
  // Resize if the load factor is > 3.
  if (ht->num_elements < 3 * ht->num_buckets)
    return;

  // This is the resize case.  Allocate a new hashtable,
  // iterate over the old hashtable, do the surgery on
  // the old hashtable record and free up the new hashtable
  // record.
  HashTable newht = AllocateHashTable(ht->num_buckets * 9);

  // Give up if out of memory.
  if (newht == NULL)
    return;
  // Loop through the old ht with an iterator,
  // inserting into the new HT.
  HTIter it = HashTableMakeIterator(ht);
  if (it == NULL) {
    // Give up if out of memory.
    FreeHashTable(newht, &HTNullFree);
    return;
  }

  while (!HTIteratorPastEnd(it)) {
    HTKeyValue item, dummy;

    assert(HTIteratorGet(it, &item) == 1);
    if (InsertHashTable(newht, item, &dummy) != 1) {
      // failure, free up everything, return.
      HTIteratorFree(it);
      FreeHashTable(newht, &HTNullFree);
      return;
    }
    HTIteratorNext(it);
  }

  // Worked!  Free the iterator.
  HTIteratorFree(it);

  // Sneaky: swap the structures, then free the new table,
  // and we're done.
  {
    HashTableRecord tmp;

    tmp = *ht;
    *ht = *newht;
    *newht = tmp;
    FreeHashTable(newht, &HTNullFree);
  }

  return;
}
