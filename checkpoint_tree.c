// Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

#include "checkpoint_tree.h"

int32_t CreateCpTreeNode(char *cpt_name,
                         CpTreeNodePtr parent_node,
                         CpTreeNodePtr *ret) {
  CpTreeNodePtr new_node;

  int32_t num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((new_node = malloc(sizeof(CpTreeNode))), NULL, num_attempts)
  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((new_node->cpt_name = malloc(sizeof(char) * strlen(cpt_name + 1))), NULL, num_attempts)
  num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((new_node->children = MakeLinkedList()), NULL, num_attempts)

  new_node->parent_node = parent_node;
  strcpy(new_node->cpt_name, cpt_name);
  
  *ret = new_node;
  return CREATE_TREE_SUCCESS;
}

int32_t InsertCpTreeNode(CpTreeNodePtr cpt_node, CpTreeNodePtr to_insert) {
  if (DEBUG) {printf("adding %s as a child of %s\n", to_insert->cpt_name, cpt_node->cpt_name);}
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
      printf("Error pushing checkpoint  %s to %s\n",
             cpt_node->cpt_name,
             to_insert->cpt_name);
    }
    return INSERT_NODE_ERROR;
  }

  return INSERT_NODE_SUCCESS;  // If we've reached here, no errors occured.
}

int32_t FindCpt(CpTreeNodePtr curr_node, char *cpt_name, CpTreeNodePtr *ret) {
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

  if (DEBUG) { printf("here: "); printf("%s\n", curr_node->cpt_name); }
  if (strcmp(curr_node->cpt_name, cpt_name) == 0) {  // The cpt we're looking for is here!
    if (DEBUG) {
      printf("\t\tSuccess! cpt_name %s found at address %x\n",
             cpt_name,
             (unsigned int)curr_node);
    }

    *ret = curr_node;
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
  uint32_t num_attempts = NUMBER_ATTEMPTS;
  ATTEMPT((children_iter = LLGetIter(curr_node->children, 0)),
          NULL,
          num_attempts)
  
  CpTreeNodePtr curr_child;
  uint32_t num_children_unchecked = LLSize(curr_node->children);
  int32_t res;
  while (num_children_unchecked > 0) {
    LLIterPayload(children_iter, (LinkedListPayload)(&curr_child));
    if ((res = FindCpt(curr_child, cpt_name, ret)) == FIND_CPT_SUCCESS) {
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
  if (DEBUG) {
    printf("\t\tThe desired checkpoint could not be found\n");
  }
  return FIND_CPT_ABSENT;
}

int32_t FreeCpTreeNode(CpTreeNodePtr curr_node) {
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
