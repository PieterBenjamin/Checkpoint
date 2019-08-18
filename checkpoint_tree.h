// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#ifndef _CHECKPOINT_TREE_H_
#define _CHECKPOINT_TREE_H_

#include "DataStructs/LinkedList.h"
#include "macros.h"

#define CREATE_TREE_SUCCESS 0

#define INSERT_NODE_SUCCESS 0
#define INSERT_NODE_ERROR -1

#define FIND_CPT_SUCCESS 123
#define FIND_CPT_ABSENT  321
#define FIND_CPT_ERROR   -123

#define TREE_FREE_OK 0

// This struct will maintain the relationship between all the
// checkpoints known in the current directory. It will have to
// be loaded from/written to disk every time an instance
// of the checkpoint  program is run.
typedef struct cpt_tree {
  // A pointer to the parent of this subtree.
  // The (absolute) root parent should be NULL.
  struct cpt_tree *parent_node;
  // A pointer to a string on the heap storing
  // the name of this checkpoint.
  char *cpt_name;
  // A pointer to a LinkedList on the heap. This
  // Linkedlist is composed of pointers on the heap
  // to other cpt_tree structs which
  LinkedList children;
} CpTreeNode, *CpTreeNodePtr;

// Assigns cpt_name and parent_tree to their corresponding fields
// (without checking if the addresses are valid) and attempts to
// allocate a LinkedList on the heap for the children field.
//
// Returns:
//
//  - MEM_ERR: on a memory error
//
//  - CREATE_TREE_SUCCESS: if allocation was successful.
int32_t CreateCpTreeNode(char *cpt_name,
                     CpTreeNodePtr parent_node,
                     CpTreeNodePtr *ret);

// Inserts a checkpoint  node into the given node.
//
// Returns:
//
//             - MEM_ERR: on a memory error.
//
// - INSERT_NODE_SUCCESS: if insert was successfull.
//
// - INSERT_NODE_ERROR: if something went wrong.
int32_t InsertCpTreeNode(CpTreeNodePtr cpt_node, CpTreeNodePtr to_insert);

// Attempts to find the checkpoint  with the name @cpt_name
// in the tree @cpt_tree. If successful, a pointer to the struct
// containing the same name will be returned through @ret.
// Returns:
//
//  - MEM_ERR: if any memory errors occur while searching.
//
//  - FIND_CPT_SUCCESS: if a CpTreeNode was found with a matching cpt_name
//
//  - FIND_CPT_ABSENT: if the search determined there is no CpTreeNode
//                      below (and including) cpt_tre with a matching name.
//
//  - FIND_CPT_ERROR: when a generic error occurs while searching.
int32_t FindCpt(CpTreeNodePtr cpt_tree, char *cpt_name, CpTreeNodePtr *ret);

// Frees a given node and all it's children.
//
// Returns:
//
//  - MEM_ERROR: on a memory error
//
//  - TREE_FREE_OK: if all went well
int32_t FreeCpTreeNode(CpTreeNodePtr curr_node);

#endif  // _CHECKPOINT_TREE_H_
