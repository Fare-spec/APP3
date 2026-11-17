#!/usr/bin/env python3
import html
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = "debut"


def charger(chemin):
    carte = {}
    for ligne in chemin.read_text(encoding="utf-8").splitlines():
        parts = ligne.split("\t", 2)
        if len(parts) < 3:
            continue
        pid, opts, texte = parts
        carte[pid] = ([o for o in opts.split(",") if o], texte.replace("\\n", "\n"))
    return carte


def couleur(pid, opts, texte):
    if pid == ROOT:
        return "lightblue"
    if not opts and "gagné" in texte:
        return "gold"
    if not opts:
        return "mistyrose"
    return "white"


def ecrire_dot(carte, sortie):
    lignes = ["digraph carte {", "  rankdir=TB;", "  node [shape=box, fontsize=10];"]
    for pid, (opts, texte) in carte.items():
        court = texte.split("\n")[0][:60].replace('"', "'").replace("\\", "/")
        etiquette = "debut" if pid == ROOT else f"{pid}\\n{court}"
        lignes.append(
            f'  "{pid}" [label="{etiquette}", style=filled, fillcolor={couleur(pid, opts, texte)}];'
        )
    for pid, (opts, _) in carte.items():
        for i, option in enumerate(opts, 1):
            if option in carte:
                lignes.append(f'  "{pid}" -> "{option}" [label="{i}"];')
    lignes.append("}")
    sortie.write_text("\n".join(lignes), encoding="utf-8")


def noeud_html(carte, pid, profondeur):
    opts, texte = carte[pid]
    classe = "debut" if pid == ROOT else ("gagne" if (not opts and "gagné" in texte) else ("fin" if not opts else "choix"))
    premiere = html.escape(texte.split("\n")[0][:100])
    titre = f'<span class="{classe}" title="{html.escape(texte)}">{html.escape(pid)} — {premiere}</span>'
    enfants = [o for o in opts if o in carte]
    if not enfants:
        return f"<li>{titre}</li>"
    ouvert = " open" if profondeur < 3 else ""
    contenu = "".join(noeud_html(carte, o, profondeur + 1) for o in enfants)
    return f"<li><details{ouvert}><summary>{titre}</summary><ul>{contenu}</ul></details></li>"


def ecrire_html(carte, sortie):
    fins = [pid for pid, (opts, _) in carte.items() if not opts]
    gagnes = [pid for pid in fins if "gagné" in carte[pid][1]]
    entete = (
        f"<p>{len(carte)} pages, {len(fins)} fins, dont {len(gagnes)} avec « gagné ». "
        "Cliquez sur une ligne pour replier ou déplier. Survolez un texte pour voir la page complète.</p>"
    )
    style = """
body { font-family: sans-serif; font-size: 14px; }
ul { list-style: none; padding-left: 1.2em; border-left: 1px dotted #999; }
summary { cursor: pointer; }
.debut { font-weight: bold; color: #1a4d8f; }
.choix { color: #222; }
.fin { color: #a33; }
.gagne { background: gold; font-weight: bold; }
"""
    corps = f"<ul>{noeud_html(carte, ROOT, 0)}</ul>" if ROOT in carte else "<p>Carte vide.</p>"
    sortie.write_text(
        f"<!doctype html><html><head><meta charset='utf-8'><title>Carte</title>"
        f"<style>{style}</style></head><body>{entete}{corps}</body></html>",
        encoding="utf-8",
    )


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("carte.txt")
    carte = charger(source)
    dot = source.with_suffix(".dot")
    page = source.with_suffix(".html")
    ecrire_dot(carte, dot)
    ecrire_html(carte, page)
    print(f"{len(carte)} pages -> {dot} et {page}")
    if shutil.which("dot"):
        svg = source.with_suffix(".svg")
        subprocess.run(["dot", "-Tsvg", str(dot), "-o", str(svg)], check=True)
        print(f"SVG -> {svg}")
    else:
        print("(Graphviz 'dot' absent : ouvrez le fichier .html, ou installez graphviz)")


if __name__ == "__main__":
    main()
