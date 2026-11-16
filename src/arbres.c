#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arbres.h"

noeud *nouveau_noeud(void) {
  noeud *n = (noeud *)malloc(sizeof(noeud));
  assert(n != NULL);
  n->valeur = NULL;
  n->gauche = NULL;
  n->droit = NULL;
  return n;
}

/* buffer pour lire les caractères des espèces sous forme de "mots" (words) */
#define MAX_WORD_SIZE 255
char buffer[MAX_WORD_SIZE + 1];

/* Variable globale qui contient le prochain caractère à traiter */
static char next_char = ' ';

/* Supprime tous les espaces, tabulations, retour à la ligne */
#define GLOB(f)                                                                \
  while (isspace(next_char)) {                                                 \
    next_char = fgetc(f);                                                      \
  }

/* Fonction récursive qui lit un sous-arbre */
/* Appelée une fois à la racine (debut du fichier), puis récursivement
 * pour chaque nœud interne rencontré. */
arbre lire_arbre(FILE *f) {
  arbre racine;

  GLOB(f); /* lit dans next_char le premier caractère non vide */

  if (next_char == '/') {
    next_char = ' '; /* on ne garde pas '/' en mémoire */
    return NULL;
  }

  if (next_char == ')') {
    return NULL;
  }

  if (next_char != '(') {
    fprintf(stderr,
            "Error while reading binary tree : '(' or ')' expected at "
            "position %ld\n",
            ftell(f));
    exit(1);
  }

  /* On remplit le buffer tant qu'on lit des caractères alphanumériques */
  char *p = buffer; /* début du buffer */
  next_char = ' ';
  GLOB(f);

  do {
    *p = next_char; /* sauvegarde du char courant */
    next_char = fgetc(f);
    p++;
    assert(p < buffer + MAX_WORD_SIZE);
  } while (!isspace(next_char) && next_char != '(' && next_char != ')');
  /* on arrète si le char suivant est un espace ou une parenthèse */
  *p = '\0'; /* on ferme la chaîne de caractères dans le buffer */

  racine = nouveau_noeud();
  racine->valeur = strdup(buffer); /* dupliquer le mot lu */

  GLOB(f);

  if (next_char == ')') {
    next_char = ' '; /* on est sur une feuille, on prépare la lecture du
                        prochain nœud */
  } else {
    racine->gauche = lire_arbre(f); /* appel récursif pour le fils gauche */
    racine->droit = lire_arbre(f);  /* idem pour le droit */

    GLOB(f); /* lit jusqu'au ')' fermant */

    if (next_char != ')') {
      fprintf(stderr, "Error while reading binary tree: ')' expected\n");
      exit(1);
    }
    next_char = ' '; /* on ne garde pas la parenthèse en mémoire */
  }
  return racine;
}

int depth_tree(const noeud *racine) {
  if (racine == NULL) {
    return 0;
  }
  int g = depth_tree(racine->gauche);
  int d = depth_tree(racine->droit);
  return 1 + (g > d ? g : d);
}

static void affiche_sous_arbre(noeud *n, const char *prefixe,
                               const char *etiquette, bool dernier,
                               bool est_racine) {
  /* this was a pain, enjoy */
  if (est_racine) {
    printf("%s\n", n == NULL ? "/" : n->valeur);
  } else {
    printf("%s%s%s%s\n", prefixe, dernier ? "└── " : "├── ", etiquette,
           n == NULL ? "/" : n->valeur);
  }

  if (n == NULL || (n->gauche == NULL && n->droit == NULL)) {
    return;
  }

  const char *ajout = est_racine ? "" : (dernier ? "    " : "│   ");
  size_t taille = strlen(prefixe) + strlen(ajout) + 1;
  char *nouveau = malloc(taille);
  assert(nouveau != NULL);
  snprintf(nouveau, taille, "%s%s", prefixe, ajout);

  affiche_sous_arbre(n->gauche, nouveau, "non: ", false, false);
  affiche_sous_arbre(n->droit, nouveau, "oui: ", true, false);

  free(nouveau);
}

void affiche_arbre(noeud *racine) {
  if (racine == NULL) {
    printf("(arbre vide)\n");
    return;
  }
  affiche_sous_arbre(racine, "", "", true, true);
}
