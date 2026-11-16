#include "listes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_liste_vide(liste_t *L) { L->tete = NULL; }

void liberer_liste(liste_t *L) {
  cellule_t *cell = L->tete;
  while (cell != NULL) {
    cellule_t *next = cell->suivant;
    free(cell);
    cell = next;
  }
  L->tete = NULL;
}

int ajouter_tete(liste_t *L, string c) {
  cellule_t *cell = malloc(sizeof(cellule_t));

  if (cell == NULL) {
    return 1;
  }

  cell->val = c;
  cell->suivant = L->tete;
  L->tete = cell;
  return 0;
}

void helper(const liste_t *L) {
  int i = 0;
  cellule_t *cell = L->tete;
  while (cell != NULL) {
    i += 1;
    printf("%s\n", cell->val);
    cell = cell->suivant;
  }
  printf("%d\n", i);
}
