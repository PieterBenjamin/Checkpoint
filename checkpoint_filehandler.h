// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#ifndef _CHECKPOINT_FILEHANDLER_H_
#define _CHECKPOINT_FILEHANDLER_H_
// This module is the sole point of file i/o for the entire program.
// As such, it is somewhat of a "god class" in that it has access to
// every struct used in the entire program.

#include "DataStructs/HashTable.h"
#include "checkpoint_tree.h"

#include <search.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>


// THIS VALUE MUST BE NEGATIVE
#define FILE_WRITE_ERR -1

#define CHECK_HASHTABLE_LENGTH(x)\
  if (x == MEM_ERR || x == FILE_WRITE_ERR) { return FILE_WRITE_ERR; }

#pragma pack(push,1)

// WriteHashTable takes a function which will write all the
// buckets in the given table to a file. Since there are two
// types of HashTables, the function needs to be a parameter.
typedef int32_t (*write_bucket_fn)(FILE *f,
                                   uint32_t offset,
                                   HashTabKV kv);
typedef int32_t (*read_bucket_fn)(FILE *f,
                                  uint32_t offset,
                                  HashTabKV *kv);

// This is a struct which will hold pointers to all the data structs
// required to maintain this VC system.
typedef struct cpt_manager {
  // Key: the hash of a source filename.
  // Value: a pointer to a string on the heap (the filename).
  HashTable src_filehash_to_filename;
  // Key: the hash of a source filename
  // Value: a pointer to a string on the heap (the current cp of the file)
  HashTable src_filehash_to_cptname;
  // Key: the hash of a checkpoint filename (note that the filename
  //      should be IDENTICAL to the checkpoint name)
  // Value: a pointer to the heap which contains the name of the
  //        checkpoint file (i.e. the stored difference)
  HashTable cpt_namehash_to_cptfilename;

  // This table is by far the most complex. It keeps track
  // of all the checkpoint trees for the entire directory.
  // Key: the hash of a source filename.
  // Value: a pointer to a CpTreeNode on the heap.
  HashTable dir_tree;
} CheckPointLog, *CheckPointLogPtr;

typedef struct cpt_log_header {
  // These two fields are used to check if the file has been corrupted.
  u_int32_t magic_number;
  u_int32_t checksum;
  // These four fields are used to store the number of bytes written
  // for each file.
  u_int32_t src_filehash_to_filename_size;
  u_int32_t src_filehash_to_cptname_size;
  u_int32_t cpt_namehash_to_cptfilename_size;
  u_int32_t dir_tree_size;

} CpLogFileHeader;

// The header field for a list of bucket recs.
typedef struct bucket_reclist_header {
  // The number of bucket records in this list.
 uint32_t num_bucket_recs;
} BucketRecListHeader;

typedef struct bucket_rec {
  // The number of bytes written for the given bucket.
  uint32_t bucket_size;
  // The offset for the given bucket (startinf from
  // the beginning of the file).
  uint32_t bucket_pos;
} BucketRec;

// This struct should be written at the 
// beginning of every bucket.
typedef struct bucket_header {
  // Used while rehashing.
  uint64_t key;
} BucketHeader;

typedef struct string_bucket_header {
  uint32_t len;
} StringBucketHeader;

// Used for writing a CpTreeNode's bookkeeping information.
// Since the only other data being written is variable length,
// (the name of the node and offsets of children) not much
// else can be stored in a struct.
typedef struct file_tree_header {
  uint32_t name_length;
  uint32_t num_children;
} FileTreeHeader;

// Loads the stored checkpoints from the bookkeeping dir into 
// @cpt_log. If there is no file (or the file is empty), nothing 
// will be added into the tables.
//
// Returns:
//  - READ_SUCCESS - if all went well
//
//  - READ_ERROR - if an ERROR occurs, in which case errno should be checked.
int32_t ReadCheckPointLog(CheckPointLogPtr cpt_log);

static int32_t ReadHashTable(FILE *f,
                             uint32_t offset,
                             HashTable table,
                             read_bucket_fn fn);

// Helper method to ReadCheckPointLog.
// Reads in a Hash table whose values are supposed to be pointers
// to strings on the heap.
static int32_t ReadStringBucket(FILE *f, uint32_t offset, HashTabKV *kv);


static int32_t ReadTreeBucket(FILE *f, uint32_t offset, HashTabKV *kv);

static int32_t ReadTreeNode(FILE *f, uint32_t offset, CpTreeNodePtr curr_node);

// Helper method to ReadTreeNode. Reads the children of @parent into
// its linked list of children, starting from offset, which is assumed
// to be the beginning of an array of int32_ts which are themselves
// further offsets from @offset to the children of parent.
static int32_t ReadTreeChildren(FILE *f,
                                uint32_t offset,
                                uint32_t num_children,
                                CpTreeNodePtr parent);

// Writes a copy of the checkpoint Log to disk so that the program
// will not "forget" all the work it has done.
int32_t WriteCheckPointLog(CheckPointLogPtr cpt_log);

// This VC system is composed of four hash tables. This method
// takes care pf writing them, with the assistance of @fn.
//
// Returns:
//
//  - MEM_ERR: upon a memory ERROR.
//
//  - FILE_WRITE_ERR: if an ERROR arises while writing.
//
//  - The number of bytes written otherwise.
static int32_t WriteHashTable(FILE *f,
                              HashTable table,
                              uint32_t offset,
                              write_bucket_fn fn);

// Simply writes the given string to the given file at the given offset.
// DOES NOT INCLUDE NULL TERMINATOR.
//
// Returns:
//
//  - FILE_WRITE_ERR: if any ERRORs arose while writing.
//
//  - The number of chars written otherwise.
static int32_t WriteStringBucket(FILE *f, uint32_t offset, HashTabKV kv);

// Writes a bucket (including a bucket header) to file f,
// containing the contents of kv.value (assumed to be a
// CpTreeNodePtr).
//
// Returns:
//
//  - MEM_ERR: on memory ERROR.
//
//  - FILE_WRITE_ERR: if an ERROR arose while writing.
//
//  - The number of bytes written for curr_node(and it's children).
static int32_t WriteTreeBucket(FILE *f, uint32_t offset, HashTabKV kv);

// Writes a CpTreeNodePtr to file @f. Returns the size of the tree (in bytes). 
static int32_t WriteTree(FILE *f, uint32_t offset, CpTreeNodePtr curr_node);

// Helper method to WriteTree, writes the contents of all of curr_nodes
// children to file @f, starting at offset @offset
static int32_t WriteChildren(CpTreeNodePtr curr_node,
                             uint32_t *children_offsets,
                             uint32_t offset,
                             FILE *f);

// Stores the state of @src_filename in @cpt_filename.
//
// Returns:
//
//  -2: if a reading ERROR occured.
//
//  -1: if a writing ERROR occured.
//
//   0: if all went well.
int32_t WriteSrcToCheckpoint(char *src_filename, char *cpt_filename);

#pragma pack(pop)

#endif  // _CHECKPOINT_FILEHANDLER_H_