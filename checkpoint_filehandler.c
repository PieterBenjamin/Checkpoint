// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#include "DataStructs/HashTable_priv.h"
#include "checkpoint_filehandler.h"

void FileHandlerNullFree(void *val) { }

int32_t ReadCheckPointLog(CheckPointLogPtr cpt_log) {
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
    return MEM_ERR;
  }

  // Read the stored tables (if they exist)
  // open file
  if (access(CP_LOG_FILE, F_OK) == -1) {
    // nothing to read
    if (DEBUG) {
      printf("checkpoint log file not found\n");
    }
    return READ_SUCCESS;
  }
  FILE *f;
  if ((f = fopen(CP_LOG_FILE, "r")) == NULL) {
    if (DEBUG) {
      printf("\t\tERROR: could not open file %s\n", CP_LOG_FILE);
    }
    return READ_ERROR;
  }
  
  if (DEBUG) {
    fseek(f, 0L, SEEK_END);
    printf("\t\treading %d bytes in from %s\n", ftell(f), CP_LOG_FILE);
  }

  // Read the header;
  CpLogFileHeader header;
  int32_t res, offset = 0;
  if (fseek(f, 0, SEEK_SET) != 0) {
    if (DEBUG) {
      printf("\t\tERROR: could not fseek to start of %s\n", CP_LOG_FILE);
    }
    return READ_ERROR;
  }
  if (fread(&header, sizeof(CpLogFileHeader), 1, f) != 1) {
    if (DEBUG) {
      printf("\t\tERROR: could not fread at start of %s\n", CP_LOG_FILE);
    }
    return READ_ERROR;
  }
  if (header.magic_number != 0xcafe00d) {
    if (DEBUG) {
      printf("\t\tERROR: corrupted file. Header is %x\n", header.magic_number);
    }
    return READ_ERROR;
  }
  // TODO: checksum

  if (DEBUG) {
    printf("\t\treading string tables in from disk\n");
  }

  // read String tables
  offset += sizeof(CpLogFileHeader);
  res = ReadHashTable(f, offset, cpt_log->src_filehash_to_filename, &ReadStringBucket);
  if (res == READ_ERROR || res == MEM_ERR ) {
    if (DEBUG) {
      printf("\t\terror %d reading src filenames\n", res);
    }
    return READ_ERROR;
  }
  if (DEBUG) { printf("\n\t\tsrc_filenames done\n"); }
  offset += header.src_filehash_to_filename_size;

  res = ReadHashTable(f, offset, cpt_log->src_filehash_to_cptname, &ReadStringBucket);
  if (res == READ_ERROR || res == MEM_ERR) {
    if (DEBUG) {
      printf("\t\terror %d reading current cpt names\n", res);
    }
    return READ_ERROR;
  }
  if (DEBUG) { printf("\n\t\tsrc cpts done\n"); }
  offset += header.src_filehash_to_cptname_size;

  res = ReadHashTable(f, offset, cpt_log->cpt_namehash_to_cptfilename, &ReadStringBucket);
  if (res == READ_ERROR || res == MEM_ERR) {
    return READ_ERROR;
    if (DEBUG) {
      printf("\t\terror %d reading cpt file names\n", res);
    }
  }
  if (DEBUG) { printf("\n\t\tcpt filenames done\n"); }
  offset += header.cpt_namehash_to_cptfilename_size;

  if (DEBUG) {
    printf("\t\treading tree table in from disk\n");
  }
  // read tree table
  res = ReadHashTable(f, offset, cpt_log->dir_tree, &ReadTreeBucket);
  if (res == READ_ERROR || res == MEM_ERR) {
    if (DEBUG) {
      printf("\t\terror %d reading dirtree\n", res);
    }
    return READ_ERROR;
  }
  if (DEBUG) { printf("\n\t\tdirtree done\n"); }

  // close file
  fclose(f);
  return READ_SUCCESS;
}

static int32_t ReadHashTable(FILE *f,
                             uint32_t offset,
                             HashTable table,
                             read_bucket_fn fn) {
  if (fseek (f, offset, SEEK_SET) != 0) {
    return READ_ERROR;
  }
  if (DEBUG) { printf("\t\t\treading hashtable at offset %x\n", offset);}
  BucketRecListHeader brl_h;
  BucketRec br;
  uint32_t next_bucketrec_offset = 0, next_bucket_offset = 0, num_attempts;
  int32_t bytes_read = 0, res;

  // Read in the bucket rec list header
  if (fread(&brl_h, sizeof(BucketRecListHeader), 1, f) != 1) {
    return READ_ERROR;
  }
  bytes_read += sizeof(BucketRecListHeader);
  next_bucketrec_offset += offset + sizeof(BucketRecListHeader);

  // Now read in all the bucket rec/buckets
  HashTabKV kv, storage;
  for (int32_t i = 0; i < brl_h.num_bucket_recs; i++) {
    // Read the next bucket rec
    if (fseek(f, next_bucketrec_offset, SEEK_SET) != 0) {
      if (DEBUG) {
        printf("\t\t\tERROR: could not fseek in ReadHashTable\n");
      }
      return READ_ERROR;
    }
    if (fread(&br, sizeof(BucketRec), 1, f) != 1) {
      if (DEBUG) {
        printf("\t\t\tERROR: could not fread in ReadHashTable\n");
      }
      return READ_ERROR;
    }
    if (DEBUG) { printf("\t\t\tnext bucketrec at %x\n", next_bucketrec_offset);}
    bytes_read += sizeof(BucketRec);
    next_bucket_offset = br.bucket_pos;
    if (DEBUG) {printf("\t\t\treading bucket of size %d at offset %x\n", br.bucket_size, next_bucket_offset);}

    // Read the bucket
    res = fn(f, next_bucket_offset, &kv);
    if (res == READ_ERROR || res == MEM_ERR) {
      if (res == MEM_ERR) {
        if (DEBUG) {
          printf("\t\t\tERROR: mem error arose while reading bucket\n");
        }
      }
      return READ_ERROR;
    }
    bytes_read += res;
  
    HTInsert(table, kv, &storage);
    next_bucketrec_offset += sizeof(BucketRec);
  }

  return bytes_read;
}

static int32_t ReadStringBucket(FILE *f, uint32_t offset, HashTabKV *kv) {
  if (fseek (f, offset, SEEK_SET) != 0) {
    if (DEBUG) {
      printf("\t\t\tERROR: could not fseek in ReadStringBucket\n");
    }
    return READ_ERROR;
  }
  if (DEBUG) {
    printf("\t\t\tReading string bucket from offset %x\n", offset);
  }
  int32_t bytes_read = 0, num_attempts, res;
  BucketHeader bh;
  StringBucketHeader sh;
  char *str;
  if ((res = fread(&bh, sizeof(BucketHeader), 1, f)) != 1) {
    if (DEBUG) {
      printf("\t\t\tERROR[%d]: could not fread (1) from offset %x "\
             "in ReadStringBucket\n", res, offset);
    }
    return READ_ERROR;
  }
  bytes_read += sizeof(BucketHeader);
  if ((res = fread(&sh, sizeof(StringBucketHeader), 1, f)) != 1) {
    if (DEBUG) {
      printf("\t\t\tERROR:[%d] could not fread (2) from offset %x "\
             "in ReadStringBucket\n", res, offset + sizeof(BucketHeader));
    }
    return READ_ERROR;
  }
  bytes_read += sizeof(StringBucketHeader);

  // kv.value needs to be a pointer to a string
  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((str = malloc(sizeof(char) * (sh.len + 1))), NULL, num_attempts)
  
  if ((res = fread(str, sizeof(char) * sh.len, 1, f)) != 1) {
    if (DEBUG) {
      printf("\t\t\tERROR[%d]: could not fread %d bytes from offset %x "\
             "in ReadStringBucket\n", res, sh.len,
             offset + sizeof(BucketHeader) + sizeof(StringBucketHeader));
    }
    return READ_ERROR;
  }

  bytes_read += sizeof(char) * sh.len;
  str[sh.len] = '\0';
  kv->key = bh.key;
  kv->value = str;

  return bytes_read;
}

static int32_t ReadTreeBucket(FILE *f, uint32_t offset, HashTabKV *kv) {
  if (fseek (f, offset, SEEK_SET) != 0) {
    return READ_ERROR;
  }

  BucketHeader bh;
  int32_t bytes_read = 0, res, num_attempts;
  CpTreeNodePtr node;

  if (fread(&bh, sizeof(BucketHeader), 1, f) != 1) {
    return READ_ERROR;
  }
  bytes_read += sizeof(BucketHeader);
  
  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((node = malloc(sizeof(CpTreeNode))), NULL, num_attempts)
  if (DEBUG) { printf("Reading a tree bucket of key %x from %x\n", bh.key, ftell(f)); }
  res = ReadTreeNode(f, offset + sizeof(BucketHeader), node);
  if (res == READ_ERROR || res == MEM_ERR) {
    return READ_ERROR;
  }
  bytes_read += res;

  kv->key = bh.key;
  kv->value = node;

  return bytes_read;
}

static int32_t ReadTreeNode(FILE *f, uint32_t offset, CpTreeNodePtr curr_node) {
  if (fseek(f, offset, SEEK_SET) != 0) {
    return READ_ERROR;
  }

  int32_t bytes_read = 0, num_attempts, res;
  FileTreeHeader header;
  if (fread(&header, sizeof(FileTreeHeader), 1, f) != 1) {
    return READ_ERROR;
  }
  bytes_read += sizeof(FileTreeHeader);
  if (DEBUG) { printf("reading treenode with name length %d and %d children from %x\n", header.name_length, header.num_children, ftell(f)); }
  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((curr_node->cpt_name = malloc(sizeof(char) * (header.name_length + 1))),
          NULL,
          num_attempts)

  if (fread(curr_node->cpt_name, sizeof(char) * header.name_length, 1, f) != 1) {
    return READ_ERROR;
  }
  curr_node->cpt_name[header.name_length] = '\0';

  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((curr_node->children = MakeLinkedList()), NULL, num_attempts)

  if (header.num_children > 0) {
    // Read children
    res = ReadTreeChildren(f,
                           offset + sizeof(FileTreeHeader) + header.name_length,
                           header.num_children,
                           curr_node);
    if (res == READ_ERROR || res == MEM_ERR) {
      if(DEBUG) { printf("error reading children\n"); }
      return READ_ERROR;
    }
  }

  return bytes_read;
}

static int32_t ReadTreeChildren(FILE *f,
                                uint32_t offset,
                                uint32_t num_children,
                                CpTreeNodePtr parent) {
  int32_t bytes_read = 0, res, num_attempts;
  CpTreeNodePtr curr_child;
  uint32_t children_offsets[num_children];
  if (fseek(f, offset, SEEK_SET) != 0) {
    return READ_ERROR;
  }
  if (fread(&children_offsets, sizeof(int32_t) * num_children, 1, f) != 1) {
    return READ_ERROR;
  }
  bytes_read += sizeof(int32_t) * num_children;

  for (int i = 0; i < num_children; i++) {
    // Go to the next child offset
    if (fseek(f, offset + children_offsets[i], SEEK_SET) != 0) {
      return READ_ERROR;
    }
    if (DEBUG) { printf("reading child from %x\n", ftell(f)); }
    num_attempts = NUMBER_ATTEMPTS;
    ATTEMPT((curr_child = malloc(sizeof(CpTreeNode))), NULL, num_attempts)
    num_attempts = NUMBER_ATTEMPTS;
    ATTEMPT((curr_child->children = MakeLinkedList()), NULL, num_attempts)

    res = ReadTreeNode(f, offset + children_offsets[i], curr_child);
    if (res == READ_ERROR || res == MEM_ERR) {
      return READ_ERROR;
    }
    bytes_read += res;

    curr_child->parent_node = parent;
    LLAppend(parent->children, curr_child);
  }

  return bytes_read;
}

static void ZeroHeader(CpLogFileHeader *h) {
  h->magic_number = 0;
  h->checksum = 0;
  h->src_filehash_to_filename_size = 0;
  h->src_filehash_to_cptname_size = 0;
  h->cpt_namehash_to_cptfilename_size = 0;
  h->dir_tree_size = 0;
}

int32_t WriteCheckPointLog(CheckPointLogPtr cpt_log) {
  CpLogFileHeader header;
  FILE *f = fopen(CP_LOG_FILE, "w+");
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
    printf("\t\tWrote %d bytes for src_filehash_to_filename\n", src_name);
  }

  src_cptname = WriteHashTable(f,
                               cpt_log->src_filehash_to_cptname,
                               offset,
                               &WriteStringBucket);
  CHECK_HASHTABLE_LENGTH(src_cptname)   
  offset += src_cptname;
  if (DEBUG) {
    printf("\t\tWrote %d bytes for src_filehash_to_cptname\n", src_cptname);
  }

  cptname = WriteHashTable(f,
                           cpt_log->cpt_namehash_to_cptfilename,
                           offset,
                           &WriteStringBucket);
  CHECK_HASHTABLE_LENGTH(cptname);
  offset += cptname;   
  if (DEBUG) {
    printf("\t\tWrote %d bytes for cpt_namehash_to_cptfilename\n", cptname);
  }

  if (DEBUG) {
    printf("\t\tfinished writing (hash->name) hashtables\n");
  }
  dirtree = WriteHashTable(f,
                           cpt_log->dir_tree,
                           offset,
                           &WriteTreeBucket);
  CHECK_HASHTABLE_LENGTH(dirtree);
  offset += dirtree;
  if (DEBUG) {
    printf("\t\tWrote %d bytes for dir_tree\n", dirtree);
  }

  if (DEBUG) {
    printf("\t\t\t%d bytes written to log file\n", offset);
  }
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

  fclose(f);
  return offset;
}

static int32_t WriteHashTable(FILE *f,
                              HashTable table,
                              uint32_t offset,
                              write_bucket_fn fn) {
  int32_t res;
  BucketRecListHeader reclist_header = {HTSize(table)};
  BucketRec br;
  uint32_t next_bucket_rec_offset = offset + sizeof(BucketRecListHeader);
  uint32_t i, next_bucket_offset, num_attempts;
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
    HTIncrementIter(it);
  }
  DiscardHTIter(it);
  return next_bucket_offset - offset;
}

static int32_t WriteStringBucket(FILE *f, uint32_t offset, HashTabKV kv) {
  if (fseek(f, offset, SEEK_SET) != 0) {
    return FILE_WRITE_ERR;
  }

  int32_t len = strlen(kv.value);  // kv value should be pointer to heap string
  BucketHeader bh = {kv.key};
  
  StringBucketHeader sh = {len};
  if (fwrite(&bh, sizeof(bh), 1, f) != 1) {  // write bucket header
    return FILE_WRITE_ERR;
  }
  if (fwrite(&sh, sizeof(StringBucketHeader), 1, f) != 1) {  // write size of str
    return FILE_WRITE_ERR;
  }
  if (fwrite(kv.value, len, 1, f) != 1) {  // write string itself
    return FILE_WRITE_ERR;
  }

  return sizeof(BucketHeader) + sizeof(StringBucketHeader) + len;
}

static int32_t WriteTreeBucket(FILE *f, uint32_t offset, HashTabKV kv) {
  CpTreeNodePtr curr_node = kv.value;
  if (curr_node == NULL) {
    return 0;
  }

  // Before we do anything, let's write the bucket header
  // (which is just a key) and increment the offset.
  BucketHeader bh = {kv.key};
  if (fseek(f, offset, SEEK_SET) != 0) {
    if (DEBUG) {
      printf("\t\tERROR: could not fseek to offset %d in WriteTreeBucket\n",
             offset);
    }
    return FILE_WRITE_ERR;
  }
  if (DEBUG) { printf("writing treebucket at %x\n", ftell(f)); }
  if (fwrite(&bh, sizeof(BucketHeader), 1, f) != 1) {
    if (DEBUG) {
      printf("\t\tERROR: could not write header in WriteTreeBucket\n");
    }
    return FILE_WRITE_ERR;
  }
  offset += sizeof(BucketHeader);

  return sizeof(BucketHeader) + WriteTree(f, offset, curr_node);
}

static int32_t WriteTree(FILE *f, uint32_t offset, CpTreeNodePtr curr_node) {
  int32_t child_bytes_written = 0, name_len, num_children, self_content_length;
  int32_t res;
  name_len     = strlen(curr_node->cpt_name) + 1;
  num_children = LLSize(curr_node->children);

  uint32_t children_offsets[num_children];

  // A file_treenode is written the following way:
  //
  // [name_len][num_children]["name\0"][children_offsets][      children      ]
  //
  // name_len and num_children are type ssize_t
  // name is a char array (which omits the null terminating byte)
  // children_offsets is an array of uint32_t values
  //
  // It's impossible to tell anything about the length of children, unless
  // num_children is 0, in which case there will not be any children_offsets
  // or children field written.
  //
  // Thus, the length of a "prefix" for a file_treenode can be expressed as
  self_content_length = (sizeof(uint32_t) * (2 + num_children))
                      + (sizeof(char)  * name_len);
                      
  res = WriteChildren(curr_node,
                      &children_offsets,
                      offset + self_content_length,
                      f);
  if (res == FILE_WRITE_ERR || res == MEM_ERR) {
    if (DEBUG) {
      printf("\t\t\tERROR[%d]: while writing children\n", res);
    }
    return FILE_WRITE_ERR;
  }
  child_bytes_written = res;

  // Write self:
  if (fseek(f, offset, SEEK_SET) != 0) {
    if (DEBUG) {
      printf("\t\t\tERROR: could not fseek to offset %d in WriteTree\n", offset);
    }
    return FILE_WRITE_ERR;
  }

  // Unfortunately, we have to do two (often three) writes since
  // name and children_offsets are variable length
  FileTreeHeader header = {name_len, num_children};

  // Write the bookkeeping information.
  if (fwrite(&header, sizeof(FileTreeHeader), 1, f) != 1) {
    if (DEBUG) {
      printf("\t\t\tERROR: could not write header in WriteTree\n");
    }
    return FILE_WRITE_ERR;
  }
  // Write the name field.
  if (fwrite(curr_node->cpt_name, sizeof(char) * name_len, 1, f) != 1) {
    if (DEBUG) {
      printf("\t\t\tERROR: could not write cpt name in WriteTree\n");
    }
    return FILE_WRITE_ERR;
  }
  if (num_children > 0) {
    // If we have any children, write their offsets.
    if (fwrite(children_offsets, sizeof(uint32_t) * num_children, 1, f) != 1) {
      if (DEBUG) {
        printf("\t\t\tERROR: writing children in WriteTree\n");
      }
      return FILE_WRITE_ERR;
    }
  }

  // The total amount written for this tree is the amount
  // written for the current node plus the amount written
  // for the current nodes children.
  return self_content_length + child_bytes_written;
}

static int32_t WriteChildren(CpTreeNodePtr curr_node,
                             uint32_t *children_offsets,
                             uint32_t offset,
                             FILE *f) {
  if (curr_node == NULL || curr_node->children == NULL) {
    if (DEBUG) {
      printf("\t\t\tskipping null node/children\n");
    }
    return 0;
  }
  LLIter it;
  uint32_t res,
           child_bytes_written = 0,
           num_attempts = 20,
           num_children = LLSize(curr_node->children);
  if (num_children == 0) {
    return 0;
  }
  ATTEMPT((it = LLGetIter(curr_node->children, 0)), NULL, num_attempts)
  if (DEBUG) { printf("\t\t\twriting Child %s\n", curr_node->cpt_name);  }
  CpTreeNodePtr curr_child;
  for (uint32_t i = 0; i < num_children; i++) {
    children_offsets[i] = ((num_children - (i)) * sizeof(u_int32_t)) + child_bytes_written;
    LLIterPayload(it, &curr_child);

    res = WriteTree(f, offset + child_bytes_written, curr_child);

    if (res == FILE_WRITE_ERR || res == MEM_ERR) {  // Something went wrong writing the child.
      LLIterFree(it);
      if (DEBUG) {
        printf("\t\t\tERROR[%d]: could not write child in WriteChildren\n");
      }
      return FILE_WRITE_ERR;
    }

    child_bytes_written += res;
    LLIterAdvance(it);
  }
  LLIterFree(it);

  return child_bytes_written;
}

int32_t WriteSrcToCheckpoint(char *src_filename, char *cpt_name) {
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