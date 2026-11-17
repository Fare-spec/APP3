#include <stdio.h>

#include "arbres.h"

int DEBUG = 0;

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage : %s <fichier arbre>\n", argv[0]);
    return 1;
  }
  FILE *f = fopen(argv[1], "r");
  if (f == NULL) {
    perror(argv[1]);
    return 1;
  }
  arbre a = lire_arbre(f);
  fclose(f);
  ecrire_dot(a, stdout);
  return 0;
}
