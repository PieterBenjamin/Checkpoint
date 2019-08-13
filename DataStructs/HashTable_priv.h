#ifndef _HASHTABLE_PRIV_H_
#define _HASHTABLE_PRIV_H_

#include "./CP.h"
#include "./LinkedList.h"
#include "./HashTable.h"

// Define the internal, private structs and helper functions associated with a
// HashTable.

// This is the struct that we use to represent a hash table. Quite simply, a
// hash table is just an array of buckets, where each bucket is a linked list
// of HashTabKV structs.
typedef struct hashtablerecord {
  CPSize_t        bucket_count;   // # of buckets in this HT?
  CPSize_t        ht_size;  // # of elements currently in this HT?
  LinkedList     *buckets;       // the array of buckets
} HashTableRecord;

// This is the struct we use to represent an iterator.
typedef struct ht_itrec {
  bool       valid;    // is this iterator valid?
  HashTable  ht;          // the HT we're pointing into
  CPSize_t   bucket;  // which bucket are we in?
  LLIter     bucket_iter;   // iterator for the bucket, or NULL
} HTIterRecord;

// This is the internal hash function we use to map from HashTabKey_t keys to a
// CPSize_t bucket number.
CPSize_t HTKeyToBucket(HashTable ht, HashTabKey_t key);

#endif  // _HASHTABLE_PRIV_H_
