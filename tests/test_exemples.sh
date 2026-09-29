#!/bin/sh
# Tests de non-régression : pour chaque exemple, le tableau récapitulatif
# doit être exactement celui attendu (résultats du cours ou calcul à la main).
# Usage : make test   (ou : sh tests/test_exemples.sh depuis la racine)

ok=0; ko=0

verifie() {  # $1 = fichier d'exemple, $2 = lignes attendues du récapitulatif
    obtenu=$(./ordo "exemples/$1" | grep -E '^\| (HPF|RM|DM|EDF) ')
    if [ "$obtenu" = "$2" ]; then
        echo "OK    $1"; ok=$((ok + 1))
    else
        echo "ECHEC $1"; echo "  attendu :"; echo "$2"; echo "  obtenu :"; echo "$obtenu"
        ko=$((ko + 1))
    fi
}

verifie taches_tp.txt "\
| HPF       | NON FAISABLE   | NON (t=13, Thread3) | FAISABLE            |
| RM        | NON FAISABLE   | NON (t=13, Thread3) | FAISABLE            |
| DM        | NON FAISABLE   | NON (t=13, Thread3) | FAISABLE            |
| EDF       | FAISABLE       | FAISABLE            | FAISABLE            |"

verifie cours_cas2.txt "\
| HPF       | FAISABLE       | FAISABLE            | FAISABLE            |
| RM        | FAISABLE       | FAISABLE            | FAISABLE            |
| DM        | FAISABLE       | FAISABLE            | FAISABLE            |
| EDF       | FAISABLE       | FAISABLE            | FAISABLE            |"

verifie cours_optimalite.txt "\
| HPF       | NON FAISABLE   | NON (t=4, tau1)     | NON (t=4, tau1)     |
| RM        | NON FAISABLE   | NON (t=4, tau1)     | NON (t=4, tau1)     |
| DM        | FAISABLE       | FAISABLE            | FAISABLE            |
| EDF       | FAISABLE       | FAISABLE            | FAISABLE            |"

verifie cours_edf_p134.txt "\
| HPF       | FAISABLE       | FAISABLE            | FAISABLE            |
| RM        | FAISABLE       | FAISABLE            | FAISABLE            |
| DM        | FAISABLE       | FAISABLE            | FAISABLE            |
| EDF       | FAISABLE       | FAISABLE            | FAISABLE            |"

verifie non_preemptif.txt "\
| HPF       | FAISABLE       | FAISABLE            | NON (t=6, tau1)     |
| RM        | FAISABLE       | FAISABLE            | NON (t=6, tau1)     |
| DM        | FAISABLE       | FAISABLE            | NON (t=6, tau1)     |
| EDF       | FAISABLE       | FAISABLE            | NON (t=6, tau1)     |"

verifie priorites_inversees.txt "\
| HPF       | NON FAISABLE   | NON (t=7, Thread1)  | NON (t=7, Thread1)  |
| RM        | NON FAISABLE   | NON (t=13, Thread3) | FAISABLE            |
| DM        | NON FAISABLE   | NON (t=13, Thread3) | FAISABLE            |
| EDF       | FAISABLE       | FAISABLE            | FAISABLE            |"

verifie surcharge.txt "\
| HPF       | NON FAISABLE   | NON (t=6, B)        | NON (t=8, A)        |
| RM        | NON FAISABLE   | NON (t=6, B)        | NON (t=8, A)        |
| DM        | NON FAISABLE   | NON (t=6, B)        | NON (t=8, A)        |
| EDF       | NON FAISABLE   | NON (t=8, A)        | NON (t=8, A)        |"

verifie charge_exactement_1.txt "\
| HPF       | FAISABLE       | FAISABLE            | FAISABLE            |
| RM        | FAISABLE       | FAISABLE            | FAISABLE            |
| DM        | FAISABLE       | FAISABLE            | FAISABLE            |
| EDF       | FAISABLE       | FAISABLE            | FAISABLE            |"

# Temps de réponse de l'énoncé et trace EDF du cours p.134
if ./ordo exemples/taches_tp.txt -p hpf -v | grep -q "r(5) = 17" \
   && ./ordo exemples/cours_edf_p134.txt -p edf -v -a 28 | grep "fin de tau2" \
      | sed 's/.*réponse \([0-9]*\).*/\1/' | tr '\n' ' ' | grep -q "^5 5 5 4 $"; then
    echo "OK    temps de réponse (R3 = 17 ; tau2 EDF = 5 5 5 4)"; ok=$((ok + 1))
else
    echo "ECHEC temps de réponse"; ko=$((ko + 1))
fi

# EDF, méthode de Spuri (cours p.133) : pires temps de réponse R de chaque tâche,
# puis pires temps de réponse observés dans la simulation sur l'hyperpériode
spuri() {  # $1 = fichier, $2 = R attendus, $3 = pires cas observés attendus
    R=$(./ordo "exemples/$1" -p edf | grep -- '-> R = ' | sed 's/.*R = \([0-9]*\).*/\1/' | tr '\n' ' ')
    obs=$(./ordo "exemples/$1" -p edf | grep 'Pire temps de réponse observé' | sed 's/.*: //')
    if [ "$R" = "$2" ] && [ "$obs" = "$3" ]; then
        echo "OK    Spuri $1 (R = $2; observé : $3)"; ok=$((ok + 1))
    else
        echo "ECHEC Spuri $1 : R = '$R' (attendu '$2'), observé '$obs' (attendu '$3')"
        ko=$((ko + 1))
    fi
}
spuri taches_tp.txt      "5 9 11 " "Thread1=5 Thread2=9 Thread3=10"
spuri cours_edf_p134.txt "3 6 "    "tau1=3 tau2=5"

echo "---- $ok réussi(s), $ko échec(s)"
[ "$ko" -eq 0 ]
