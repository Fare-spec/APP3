#include "table.h"
#include <stdlib.h>
#include <string.h>

#define LIGNE_MAX 4096
#define SEPARATEURS " \t\r\n"

static int valeur(const table *t, int espece, int carac) {
  return t->valeurs[espece * t->nb_caracs + carac];
}

static int indice_carac(const table *t, const char *nom) {
  for (int k = 0; k < t->nb_caracs; k++) {
    if (strcmp(t->caracs[k], nom) == 0) {
      return k;
    }
  }
  return -1;
}

static int lire_entete(table *t, char *debut) {
  char *mot = strtok(debut, SEPARATEURS);
  if (mot == NULL || strcmp(mot, "caracteristiques") != 0) {
    fprintf(stderr, "table : la première ligne doit commencer par 'caracteristiques'\n");
    return 1;
  }
  for (mot = strtok(NULL, SEPARATEURS); mot != NULL; mot = strtok(NULL, SEPARATEURS)) {
    char **nouveaux = realloc(t->caracs, (t->nb_caracs + 1) * sizeof *nouveaux);
    if (nouveaux == NULL) {
      return 1;
    }
    t->caracs = nouveaux;
    t->caracs[t->nb_caracs] = strdup(mot);
    t->nb_caracs++;
  }
  return 0;
}

static int lire_espece(table *t, char *debut) {
  char *nom = strtok(debut, SEPARATEURS);
  if (nom == NULL) {
    return 1;
  }
  char **nouveaux = realloc(t->especes, (t->nb_especes + 1) * sizeof *nouveaux);
  char *vals = realloc(t->valeurs, (t->nb_especes + 1) * t->nb_caracs + 1);
  if (nouveaux == NULL || vals == NULL) {
    return 1;
  }
  t->especes = nouveaux;
  t->valeurs = vals;
  t->especes[t->nb_especes] = strdup(nom);
  t->nb_especes++;

  for (int k = 0; k < t->nb_caracs; k++) {
    char *mot = strtok(NULL, SEPARATEURS);
    if (mot == NULL || (strcmp(mot, "0") != 0 && strcmp(mot, "1") != 0)) {
      fprintf(stderr, "table : valeur 0 ou 1 attendue pour %s\n", nom);
      return 1;
    }
    t->valeurs[(t->nb_especes - 1) * t->nb_caracs + k] = mot[0] - '0';
  }
  if (strtok(NULL, SEPARATEURS) != NULL) {
    fprintf(stderr, "table : trop de valeurs pour %s\n", nom);
    return 1;
  }
  return 0;
}

int lire_table(FILE *f, table *t) {
  char ligne[LIGNE_MAX];
  int entete_lue = 0;

  t->nb_especes = 0;
  t->nb_caracs = 0;
  t->especes = NULL;
  t->caracs = NULL;
  t->valeurs = NULL;

  while (fgets(ligne, sizeof ligne, f) != NULL) {
    char *debut = ligne + strspn(ligne, SEPARATEURS);
    if (*debut == '\0' || *debut == '#') {
      continue;
    }
    if (!entete_lue) {
      if (lire_entete(t, debut) != 0) {
        return 1;
      }
      entete_lue = 1;
    } else if (lire_espece(t, debut) != 0) {
      return 1;
    }
  }
  if (!entete_lue) {
    fprintf(stderr, "table : fichier vide\n");
    return 1;
  }
  return 0;
}

void liberer_table(table *t) {
  for (int i = 0; i < t->nb_especes; i++) {
    free(t->especes[i]);
  }
  for (int k = 0; k < t->nb_caracs; k++) {
    free(t->caracs[k]);
  }
  free(t->especes);
  free(t->caracs);
  free(t->valeurs);
  t->especes = NULL;
  t->caracs = NULL;
  t->valeurs = NULL;
  t->nb_especes = 0;
  t->nb_caracs = 0;
}

static int construire_rec(const table *t, const int *indices, int n, arbre *resultat) {
  if (n == 0) {
    *resultat = NULL;
    return 0;
  }
  if (n == 1) {
    noeud *feuille = nouveau_noeud();
    feuille->valeur = strdup(t->especes[indices[0]]);
    *resultat = feuille;
    return 0;
  }
  for (int k = 0; k < t->nb_caracs; k++) {
    int nb_oui = 0;
    for (int i = 0; i < n; i++) {
      nb_oui += valeur(t, indices[i], k);
    }
    if (nb_oui == 0 || nb_oui == n) {
      continue;
    }

    int *oui = malloc(n * sizeof *oui);
    int *non = malloc(n * sizeof *non);
    int nb_non = 0, nb_yes = 0;
    for (int i = 0; i < n; i++) {
      if (valeur(t, indices[i], k)) {
        oui[nb_yes++] = indices[i];
      } else {
        non[nb_non++] = indices[i];
      }
    }

    noeud *racine = nouveau_noeud();
    racine->valeur = strdup(t->caracs[k]);
    int r_oui = construire_rec(t, oui, nb_yes, &racine->droit);
    int r_non = construire_rec(t, non, nb_non, &racine->gauche);
    free(oui);
    free(non);

    if (r_oui != 0 || r_non != 0) {
      liberer_arbre(racine);
      *resultat = NULL;
      return 1;
    }
    *resultat = racine;
    return 0;
  }
  *resultat = NULL;
  return 1;
}

int construire_arbre(const table *t, arbre *resultat) {
  int *indices = malloc((t->nb_especes > 0 ? t->nb_especes : 1) * sizeof *indices);
  for (int i = 0; i < t->nb_especes; i++) {
    indices[i] = i;
  }
  int erreur = construire_rec(t, indices, t->nb_especes, resultat);
  free(indices);
  return erreur;
}

static int compter_feuilles(const noeud *n) {
  if (n == NULL) {
    return 0;
  }
  if (n->gauche == NULL && n->droit == NULL) {
    return 1;
  }
  return compter_feuilles(n->gauche) + compter_feuilles(n->droit);
}

int arbre_respecte_table(arbre a, const table *t) {
  if (compter_feuilles(a) != t->nb_especes) {
    return 0;
  }
  for (int i = 0; i < t->nb_especes; i++) {
    const noeud *n = a;
    while (n != NULL && (n->gauche != NULL || n->droit != NULL)) {
      int k = indice_carac(t, n->valeur);
      if (k < 0) {
        return 0;
      }
      n = valeur(t, i, k) ? n->droit : n->gauche;
    }
    if (n == NULL || strcmp(n->valeur, t->especes[i]) != 0) {
      return 0;
    }
  }
  return 1;
}

void liberer_arbre(arbre a) {
  if (a == NULL) {
    return;
  }
  liberer_arbre(a->gauche);
  liberer_arbre(a->droit);
  free(a->valeur);
  free(a);
}
