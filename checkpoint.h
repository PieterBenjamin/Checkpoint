// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#include "checkpoint_filehandler.h"

#define INVALID_COMMAND -1
#define SETUP_SUCCESS 0
#define SETUP_DIR_ERROR 1
#define SETUP_TAB_ERROR 2

#define CREATE_CPT_SUCCESS 0
#define CREATE_CPT_ERROR -1

#define SWAPTO_SUCCESS 0
#define DELETE_SUCCESS 0

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
//  SETUP_SUCCESS - if all went well, and an error code otherwise.
static int Setup(CheckPointLogPtr cpt_log);

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
//  CREATE_CPT_SUCCESS - if all went well, and an error code otherwise.
static int CreateCheckpoint(char *cpt_name,
                            char *filename,
                            CheckPointLogPtr cpt_log);

// Changes to the checkpoint @cpt_name. Note that this will
// overwrite/delete the current copy of the file for the
// given checkpoint name, and replace it with the version
// saved for the checkpoint.
//
// Returns:
//  SWAPTO_SUCCESS if all went well, and SWAPTO_* error code otherwise.
static int SwapTo(char *cpt_name, CheckPointLogPtr cpt_log);

// Deletes the checkpoint @cpt_name (modifies cpt_log).
// Also removes any saved info on @checkpointname from the working dir.
//
// Returns:
//  DELETE_SUCCESS if all went well, and DELETE_* error code otherwise.
static int Delete(char *cpt_name, CheckPointLogPtr cpt_log);

// Prints a list of all current checkpoints (and their corresponding
// files) to stdout.
//
// Returns:
//  The number of checkpoints stored for this dir.
static size_t List(CheckPointLogPtr cpt_log);

// Handles freeing all the tables and their contents.
static void FreeCheckPointLog(CheckPointLogPtr cpt_log);
