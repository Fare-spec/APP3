#include "arbres.h"
#include "arbresphylo.h"
#include "table.h"
#include <stdio.h>
#include <stdlib.h>

int DEBUG = 0;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage : %s <table.txt>\n", argv[0]);
    return 1;
  }

  FILE *f = fopen(argv[1], "r");
  if (f == NULL) {
    perror(argv[1]);
    return 1;
  }
  table t;
  int erreur = lire_table(f, &t);
  fclose(f);
  if (erreur != 0) {
    liberer_table(&t);
    return 1;
  }

  arbre a = NULL;
  if (construire_arbre(&t, &a) != 0) {
    fprintf(stderr, "Impossible : deux espèces ont exactement les mêmes caractéristiques.\n");
    liberer_table(&t);
    return 1;
  }
  if (!arbre_respecte_table(a, &t)) {
    fprintf(stderr, "ERREUR interne : l'arbre ne respecte pas la table.\n");
    liberer_arbre(a);
    liberer_table(&t);
    return 1;
  }

  affiche_arbre(a);
  int nb_esp, nb_carac;
  analyse_arbre(a, &nb_esp, &nb_carac);
  printf("Espèces : %d, caractéristiques : %d\n", nb_esp, nb_carac);

  liberer_arbre(a);
  liberer_table(&t);
  return 0;
}
