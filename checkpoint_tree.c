// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#include "checkpoint_tree.h"

int CreateCpTreeNode(char *cpt_name,
                     CpTreeNodePtr parent_node,
                     CpTreeNodePtr ret) {
  CpTreeNodePtr new_node;

  int num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((new_node = malloc(sizeof(CpTreeNode))), NULL, num_attempts)
  
  new_node->parent_node = parent_node;
  new_node->cpt_name    = cpt_name;

  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((new_node->children = MakeLinkedList()), NULL, num_attempts)

  return CREATE_TREE_SUCCESS;
}

int InsertCpTreeNode(CpTreeNodePtr cpt_node, CpTreeNodePtr to_insert) {
  if (cpt_node == NULL) {
    if (DEBUG) {
      printf("Error, given a null node to insert into\n");
    }
    return INSERT_NODE_ERROR;
  }

  if (cpt_node->children == NULL) {
    if (DEBUG) {
      printf("Error, node %s at %x has null children field\n",
             cpt_node->cpt_name,
             (unsigned int)cpt_node);
    }
    return INSERT_NODE_ERROR;
  }

  if (!LLPush(cpt_node->children, (LinkedListPayload)(to_insert))) {
    if (DEBUG) {
      printf("Error pushing checkpoint %s to %s\n",
             cpt_node->cpt_name,
             to_insert->cpt_name);
    }
    return INSERT_NODE_ERROR;
  }

  return INSERT_NODE_SUCCESS;  // If we've reached here, no errors occured.
}

int FindCpt(CpTreeNodePtr curr_node, char *cpt_name, CpTreeNodePtr ret) {
  if (curr_node == NULL) {  // We can be quite certain cpt_name is not here!
    return FIND_CPT_ABSENT;
  }

  if (curr_node->cpt_name == NULL) {  // This should never occur.
    if (DEBUG) {
      printf("Error, cpt_node at address %x has no name field\n",
             (unsigned int)curr_node);
    }
    return FIND_CPT_ERROR;
  }

  if (curr_node->cpt_name == cpt_name) {  // The cpt we're looking for is here!
    if (DEBUG) {
      printf("Success! cpt_name %s found at address %x\n",
             cpt_name,
             (unsigned int)curr_node);
    }

    *ret = *curr_node;
    return FIND_CPT_SUCCESS;
  }

  // At this point, we know that this is a valid node, but not the one we want.
  // This means we must now search through all the children of this node.
  // We will use a simple DFS to perform this search.

  if (curr_node->children == NULL) {  // This should never occur.
    if (DEBUG) {
      printf("error: curr node %s at %x has null children field\n",
             curr_node->cpt_name,
             (unsigned int)curr_node);
    }
    return FIND_CPT_ERROR;
  }

  LLIter children_iter;
  ssize_t num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((children_iter = LLGetIter(curr_node->children, 0)),
          NULL,
          num_attempts)
  
  CpTreeNode curr_child;
  ssize_t num_children_unchecked = LLSize(curr_node->children);
  int res;
  while (num_children_unchecked > 0) {
    LLIterPayload(children_iter, (LinkedListPayload)(&curr_child));
    // We have to check two things:
    
    // 1. Does this child node have the name we want?
    if (curr_child.cpt_name == cpt_name) {
      *ret = curr_child;  // Copy into return param
      LLIterFree(children_iter);
      return FIND_CPT_SUCCESS;
    }

    // 2. Does the desired cpt_name exist in the grandchildren?
    if ((res = FindCpt(&curr_child, cpt_name, ret)) == FIND_CPT_SUCCESS) {
      LLIterFree(children_iter);
      return FIND_CPT_SUCCESS;
    } else if (res == FIND_CPT_ERROR) {
      LLIterFree(children_iter);
      return FIND_CPT_ERROR;
    }
    LLIterAdvance(children_iter);
    num_children_unchecked--;
  }

  LLIterFree(children_iter);
  // We checked the current node, and all it's children. The cpt
  // must not have been created before.
  return FIND_CPT_ABSENT;
}

int FreeCpTreeNode(CpTreeNodePtr curr_node) {
  if (curr_node == NULL) {
    return TREE_FREE_OK;
  }
  free(curr_node->cpt_name);
  if (curr_node->children != NULL) {  // free this nodes children
    FreeLinkedList(curr_node->children, &FreeCpTreeNode);
  }
  free(curr_node);
  return TREE_FREE_OK;
}