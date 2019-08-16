// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#include "DataStructs/HashTable_priv.h"
#include "checkpoint_filehandler.h"

void FileHandlerNullFree(void *val) { }

static void ZeroHeader(CpLogFileHeader *h) {
  h->magic_number = 0;
  h->checksum = 0;
  h->src_filehash_to_filename_size = 0;
  h->src_filehash_to_cptname_size = 0;
  h->cpt_namehash_to_cptfilename_size = 0;
  h->dir_tree_size = 0;
}

int WriteCheckPointLog(CheckPointLogPtr cpt_log) {
  CpLogFileHeader header;
  FILE *f = fopen(CP_LOG_FILE, "wb");
  if (f == NULL) {
    if (DEBUG) {
      printf("\tERROR: could not open file %s\n", CP_LOG_FILE);
    }
    return FILE_WRITE_ERR;
  }
  // These four size variables are used to store the
  // size of each hashtable when it has been written.
  int32_t offset = 0, src_name, src_cptname, cptname, dirtree;
  // Before we advance, we will intentionally corrupt the header
  // so that if we crash while writing this file, nobody thinks
  // the file is valid.
  ZeroHeader(&header);

  if (fseek(f, 0, SEEK_SET) != 0) {
    if (DEBUG) {
      printf("\tERROR: could not seek to beginning of file\n");
    }
    return FILE_WRITE_ERR;
  }
  if (fwrite(&header, sizeof(CpLogFileHeader), 1, f) != 1) {
    if (DEBUG) {
      printf("\tERROR: could not write file header\n");
    }
    return FILE_WRITE_ERR;
  }
  offset += sizeof(CpLogFileHeader);

  src_name = WriteHashTable(f,
                            cpt_log->src_filehash_to_filename,
                            offset,
                            &WriteStringBucket);
  CHECK_HASHTABLE_LENGTH(src_name)
  offset += src_name;
  if (DEBUG) {
    printf("\tWrote %d bytes for src_filehash_to_filename\n", src_name);
  }

  src_cptname = WriteHashTable(f,
                               cpt_log->src_filehash_to_cptname,
                               offset,
                               &WriteStringBucket);
  CHECK_HASHTABLE_LENGTH(src_cptname)   
  offset += src_cptname;
  if (DEBUG) {
    printf("\tWrote %d bytes for src_filehash_to_cptname\n", src_cptname);
  }

  cptname = WriteHashTable(f,
                           cpt_log->cpt_namehash_to_cptfilename,
                           offset,
                           &WriteStringBucket);
  CHECK_HASHTABLE_LENGTH(cptname);
  offset += cptname;   
  if (DEBUG) {
    printf("\tWrote %d bytes for cpt_namehash_to_cptfilename\n", cptname);
  }

  if (DEBUG) {
    printf("\tfinished writing hash->name hashtables\n");
  }
  dirtree = WriteHashTable(f,
                           cpt_log->dir_tree,
                           offset,
                           &WriteTreeBucket);
  CHECK_HASHTABLE_LENGTH(dirtree);
  offset += dirtree;

  header.magic_number = ((uint32_t)0xCAFE00D);
  header.checksum = offset;
  header.src_filehash_to_filename_size = src_name;
  header.src_filehash_to_cptname_size = src_cptname;
  header.cpt_namehash_to_cptfilename_size = cptname;
  header.dir_tree_size = dirtree;

  if (fseek(f, 0, SEEK_SET) != 0) {
    return FILE_WRITE_ERR;
  }
  if (fwrite(&header, sizeof(CpLogFileHeader), 1, f) != 1) {
    return FILE_WRITE_ERR;
  }              

  return offset;
}

int ReadCheckPointLog(CheckPointLogPtr cpt_log) {
  struct stat;
  if (DEBUG) {
    printf("\t\tloading tables . . .\n");
  }

  // Allocate space for the hashtables
  cpt_log->src_filehash_to_filename = MakeHashTable(INITIAL_BUCKET_COUNT);
  cpt_log->src_filehash_to_cptname  = MakeHashTable(INITIAL_BUCKET_COUNT);
  cpt_log->cpt_namehash_to_cptfilename = MakeHashTable(INITIAL_BUCKET_COUNT);
  cpt_log->dir_tree = MakeHashTable(INITIAL_BUCKET_COUNT);

  if (cpt_log->src_filehash_to_filename == NULL ||
      cpt_log->src_filehash_to_cptname  == NULL ||
      cpt_log->cpt_namehash_to_cptfilename == NULL ||
      cpt_log->dir_tree == NULL) {
    FreeHashTable(cpt_log->src_filehash_to_filename, &FileHandlerNullFree);
    FreeHashTable(cpt_log->src_filehash_to_cptname, &FileHandlerNullFree);
    FreeHashTable(cpt_log->cpt_namehash_to_cptfilename, &FileHandlerNullFree);
    FreeHashTable(cpt_log->dir_tree, &FreeCpTreeNode);
    if (DEBUG) {
      printf("ERROR allocating space for hashables\n");
    }
    return READ_ERROR;
  }

  // Read the stored tables (if they exist)
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

  if ((cpt_file = fopen(cpt_filename, "wb")) == NULL) {
    fprintf(stderr,
            "\tERROR opening file %s%s.\n"\
            "\tAborting program now.\n", WORKING_DIR, cpt_filename);
    return -2;
  }

  if ((src_file = fopen(src_filename, "r")) == NULL) {
    fprintf(stderr,
            "\tERROR opening file %s.\n\tProgram will now be aborted.\n",
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

int32_t WriteHashTable(FILE *f,
                       HashTable table,
                       ssize_t offset,
                       write_bucket_fn fn) {
  int num_bytes_written = 0, res;
  BucketRecListHeader reclist_header = {HTSize(table)};
  BucketRec br;
  ssize_t next_bucket_rec_offset = offset + sizeof(BucketRecListHeader);
  ssize_t i, next_bucket_offset, num_attempts;
  HTIter it;
  HashTabKV kv;

  // Write the table header
  if (fseek(f, offset, SEEK_SET) != 0) {
    return FILE_WRITE_ERR;
  }
  if (fwrite(&reclist_header, sizeof(BucketRecListHeader), 1, f) != 1) {
    return FILE_WRITE_ERR;
  }

  if (DEBUG) {
    printf("\t\twriting %d table element(s)\n", reclist_header.num_bucket_recs);
  }

  // Write the bucket_rec list. Since each bucket will be a variable
  // number of bytes, the bucket rec and matching bucket must both be
  // written before the next bucket rec.
  next_bucket_offset = next_bucket_rec_offset
                     + (sizeof(BucketRec) * reclist_header.num_bucket_recs);
  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((it = MakeHTIter(table)), NULL, num_attempts)
  for (i = 0; i < reclist_header.num_bucket_recs; i ++) {
    num_attempts = 20;
    if (HTIterKV(it, &kv) == 0) {
      DiscardHTIter(it);
      if (DEBUG) {
        printf("\tERROR: expected more values in ht\n");
      }
      return FILE_WRITE_ERR;
    }

    // Write bucket
    res = fn(f, next_bucket_offset, kv);
    if (res == FILE_WRITE_ERR || res == MEM_ERR) {
      DiscardHTIter(it);
      return FILE_WRITE_ERR;
    }

    // store the size and location of bucket
    br.bucket_size = res;
    br.bucket_pos = next_bucket_offset;
    next_bucket_offset += res;

    // Write bucket_rec
    if (fseek(f, next_bucket_rec_offset, SEEK_SET) != 0) {
      DiscardHTIter(it);
      if (DEBUG) {
        printf("\tERROR: fseek failed in WriteHashTable\n");
      }
      return FILE_WRITE_ERR;
    }
    if (fwrite(&br, sizeof(BucketRec), 1, f) != 1) {
      DiscardHTIter(it);
      if (DEBUG) {
        printf("\tERROR: fwrite failed in WriteHashTable\n");
      }
      return FILE_WRITE_ERR;
    }
    next_bucket_rec_offset += sizeof(BucketRec);
  }
  DiscardHTIter(it);
  return next_bucket_offset - offset;
}

int WriteStringBucket(FILE *f, ssize_t offset, HashTabKV kv) {
  if (fseek(f, offset, SEEK_SET) != 0) {
    return FILE_WRITE_ERR;
  }

  char *str = kv.value;
  int len = strlen(str);
  BucketHeader header = {kv.key};
  if (fwrite(&header, sizeof(BucketHeader), 1, f) != 1) {
    return FILE_WRITE_ERR;
  }
  if (fwrite(str, sizeof(char) * len, 1, f) != 1) {
    return FILE_WRITE_ERR;
  }

  return sizeof(BucketHeader) + len;
}

static int WriteTreeBucket(FILE *f, ssize_t offset, HashTabKV kv) {
  CpTreeNodePtr curr_node = kv.value;
  if (curr_node == NULL) {
    return 0;
  }

  // Before we do anything, let's wright the bucket header
  // (which is just a key) and increment the offset.
  BucketHeader bh = {kv.key};
  if (fseek(f, offset, SEEK_SET) != 0) {
    if (DEBUG) {
      printf("\t\tERROR: could not fseek to offset %d in WriteTreeBucket\n",
             offset);
    }
    return FILE_WRITE_ERR;
  }
  if (fwrite(&bh, sizeof(BucketHeader), 1, f) != 1) {
    if (DEBUG) {
      printf("\t\tERROR: could not write header in WriteTreeBucket\n");
    }
    return FILE_WRITE_ERR;
  }
  offset += sizeof(BucketHeader);

  return WriteTree(f, offset, curr_node);
}

static int WriteTree(FILE *f, ssize_t offset, CpTreeNodePtr curr_node) {
  ssize_t child_bytes_written = 0, name_len, num_children, self_content_length;
  int res;
  name_len     = strlen(curr_node->cpt_name) + 1;
  num_children = LLSize(curr_node->children);
  ssize_t children_offsets[num_children];

  // A file_treenode is written the following way:
  //
  // [name_len][num_children]["name\0"][children_offsets][      children      ]
  //
  // name_len and num_children are type ssize_t
  // name is a char array (which omits the null terminating byte)
  // children_offsets is an array of ssize_t values
  //
  // It's impossible to tell anything about the length of children, unless
  // num_children is 0, in which case there will not be any children_offsets
  // or children field written.
  //
  // Thus, the length of a "prefix" for a file_treenode can be evaluated as
  self_content_length = (sizeof(ssize_t) * (2 + num_children))
                      + (sizeof(char)  * name_len);

  if ((res = WriteChildren(curr_node,
                           children_offsets,
                           offset + self_content_length,
                           f)) == FILE_WRITE_ERR) {
    return FILE_WRITE_ERR;
  }
  child_bytes_written = res;

  // Write self:
  if (fseek(f, offset, SEEK_SET) != 0) {
    if (DEBUG) {
      printf("\t\tERROR: could not fseek to offset %d in WriteTree\n", offset);
    }
    return FILE_WRITE_ERR;
  }

  // Unfortunately, we have to do two (often three) writes since
  // name and children_offsets are variable length
  FileTreeHeader header = {name_len, num_children};
  // Write the bookkeeping information.
  if (fwrite(&header, sizeof(FileTreeHeader), 1, f) != 1) {
    if (DEBUG) {
      printf("\t\tERROR: could not write header in WriteTree\n");
    }
    return FILE_WRITE_ERR;
  }
  // Write the name field.
  if (fwrite(curr_node->cpt_name, sizeof(char) * name_len, 1, f) != 1) {
    if (DEBUG) {
      printf("\t\tERROR: could not write cpt name in WriteTree\n");
    }
    return FILE_WRITE_ERR;
  }
  if (num_children > 0) {
    // If we have any children, write their offsets.
    if (fwrite(children_offsets, sizeof(ssize_t) * num_children, 1, f) != 1) {
      if (DEBUG) {
        printf("\t\tERROR: writing children in WriteTree\n");
      }
      return FILE_WRITE_ERR;
    }
  }

  // The total amount written for this tree is the amount
  // written for the current node plus the amount written
  // for the current nodes children.
  return self_content_length + child_bytes_written;
}

static int WriteChildren(CpTreeNodePtr curr_node,
                         ssize_t *children_offsets,
                         ssize_t offset,
                         FILE *f) {
  LLIter it;
  ssize_t res,
          child_bytes_written = 0,
          num_attempts = 20,
          num_children = LLSize(curr_node->children);
  ATTEMPT( (it = LLGetIter(curr_node->children, 0)), NULL, num_attempts)

  CpTreeNodePtr curr_child;
  for (ssize_t i = 0; i < num_children; i++) {
    children_offsets[i] = ((num_children - (i + 1)) * 4) + child_bytes_written;
    LLIterPayload(it, &curr_child);
    res = WriteTree(f, offset + child_bytes_written, curr_child);

    if (res == FILE_WRITE_ERR) {  // Something went wrong writing the child.
      LLIterFree(it);
      return FILE_WRITE_ERR;
    }

    child_bytes_written += res;
  }
  LLIterFree(it);

  return child_bytes_written;
}
