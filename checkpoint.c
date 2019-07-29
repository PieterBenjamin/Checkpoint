// Copyright 2019, Pieter Benjamin, pieterb@cs.washington.edu

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <search.h>
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

int main (int argc, char *argv[]) {
  HashTable cpts;
  int res, setup;
  if (argc > 4 || argc < 2)  // check valid use
    Usage();

  res = IsValidCommand(argv[1]);
  if (res == -1) {
    fprintf(stderr, "invalid command: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  cpts = AllocateHashTable(INITIAL_BUCKET_COUNT);
  if ((setup = Setup(cpts)) != SETUP_OK) {
    printf("ERROR[%d] in Setup, exiting now.\n", setup);
    return EXIT_FAILURE;
  }

  switch (res) {
    case 0:  // create
      CHECK_ARG_COUNT(4)
      create(&argv[2], &argv[3], cpts);
      break;
    case 1:  // swapto
      CHECK_ARG_COUNT(3)
      swapto(&argv[2], cpts);
      break;
    case 2:  // delete
      CHECK_ARG_COUNT(3)
      delete(&argv[2], cpts);
      break;
    case 3:  // list
      CHECK_ARG_COUNT(2)
      if (list(cpts) == 0) {
        printf("There are no stored checkpoints for this dir.\n");
      }
      break;
    default: 
      fprintf(stderr, "unknown result %d\n", res);
      return EXIT_FAILURE;
  }
}

static int Setup(HashTable cpts) {
  if (DEBUG) {
    printf("Setting up working dir (%s)\n", WORKING_DIR);
  }
  return SETUP_OK;
}

static int create(char **checkpointname, char **filename, HashTable cpts) {
  if (DEBUG) {
    printf("creating checkpoint %s for %s\n", *checkpointname, *filename);
  }
  return CREATE_OK;
}

static int swapto(char **checkpointname, HashTable cpts) {
  if (DEBUG) {
    printf("swapping to %s\n", *checkpointname);
  }
  return SWAPTO_OK;
}

static int delete(char **checkpointname, HashTable cpts) {
  if (DEBUG) {
    printf("deleting %s\n", *checkpointname);
  }
  return DELETE_OK;
}

size_t list(HashTable cpts) {
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

  return -1;
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