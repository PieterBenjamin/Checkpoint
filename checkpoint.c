// Copyright 2019, Pieter Benjamin, pieterb@cs.washington.edu

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <search.h>

#define VALID_COMANDS {"create", "swapto", "create", "get", "delete"}
#define VALID_COMMAND_COUNT 5

// Prints usage to stderr
static void Usage(void);

// Checks that the supplied command is valid.
static bool IsValidCommand(char *command);

int main (int argc, char *argv[]) {
  if (argc != 3)
    Usage();
}

static bool IsValidCommand(char *command) {
  bool is_valid = false;
  int i;

  // Compare commmand to all valid commands
  for (i = 0; i < VALID_COMMAND_COUNT; i++) {

  }

  return is_valid;
}

static void Usage() {
  fprintf(stderr, "\nCheckpoint: copyright 2019, Pieter Benjamin\n");
  fprintf(stderr, "\nUsage: ./checkpoint <filename> <option>\n");
  fprintf(stderr, "Where <filename> is an absolute or relative pathway\n"\
                  "to the file you would like to checkpoint.\n\n"\
                  "options:\n"\
                  "\tcreate <checkpoint name>\n"\
                  "\tswapto <checkpoint name>\n"\
                  "\tget    <checkpoint name>\n"\
                  "\tdelete <checkpoint name>\n\n"\
                  "PLEASE NOTE: delete is irreversible.\n\n");  
                  // TODO: make delete reversible

  exit(EXIT_FAILURE);
}