#ifndef TABLE_H
#define TABLE_H

#include "arbres.h"
#include <stdio.h>

typedef struct {
  int nb_especes;
  int nb_caracs;
  char **especes;
  char **caracs;
  char *valeurs;
} table;

int lire_table(FILE *f, table *t);
void liberer_table(table *t);
int construire_arbre(const table *t, arbre *resultat);
int arbre_respecte_table(arbre a, const table *t);
void liberer_arbre(arbre a);

#endif
