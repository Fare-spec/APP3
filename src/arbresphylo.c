#include "arbresphylo.h"
#include "arbres.h"
#include "listes.h"
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

static int rec_search(const noeud *n, char *espece, liste_t *seq) {
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

/* ACTE II */
/* Recherche l'espece dans l'arbre. Modifie la liste passée en paramètre pour y
 * mettre les caractéristiques. Retourne 0 si l'espèce a été retrouvée, 1 sinon.
 */
int rechercher_espece(arbre racine, char *espece, liste_t *seq) {

  return rec_search(racine, espece, seq);
}

/* Doit renvoyer 0 si l'espece a bien ete ajoutee, 1 sinon, et ecrire un
 * message d'erreur.
 */
int ajouter_espece(arbre *a, char *espece, cellule_t *seq) { return 1; }

/* Doit afficher la liste des caractéristiques niveau par niveau, de gauche
 * à droite, dans le fichier fout.
 * Appeler la fonction avec fout=stdin pour afficher sur la sortie standard.
 */
void afficher_par_niveau(arbre racine, FILE *fout) {
  printf("<<<<< À faire: fonction afficher_par_niveau fichier " __FILE__
         " >>>>>\n");
}

// Acte 4

int ajouter_carac(arbre *a, char *carac, cellule_t *seq) {
  printf("<<<<< À faire: fonction ajouter_carac fichier " __FILE__ " >>>>>\n");
  return 0;
}
