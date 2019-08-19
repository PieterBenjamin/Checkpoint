// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#include "checkpoint.h"

#define VALID_COMMAND_COUNT 5
#define BUFFSIZE 1024  // Hopefully larger than will ever be necessary

static void CheckMacros() {
  assert(INVALID_COMMAND < 0);  // Must be neg. since an index is expected.
  assert(SETUP_SUCCESS != SETUP_DIR_ERROR);
  assert(SETUP_SUCCESS != SETUP_TAB_ERROR);
  assert(READ_SUCCESS != READ_ERROR);

  assert(FILE_WRITE_ERR < 0);
}

int32_t main (int32_t argc, char *argv[]) {
  CheckPointLog cpt_log;
  int32_t res, setup;
  if (argc > 4 || argc < 2) {  // check valid use
    Usage();
  }

  CheckMacros();

  // The return value of IsValidCommand will be treated as an
  // index in a switch statement to determine what to do.
  if ((res = IsValidCommand(argv[1])) == INVALID_COMMAND) {
    fprintf(stderr, "invalid command: %s\n", argv[1]);
    fprintf(stderr, "Valid commands are %s, %s, %s, and %s.\n",
            valid_commands[0], valid_commands[1], valid_commands[2],
            valid_commands[3]);
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
    case 1:  // back
      CHECK_ARG_COUNT(3)
      Back(argv[2], &cpt_log);
      break;
    case 2:  // swapto
      CHECK_ARG_COUNT(4)
      SwapTo(argv[2], argv[3], &cpt_log);
      break;
    case 3:  // delete
      CHECK_ARG_COUNT(3)
      Delete(argv[2], &cpt_log);
      break;
    case 4:  // list
      CHECK_ARG_COUNT(2)
      res = List(&cpt_log);
      if (res == MEM_ERR || res == LIST_ERR) {
        FreeCheckPointLog(&cpt_log);
        return EXIT_FAILURE;
      }
      if (res == 0) {
        printf("There are no saved checkpoints for this dir.\n");
      }
      break;
    default: 
      fprintf(stderr, "unknown result %d\n", res);
      return EXIT_FAILURE;
  }

  if (WriteCheckPointLog(&cpt_log) == FILE_WRITE_ERR) {
    printf("Error writing tables. This dir is now considered corrupt.\n");
    FreeCheckPointLog(&cpt_log);
    return EXIT_FAILURE;
  }

  FreeCheckPointLog(&cpt_log);
  return EXIT_SUCCESS;
}

static int32_t Setup(CheckPointLogPtr cpt_log) {
  if (DEBUG) {
    printf("Setting up working dir . . .\n");
  }
  DIR *dp;
  int32_t status;

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

static int32_t CreateCheckpoint(char *src_filename,
                                char *cpt_name,
                                CheckPointLogPtr cpt_log) {
  HashTabKey_t src_filename_hash = HashFunc((unsigned char *)src_filename,
                                            strlen(src_filename));
  HashTabKV kv, storage;
  int32_t res;
  uint32_t num_attempts = NUMBER_ATTEMPTS;


  // Update mapping from cpt_name_hash to cpt_file,
  // and update the mapping from src_filename to cpt_name.
  HashTabKey_t cpt_filename_hash = HashFunc((unsigned char *)cpt_name,
                                              strlen(cpt_name));
  if (DEBUG) {
    printf("\tcreating checkpoint  %s for %s\n", cpt_name, src_filename);
  }
  
  // Is there a mapping from hash(src_filename)? If there is not,
  // we will also assume there is no mapping from the hash to a
  // LinkedList of checkpoints.
  ATTEMPT((res = HTLookup(cpt_log->src_filehash_to_filename,
                          src_filename_hash,
                          &storage)), -1, num_attempts)

  if (res == 0) {  // This filename has not yet had a checkpoint  created!
    if (AddCheckpointNewFile(cpt_name,
                             src_filename,
                             src_filename_hash,
                             cpt_log) != CREATE_CPT_SUCCESS) {
      return CREATE_CPT_ERROR;
    }
  } else {
    // This filename already has checkpoints, we should add
    // the new one to the tree.
    // Specifically, we want this to be a new child of the current
    // checkpoint  for src_filename.
    if (AddCheckpointExistingFile(cpt_name,
                                  src_filename,
                                  src_filename_hash,
                                  cpt_log) != CREATE_CPT_SUCCESS) {
      return CREATE_CPT_ERROR;
    }
  }

  ATTEMPT((res = HTLookup(cpt_log->cpt_namehash_to_cptfilename,
                          cpt_filename_hash,
                          &storage)), -1, num_attempts)
  if (res == 0) {  // No checkpoint  filename mapping exists for the checkpoint!
    if (WriteSrcCheckpoint(src_filename, cpt_name, true) != 0) {  // I/O error
      return CREATE_CPT_ERROR;
    }

    // No I/O error - we can now update our mapping
    num_attempts = NUMBER_ATTEMPTS;
    char *cpt_name_copy1, *cpt_name_copy2;
    ATTEMPT((cpt_name_copy1 = malloc((sizeof(char) * strlen(cpt_name)) + 1)),
             NULL,
             num_attempts)
    num_attempts = NUMBER_ATTEMPTS;
    strcpy(cpt_name_copy1, cpt_name);
    kv.key = src_filename_hash;
    kv.value = cpt_name_copy1;
    ATTEMPT((HTInsert(cpt_log->src_filehash_to_cptname, kv, &storage)), 0 , num_attempts)
  
    ATTEMPT((cpt_name_copy2 = malloc((sizeof(char) * strlen(cpt_name)) + 1)),
             NULL,
             num_attempts)
    num_attempts = NUMBER_ATTEMPTS;
    strcpy(cpt_name_copy2, cpt_name);
    kv.key = cpt_filename_hash;
    kv.value = cpt_name_copy2;
    num_attempts = NUMBER_ATTEMPTS;
    ATTEMPT((HTInsert(cpt_log->cpt_namehash_to_cptfilename, kv, &storage)),
            0,
            num_attempts)
  } else if (res == 1) {  // The client is trying to overwrite data! Stop them!
    fprintf(stderr,
           "\tSorry, checkpoint  name [%s] already exists. Try another.\n"\
           "\t(maybe %s2)\n", cpt_name, cpt_name);
    return CREATE_CPT_ERROR;
  }

  return CREATE_CPT_SUCCESS;
}

static int32_t AddCheckpointExistingFile(char *cpt_name,
                                         char *src_filename,
                                         HashTabKey_t src_filename_hash,
                                         CheckPointLogPtr cpt_log) {
  // Here, we are going to need to lookup a lot. We want to
  // add the new cpt_name as a child of the src_filename's
  // current checkpoint.
  HashTabKV storage;
  CpTreeNodePtr root_node, parent_node, new_node;
  uint32_t num_attempts = NUMBER_ATTEMPTS;
  int32_t res;

  if (DEBUG) { printf("adding cp %s to tree for %s\n", cpt_name, src_filename); }
  // Here we are getting the root node for src_filename
  ATTEMPT((res = HTLookup(cpt_log->dir_tree, src_filename_hash, &storage)),
          -1,
          num_attempts)
  if (res == 0) {
    if (DEBUG) {
      printf("STATE ERROR: no mapping from filename hash to root node\n");
    }
    return CREATE_CPT_ERROR;
  }
  root_node = (CpTreeNodePtr)storage.value;

   // Now we need to lookup the current checkpoint  for the src file
   num_attempts = NUMBER_ATTEMPTS;
   ATTEMPT((res = HTLookup(cpt_log->src_filehash_to_cptname,
                           src_filename_hash,
                           &storage)),
           -1,
           num_attempts)
  if (res == 0) {
    if (DEBUG) {
      printf("STATE ERROR: no mapping from filename hash to curr checkpoint\n");
    }
    return CREATE_CPT_ERROR;
  }

  if (DEBUG) { printf("looking for cpt %s\n", storage.value); }
  // Now, starting from the root node, we need to go down until we find the
  // node with the same checkpoint  name that the src file was last saved at.
  if (FindCpt(root_node, storage.value, &parent_node) != FIND_CPT_SUCCESS) {
    if (DEBUG) {
      printf("could not find parent node %s for checkpoint  %s, file %s\n",
             storage.value,
             cpt_name,
             src_filename);
    }
    return CREATE_CPT_ERROR;
  }

  // Let's make space for the new node
  CreateCpTreeNode(cpt_name, parent_node, &new_node);

  return InsertCpTreeNode(parent_node, new_node) == INSERT_NODE_SUCCESS ?
                                          CREATE_CPT_SUCCESS : CREATE_CPT_ERROR;
}

static int32_t AddCheckpointNewFile(char *cpt_name,
                                    char *src_filename,
                                    HashTabKey_t src_filename_hash,
                                    CheckPointLogPtr cpt_log) {
  HashTabKV keyval, storage;
  int32_t res;
  uint32_t num_attempts = NUMBER_ATTEMPTS;                                
  if (DEBUG) {
    printf("Storing new file %s with cp %s\n", src_filename, cpt_name);
  }
  keyval.key = (HashTabKey_t)(src_filename_hash);
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
  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((new_tree = (CpTreeNodePtr)malloc(sizeof(CpTreeNode))),
          NULL,
          num_attempts)
  new_tree->parent_node = NULL;

  // Now attempt to make space on the heap for a checkpoint  name.
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

  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((HTInsert(cpt_log->src_filehash_to_cptname, keyval, &storage)), 
                    0,
                    num_attempts)
  return CREATE_CPT_SUCCESS;
}

static int32_t Back(char *src_filename, CheckPointLogPtr cpt_log) {
  HashTabKey_t key = HashFunc(src_filename, strlen(src_filename));
  HashTabKV storage;
  if (HTLookup(cpt_log->src_filehash_to_filename, key, &storage) == 0) {
    printf("Sorry, %s is not currently being tracked.\n", src_filename);
    return BACK_ERROR;
  }

  // Get the current cp name
  char *cpt_name;
  storage.value = NULL;
  HTLookup(cpt_log->src_filehash_to_cptname, key, &storage);
  if (storage.value == NULL) { return BACK_ERROR; }

  cpt_name = storage.value;

  storage.value = NULL;
  HTLookup(cpt_log->dir_tree, key, &storage);
  if (storage.value == NULL) { return BACK_ERROR; }

  CpTreeNodePtr target_node;
  if(FindCpt(storage.value, cpt_name, &target_node) != FIND_CPT_SUCCESS) {
    return BACK_ERROR;
  }

  if (target_node->parent_node == NULL) {
    printf("%s is the root checkpoint. Cannot go further back\n", cpt_name);
    return BACK_SUCCESS;
  }

  char *cpt_name_copy;
  int32_t num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((cpt_name_copy = malloc(sizeof(char) * (strlen(cpt_name) + 1))), NULL, num_attempts)  
  strcpy(cpt_name_copy, target_node->parent_node->cpt_name);
  
  HashTabKV kv = {key, cpt_name_copy};
  num_attempts = 20;
  HTInsert(cpt_log->src_filehash_to_cptname, kv, &storage);
  WriteSrcCheckpoint(src_filename, cpt_name_copy, false);
  return BACK_SUCCESS;
}

static int32_t SwapTo(char *src_filename, char *cpt_name, CheckPointLogPtr cpt_log) {
  if (DEBUG) {
    printf("swapping to %s\n", cpt_name);
  }

  HashTabKV kv, storage;
  HashTabKey_t key = HashFunc(cpt_name, strlen(cpt_name));

  if (HTLookup(cpt_log->cpt_namehash_to_cptfilename, key, &storage) == 0) {
    printf("Sorry, %s isn't a valid checkpoint name.\n", cpt_name);
  }

  kv.key = HashFunc(src_filename, strlen(src_filename));
  kv.value = malloc(sizeof(char) *(strlen(storage.value) + 1));
  strcpy(kv.value, storage.value);
  if (HTInsert(cpt_log->src_filehash_to_cptname, kv, &storage) != 2) {
    return SWAPTO_ERROR;
  }

  if (WriteSrcCheckpoint(src_filename, cpt_name, false) != FILE_WRITE_SUCCESS) {
    return SWAPTO_ERROR;
  } 

  return SWAPTO_SUCCESS;
}

static int32_t Delete(char *src_filename, CheckPointLogPtr cpt_log) {
  if (DEBUG) {
    printf("deleting %s\n", src_filename);
  }

  int32_t res, num_attempts;
  HashTabKV storage;
  HashTabKey_t key = HashFunc(src_filename, strlen(src_filename));
  if (HTLookup(cpt_log->src_filehash_to_filename, key, &storage) == 0) {
    printf("Sorry, %s is not currently being tracked.\n", src_filename);
    return DELETE_SUCCESS;
  }

  // Delete all the mappings.
  // The first two are easy, just remove the mappings.
  HTRemove(cpt_log->src_filehash_to_filename, key, &storage);
  HTRemove(cpt_log->src_filehash_to_cptname, key, &storage);

  // The last two are related - we must free all the mappings of cp name hashes
  // before we free the checkpoint tree, or else we will maintain information
  // we don't care about.
  storage.value = NULL;
  HTRemove(cpt_log->dir_tree, key, &storage);
  FreeTreeCpHash(cpt_log, storage.value);
  FreeCpTreeNode(storage.value);

  return DELETE_SUCCESS;
}

static int32_t FreeTreeCpHash(CheckPointLogPtr cpt_log, CpTreeNodePtr curr_node) {
  if (curr_node == NULL) {
    return 0;
  }

  // Remove the mapping and free the string.
  HashTabKey_t key = HashFunc(curr_node->cpt_name, strlen(curr_node->cpt_name));
  HashTabKV storage;
  storage.value = NULL;
  HTRemove(cpt_log->cpt_namehash_to_cptfilename, key, &storage);
  free(storage.value);
  
  // Do the same for all children.
  if (curr_node->children != NULL) {
    int32_t num_children = LLSize(curr_node->children), num_attempts = NUMBER_ATTEMPTS;
    if (num_children > 0) {
      LLIter it;
      CpTreeNodePtr curr_child;
      ATTEMPT((it = LLGetIter(curr_node->children, 0)), NULL, num_attempts);

      for (int32_t i = 0; i < num_children; i++) {
        LLIterPayload(it, &curr_child);
        FreeTreeCpHash(cpt_log, curr_child);
        LLIterAdvance(it);
      }

      LLIterFree(it);
    }
  }

  return 0;
}

static int32_t List(CheckPointLogPtr cpt_log) {
  int32_t num_cpts = 0, num_attempts, num_files, i, res;
  HTIter it;
  HashTabKV f_name, cpt_tree, cptname;

  // We'll be printing a list of the stored checkpoints on a per-file basis.
  // The format is as follows:
  // scrc_filename_1 (curr_cpt_name)
  //    init: cptname
  //    parent_cpt: child1, child2, childn
  //    . . .
  //    cptname_n: children
  // . . .
  // src_filename_n (curr_cpt_name)
  //    . . .
  // Note that children will be listed after their parents, and those
  // same children will be listed on a new line if they in turn have
  // children

  // To achieve the above output, we'll only need one iterator. Since
  // all hashtables should share the keyset, one iterator should be enough
  // to get all the keys we need.

  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((it = MakeHTIter(cpt_log->src_filehash_to_filename)),
          NULL,
          num_attempts)

  num_files = HTSize(cpt_log->dir_tree);
  if (num_files != HTSize(cpt_log->src_filehash_to_filename) ||
      num_files != HTSize(cpt_log->src_filehash_to_cptname)) {
      if (DEBUG) {
        printf("ERROR: invalid state - not an equal keyset for all hashtables\n");
      }
      return LIST_ERR;
  }

  char *fname;
  if (DEBUG) { printf("printing the state of %d files\n", num_files); }
  for (i = 0; i < num_files; i++) {
    // Get current key.
    if (HTIterKV(it, &f_name) == 0 && num_files > 1) {
      if (DEBUG) {
        printf("ERROR: could obtain HTKV List\n");
      }
      DiscardHTIter(it);
      return LIST_ERR;
    }
    // Get other two values.
    res = HTLookup(cpt_log->src_filehash_to_cptname, f_name.key, &cptname);
    if (res == 0 || res == -1) {
      DiscardHTIter(it);
      return LIST_ERR;
    }
    res = HTLookup(cpt_log->dir_tree, f_name.key, &cpt_tree);
    if (res == 0 || res == -1) {
      DiscardHTIter(it);
      return LIST_ERR;
    }
  
    // print output.
    printf("%s (curr cp: %s)\n", f_name.value, cptname.value);
    res = PrintTree(cpt_tree.value);
    if (res == MEM_ERR || res == PRINT_ERR) {
      DiscardHTIter(it);
      return LIST_ERR;
    }
    num_cpts += res;
    printf("\n");

    // Advance iterator.
    if (i < num_files - 1) {
      if (HTIncrementIter(it) == 0 && num_files > 1) {
        if (DEBUG) {
          printf("ERROR: could not advance iterator in List\n");
        }
        DiscardHTIter(it);
        return LIST_ERR;
      }
    }
  }

  DiscardHTIter(it);
  return num_cpts;
}

static int32_t PrintTree(CpTreeNodePtr tree) {
  int32_t num_cps = 0, res;
  if (tree == NULL) {
    return num_cps;
  }
  if (tree->cpt_name == NULL || tree->children == NULL) {
    return PRINT_ERR;
  }

  LLIter it;
  CpTreeNodePtr curr_child;
  int32_t i, num_children, num_attempts;

  printf("\t%s: ", tree->cpt_name);
  num_children = LLSize(tree->children);
  if (num_children > 0) {  // Print32_t current node + children
    num_attempts = NUMBER_ATTEMPTS;
    ATTEMPT((it = LLGetIter(tree->children, 0)), NULL, num_attempts)
    for (i = 0; i < num_children; i++) {
      LLIterPayload(it, &curr_child);
      printf("%s%s", curr_child->cpt_name, i < num_children - 1 ? ", " : "");
      LLIterAdvance(it);
    }
    LLIterFree(it);
  }
  printf("\n");

  if (num_children > 0) {  // Recursively print children
    num_attempts = NUMBER_ATTEMPTS;
    ATTEMPT((it = LLGetIter(tree->children, 0)), NULL, num_attempts)
    for (i = 0; i < num_children; i++) {
      LLIterPayload(it, &curr_child);
      res = PrintTree(curr_child);
      if (res == PRINT_ERR) {
        LLIterFree(it);
        return PRINT_ERR;
      }
      num_cps += res;
      LLIterAdvance(it);
    }
    LLIterFree(it);
  }

  return 1 + num_cps;
}

static void FreeCheckPointLog(CheckPointLogPtr cpt_log) {
  if (DEBUG) {
    printf("Freeing tables . . .\nNum elements:\n"\
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

static int32_t IsValidCommand(char *command) {
  int32_t i;

  // Compare commmand to all valid commands
  for (i = 0; i < VALID_COMMAND_COUNT; i++) {
    if (!strcmp(command, valid_commands[i]))
      return i;
  }

  return INVALID_COMMAND;
}

static void Usage() {
  fprintf(stderr, "\nCheckpoint: copyright 2019, Pieter Benjamin\n");
  fprintf(stderr, "\nUsage: ./checkpoint  <option>\n");
  fprintf(stderr, "Where <filename> is an absolute or relative pathway\n"\
                  "to the file you would like to checkpoint.\n\n"\
                  "options:\n"\
                  "\tcreate <source file name> <checkpoint name>\n"\
                  "\tback   <source file name>\n"\
                  "\tswapto <source file name> <checkpoint name>\n"\
                  "\tdelete <source file name>\n"\
                  "\t\tNOTE: \"delete\" does not remove your source file,\n"\
                  "\t\t      but will remove all trace of it from the\n"\
                  "\t\t      current checkpoint log for this directory.n"\
                  "\tlist   (lists all Checkpoints for the current dir)\n\n"\
                  "PLEASE NOTE:"\
                  "\t- Checkpoints will be stored in files labeled with\n"\
                  "\t  the name of the checkpoint  you provide. If you\n"\
                  "\t  provide the name of a preexisting file (checkpoint),\n"\
                  "\t  it will NOT be overwritten.\n\n"
                  "\t- Delete is irreversible.\n\n");  
                  // TODO: make delete reversible

  exit(EXIT_FAILURE);
}
