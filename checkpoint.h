#ifndef _CHECKPOINT_H_
#define _CHECKPOINT_H_

#include "DataStructs/HashTable.h"

#define WORKING_DIR "./checkpoint_bookkeeping"
// Various error/info codes
#define SETUP_OK 0
#define SETUP_DIR_ERROR 1
#define SETUP_TAB_ERROR 2

#define CREATE_OK 0
#define SWAPTO_OK 0
#define DELETE_OK 0

// Prints usage to stderr
static void Usage(void);

// Checks that the supplied command is valid.
static int IsValidCommand(char *command);

// This method does two things:
//  1. Ensures the hidden checkpoint dir is setup for storage
//  2. Loads the data stored in the dir into @cpts (nothing is loaded
//     if the dir has not yet been setup)
// Returns:
//  SETUP_OK - if all went well, and a SETUP_* error code otherwise.
static int Setup(HashTable cpts);

// Creates the checkpoint name @checkpointname for the file
// @filename, and saves it in @cpts.
//
// Returns:
//  CREATE_OK - if all went well, and a CREATE_* error code otherwise.
static int create(char **checkpointname, char **filename, HashTable cpts);

// Changes to the checkpoint @checkpointname. Note that this will
// effectively overwrite/delete the current copy of the file for
// the given checkpoint name, and replace it with the file saved 
// for the checkpoint.
//
// Returns:
//  SWAPTO_OK if all went well, and SWAPTO_* error code otherwise.
static int swapto(char **checkpointname, HashTable cpts);

// Deletes the checkpoint @checkpointname (modifies cpts).
// Also removes any saved info on @checkpointname from the working dir.
//
// Returns:
//  DELETE_OK if all went well, and DELETE_* error code otherwise.
static int delete(char **checkpointname, HashTable cpts);

// Prints a list of all current checkpoints (and their corresponding
// files) to stdout.
//
// Returns:
//  The number of checkpoints stored for this dir.
static size_t list(HashTable cpts);

#endif  // _CHECKPOINT_H_