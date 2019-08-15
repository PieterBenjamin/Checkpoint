// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#include "checkpoint.h"

#define VALID_COMMAND_COUNT 4
#define BUFFSIZE 1024  // Hopefully larger than will ever be necessary

static void CheckMacros() {
  assert(INVALID_COMMAND < 0);  // Must be neg. since an index is expected.
  assert(SETUP_SUCCESS != SETUP_DIR_ERROR);
  assert(SETUP_SUCCESS != SETUP_TAB_ERROR);
  assert(READ_SUCCESS != READ_ERROR);
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

  if ((setup = Setup(&cpt_log)) != SETUP_SUCCESS) {
    FreeCheckPointLog(&cpt_log);
    printf("ERROR[%d] in Setup, exiting now.\n", setup);
    return EXIT_FAILURE;
  }

  switch (res) {
    case 0:  // create
      CHECK_ARG_COUNT(4)
      CreateCheckpoint(argv[2], argv[3], &cpt_log);
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

  if ((res = WriteCheckPointLog(&cpt_log)) != WRITE_SUCCESS) {
    printf("Error %d writing tables. This dir is now considered corrupt.\n",
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
  DIR *dp;
  int status;

  if ((dp = opendir(WORKING_DIR)) != NULL) {  // Directory exists
    if (DEBUG) {
      printf("\tCheckpoint: working dir detected, loading tables . . .\n");
    }
    status = ReadCheckPointLog(cpt_log) == READ_SUCCESS ?
                                                SETUP_SUCCESS : SETUP_TAB_ERROR;
    closedir(dp);
    return status;
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

    return ReadCheckPointLog(cpt_log) == READ_SUCCESS ?
                                                SETUP_SUCCESS : SETUP_TAB_ERROR;
  } else {  // Some other error
    if (DEBUG) {
      printf("\topendir(\"%s\") resulted in errno: %d", WORKING_DIR, errno);
    }
    
    return SETUP_DIR_ERROR;
  }

  return SETUP_SUCCESS;
}

static int CreateCheckpoint(char *cpt_name,
                            char *src_filename,
                            CheckPointLogPtr cpt_log) {
  HashTabKey_t src_filename_hash = HashFunc((unsigned char *)src_filename,
                                            strlen(src_filename));
  HashTabKV kv, storage;
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
    if (AddCheckpointNewFile(cpt_name,
                             src_filename,
                             src_filename_hash,
                             cpt_log) != CREATE_CPT_SUCCESS) {
      return CREATE_CPT_ERROR;
    }
  } else {
    // This filename already has checkpoints, we should add
    // the new one to the tree.
    if (AddCheckpointExistingFile(cpt_name,
                                  src_filename,
                                  src_filename_hash,
                                  cpt_log) != CREATE_CPT_SUCCESS) {
      return CREATE_CPT_ERROR;
    }
  }

  // At this point, we can be certain two mappings exist:
  // 1. hash(src_filename) -> src_filename
  // 2. hash(src_filename) -> Tree of checkpoint names for the src file
  //
  // Now we must verify there is a mapping from hash(cpt_name) to the
  // name of a file containing the data for that cpt. We will name that
  // file cpt_name.
  HashTabKey_t cpt_filename_hash = HashFunc((unsigned char *)cpt_name,
                                            strlen(cpt_name));
  ATTEMPT((res = HTLookup(cpt_log->cpt_namehash_to_cptfilename,
                          cpt_filename_hash,
                          &storage)), -1, num_attempts)
  if (res == 0) {  // No checkpoint filename mapping exists for the checkpoint!
    if (WriteSrcToCheckpoint(src_filename, cpt_name) != 0) {  // I/O error
      return CREATE_CPT_ERROR;
    }

    // No I/O error - we can now update our mapping
    char *src_filename_copy;
    num_attempts = NUMBER_ATTMEPTS;
    ATTEMPT((src_filename_copy = malloc(sizeof(char) * (strlen(src_filename) + 1))),
            NULL,
            num_attempts)
    strcpy(src_filename_copy, src_filename);

    kv.key = src_filename_hash;
    kv.value = src_filename_copy;
    num_attempts = NUMBER_ATTMEPTS;
    ATTEMPT((HTInsert(cpt_log->cpt_namehash_to_cptfilename, kv, &storage)),
            0,
            num_attempts)
  } else if (res == 1) {  // The client is trying to overwrite data! Stop them!
    fprintf(stderr,
           "\tSorry, checkpoint name [%s] already exists. Try another.\n"\
           "\t(maybe %s2)\n", cpt_name, cpt_name);
    return CREATE_CPT_ERROR;
  }

  return CREATE_CPT_SUCCESS;
}

static int AddCheckpointExistingFile(char *cpt_name,
                                     char *src_filename,
                                     HashTabKey_t src_filename_hash,
                                     CheckPointLogPtr cpt_log) {
  HashTabKV keyval;
  ssize_t num_attempts = NUMBER_ATTMEPTS;
  CpTreeNode new_cp;
  int res;
  ATTEMPT((res = HTLookup(cpt_log->dir_tree, src_filename_hash, &keyval)),
            -1,
            num_attempts)
  if (res == 0) {  // This should not occur
    if(DEBUG) {
      printf("ERROR INVALID STATE: no mapping for file hash %x in"\
              "filename table, but mapping exists in dir_tree\n",
              (unsigned int)src_filename_hash);
    }
    return CREATE_CPT_ERROR;
  }

  num_attempts = NUMBER_ATTMEPTS;
  ATTEMPT((new_cp.children = MakeLinkedList()), NULL, num_attempts);
  new_cp.cpt_name = malloc(sizeof(char) * (strlen(cpt_name + 1)));
  if (new_cp.cpt_name == NULL) {
    if (DEBUG) {
      printf("Ran out of memory to hold %s\n", cpt_name);
    }
    FreeLinkedList(new_cp.children, &free);
    return MEM_ERR;
  }
  strcpy(new_cp.cpt_name, cpt_name);
  new_cp.parent_node = ((CpTreeNodePtr)keyval.value);
  return InsertCpTreeNode((CpTreeNodePtr)keyval.value, &new_cp) ==
                    INSERT_NODE_SUCCESS ? CREATE_CPT_SUCCESS : CREATE_CPT_ERROR;
}

static int AddCheckpointNewFile(char *cpt_name,
                                char *src_filename,
                                HashTabKey_t src_filename_hash,
                                CheckPointLogPtr cpt_log) {
  HashTabKV keyval, storage;
  int res;
  ssize_t num_attempts = NUMBER_ATTMEPTS;                                
  if (DEBUG) {
    printf("Storing new file %s with cp %s\n", src_filename, cpt_name);
  }
  keyval.key   = (HashTabKey_t)(src_filename_hash);
  // Add the mapping from source filename hash to source filename
  char *src_name_copy;
  ATTEMPT((src_name_copy = malloc(sizeof(char) * (strlen(src_filename) + 1))),
            NULL, num_attempts)
  strcpy(src_name_copy, src_filename);
  keyval.value = (HashTabVal_t)(src_name_copy);
  ATTEMPT((res = HTInsert(cpt_log->src_filehash_to_filename,
                          keyval,
                          &storage)), -1, num_attempts)
  PREEXISTING("\ta file name", src_filename, res)


  // Now add the mapping from the source file hash to the checkpoint
  // tree for that file.
  
  // Make space for a CpTreeNode
  CpTreeNodePtr new_tree;
  num_attempts = NUMBER_ATTMEPTS;
  ATTEMPT((new_tree = (CpTreeNodePtr)malloc(sizeof(CpTreeNode))),
          NULL,
          num_attempts)
  new_tree->parent_node = NULL;

  // Now attempt to make space on the heap for a checkpoint name.
  new_tree->cpt_name = malloc(sizeof(char) * (strlen(cpt_name) + 1));
  if (new_tree->cpt_name == NULL) {
    if (DEBUG) {
      printf("Ran out of memory to hold %s\n", cpt_name);
    }
    free(new_tree);
    return MEM_ERR;
  }
  strcpy(new_tree->cpt_name, cpt_name);

  // now allocate a linked list to keep track of the children
  new_tree->children = MakeLinkedList();
  if (new_tree->children == NULL) {
    if (DEBUG) {
      printf("Ran out of memory for a linkedlist\n");
    }
    free(new_tree);
    free(src_name_copy);
    return MEM_ERR;
  }

  keyval.value = (HashTabVal_t)(new_tree);

  // Attempt to add the new mapping from src_filename to the new
  // LinkedList which contains
  ATTEMPT((res = HTInsert(cpt_log->dir_tree,
                          keyval,
                          &storage)), -1, num_attempts)
  PREEXISTING("\ta LL of cpts", src_filename, res)
  
  // Store the recorded cpt for the source file
  keyval.value = (char *)malloc(sizeof(char) * (strlen(cpt_name) + 1));
  if (keyval.value == NULL) {
    if (DEBUG) {
      printf("Ran out of memory to hold %s\n", cpt_name);
    }
    return MEM_ERR;
  }
  strcpy(keyval.value, cpt_name);

  num_attempts = NUMBER_ATTMEPTS;
  ATTEMPT((HTInsert(cpt_log->src_filehash_to_cptname, keyval, &storage)), 
                    0,
                    num_attempts)
  return CREATE_CPT_SUCCESS;
}

static int SwapTo(char *cpt_name, CheckPointLogPtr cpt_log) {
  if (DEBUG) {
    printf("swapping to %s\n", cpt_name);
  }
  return SWAPTO_SUCCESS;
}

static int Delete(char *cpt_name, CheckPointLogPtr cpt_log) {
  if (DEBUG) {
    printf("deleting %s\n", cpt_name);
  }
  return DELETE_SUCCESS;
}

static size_t List(CheckPointLogPtr cpt_log) {
  size_t num_cpts = 0;
  if (DEBUG) {
    printf("\tlisting\n");
  }
  return num_cpts;
}

static void FreeCheckPointLog(CheckPointLogPtr cpt_log) {
  if (DEBUG) {
    printf("Freeing tables. num elements:\n"\
           "\tsrc_filehash_to_filename:    %d\n"\
           "\tsrc_filehash_to_cptname:     %d\n"\
           "\tcpt_namehash_to_cptfilename: %d\n"\
           "\tdir_tree:                    %d\n",
           HTSize(cpt_log->src_filehash_to_filename),
           HTSize(cpt_log->src_filehash_to_cptname),
           HTSize(cpt_log->cpt_namehash_to_cptfilename),
           HTSize(cpt_log->dir_tree));
  }
  FreeHashTable(cpt_log->src_filehash_to_filename, &free);
  FreeHashTable(cpt_log->src_filehash_to_cptname, &free);
  FreeHashTable(cpt_log->cpt_namehash_to_cptfilename, &free);
  FreeHashTable(cpt_log->dir_tree, &FreeCpTreeNode);
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
