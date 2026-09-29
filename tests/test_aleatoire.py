#!/usr/bin/env python3
"""Test aléatoire : compare l'analyse théorique et la simulation sur des
jeux de tâches générés au hasard (graine fixe -> résultat reproductible).

Vérifie, pour chaque jeu :
  1. priorités fixes (HPF, RM, DM) : verdict théorique == verdict simulé (préemptif) ;
  2. EDF : quand la théorie conclut, elle est d'accord avec la simulation ;
  3. EDF : si une échéance est ratée alors que U <= 1, c'est dans la période active ;
  4. le contrôle interne "temps de réponse simulés == calculés" ne signale rien.

Usage : python3 tests/test_aleatoire.py [nombre_de_jeux] [graine]
"""
import os
import random
import re
import subprocess
import sys
import tempfile

NB = int(sys.argv[1]) if len(sys.argv) > 1 else 300
random.seed(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
PERIODES = [4, 5, 6, 8, 10, 12, 15, 20]
LIGNE = re.compile(r'^\| (HPF|RM|DM|EDF) +\| (.+?) +\| (.+?) +\| (.+?) +\|$', re.M)

erreurs, cas, stats = 0, 0, {'faisable': 0, 'non': 0}
with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
    chemin = f.name

try:
    for k in range(NB):
        d_egal_t = random.random() < 0.5
        lignes = []
        for i in range(random.randint(2, 5)):
            T = random.choice(PERIODES)
            D = T if d_egal_t else random.randint(1, T)
            C = random.randint(1, D)
            lignes.append(f"t{i} {C} {D} {T} {random.randint(1, 50)}")
        with open(chemin, 'w') as f:
            f.write("\n".join(lignes) + "\n")
        sortie = subprocess.run(['./ordo', chemin], capture_output=True, text=True).stdout

        def erreur(msg):
            global erreurs
            erreurs += 1
            print(f"[jeu {k}] {msg} :", " | ".join(lignes))

        if 'ATTENTION' in sortie:
            erreur("contrôle interne en échec")
        m = re.search(r'Période active = (\d+)', sortie)
        periode_active = int(m.group(1)) if m else None

        for pol, theorie, sim_p, _ in LIGNE.findall(sortie):
            cas += 1
            sim_ok = sim_p == 'FAISABLE'
            stats['faisable' if sim_ok else 'non'] += 1
            if theorie in ('FAISABLE', 'NON FAISABLE') and (theorie == 'FAISABLE') != sim_ok:
                erreur(f"{pol} : théorie {theorie} / simulation {sim_p}")
            if pol == 'EDF' and not sim_ok and periode_active is not None:
                t_echec = int(re.search(r't=(\d+)', sim_p).group(1))
                if t_echec > periode_active:
                    erreur(f"EDF : 1er échec à t={t_echec} après la période active {periode_active}")
finally:
    os.unlink(chemin)

print(f"{NB} jeux, {cas} cas (politique x jeu) : {erreurs} erreur(s) "
      f"[simulations faisables : {stats['faisable']}, non faisables : {stats['non']}]")
sys.exit(1 if erreurs else 0)
