#include "queue.h"
#include <stdio.h>
#include <stdlib.h>

queue init_q(int capacite) {
  queue q;
  q.element = malloc(capacite * sizeof(noeud *));
  q.capacite = capacite;
  q.head = 0;
  q.tail = 0;
  return q;
}

int push(queue *q, noeud *n) {
  if (q->tail == q->capacite) {
    fprintf(stderr, "Error, the queue if full.\n");
    return 1;
  }
  q->element[q->tail] = n;
  q->tail++;
  return 0;
}

noeud *pop(queue *q) {
  if (q->head == q->tail) {
    return NULL;
  }
  noeud *n = q->element[q->head];
  q->head++;
  return n;
}

bool empty_queue(const queue *q) { return q->head == q->tail; }

void free_queue(queue *q) {
  free(q->element);
  q->element = NULL;
  q->capacite = 0;
  q->head = 0;
  q->tail = 0;
}
