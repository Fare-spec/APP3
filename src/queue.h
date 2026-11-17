#ifndef QUEUE_H
#define QUEUE_H

#include "arbres.h"
#include <stdbool.h>

typedef struct {
  noeud **element;
  int capacite;
  int head;
  int tail;
} queue;

queue init_q(int capacite);
int push(queue *q, noeud *n);
noeud *pop(queue *q);
bool empty_queue(const queue *q);
void free_queue(queue *q);

#endif
