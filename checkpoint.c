// Copyright 2019, Pieter Benjamin, pieterb@cs.washington.edu

#include "checkpoint.h"

const char *valid_commands[] = {"create", "swapto", "delete", "list"};
#ifdef DEBUG_
  #define DEBUG true
#else
  #define DEBUG false
#endif
#define VALID_COMMAND_COUNT 4
#define BUFFSIZE 1024  // Hopefully larger than will ever be necessary
#define INITIAL_BUCKET_COUNT 10
#define CHECK_ARG_COUNT(c) \
  if (argc != c) {\
    fprintf(stderr, "invalid # of commands: got %d, expected %d\n", argc, c);\
    return EXIT_FAILURE;\
  }

static void CheckMacros() {
  assert(INVALID_COMMAND < 0);  // Must be neg. since an index is expected.
  assert(SETUP_OK != SETUP_DIR_ERROR);
  assert(SETUP_OK != SETUP_TAB_ERROR);
  assert(LOAD_OK != LOAD_BAD);
}

int main (int argc, char *argv[]) {
  HashTable cpts;
  int res, setup;
  if (argc > 4 || argc < 2) {  // check valid use
    Usage();
  }

  CheckMacros();

  if ((res = IsValidCommand(argv[1])) == INVALID_COMMAND) {
    fprintf(stderr, "invalid command: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  cpts = MakeHashTable(INITIAL_BUCKET_COUNT);
  if ((setup = Setup(cpts)) != SETUP_OK) {
    printf("ERROR[%d] in Setup, exiting now.\n", setup);
    return EXIT_FAILURE;
  }

  switch (res) {
    case 0:  // create
      CHECK_ARG_COUNT(4)
      Create(&argv[2], &argv[3], cpts);
      break;
    case 1:  // swapto
      CHECK_ARG_COUNT(3)
      SwapTo(&argv[2], cpts);
      break;
    case 2:  // delete
      CHECK_ARG_COUNT(3)
      Delete(&argv[2], cpts);
      break;
    case 3:  // list
      CHECK_ARG_COUNT(2)
      if (List(cpts) == 0) {
        printf("There are no stored checkpoints for this dir.\n");
      }
      break;
    default: 
      fprintf(stderr, "unknown result %d\n", res);
      return EXIT_FAILURE;
  }

  FreeHashTable(cpts, &free);
  return EXIT_SUCCESS;
}

static int Setup(HashTable cpts) {
  if (DEBUG) {
    printf("Setting up working dir (%s)\n", WORKING_DIR);
  }
  DIR* working_dir;
  int status;

  working_dir = opendir(WORKING_DIR);
  if (working_dir) {  // Directory exists
    if (DEBUG) {
      printf("\tdir [%s] detected, loading table now ...\n", WORKING_DIR);
    }
    return LoadTable(cpts);
  } else if (errno == ENOENT) {  // Directory does not exist
    if (DEBUG) {
      printf("\tdir nonexistent, making right now ...\n");
    }
    status = mkdir(WORKING_DIR, S_IRWXU);
    if (status != 0) {
      if (DEBUG) {
        printf("\terror [%d] making working directory\n", errno);
      }
      return SETUP_DIR_ERROR;
    }
  } else {  // Some other error
    if (DEBUG) {
      printf("\topendir(\"%s\") resulted in errno: %d", WORKING_DIR, errno);
    }
    
    return SETUP_DIR_ERROR;
  }

  return SETUP_OK;
}

static int LoadTable(HashTable cpts) {
  struct stat;
  if (DEBUG) {
    printf("\t\tloading table ...\n");
  }
  // if (stat(STORED_CPTS_FILE, &stat) == 0

  return LOAD_OK;
}

static int Create(char **checkpointname, char **filename, HashTable cpts) {
  if (DEBUG) {
    printf("creating checkpoint %s for %s\n", *checkpointname, *filename);
  }
  return CREATE_OK;
}

static int SwapTo(char **checkpointname, HashTable cpts) {
  if (DEBUG) {
    printf("swapping to %s\n", *checkpointname);
  }
  return SWAPTO_OK;
}

static int Delete(char **checkpointname, HashTable cpts) {
  if (DEBUG) {
    printf("deleting %s\n", *checkpointname);
  }
  return DELETE_OK;
}

size_t List(HashTable cpts) {
  size_t num_cpts = 0;
  if (DEBUG) {
    printf("listing\n");
  }
  return num_cpts;
}


static int IsValidCommand(char *command) {
  int i;

  // Compare commmand to all valid commands
  for (i = 0; i < VALID_COMMAND_COUNT; i++) {
    if (!strcmp(command, valid_commands[i]))
      return i;
  }

  return INVALID_COMMAND;
}

static void Usage() {
  fprintf(stderr, "\nCheckpoint: copyright 2019, Pieter Benjamin\n");
  fprintf(stderr, "\nUsage: ./Checkpoint <filename> <option>\n");
  fprintf(stderr, "Where <filename> is an absolute or relative pathway\n"\
                  "to the file you would like to checkpoint.\n\n"\
                  "options:\n"\
                  "\tcreate <checkpoint name> <file name>\n"\
                  "\tswapto <checkpoint name>\n"\
                  "\tdelete <checkpoint name>\n"\
                  "\tlist   (lists all Checkpoints for the current dir)\n\n"\
                  "PLEASE NOTE: delete is irreversible.\n\n");  
                  // TODO: make delete reversible

  exit(EXIT_FAILURE);
}