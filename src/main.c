#include "arbres.h"
#include "arbresphylo.h"
#include "listes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int DEBUG = 0;

int main(int argc, char *argv[]) {
  char *fichier = NULL;

  if (argc < 2) {
    fprintf(stderr, "Usage:  %s [-d] <fichier>\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "\t-d\tmode debug\n");
    exit(1);
  }
  int arg = 1;

  while (arg < argc) {
    if (!strncmp(argv[arg], "-d", 2)) {
      DEBUG = 1;
      arg++;
      continue;
    }
    if (argv[arg][0] == '-') {
      fprintf(stderr, "Option inconnue : '%s'\n", argv[arg]);
      exit(1);
    }
    if (fichier == NULL) {
      fichier = argv[arg];
      arg++;
      continue;
    } else {
      fprintf(stderr, "Trop de fichiers sur la ligne de commande : '%s'\n",
              argv[arg]);
      exit(1);
    }
  }

  debug("Ouverture de %s\n", fichier);
  FILE *f = fopen(fichier, "r");
  if (!f) {
    fprintf(stderr, "Erreur à l'ouverture du fichier `%s'\n", fichier);
    perror(fichier);
    exit(1);
  }

  arbre mon_arbre = lire_arbre(f);

  int nombre_especes;
  int nombre_caract;

  /*affiche_arbre(mon_arbre);*/

  analyse_arbre(mon_arbre, &nombre_especes, &nombre_caract);

  printf("Nombre d'especes de l'arbre: %d\n", nombre_especes);
  printf("Nombre de caractéristiques de l'arbre: %d\n", nombre_caract);

  afficher_par_niveau(mon_arbre, stdout);

  liste_t seq;
  init_liste_vide(&seq);

  char espece[256];
  printf("Espèce à rechercher : ");
  if (scanf("%255s", espece) != 1) {
    fprintf(stderr, "Aucune espèce lue\n");
    return 1;
  }

  if (rechercher_espece(mon_arbre, espece, &seq) == 0) {
    printf("%s :", espece);
    for (cellule_t *cell = seq.tete; cell != NULL; cell = cell->suivant) {
      printf(" %s", cell->val);
    }
    printf("\n");
  } else {
    printf("%s est absente de l'arbre\n", espece);
  }

  liberer_liste(&seq);
  return 0;
}
