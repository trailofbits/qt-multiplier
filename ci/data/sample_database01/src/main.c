/*
  Copyright (c) 2021-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NODE_COUNT 20

#define DumpParameter(index, string_ptr) \
  do { \
    printf(" > %d is %s\n", index, string_ptr); \
  } while (0)

static void printParameterList(int argc, char *argv[]) {
  printf("Parameter list:\n");

  for (int i = 0; i < argc; i++) {
    DumpParameter(i, argv[i]);
  }
}

typedef struct {
  char *value;
} Node;

static int allocateNodeList(Node **node_list_ptr) {
  if (node_list_ptr == NULL) {
    return 1;
  }

  Node *node_list = (Node *) malloc(NODE_COUNT * sizeof(Node));
  if (node_list == NULL) {
    return 1;
  }

  *node_list_ptr = node_list;

  memset(node_list, 0, NODE_COUNT * sizeof(Node));

  srand(time(NULL));
  for (int i = 0; i < NODE_COUNT; i++) {
    node_list[i].value = (char *) malloc(32);
    if (node_list[i].value == NULL) {
      continue;
    }

    int value = rand();
    sprintf(node_list[i].value, "%d", value);
  }

  return 0;
}

static void recursiveFreeCaller(int i, void *ptr) {
  if (i == 0) {
    free(ptr);
    return;
  }

  i -= 1;
  recursiveFreeCaller(i, ptr);
}

static void nestedFreeCaller5(void *ptr) {
  recursiveFreeCaller(100, ptr);
}

static void nestedFreeCaller4(void *ptr) {
  nestedFreeCaller5(ptr);
}

static void nestedFreeCaller3(void *ptr) {
  nestedFreeCaller4(ptr);
}

static void nestedFreeCaller2(void *ptr) {
  nestedFreeCaller3(ptr);
}

static void nestedFreeCaller1(void *ptr) {
  nestedFreeCaller2(ptr);
}

static void destroyNodeList(Node *node_list) {
  if (node_list == NULL) {
    return;
  }

  for (int i = 0; i < NODE_COUNT; i++) {
    if (node_list[i].value == NULL) {
      continue;
    }

    nestedFreeCaller1(node_list[i].value);
  }

  nestedFreeCaller1(node_list);
}

static void printNodeList(Node *node_list) {
  if (node_list == NULL) {
    return;
  }

  for (int i = 0; i < NODE_COUNT; i++) {
    if (node_list[i].value == NULL) {
      continue;
    }

    printf("Node %d has value %s\n", i, node_list[i].value);
  }
}

int main(int argc, char *argv[], char *envp[]) {
  printParameterList(argc, argv);

  Node *node_list = NULL;
  if (allocateNodeList(&node_list) == 0) {
    printNodeList(node_list);
  }

  destroyNodeList(node_list);
  return 0;
}
