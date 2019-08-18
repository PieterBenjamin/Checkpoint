// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

// This file contains all the macros and imports
// needed for all checkpoint files.
#ifndef _MACROS_H_
#define _MACROS_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>

#define STORED_CPTS_FILE "./.cpt_bkpng/strd_cpts"

// ********************************
// TAKE CARE THAT THE DIRS MATCH
// IN THE NEXT TWO MACROS
#define WORKING_DIR "./.cpt_"
#define CP_LOG_FILE "./.cpt_/CpLog"
// ********************************

#define NUMBER_ATTEMPTS 20 // Number of times to try again on an out-of-mem err
#ifdef DEBUG_
  #define DEBUG true
#else
  #define DEBUG false
#endif

#define INITIAL_BUCKET_COUNT 10
// Various error/info codes
#define MEM_ERR -333

#define WRITE_SUCCESS 0

#define READ_SUCCESS 0
#define READ_ERROR -1

// Some actions may have memory issues. In the case that
// such an error occurs, we want the option to repeat the
// action a certain number of times in further attempts
// to get that sweet sweet heap memory.
#define ATTEMPT(action, failure, num_attempts_left)\
  while (action == failure) {\
    if (!num_attempts_left) { return MEM_ERR; }\
    num_attempts_left--;\
  }\
  num_attempts_left = NUMBER_ATTEMPTS;
// Used when an implication that should have been true
// ends up false.
#define PREEXISTING(msg, name, res)\
  if (res == 2) {\
    if (DEBUG) {\
      printf("\tConflicting state:\n"\
              "\t%s was not supposed to have a mapping to"\
              msg, src_filename);\
    }\
    return CREATE_CPT_ERROR;\
  }

#endif  // _MACROS_H_

