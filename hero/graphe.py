#!/usr/bin/env python3
import json
import sys
from pathlib import Path

ROOT = "debut"
X_STEP = 130
Y_STEP = 90


def charger(chemin):
    carte = {}
    for ligne in chemin.read_text(encoding="utf-8").splitlines():
        parts = ligne.split("\t", 2)
        if len(parts) < 3:
            continue
        pid, opts, texte = parts
        carte[pid] = ([o for o in opts.split(",") if o], texte.replace("\\n", "\n"))
    return carte


def classe(pid, opts, texte):
    if pid == ROOT:
        return "debut"
    if not opts and "gagné" in texte:
        return "gagne"
    if not opts:
        return "fin"
    return "choix"


def disposition(carte):
    positions = {}
    vus = set()
    compteur = [0]

    def placer(pid, profondeur):
        vus.add(pid)
        enfants = [o for o in carte[pid][0] if o in carte and o not in vus]
        if not enfants:
            x = compteur[0] * X_STEP
            compteur[0] += 1
        else:
            x = sum(placer(o, profondeur + 1) for o in enfants) / len(enfants)
        positions[pid] = (x, profondeur * Y_STEP)
        return x

    placer(ROOT, 0)
    return positions


def donnees(carte):
    positions = disposition(carte)
    parents = {}
    for pid, (opts, _) in carte.items():
        for o in opts:
            if o in carte:
                parents.setdefault(o, pid)
    out = {}
    for pid, (opts, texte) in carte.items():
        if pid not in positions:
            continue
        x, y = positions[pid]
        out[pid] = {
            "x": x,
            "y": y,
            "texte": texte,
            "classe": classe(pid, opts, texte),
            "parent": parents.get(pid),
            "enfants": [o for o in opts if o in carte and parents.get(o) == pid],
        }
    return out


MODELE = """<!doctype html>
<html lang="fr"><head><meta charset="utf-8"><title>Carte interactive</title>
<style>
body { margin: 0; font-family: sans-serif; display: flex; height: 100vh; }
#svg { flex: 1; background: #fafafa; cursor: grab; user-select: none; }
#svg.drag { cursor: grabbing; }
#panneau { width: 340px; padding: 10px; border-left: 1px solid #ccc; overflow: auto; font-size: 13px; }
line { stroke: #999; stroke-width: 1.5; }
.noeud { cursor: pointer; }
.noeud rect { stroke: #555; stroke-width: 1; }
.noeud text { pointer-events: none; }
.debut rect { fill: #cfe3ff; }
.choix rect { fill: #ffffff; }
.fin rect { fill: #fbe3e3; }
.gagne rect { fill: gold; }
.chemin rect { stroke: #d33; stroke-width: 3; }
.sel rect { stroke: #000; stroke-width: 3; }
.replie text { font-style: italic; }
</style></head>
<body>
<svg id="svg"><g id="vue"></g></svg>
<div id="panneau">
  <p><b>Carte interactive</b><br>
  Molette : zoom. Glisser : déplacer. Clic : voir la page. Double-clic : replier ou déplier une branche.</p>
  <div id="info"><p>Cliquez sur une page.</p></div>
</div>
<script>
const DONNEES = __DONNEES__;
const DEBUT = "__ROOT__";
const svg = document.getElementById("svg");
const vue = document.getElementById("vue");
const info = document.getElementById("info");
const LARGEUR = 110, HAUTEUR = 34;
const repliés = new Set();
let selection = null;
let vue_t = { x: 0, y: 0, k: 0.8 };

function echapper(s) {
  return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

function visibles() {
  const out = new Set();
  const pile = [DEBUT];
  while (pile.length) {
    const id = pile.pop();
    out.add(id);
    if (!repliés.has(id)) for (const c of DONNEES[id].enfants) pile.push(c);
  }
  return out;
}

function chemin(id) {
  const c = [];
  let cur = id;
  while (cur) { c.push(cur); cur = DONNEES[cur].parent; }
  return c.reverse();
}

function rendre() {
  const vis = visibles();
  const surChemin = new Set(selection ? chemin(selection) : []);
  let html = "";
  for (const id of vis) {
    const n = DONNEES[id];
    if (n.parent && vis.has(n.parent)) {
      const p = DONNEES[n.parent];
      html += `<line x1="${p.x}" y1="${p.y}" x2="${n.x}" y2="${n.y}"/>`;
    }
  }
  for (const id of vis) {
    const n = DONNEES[id];
    const cls = [n.classe, id === selection ? "sel" : "", surChemin.has(id) ? "chemin" : "", repliés.has(id) ? "replie" : ""].join(" ");
    const marque = repliés.has(id) && n.enfants.length ? " [+]" : "";
    html += `<g class="noeud ${cls}" data-id="${echapper(id)}" transform="translate(${n.x - LARGEUR / 2},${n.y - HAUTEUR / 2})">` +
      `<rect width="${LARGEUR}" height="${HAUTEUR}" rx="6"/>` +
      `<text x="${LARGEUR / 2}" y="${HAUTEUR / 2 + 4}" text-anchor="middle" font-size="11">${echapper(id)}${marque}</text></g>`;
  }
  vue.innerHTML = html;
  appliquer();
}

function appliquer() {
  vue.setAttribute("transform", `translate(${vue_t.x},${vue_t.y}) scale(${vue_t.k})`);
}

function afficher() {
  if (!selection) return;
  const n = DONNEES[selection];
  const chem = chemin(selection).join(" → ");
  const suites = n.enfants.length ? `Choix : ${n.enfants.join(" / ")}` : "Fin de l'histoire";
  info.innerHTML = `<h3>${echapper(selection)}</h3>` +
    `<p>${echapper(n.texte).replace(/\\n/g, "<br>")}</p>` +
    `<p><b>${suites}</b></p>` +
    `<p>Chemin depuis le début : ${echapper(chem)}</p>`;
}

svg.addEventListener("click", (e) => {
  const g = e.target.closest(".noeud");
  if (!g) return;
  selection = g.dataset.id;
  afficher();
  rendre();
});

svg.addEventListener("dblclick", (e) => {
  const g = e.target.closest(".noeud");
  if (!g) return;
  const id = g.dataset.id;
  if (!DONNEES[id].enfants.length) return;
  if (repliés.has(id)) repliés.delete(id); else repliés.add(id);
  rendre();
});

svg.addEventListener("wheel", (e) => {
  e.preventDefault();
  const f = e.deltaY < 0 ? 1.15 : 1 / 1.15;
  const r = svg.getBoundingClientRect();
  const mx = e.clientX - r.left, my = e.clientY - r.top;
  vue_t.x = mx - (mx - vue_t.x) * f;
  vue_t.y = my - (my - vue_t.y) * f;
  vue_t.k *= f;
  appliquer();
}, { passive: false });

let glisse = null;
svg.addEventListener("mousedown", (e) => {
  if (e.target.closest(".noeud")) return;
  glisse = { x: e.clientX, y: e.clientY };
  svg.classList.add("drag");
});
window.addEventListener("mousemove", (e) => {
  if (!glisse) return;
  vue_t.x += e.clientX - glisse.x;
  vue_t.y += e.clientY - glisse.y;
  glisse = { x: e.clientX, y: e.clientY };
  appliquer();
});
window.addEventListener("mouseup", () => {
  glisse = null;
  svg.classList.remove("drag");
});

const racine = DONNEES[DEBUT];
vue_t.x = svg.clientWidth / 2 - racine.x * vue_t.k;
vue_t.y = 40;
rendre();
</script>
</body></html>
"""


def ecrire(carte, sortie):
    json_donnees = json.dumps(donnees(carte), ensure_ascii=False).replace("</", "<\\/")
    page = MODELE.replace("__DONNEES__", json_donnees).replace("__ROOT__", ROOT)
    sortie.write_text(page, encoding="utf-8")


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("carte.txt")
    carte = charger(source)
    sortie = source.with_name("carte_interactive.html")
    ecrire(carte, sortie)
    print(f"{len(carte)} pages -> {sortie}")


if __name__ == "__main__":
    main()
