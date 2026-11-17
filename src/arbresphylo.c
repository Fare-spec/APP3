#include "arbresphylo.h"
#include "arbres.h"
#include "listes.h"
#include "queue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void rec_analyse(const noeud *n, int *nb_esp, int *nb_carac) {
  if (n == NULL) {
    return;
  }
  if (n->gauche == NULL && n->droit == NULL) {
    (*nb_esp)++;
  } else {
    (*nb_carac)++;
    rec_analyse(n->gauche, nb_esp, nb_carac);
    rec_analyse(n->droit, nb_esp, nb_carac);
  }
}

void analyse_arbre(arbre racine, int *nb_esp, int *nb_carac) {
  *nb_carac = 0, *nb_esp = 0;
  rec_analyse(racine, nb_esp, nb_carac);
}

static int rec_search(const noeud *n, const char *espece, liste_t *seq) {
  int g = 1, d = 1;
  if (n == NULL) {
    return 1;
  }
  if (n->gauche == NULL && n->droit == NULL) {
    return !(strcmp(espece, n->valeur) == 0);
  } else {
    g = rec_search(n->gauche, espece, seq);
    if (g) {
      d = rec_search(n->droit, espece, seq);
    }
  }

  if (g == 0) {
    return 0;
  }
  if (d == 0) {
    ajouter_tete(seq, n->valeur);
    return 0;
  }
  return 1;
}

int rechercher_espece(arbre racine, char *espece, liste_t *seq) {

  return rec_search(racine, espece, seq);
}
/* Doit renvoyer 0 si l'espece a bien ete ajoutee, 1 sinon, et ecrire un
 * message d'erreur.
 */
static int is_leaf(const noeud *n) {
  if (n->droit == NULL && n->gauche == NULL) {
    return 1;
  }
  return 0;
}
static arbre create_tree(const char *specie, const cellule_t *seq) {
  if (seq == NULL) {
    noeud *n = nouveau_noeud();
    n->valeur = strdup(specie);
    return n;
  } else {

    noeud *n = nouveau_noeud();
    n->valeur = strdup(seq->val);
    n->droit = create_tree(specie, seq->suivant);
    return n;
  }
}

static int add_specie(arbre *a, const char *specie, const cellule_t *seq) {
  if (*a == NULL) {
    (*a) = create_tree(specie, seq);
  } else {
    if (is_leaf(*a)) {
      if (seq == NULL) {
        printf("Cannot insert an empty sequence to the existing leaf.");
        return 1;
      }
      arbre left = (*a);

      (*a) = create_tree(specie, seq);
      (*a)->gauche = left;
      return 0;
    }
    return !(seq == NULL) && strcmp((*a)->valeur, seq->val) == 0
               ? add_specie(&(*a)->droit, specie, seq->suivant)
               : add_specie(&(*a)->gauche, specie, seq);
  }
  return 0;
}

int ajouter_espece(arbre *a, char *espece, cellule_t *seq) {
  return add_specie(a, espece, seq);
}

/* Doit afficher la liste des caractéristiques niveau par niveau, de gauche
 * à droite, dans le fichier fout.
 * Appeler la fonction avec fout=stdin pour afficher sur la sortie standard.
 */
void afficher_par_niveau(arbre racine, FILE *fout) {
  if (racine == NULL) {
    return;
  }
  int nb_spec, nb_car;
  analyse_arbre(racine, &nb_spec, &nb_car);
  queue q = init_q(nb_car + nb_spec);
  push(&q, racine);

  while (!empty_queue(&q)) {
    int j = (q.tail - q.head);
    int leaf = 0;
    for (int i = 0; i < j; i++) {
      noeud *n = pop(&q);
      if (!is_leaf(n)) {
        fprintf(fout, "%s ", n->valeur);
        leaf = 1;
      }

      if (!(n->gauche == NULL)) {

        push(&q, n->gauche);
      }

      if (!(n->droit == NULL)) {

        push(&q, n->droit);
      }
    }
    if (leaf) {
      fprintf(fout, "\n");
    }
  }
  free_queue(&q);
  return;
}

// Acte 4

int ajouter_carac(arbre *a, char *carac, cellule_t *seq) {
  printf("<<<<< À faire: fonction ajouter_carac fichier " __FILE__ " >>>>>\n");
  return 0;
}
