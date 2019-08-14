#ifndef _CHECKPOINT_H_
#define _CHECKPOINT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <search.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
#include "DataStructs/HashTable.h"
#include "DataStructs/LinkedList.h"

// Different options require a different number of args.
#define CHECK_ARG_COUNT(c)\
  if (argc != c) {\
    fprintf(stderr, "invalid # of commands: got %d, expected %d\n", argc, c);\
    return EXIT_FAILURE;\
  }
// Some actions may have memory issues. In the case that
// such an error occurs, we want the option to repeat the
// action a certain number of times in further attempts
// to get that sweet sweet heap memory.
#define ATTEMPT(action, failure, num_attempts_left)\
  while (action == failure) {\
    if (!num_attempts_left) { return MEM_ERR; }\
    num_attempts_left--;\
  }\
  num_attempts_left = NUMBER_ATTMEPTS;
// Used when an implication that should have been true
// ends up false.
#define PREEXISTING(msg, name, res)\
  if (res == 2) {\
    if (DEBUG) {\
      printf("\tConflicting state:\n"\
              "\t%s was not supposed to have a mapping to"\
              msg, src_filename);\
    }\
    return CREATE_BAD;\
  }

#define STORED_CPTS_FILE "./.cpt_bkpng/strd_cpts"
#define WORKING_DIR "./.cpt_"
#define NUMBER_ATTMEPTS 20 // Number of times to try again on an out-of-mem err
// Various error/info codes
#define MEM_ERR 333

#define INVALID_COMMAND -1
#define SETUP_OK 0
#define SETUP_DIR_ERROR 1
#define SETUP_TAB_ERROR 2

#define WRITE_OK 0

#define LOAD_OK 0
#define LOAD_BAD -1

#define CREATE_OK 0
#define CREATE_BAD -1

#define SWAPTO_OK 0
#define DELETE_OK 0

// This is a struct which will hold pointers to all the hashtables required
// to maintain this VC system.
typedef struct cpt_manager {
  // Key: the hash of a filename.
  // Value: a pointer to a string on the heap (the filename).
  HashTable src_filehash_to_filename;
  // Key: the hash of a filename.
  // Value: a pointer to a LinkedList of pointers to checkpoint
  //        names which are stored on the heap.
  HashTable src_filehash_to_cptnames;
  // Key: the hash of a checkpoint filename (note that the filename
  //      should be IDENTICAL to the checkpoint name)
  // Value: a pointer to the heap which contains the name of the
  //        checkpoint file (i.e. the stored difference)
  HashTable cpt_namehash_to_cptfilename;
} CheckPointLog, *CheckPointLogPtr;

const char *valid_commands[] = {"create", "swapto", "delete", "list"};

// Prints usage to stderr
static void Usage(void);

// Makes sure there won't be any fatal issues with macros.
// Will crash the program if any are detected.
// NOTE: Will only check for severe issues which would cause
//       unspecified behavior. Does not check for miscellaneous
//       issues (such as SETUP_DIR_ERROR == SETUP_TAB_ERROR).
static void CheckMacros();

// This method does two things:
//  1. Ensures the hidden checkpoint dir is setup for storage
//  2. Loads the data stored in the dir into the tables (nothing is loaded
//     if the dir has not yet been setup)
// Returns:
//  SETUP_OK - if all went well, and an error code otherwise.
static int Setup(CheckPointLogPtr cpt_log);

// Loads the stored checkpoints from the bookkeeping dir into 
// @cpt_log. If there is no file (or the file is empty), nothing 
// will be added into the tables.
//
// Returns:
//  LOAD_OK - if all went well
//
//  LOAD_BAD - if an error occurs, in which case errno should be checked.
static int LoadTables(CheckPointLogPtr cpt_log);

// Writes a copy of the current tables to disk so that the program
// will not "forget" all the work it has done.
static int WriteTables(CheckPointLogPtr cpt_log);

// Stores the state of @src_filename in @cpt_filename.
//
// Returns:
//  -2: if a reading error occured.
//
//  -1: if a writing error occured.
//
//   0: if all went well.
static int WriteSrcToCheckpoint(char *src_filename, char *cpt_filename);

// Checks that the supplied command is valid.
//
// Returns:
//
//  -1: if the command is invalid, and an index otherwise.
static int IsValidCommand(char *command);

// Creates the checkpoint name @cpt for the file
// @filename, and saves it in @cpt_log.
//
// Returns:
//  CREATE_OK - if all went well, and an error code otherwise.
static int CreateCheckpoint(char *cpt_name,
                            char *filename,
                            CheckPointLogPtr cpt_log);

// Changes to the checkpoint @cpt_name. Note that this will
// overwrite/delete the current copy of the file for the
// given checkpoint name, and replace it with the version
// saved for the checkpoint.
//
// Returns:
//  SWAPTO_OK if all went well, and SWAPTO_* error code otherwise.
static int SwapTo(char *cpt_name, CheckPointLogPtr cpt_log);

// Deletes the checkpoint @cpt_name (modifies cpt_log).
// Also removes any saved info on @checkpointname from the working dir.
//
// Returns:
//  DELETE_OK if all went well, and DELETE_* error code otherwise.
static int Delete(char *cpt_name, CheckPointLogPtr cpt_log);

// Prints a list of all current checkpoints (and their corresponding
// files) to stdout.
//
// Returns:
//  The number of checkpoints stored for this dir.
static size_t List(CheckPointLogPtr cpt_log);

// Handles freeing all the tables and their contents.
static void FreeCheckPointLog(CheckPointLogPtr cpt_log);

#endif  // _CHECKPOINT_H_