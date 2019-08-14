#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "CP.h"
#include "HashTable.h"
#include "HashTable_priv.h"

// A private utility function to grow the hashtable (increase
// the number of buckets) if its load factor has become too high.
static void ResizeHashtable(HashTable ht);

// a free function that does nothing
static void LLNullFree(LinkedListPayload freeme) { }
static void HTNullFree(HashTabVal_t freeme) { }

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
// - A ptr to the HashTabKV which possesses the desired key (if it exists in
//   the LinkedList *iter_ptr is iterating through).
//
// - NULL otherwise.
static HashTabKVPtr search(HashTabKey_t key,
                            LLIter *iter_ptr);


// Inserts a HashTabKV into the specified Linked List.
//
// Arguments:
//
// - kv_to_insert: The HashTabKV to insert.
//
// - insertchain_ptr: A pointer to the LL to insert the HashTabKV into.
//
// Returns:
//
// - 0: If any memory error occurs.
//
// - 1: If insertion was successful.
static int InsertHTKVNodeIntoLL(HashTabKV kv_to_insert,
                                LinkedList *insertchain_ptr);

HashTable MakeHashTable(CPSize_t bucket_count) {
  HashTable ht;
  CPSize_t  i;

  // defensive programming
  if (bucket_count == 0) {
    return NULL;
  }

  // allocate the hash table record
  ht = (HashTable) malloc(sizeof(HashTableRecord));
  if (ht == NULL) {
    return NULL;
  }

  // initialize the record
  ht->bucket_count = bucket_count;
  ht->ht_size = 0;
  ht->buckets =
    (LinkedList *) malloc(bucket_count * sizeof(LinkedList));
  if (ht->buckets == NULL) {
    // make sure we don't leak!
    free(ht);
    return NULL;
  }
  for (i = 0; i < bucket_count; i++) {
    ht->buckets[i] = MakeLinkedList();
    if (ht->buckets[i] == NULL) {
      // allocating one of our bucket chain lists failed,
      // so we need to free everything we allocated so far
      // before returning NULL to indicate failure.  Since
      // we know the chains are empty, we'll pass in a
      // free function pointer that does nothing; it should
      // never be called.
      CPSize_t j;
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
                   ValueFreeFnPtr free_func) {
  CPSize_t i;

  assert(table != NULL);  // be defensive

  // loop through and free the chains on each bucket
  for (i = 0; i < table->bucket_count; i++) {
    LinkedList  bl = table->buckets[i];
    HashTabKV *nextKV;

    // pop elements off the the chain list, then free the list
    while (LLSize(bl) > 0) {
      assert(LLPop(bl, (LinkedListPayload*)&nextKV));
      free_func(nextKV->value);
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

CPSize_t HTSize(HashTable table) {
  assert(table != NULL);
  return table->ht_size;
}

HashTabKey_t HashFunc(unsigned char *buffer, CPSize_t len) {
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

HashTabKey_t HashInt64(HashTabVal_t hashval) {
  unsigned char buf[8];
  int i;
  uint64_t hashme = (uint64_t)hashval;

  for (i = 0; i < 8; i++) {
    buf[i] = (unsigned char) (hashme & 0x00000000000000FFULL);
    hashme >>= 8;
  }
  return HashFunc(buf, 8);
}

CPSize_t HTKeyToBucket(HashTable ht, HashTabKey_t key) {
  return key % ht->bucket_count;
}

static int InsertHTKVNodeIntoLL(HashTabKV kv_to_insert,
                                LinkedList *insertchain_ptr) {
  LinkedList insertchain = *insertchain_ptr;
  HashTabKVPtr kv_to_insert_heap;

  kv_to_insert_heap = (HashTabKVPtr)(malloc(sizeof(HashTabKV)));
  if (kv_to_insert_heap == NULL) {
    return 0;
  }

  kv_to_insert_heap->key  = kv_to_insert.key;
  kv_to_insert_heap->value = kv_to_insert.value;

  return LLPush(insertchain, (LinkedListPayload *)kv_to_insert_heap) ? 1 : 0;
}

static HashTabKVPtr search(HashTabKey_t key,
                            LLIter *iter_ptr) {
  HashTabKVPtr p;
  LLIter iter = *iter_ptr;

  while (LLiterHasNext(iter)) {
    // *p = iter->node->payload;
    LLIterPayload(iter, (LinkedListPayload *)&p);

    if (p->key == key) {
      return p;
    }
    LLIterAdvance(iter);
  }

  // Edge case: last element.
  LLIterPayload(iter, (LinkedListPayload *)&p);
  if (p->key == key) {
    return p;
  }

  return NULL;  // Key not found.
}

int HTInsert(HashTable table,
                    HashTabKV kv_to_insert,
                    HashTabKV *old_kv_storage) {
  CPSize_t insertbucket;
  LinkedList insertchain;
  HashTabKVPtr p;
  int insert_status;

  assert(table != NULL);
  ResizeHashtable(table);

  // calculate which bucket we're inserting into,
  // grab its linked list chain
  insertbucket = HTKeyToBucket(table, kv_to_insert.key);
  insertchain = table->buckets[insertbucket];

  if (LLSize(insertchain) == 0) {  // No elems in list.
    insert_status = (int)InsertHTKVNodeIntoLL(kv_to_insert, &insertchain);
    if (insert_status == 1) {
      table->ht_size = table->ht_size + 1;
    }
    return insert_status;
  }

  LLIter iter = LLGetIter(insertchain, 0);

  if (iter == NULL) {  // Out of memory
    return 0;
  }

  p = search(kv_to_insert.key, &iter);

  if (p == NULL) {  // Elems in list, but none with the key we have.
    free(iter);
    insert_status = (int)InsertHTKVNodeIntoLL(kv_to_insert, &insertchain);
    if (insert_status == 1) {
      table->ht_size = table->ht_size + 1;
    }
    return insert_status;
  }

  // There were values in the list, and the list had our desired key.
  *old_kv_storage = *p;
  p->value = kv_to_insert.value;
  free(iter);
  return 2;
}

int HTLookup(HashTable table,
                    HashTabKey_t key,
                    HashTabKV *keyvalue) {
  assert(table != NULL);
  HashTabKVPtr p;

  CPSize_t insertbucket;
  LinkedList insertchain;

  assert(table != NULL);

  // calculate which bucket we're inserting into,
  // grab its linked list chain
  insertbucket = HTKeyToBucket(table, key);
  insertchain = table->buckets[insertbucket];

  if (LLSize(insertchain) == 0) {
    return 0;
  }

  LLIter iter = LLGetIter(insertchain, 0);

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

int HTRemove(HashTable table,
                        HashTabKey_t key,
                        HashTabKV *keyvalue) {
  assert(table != NULL);
  HashTabKVPtr p;


  CPSize_t insertbucket;
  LinkedList insertchain;

  assert(table != NULL);

  // calculate which bucket we're inserting into,
  // grab its linked list chain
  insertbucket = HTKeyToBucket(table, key);
  insertchain = table->buckets[insertbucket];

  if (LLSize(insertchain) == 0) {
    return 0;
  }

  LLIter iter = LLGetIter(insertchain, 0);

  if (iter == NULL) {  // Out of memory
    return -1;
  }

  p = search(key, &iter);

  if (p == NULL) {
    free(iter);
    return 0;
  }

  *keyvalue = *p;
  LLiterDel(iter, LLNullFree);
  table->ht_size = table->ht_size - 1;
  free(p);
  free(iter);
  return 1;
}

HTIter MakeHTIter(HashTable table) {
  HTIterRecord *iter;
  CPSize_t      i;

  assert(table != NULL);  // be defensive

  // malloc the iterator
  iter = (HTIterRecord *) malloc(sizeof(HTIterRecord));
  if (iter == NULL) {
    return NULL;
  }

  // if the hash table is empty, the iterator is immediately invalid,
  // since it can't point to anything.
  if (table->ht_size == 0) {
    iter->valid = false;
    iter->ht = table;
    iter->bucket_iter = NULL;
    return iter;
  }

  // initialize the iterator.  there is at least one element in the
  // table, so find the first element and point the iterator at it.
  iter->valid = true;
  iter->ht = table;
  for (i = 0; i < table->bucket_count; i++) {
    if (LLSize(table->buckets[i]) > 0) {
      iter->bucket = i;
      break;
    }
  }
  assert(i < table->bucket_count);  // make sure we found it.
  iter->bucket_iter = LLGetIter(table->buckets[iter->bucket], 0UL);
  if (iter->bucket_iter == NULL) {
    // out of memory!
    free(iter);
    return NULL;
  }
  return iter;
}

void DiscardHTIter(HTIter iter) {
  assert(iter != NULL);
  if (iter->bucket_iter != NULL) {
    LLIterFree(iter->bucket_iter);
    iter->bucket_iter = NULL;
  }
  iter->valid = false;
  free(iter);
}

int HTIncrementIter(HTIter iter) {
  assert(iter != NULL);

  assert(iter->bucket_iter != NULL);

  // There are 3 cases to consider
  // 1. There is a next node in the LL (trivial)
  // 2. There are no more nodes in the LL but there is another nonempty bucket
  // 3. There are no more nodes in the LL and there are no more nonempty buckets
  if (HTSize(iter->ht) == 0) {
    iter->valid = false;
    return 0;
  }

  // Case 1
  if (LLiterHasNext(iter->bucket_iter)) {
    LLIterAdvance(iter->bucket_iter);
    iter->valid = true;
    return 1;
  }

  // Case 2/3
  for (int i = iter->bucket + 1; i < iter->ht->bucket_count; i++) {
    if (LLSize(iter->ht->buckets[i]) > 0) {
      free(iter->bucket_iter);
      iter->bucket_iter = LLGetIter(iter->ht->buckets[i], 0UL);

      assert(iter->bucket_iter != NULL);

      iter->bucket = i;
      return 1;
    }
  }

  iter->bucket = iter->ht->bucket_count + 1;
  iter->valid = false;
  return 0;
}

int HTIterValid(HTIter iter) {
  assert(iter != NULL);
  return iter->valid ? 0 : 1;
}

int HTIterKV(HTIter iter, HashTabKV *keyvalue) {
  assert(iter != NULL);

  HashTabKVPtr p;

  if (!(iter->valid) | (HTSize(iter->ht) == 0)) {
    return 0;
  }

  LLIterPayload(iter->bucket_iter, (LinkedListPayload *)&p);
  *keyvalue = *p;

  return 1;
}

int HTIterDel(HTIter iter, HashTabKV *keyvalue) {
  HashTabKV kv;
  int res, retval;

  assert(iter != NULL);

  // Try to get what the iterator is pointing to.
  res = HTIterKV(iter, &kv);
  if (res == 0)
    return 0;

  // Advance the iterator.
  res = HTIncrementIter(iter);
  if (res == 0) {
    retval = 2;
  } else {
    retval = 1;
  }
  res = HTRemove(iter->ht, kv.key, keyvalue);
  assert(res == 1);
  assert(kv.key == keyvalue->key);
  assert(kv.value == keyvalue->value);

  return retval;
}

static void ResizeHashtable(HashTable ht) {
  // Resize if the load factor is > 3.
  if (ht->ht_size < 3 * ht->bucket_count)
    return;

  // This is the resize case.  Allocate a new hashtable,
  // iterate over the old hashtable, do the surgery on
  // the old hashtable record and free up the new hashtable
  // record.
  HashTable newht = MakeHashTable(ht->bucket_count * 9);

  // Give up if out of memory.
  if (newht == NULL)
    return;
  // Loop through the old ht with an iterator,
  // inserting into the new HT.
  HTIter it = MakeHTIter(ht);
  if (it == NULL) {
    // Give up if out of memory.
    FreeHashTable(newht, &HTNullFree);
    return;
  }

  while (!HTIterValid(it)) {
    HashTabKV item, dummy;

    assert(HTIterKV(it, &item) == 1);
    if (HTInsert(newht, item, &dummy) != 1) {
      // failure, free up everything, return.
      DiscardHTIter(it);
      FreeHashTable(newht, &HTNullFree);
      return;
    }
    HTIncrementIter(it);
  }

  // Worked!  Free the iterator.
  DiscardHTIter(it);

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
