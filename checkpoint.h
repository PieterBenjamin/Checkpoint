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

#define STORED_CPTS_FILE "./.cpt_bkpng/strd_cpts"
#define WORKING_DIR "./.cpt_bkpng"
// Various error/info codes
#define INVALID_COMMAND -1
#define SETUP_OK 0
#define SETUP_DIR_ERROR 1
#define SETUP_TAB_ERROR 2

#define LOAD_OK 0
#define LOAD_BAD -1

#define CREATE_OK 0
#define SWAPTO_OK 0
#define DELETE_OK 0

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
//  2. Loads the data stored in the dir into @cpts (nothing is loaded
//     if the dir has not yet been setup)
// Returns:
//  SETUP_OK - if all went well, and a SETUP_* error code otherwise.
static int Setup(HashTable cpts);

// Loads the stored checkpoints from the bookkeeping dir into 
// @cpts. If there is no file (or the file is empty), nothing 
// will be added into @cpts.
//
// If the file DNE, this method will create it.
//
// Returns:
//  LOAD_OK - if all went well
//
//  LOAD_BAD - if an error occurs, in which case errno should be checked.
static int LoadTable(HashTable cpts);

// Checks that the supplied command is valid.
static int IsValidCommand(char *command);

// Creates the checkpoint name @checkpointname for the file
// @filename, and saves it in @cpts.
//
// Returns:
//  CREATE_OK - if all went well, and a CREATE_* error code otherwise.
static int Create(char **checkpointname, char **filename, HashTable cpts);

// Changes to the checkpoint @checkpointname. Note that this will
// effectively overwrite/delete the current copy of the file for
// the given checkpoint name, and replace it with the file saved 
// for the checkpoint.
//
// Returns:
//  SWAPTO_OK if all went well, and SWAPTO_* error code otherwise.
static int SwapTo(char **checkpointname, HashTable cpts);

// Deletes the checkpoint @checkpointname (modifies cpts).
// Also removes any saved info on @checkpointname from the working dir.
//
// Returns:
//  DELETE_OK if all went well, and DELETE_* error code otherwise.
static int Delete(char **checkpointname, HashTable cpts);

// Prints a list of all current checkpoints (and their corresponding
// files) to stdout.
//
// Returns:
//  The number of checkpoints stored for this dir.
size_t List(HashTable cpts);

#endif  // _CHECKPOINT_H_