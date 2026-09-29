#!/usr/bin/env python3
"""Test aléatoire : compare l'analyse théorique et la simulation sur des
jeux de tâches générés au hasard (graine fixe -> résultat reproductible).

Seuls des jeux avec U <= 1 sont tirés (rejet des autres) : avec U > 1, le
verdict « non faisable » est trivial.

Vérifie, pour chaque jeu :
  1. HPF, RM, DM, EDF : verdict théorique == verdict simulé (préemptif) ;
  2. EDF : si une échéance est ratée, la 1re l'est dans la période active ;
  3. EDF : R de Spuri >= pire temps de réponse observé sur l'hyperpériode
     (ligne « Contrôle Spuri » du programme, sans ATTENTION) ;
  4. le contrôle interne "temps de réponse simulés == calculés" ne signale rien.

Usage : python3 tests/test_aleatoire.py [nombre_de_jeux] [graine]
"""
import math
import os
import random
import re
import subprocess
import sys
import tempfile
from fractions import Fraction

NB = int(sys.argv[1]) if len(sys.argv) > 1 else 300
random.seed(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
PERIODES = [4, 5, 6, 8, 10, 12, 15, 20]
LIGNE = re.compile(r'^\| (HPF|RM|DM|EDF) +\| (.+?) +\| (.+?) +\| (.+?) +\|$', re.M)


def tirer_jeu():
    """Tire un jeu de 2 à 5 tâches avec U <= 1 (on recommence sinon)."""
    while True:
        d_egal_t = random.random() < 0.5
        taches = []
        for _ in range(random.randint(2, 5)):
            T = random.choice(PERIODES)
            D = T if d_egal_t else random.randint(1, T)
            C = random.randint(1, D)
            taches.append((C, D, T, random.randint(1, 50)))
        U = sum(Fraction(C, T) for C, _, T, _ in taches)
        if U <= 1:
            return taches, U, d_egal_t


erreurs, cas = 0, 0
stats = {'faisable': 0, 'non': 0, 'non_trivial': 0, 'spuri_egal': 0, 'spuri_sup': 0}
with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
    chemin = f.name

try:
    for k in range(NB):
        taches, U, d_egal_t = tirer_jeu()
        n = len(taches)
        # non trivial : la charge seule ne permet pas de conclure
        if not d_egal_t or U > n * (2 ** (1 / n) - 1):
            stats['non_trivial'] += 1
        lignes = [f"t{i} {C} {D} {T} {p}" for i, (C, D, T, p) in enumerate(taches)]
        with open(chemin, 'w') as f:
            f.write("\n".join(lignes) + "\n")
        sortie = subprocess.run(['./ordo', chemin], capture_output=True, text=True).stdout

        def erreur(msg):
            global erreurs
            erreurs += 1
            print(f"[jeu {k}] {msg} :", " | ".join(lignes))

        if 'ATTENTION' in sortie or 'ANOMALIE' in sortie:
            erreur("contrôle interne en échec")
        m = re.search(r'Période active L = (\d+)', sortie)
        periode_active = int(m.group(1)) if m else None
        m = re.search(r'Contrôle Spuri : R \(Spuri\) (\S+)', sortie)
        if m and m.group(1) == '==':
            stats['spuri_egal'] += 1
        elif m and m.group(1) == '>=':
            stats['spuri_sup'] += 1

        for pol, theorie, sim_p, _ in LIGNE.findall(sortie):
            cas += 1
            sim_ok = sim_p == 'FAISABLE'
            stats['faisable' if sim_ok else 'non'] += 1
            if theorie not in ('FAISABLE', 'NON FAISABLE'):
                erreur(f"{pol} : théorie indécise ({theorie})")
            elif (theorie == 'FAISABLE') != sim_ok:
                erreur(f"{pol} : théorie {theorie} / simulation {sim_p}")
            if pol == 'EDF' and not sim_ok and periode_active is not None:
                t_echec = int(re.search(r't=(\d+)', sim_p).group(1))
                if t_echec > periode_active:
                    erreur(f"EDF : 1er échec à t={t_echec} après la période active {periode_active}")
finally:
    os.unlink(chemin)

print(f"{NB} jeux (tous avec U <= 1), dont {stats['non_trivial']} non triviaux "
      f"(la charge seule ne permet pas de conclure)")
print(f"{cas} cas (politique x jeu) : {erreurs} erreur(s) "
      f"[simulations faisables : {stats['faisable']}, non faisables : {stats['non']}]")
print(f"EDF, contrôle Spuri : R == pire observé pour {stats['spuri_egal']} jeux, "
      f"R > pire observé pour {stats['spuri_sup']} (pire scénario de Spuri absent du déroulement synchrone)")
sys.exit(1 if erreurs else 0)
