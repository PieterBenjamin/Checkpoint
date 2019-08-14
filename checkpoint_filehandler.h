// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

// This module is the sole point of file i/o for the entire program.
// As such, it is somewhat of a "god class" in that it has access to
// ecery struct used in the entire program.

#include "DataStructs/HashTable.h"
#include "checkpoint_tree.h"

#include <search.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

// This is a struct which will hold pointers to all the data structs
// required to maintain this VC system.
typedef struct cpt_manager {
  // Key: the hash of a filename.
  // Value: a pointer to a string on the heap (the filename).
  HashTable src_filehash_to_filename;
  // Key: the hash of a filename.
  // Value: a pointer to a cpt_tree struct on the heap which
  //        stores all the information about the checkpoints
  //        for the file whose name hashes to the key.
  HashTable src_filehash_to_cptnames;
  // Key: the hash of a checkpoint filename (note that the filename
  //      should be IDENTICAL to the checkpoint name)
  // Value: a pointer to the heap which contains the name of the
  //        checkpoint file (i.e. the stored difference)
  HashTable cpt_namehash_to_cptfilename;

  // This struct is separate from the others, and is solely
  // responsible for maintaining all the information about
  // the relationships between checkpoints.
  CpTreeNodePtr root_node;
} CheckPointLog, *CheckPointLogPtr;

// Loads the stored checkpoints from the bookkeeping dir into 
// @cpt_log. If there is no file (or the file is empty), nothing 
// will be added into the tables.
//
// Returns:
//  READ_SUCCESS - if all went well
//
//  READ_ERROR - if an error occurs, in which case errno should be checked.
int ReadCheckPointLog(CheckPointLogPtr cpt_log);

// Writes a copy of the current tables to disk so that the program
// will not "forget" all the work it has done.
int WriteCheckPointLog(CheckPointLogPtr cpt_log);

// Stores the state of @src_filename in @cpt_filename.
//
// Returns:
//  -2: if a reading error occured.
//
//  -1: if a writing error occured.
//
//   0: if all went well.
int WriteSrcToCheckpoint(char *src_filename, char *cpt_filename);