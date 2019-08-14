// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#include "checkpoint_filehandler.h"

int WriteCheckPointLog(CheckPointLogPtr cpt_log) {
  // TODO
  return WRITE_SUCCESS;
}

int ReadCheckPointLog(CheckPointLogPtr cpt_log) {
  struct stat;
  if (DEBUG) {
    printf("\t\tloading tables ...\n");
  }

  cpt_log->src_filehash_to_filename = MakeHashTable(INITIAL_BUCKET_COUNT);
  cpt_log->src_filehash_to_cptnames = MakeHashTable(INITIAL_BUCKET_COUNT);
  cpt_log->cpt_namehash_to_cptfilename = MakeHashTable(INITIAL_BUCKET_COUNT);

  if (cpt_log->src_filehash_to_filename == NULL ||
      cpt_log->src_filehash_to_cptnames == NULL ||
      cpt_log->cpt_namehash_to_cptfilename == NULL) {
    return READ_ERROR;
  }
  // if (stat(STORED_CPTS_FILE, &stat) == 0

  return READ_SUCCESS;
}

int WriteSrcToCheckpoint(char *src_filename, char *cpt_name) {
  FILE *cpt_file, *src_file;
  size_t dir_len = strlen(WORKING_DIR), name_len = strlen(cpt_name);
  char cpt_filename[dir_len + name_len + 2];
  strcpy(cpt_filename, WORKING_DIR);
  cpt_filename[dir_len] = '/';
  cpt_filename[dir_len + 1] = '\0';
  strcat(cpt_filename, cpt_name);

  if ((cpt_file = fopen(cpt_filename, "ab+")) == NULL) {
    fprintf(stderr,
            "\tError creating file %s%s.\n"\
            "\tAborting program now.\n", WORKING_DIR, cpt_filename);
    return -2;
  }

  if ((src_file = fopen(src_filename, "r")) == NULL) {
    fprintf(stderr,
            "\tError opening file %s.\n\tProgram will now be aborted.\n",
            src_filename);
    return -2;
  }

  char buffer[1024];
  size_t bytes;

  if (fseek(src_file, 0, SEEK_SET) != 0) {
    return -1;
  }
  if (fseek(cpt_file, 0, SEEK_SET) != 0) {
    return -1;
  }
  
  while (0 < (bytes = fread(buffer, 1, sizeof(buffer), src_file))) {
      fwrite(buffer, 1, bytes, cpt_file);
  }

  return 0;
}