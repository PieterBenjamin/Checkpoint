// Copyright 2019, Pieter Benjamin, pieterb@cs.washington.edu

#include "checkpoint.h"

#ifdef DEBUG_
  #define DEBUG true
#else
  #define DEBUG false
#endif
#define VALID_COMMAND_COUNT 4
#define BUFFSIZE 1024  // Hopefully larger than will ever be necessary
#define INITIAL_BUCKET_COUNT 10

static void CheckMacros() {
  assert(INVALID_COMMAND < 0);  // Must be neg. since an index is expected.
  assert(SETUP_OK != SETUP_DIR_ERROR);
  assert(SETUP_OK != SETUP_TAB_ERROR);
  assert(LOAD_OK != LOAD_BAD);
}

int main (int argc, char *argv[]) {
  CheckPointLog cpt_log;
  int res, setup;
  if (argc > 4 || argc < 2) {  // check valid use
    Usage();
  }

  CheckMacros();

  // The return value of IsValidCommand will be treated as an
  // index in a switch statement to determine what to do.
  if ((res = IsValidCommand(argv[1])) == INVALID_COMMAND) {
    fprintf(stderr, "invalid command: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  if ((setup = Setup(&cpt_log)) != SETUP_OK) {
    printf("ERROR[%d] in Setup, exiting now.\n", setup);
    return EXIT_FAILURE;
  }

  switch (res) {
    case 0:  // create
      CHECK_ARG_COUNT(4)
      CreateCheckpoint(argv[2],
                       argv[3],
                       &cpt_log);
      break;
    case 1:  // swapto
      CHECK_ARG_COUNT(3)
      SwapTo(argv[2], &cpt_log);
      break;
    case 2:  // delete
      CHECK_ARG_COUNT(3)
      Delete(argv[2], &cpt_log);
      break;
    case 3:  // list
      CHECK_ARG_COUNT(2)
      if (List(&cpt_log) == 0) {
        printf("There are no stored checkpoints for this dir.\n");
      }
      break;
    default: 
      fprintf(stderr, "unknown result %d\n", res);
      return EXIT_FAILURE;
  }

  if ((res = WriteTables(&cpt_log)) != WRITE_OK) {
    printf("Error %d writing tables. This dir is now considered corrupted.\n",
           res);
    FreeCheckPointLog(&cpt_log);
    return EXIT_FAILURE;
  }
  FreeCheckPointLog(&cpt_log);
  return EXIT_SUCCESS;
}

static int Setup(CheckPointLogPtr cpt_log) {
  if (DEBUG) {
    printf("Setting up working dir . . .\n");
  }
  int status;

  if (opendir(WORKING_DIR) != NULL) {  // Directory exists
    if (DEBUG) {
      printf("\tCheckpoint: working dir detected, loading tables . . .\n");
    }

    return LoadTables(cpt_log) == LOAD_OK ? SETUP_OK : SETUP_TAB_ERROR;
  } else if (errno == ENOENT) {  // Directory does not exist
    if (DEBUG) {
      printf("\tworking dir nonexistent, making right now . . .\n");
    }
    status = mkdir(WORKING_DIR, S_IRWXU);
    if (status != 0) {
      if (DEBUG) {
        printf("\terror [%d] making working directory\n", errno);
      }
      return SETUP_DIR_ERROR;
    }

    return LoadTables(cpt_log) == LOAD_OK ? SETUP_OK : SETUP_TAB_ERROR;
  } else {  // Some other error
    if (DEBUG) {
      printf("\topendir(\"%s\") resulted in errno: %d", WORKING_DIR, errno);
    }
    
    return SETUP_DIR_ERROR;
  }

  return SETUP_OK;
}

static int WriteTables(CheckPointLogPtr cpt_log) {
  // TODO
  return WRITE_OK;
}

static int LoadTables(CheckPointLogPtr cpt_log) {
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
    return LOAD_BAD;
  }
  // if (stat(STORED_CPTS_FILE, &stat) == 0

  return LOAD_OK;
}

static int CreateCheckpoint(char *cpt_name,
                            char *src_filename,
                            CheckPointLogPtr cpt_log) {
  HashTabKey_t src_filename_hash = HashFunc((unsigned char *)src_filename,
                                            strlen(src_filename));
  HashTabKV keyval, storage;
  LinkedList cpts;
  int res;
  ssize_t num_attempts = NUMBER_ATTMEPTS;

  if (DEBUG) {
    printf("\tcreating checkpoint %s for %s\n", cpt_name, src_filename);
  }
  
  // Is there a mapping from hash(src_filename)? If there is not,
  // we will also assume there is no mapping from the hash to a
  // LinkedList of checkpoints.
  ATTEMPT((res = HTLookup(cpt_log->src_filehash_to_filename,
                          src_filename_hash,
                          &storage)), -1, num_attempts)

  if (res == 0) {  // This filename has not yet had a checkpoint created!
    keyval.key   = (HashTabKey_t)(src_filename_hash);
    // Add the mapping from source filename hash to source filename
    char *src_name_copy, *cpt_name_copy;
    ATTEMPT((src_name_copy = malloc(sizeof(char) * (strlen(src_filename + 1)))),
             NULL, num_attempts)
    strcpy(src_name_copy, src_filename);
    keyval.value = (HashTabVal_t)(src_name_copy);
    ATTEMPT((res = HTInsert(cpt_log->src_filehash_to_filename,
                            keyval,
                            &storage)), -1, num_attempts)
    PREEXISTING("\ta file name", src_filename, res)


    // Now add the mapping from the source file hash to the checkpoint
    // names stored for that file.
    // Attempt to allocate a Linked List.
    ATTEMPT((cpts = MakeLinkedList()), NULL, num_attempts)
    
    // Attempt to make space on the heap for a checkpoint name.
    ATTEMPT((cpt_name_copy = malloc(sizeof(char) * (strlen(cpt_name + 1)))),
             NULL, num_attempts)
    strcpy(cpt_name_copy, cpt_name);
    
    // Add the pointer to the checkpoint name into the linked list
    ATTEMPT((LLPush(cpts, (LinkedListPayload)(cpt_name_copy))),
            false,
            num_attempts)
    keyval.value = (HashTabVal_t)(cpts);

    // Attempt to add the new mapping from src_filename to the new
    // LinkedList which contains
    ATTEMPT((res = HTInsert(cpt_log->src_filehash_to_cptnames,
                            keyval,
                            &storage)), -1, num_attempts)
    PREEXISTING("\ta LL of cpts", src_filename, res)
  }

  // At this point, we can be certain two mappings exist:
  // 1. hash(src_filename) -> src_filename
  // 2. hash(src_filename) -> LL of checkpoint names for the src file
  //
  // Now we must verify there is a mapping from hash(cpt_name) to the
  // name of a specific file.
  HashTabKey_t cpt_filename_hash = HashFunc((unsigned char *)cpt_name,
                                            strlen(cpt_name));
  ATTEMPT((res = HTLookup(cpt_log->cpt_namehash_to_cptfilename,
                          cpt_filename_hash,
                          &storage)), -1, num_attempts)
  if (res == 0) {  // No checkpoint filename mapping exists for the checkpoint!
    if (WriteSrcToCheckpoint(src_filename, cpt_name) != 0) {  // I/O error
      return CREATE_BAD;
    }

    // No I/O error - we can now update our mapping 
  } else if (res == 1) {  // The client is trying to overwrite data! Stop them!
    fprintf(stderr,
           "\tSorry, checkpoint name [%s] already exists. Try another.\n"\
           "\t(maybe %s2)\n", cpt_name, cpt_name);
    return CREATE_BAD;
  }

  return CREATE_OK;
}

static int WriteSrcToCheckpoint(char *src_filename, char *cpt_name) {
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

static int SwapTo(char *cpt_name, CheckPointLogPtr cpt_log) {
  if (DEBUG) {
    printf("swapping to %s\n", cpt_name);
  }
  return SWAPTO_OK;
}

static int Delete(char *cpt_name, CheckPointLogPtr cpt_log) {
  if (DEBUG) {
    printf("deleting %s\n", cpt_name);
  }
  return DELETE_OK;
}

static size_t List(CheckPointLogPtr cpt_log) {
  size_t num_cpts = 0;
  if (DEBUG) {
    printf("\tlisting\n");
  }
  return num_cpts;
}

static void FreeCheckPointLog(CheckPointLogPtr cpt_log) {
  // The first two frees are easy, the keys are simple hashes,
  // and the values are single pointers to char arrays on the heap.
  FreeHashTable(cpt_log->src_filehash_to_filename, &free);
  FreeHashTable(cpt_log->cpt_namehash_to_cptfilename, &free);
  // The second free is a little more complex, since every bucket
  // points to a LL which sits on the heap, with pointers to strings
  // on the heap. This will take a little more work.
  HTIter ht_iter = MakeHTIter(cpt_log->src_filehash_to_cptnames);
  HashTabKV kv;

  kv.value = NULL;
  while (!HTIterValid(ht_iter)) {
    if (!HTIterKV(ht_iter, &kv)) {
      continue;
    }

    // The LL is free of pointers to the heap, which we want to get rid of.
    if (kv.value != NULL) {
      FreeLinkedList((LinkedList)(kv.value), &free);
    }
    kv.value = NULL;
    HTIncrementIter(ht_iter);
  }
  
  DiscardHTIter(ht_iter);
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
  fprintf(stderr, "\nUsage: ./Checkpoint <option>\n");
  fprintf(stderr, "Where <filename> is an absolute or relative pathway\n"\
                  "to the file you would like to checkpoint.\n\n"\
                  "options:\n"\
                  "\tcreate <checkpoint name> <file name>\n"\
                  "\tswapto <checkpoint name>\n"\
                  "\tdelete <checkpoint name>\n"\
                  "\tlist   (lists all Checkpoints for the current dir)\n\n"\
                  "PLEASE NOTE:"\
                  "\t- Checkpoints will be stored in files labeled with\n"\
                  "\t  the name of the checkpoint you provide. If you\n"\
                  "\t  provide the name of a preexisting file, it will\n"\
                  "\t  NOT be overwritten.\n\n"
                  "\t- Delete is irreversible.\n\n");  
                  // TODO: make delete reversible

  exit(EXIT_FAILURE);
}