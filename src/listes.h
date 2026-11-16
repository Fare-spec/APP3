#ifndef _LISTES_H
#define _LISTES_H

typedef struct liste liste_t;

typedef char *string;

struct cellule {
  string val;
  struct cellule *suivant;
};

typedef struct cellule cellule_t;

struct liste {
  cellule_t *tete;
};

typedef struct liste liste_t;

void init_liste_vide(liste_t *L);

void liberer_liste(liste_t *L);

int ajouter_tete(liste_t *L, string c);

void helper(const liste_t *L);

#endif /* _LISTES_H */
