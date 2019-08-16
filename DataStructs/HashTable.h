#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <stdint.h>     // so we can use uint64_t, etc.
#include "./CP.h"     // for CPSize_t

struct hashtablerecord;
typedef struct hashtablerecord *HashTable;

typedef uint64_t HashTabKey_t;      // hash table key type
typedef void*    HashTabVal_t;    // hash table value type

// When freeing a HashTable, customers need to pass a pointer to a function
// that frees the payload.  The pointed-to function is invoked once for each
// value in the HashTable.
typedef void(*ValueFreeFnPtr)(HashTabVal_t value);

// Returns NULL on ERROR, non-NULL on success.
HashTable MakeHashTable(CPSize_t bucket_count);

// Frees the HashTable table, and invokes free_func on all
// (HashTabVal_t)s in table.
void FreeHashTable(HashTable table, ValueFreeFnPtr free_func);

// Returns the number of elements in the hash table.
CPSize_t HTSize(HashTable table);

// HashTables store key/value pairs.  We'll define a key to be an
// unsigned 64-bit integer; it's up to the customer to figure out how
// to produce an appropriate hash key, but below we provide an
// implementation of FNV hashing to help them out.  We'll define
// a value to be a (void *), so that customers can pass in pointers to
// arbitrary structs as values, but of course we have to worry about
// memory management as a side-effect.
typedef struct {
  HashTabKey_t     key;    // the key in the key/value pair
  HashTabVal_t value;  // the value in the key/value pair
} HashTabKV, *HashTabKVPtr;

// This is an implementation of FNV hashing.  Customers
// can use it to hash an arbitrary sequence of bytes into
// a 64-bit key suitable for using as a hash key.
//
// Returns:
//
// - a nicely distributed 64-bit hash value suitable for
//   use in a HashTabKV
HashTabKey_t HashFunc(unsigned char *buffer, CPSize_t len);

// This is a convenience routine to produce a nice, evenly
// distributed 64-bit hash key from a potentially poorly
// distributed 64 bit number.  It uses HashFunc to get its
// job done.
//
// Returns:
//
// - a nicely distributed 64-bit hash value suitable for
//   use in a HashTabKV
HashTabKey_t HashInt64(HashTabVal_t hashme);

// Inserts a key,value pair into the HashTable.
//
// Arguments:
//
// - table: the HashTable to insert into
//
// - kv_to_insert: the HashTabKV to insert into the table
//
// - oldkeyval: if the key is in this HT, kv_to_insert will
//   effectively be inserted, and the old keyval pair removed
//   and stored in old_kv_storage
//
// Returns:
//
//  - 0 on failure (e.g., out of memory)
//
//  - +1 if the kv_to_insert was inserted and there was no
//    existing key/value with that key
//
//  - +2 if the kv_to_insert was inserted and an old key/value
//    with the same key was replaced
int HTInsert(HashTable table,
                    HashTabKV kv_to_insert,
                    HashTabKV *old_kv_storage);

// Looks up a key in the HashTable, and if it is
// present, returns the key/value associated with it.
//
// Arguments:
//
// - table: the HashTable to look in
//
// - key: the key to look up
//
// - keyvalue: if the key is present, a copy of the key/value is
//   returned to the caller via this return parameter. table
//   maintains ownership over the ptr stored in keyvalue->value,
//   so DO NOT free it.
//
// Returns:
//
//  - -1 if there was an ERROR (e.g., out of memory)
//
//  - 0 if the key wasn't found in the HashTable
//
//  - +1 if the key was found
int HTLookup(HashTable table,
             HashTabKey_t key,
             HashTabKV *keyvalue);

// Removes a key/value from the HashTable and returns it to the
// caller.
//
// Arguments:
//
// - table: the HashTable to look in
//
// - key: the key to look up
//
// - keyvalue: if the key is present, a copy of key/value is returned
//   to the caller via this return parameter and the key/value is
//   removed from the HashTable. Since the pointer in keyvalue->value
//   is no longer stored in table, ownership is now upon the client
//   (client must free the pointer).
//
// Returns:
//  - -1 on ERROR (e.g., out of memory)
//
//  - 0 if the key wasn't found in the HashTable
//
//  - +1 if the key was found
int HTRemove(HashTable table,
                        HashTabKey_t key,
                        HashTabKV *keyvalue);

struct ht_itrec;
typedef struct ht_itrec *HTIter;  // same trick to hide implementation.

// Makes an iterator for the table.
//
// Arguments:
//
// - table:  the table from which to return an iterator
//
// Returns NULL on failure, non-NULL on success.
HTIter MakeHTIter(HashTable table);

// Function for freeing an iterator after use.
//
// Arguments:
//
// - iter: the iterator to free.  It is not safe to use
//   the iterator after freeing it.
void DiscardHTIter(HTIter iter);

// Move the iterator to the next element of the table.
//
// Arguments:
//
// - iter: the iterator to move
//
// Returns:
//
// - +1 on success
//
// - 0 if the iterator cannot be moved forward to the next
//   valid element, either because the table is empty, or
//   because the iterator (after moving it forward) is past the
//   end of the table.  In either case, the iterator is no
//   longer usable.
int HTIncrementIter(HTIter iter);

// Tests to see if the iterator is past the end
// of its table.
//
// Arguments:
//
// - iter: the iterator to test
//
// Returns:
//
// - +1 if iter is past the end of the table or the table is empty.
//
// - 0 if iter is not at the end of the table (and therefore
//   the table is non-empty).
int HTIterValid(HTIter iter);

// Returns a copy of the key/value that the iterator is currently
// pointing at.
//
// Arguments:
//
// - iter: the iterator to fetch the key/value from
//
// - keyvalue: a return parameter through which the key/value
//   is returned.
//
// Returns:
//
// - 0 if the iterator is not valid or the table is empty
//
// - +1 on success.
int HTIterKV(HTIter iter, HashTabKV *keyvalue);

// Returns a copy of key/value that the iterator is currently
// pointing at, and removes that key/value from the
// hashtable.  The caller assumes ownership of any memory
// pointed to by the value.  As well, this advances
// the iterator to the next element in the hashtable.
//
// Arguments:
//
// - iter: the iterator to fetch the key/value from
//
// - keyvalue: a return parameter through which the key/value
//   is returned.
//
// Returns:
//
// - 0 if the iterator is not valid or the table is empty
//
// - +1 on success, and the iterator is still valid.
//
// - +2 on success, but the iterator is now invalid because
//   it has advanced past the end of the hash table.
int HTIterDel(HTIter iter, HashTabKV *keyvalue);

#endif  // _HASHTABLE_H_
