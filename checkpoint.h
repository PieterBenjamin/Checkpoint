// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#ifndef _CHECKPOINT_H_
#define _CHECKPOINT_H_

#include "checkpoint_filehandler.h"

#define INVALID_COMMAND -1
#define SETUP_SUCCESS 0
#define SETUP_DIR_ERROR 1
#define SETUP_TAB_ERROR 2

#define CREATE_CPT_SUCCESS 0
#define CREATE_CPT_ERROR -1

#define SWAPTO_SUCCESS 0
#define SWAPTO_ERROR -1

#define DELETE_SUCCESS 0
#define DELETE_ERROR -1

#define BACK_SUCCESS 0
#define BACK_ERROR -1

#define LIST_ERR -1

#define PRINT_ERR -1

// Different options require a different number of args.
#define CHECK_ARG_COUNT(c)\
  if (argc != c) {\
    fprintf(stderr, "invalid # of commands: got %d, expected %d\n", argc, c);\
    return EXIT_FAILURE;\
  }

const char *valid_commands[] = {"create", "back", "swapto", "delete", "list"};

// Prints usage to stderr
static void Usage(void);

// Makes sure there won't be any fatal issues with macros.
// Will crash the program if any are detected.
// NOTE: Will only check for severe issues which would cause
//       unspecified behavior. Does not check for miscellaneous
//       issues (such as SETUP_DIR_ERROR == SETUP_TAB_ERROR).
static void CheckMacros();

// This method does two things:
//  1. Ensures the hidden checkpoint  dir is setup for storage
//  2. Loads the data stored in the dir into the tables (nothing is loaded
//     if the dir has not yet been setup)
// Returns:
//  SETUP_SUCCESS - if all went well, and an error code otherwise.
static int32_t Setup(CheckPointLogPtr cpt_log);

// Checks that the supplied command is valid.
//
// Returns:
//
//  -1: if the command is invalid, and an index otherwise.
static int32_t IsValidCommand(char *command);

// Creates the checkpoint  name @cpt for the file
// @filename, and saves it in @cpt_log.
//
// Returns:
//  CREATE_CPT_SUCCESS - if all went well, and an error code otherwise.
static int32_t CreateCheckpoint(char *cpt_name,
                            char *filename,
                            CheckPointLogPtr cpt_log);

// Adds a checkpoint  with the knowledge that this file has not yet had
// a checkpoint  stored.
static int32_t AddCheckpointNewFile(char *cpt_name,
                                char *src_filename,
                                HashTabKey_t src_filename_hash,
                                CheckPointLogPtr cpt_log);

// Adds a checkpoint  with the knowledge that this file has had
// a checkpoint  stored.
static int32_t AddCheckpointExistingFile(char *cpt_name,
                                     char *src_filename,
                                     HashTabKey_t src_filename_hash,
                                     CheckPointLogPtr cpt_log);

// Changes to the checkpoint  @cpt_name. Note that this will
// overwrite/delete the current copy of the file for the
// given checkpoint  name, and replace it with the version
// saved for the checkpoint.
//
// Returns:
//  SWAPTO_SUCCESS if all went well, and SWAPTO_* error code otherwise.
static int32_t SwapTo(char *src_filename, char *cpt_name, CheckPointLogPtr cpt_log);

// Attempts to go backwards one step up the checkpoint tree stored for
// @src_filename.
//
// Returns:
//
//  - BACK_ERROR: if anything goes wrong.
//
//  - BACK_SUCCESS: if all goes well.
static int32_t Back(char *src_filename, CheckPointLogPtr cpt_log);

// Deletes all traces of @src_filename from the current checkpoint system.
//
// Returns:
//  DELETE_SUCCESS if all went well, and DELETE_* error code otherwise.
static int32_t Delete(char *src_filename, CheckPointLogPtr cpt_log);

// Helper method to Delete. Removes all mappings from the names in the tree
// stored inside cp_log->cpt_namehash_to_cptfilename.
//
// Returns:
//
//  - MEM_ERR: on a memory error.
//
//  - 0: upon successful completion.
static int32_t FreeTreeCpHash(CheckPointLogPtr cpt_log, CpTreeNodePtr curr_node);

// Prints a list of all current checkpoints (and their corresponding
// files) to stdout.
//
// Returns:
//  The number of checkpoints stored for this dir.
static int32_t List(CheckPointLogPtr cpt_log);

// Helper method to List. Prints all the parent/children
// lists for a given tree.
//
// Returns:
//
// - MEM_ERR: on a memory error
//
// - PRINT_ERR: if any other errors arise
//
// - The number of checkpoints in @tree otherwise.
static int32_t PrintTree(CpTreeNodePtr tree);

// Handles freeing all the tables and their contents.
static void FreeCheckPointLog(CheckPointLogPtr cpt_log);

#endif  // _CHECKPOINT_H_
