# TP Temps réel — Exercices 1 et 2

ST2STR – Systèmes Temps-Réel (M. Bougueroua)

Programme en **C** qui :
- **analyse la faisabilité** d'un ensemble de n tâches périodiques avec des priorités fixes (HPF, RM, DM), à partir de la charge U et des temps de réponse (Exercice 1) ;
- **simule** leur ordonnancement en mode préemptif et non préemptif, puis trace le résultat : chronogramme et journal des événements (Exercice 1 Q4) ;
- propose l'ordonnanceur dynamique **EDF**, avec son analyse et sa simulation tracée (Exercice 2).

---

## 1. Compilation et utilisation

```bash
make                                   # compile -> ./ordo
./ordo exemples/taches_tp.txt          # les 4 politiques + récapitulatif
make exo1                              # démo exercice 1
make exo2                              # démo exercice 2 (EDF, trace détaillée)
make test                              # tests : exemples du cours + 300 jeux aléatoires
make sanitize                          # exemples rejoués avec détection d'erreurs mémoire (ASan/UBSan)
make resultats                         # regénère les sorties de référence de resultats/
```

Options :

| Option | Rôle |
|---|---|
| `-p hpf\|rm\|dm\|edf\|tout` | politique(s) étudiée(s) (défaut : `tout`) |
| `-n` | simulation détaillée en **non préemptif** (défaut : préemptif) |
| `-a N` | largeur du chronogramme et du journal, de 1 à 200 (défaut : 60) |
| `-d N` | durée de simulation, de 1 à 10⁶ (défaut : PPCM des périodes) |
| `-v` | détail : itérations des calculs et journal des événements |

Format d'un fichier de tâches (une tâche par ligne, `#` = commentaire) :

```
# nom       C   D   T   prio
Thread1     2   7   7   20
Thread2     3   11  11  15
Thread3     5   13  13  10
```

Les valeurs sont des entiers (unité de temps : la seconde) : C, D et T de 1 à 10⁶, la priorité est un entier quelconque. Le nom fait au plus 10 caractères.

**Pour HPF, une valeur plus grande signifie plus prioritaire.** C'est la convention du cours : p.93, la tâche de priorité 15 préempte celle de priorité 10. C'est aussi celle de POSIX `SCHED_FIFO` et de `PriorityParameters` en Java temps réel. Sur l'exemple de l'énoncé, cet ordre coïncide avec RM et DM : le verdict ne dépend donc pas de la convention choisie.

---

## 2. Organisation du code

| Fichier | Contenu |
|---|---|
| `tache.h/.c` | Modèle d'une tâche (C, D, T, P, et date de 1re activation S), lecture et contrôle du fichier |
| `analyse.h/.c` | Attribution des priorités (HPF/RM/DM), charge U (test exact), borne de Liu & Layland, temps de réponse, période active, analyse EDF (méthode de Spuri) |
| `simulateur.h/.c` | Simulateur en temps discret : files de jobs, élection, préemption, détection des dépassements, chronogramme, journal |
| `main.c` | Options de la ligne de commande, enchaînement analyse + simulation, tableau récapitulatif |
| `exemples/` | Jeux de tâches (énoncé et exemples du cours) |
| `resultats/` | Sorties de référence du programme (la 1re ligne de chaque fichier donne la commande) |
| `tests/` | `test_exemples.sh` (non-régression) et `test_aleatoire.py` (théorie ↔ simulation) |

Compilation stricte `-std=c11 -Wall -Wextra -pedantic` : **aucun avertissement**. Le programme est aussi compilé et testé avec `-fsanitize=address,undefined` (`make sanitize`) : aucune erreur.

---

## 3. Exercice 1 — Étude de faisabilité

### Q1. Faisabilité avec HPF

L'analyse suit les deux étapes du cours.

**Étape 1 : charge processeur (condition nécessaire)**

U = Σ Cᵢ/Tᵢ = 2/7 + 3/11 + 5/13 = **0,943**

- si U > 1 : non faisable. Ce test est fait **en entiers** (fractions exactes), sinon un ensemble dont U vaut exactement 1 serait rejeté à cause d'une erreur d'arrondi : en virgule flottante, 1/5 + 2/5 + 3/10 + 1/10 = 1,0000000000000002 (voir `exemples/charge_exactement_1.txt`) ;
- la borne de Liu & Layland U_RM = n(2^(1/n) − 1) = 3(2^(1/3) − 1) = **0,7798**. Le cours p.119 écrit 0,779 ; c'est la même valeur, arrondie différemment. C'est une condition suffisante **pour RM avec D = T**. Elle s'applique donc aussi à HPF ou DM quand D = T et que l'ordre obtenu est celui des périodes croissantes. C'est le cas ici : Thread1 > Thread2 > Thread3 est à la fois l'ordre HPF et l'ordre RM. Le programme le vérifie avant d'utiliser la borne ;
- ici 0,7798 < U ≤ 1 : **la charge ne permet pas de conclure** (même conclusion que le cours p.119).

**Étape 2 : temps de réponse (condition nécessaire et suffisante, D ≤ T)**

rᵢ⁽⁰⁾ = Cᵢ,  rᵢ⁽ⁿ⁺¹⁾ = Cᵢ + Σ_{j ∈ hp(i)} ⌈rᵢ⁽ⁿ⁾ / Tⱼ⌉ · Cⱼ, jusqu'à ce que rᵢ⁽ⁿ⁺¹⁾ = rᵢ⁽ⁿ⁾.

Pour Thread3 : 5 → 10 → 12 → 15 → 17 → 17 (option `-v` pour voir le détail).

| Tâche | C | D | T | Prio | R | R ≤ D ? |
|---|---|---|---|---|---|---|
| Thread1 | 2 | 7 | 7 | 20 | 2 | oui |
| Thread2 | 3 | 11 | 11 | 15 | 5 | oui |
| Thread3 | 5 | 13 | 13 | 10 | **17** | **NON** |

**⇒ L'ensemble n'est pas faisable avec HPF** : Thread3 finit à 17, soit après son échéance 13. Ce résultat est identique à celui du cours (p.122–124).

Remarque : comme R₃ = 17 > T₃ = 13, la formule, écrite pour R ≤ T, ne donne plus forcément le pire temps de réponse exact. Le 2ᵉ job de Thread3 démarre en retard. Mais R₃ > D₃ suffit à prouver la non-faisabilité, et c'est la seule conclusion qu'on en tire.

La simulation le confirme : première échéance ratée à t = 13. Le premier job de Thread3 se termine à t = 17, comme le prévoit le calcul :

```
t          0    5    10   15   20   25
Thread1    ##.....##.....##.....##.....##
Thread2    --###......###........-###....
Thread3    -----##--##-----#####-----##--
Depasse                 X            X
```
(`#` exécution, `-` prête mais en attente, `.` inactive, `X` échéance ratée)

### Q2. Nombre quelconque de tâches

Les tâches sont lues depuis un fichier, jusqu'à `TACHES_MAX` = 64. Rien n'est codé en dur, il suffit d'écrire un autre fichier. La lecture refuse les données incohérentes :
- C, D ou T hors de [1, 10⁶], ou priorité hors des entiers représentables. Les nombres sont lus avec `strtol` et contrôlés, pas avec `sscanf("%d")`, dont le comportement est indéfini en cas de dépassement ;
- **C > D** (la tâche ne peut jamais respecter son échéance) ;
- **D > T** (hors du modèle du cours) ;
- ligne mal formée : champ manquant ou en trop (un commentaire `#` en fin de ligne est accepté), nom de plus de 10 caractères, ligne de plus de 254 caractères.

### Q3. Autres ordonnanceurs statiques : RM et DM

Seule la règle d'attribution des priorités change, c'est-à-dire la fonction de tri. Le calcul de faisabilité reste le même.

| Politique | Règle | Égalité |
|---|---|---|
| HPF | plus grande importance d'abord | ordre du fichier |
| RM | plus petite période T d'abord | ordre du fichier |
| DM | plus petite échéance relative D d'abord | ordre du fichier |

Sur l'exemple de l'énoncé, D = T et les importances sont décroissantes avec T. **Les trois politiques donnent donc le même ordre, et le même verdict : non faisable.**

L'exemple `exemples/cours_optimalite.txt` (cours p.136, avec D < T) montre qu'elles peuvent donner des résultats différents :

| Politique | Résultat |
|---|---|
| RM | NON faisable (τ1 : R = 5 > D = 4) |
| DM | **faisable** (R = 2 et 5) |
| EDF | **faisable** |

C'est le résultat attendu : **DM est optimal parmi les priorités fixes lorsque D ≤ T**, alors que RM ne l'est que pour D = T.

### Q4 (optionnelle). Préemptif / non préemptif

Le simulateur prend les deux modes en charge :
- **préemptif** : on réélit la tâche à exécuter à chaque événement (activation ou fin d'un job). Une tâche plus prioritaire interrompt la tâche en cours ;
- **non préemptif** : on n'élit une nouvelle tâche que lorsque le processeur est libre. Un job commencé va jusqu'au bout.

Résultats sur l'exemple de l'énoncé (tableau récapitulatif du programme) :

| Politique | Théorie (préemptif) | Simulation préemptive | Simulation non préemptive |
|---|---|---|---|
| HPF | NON FAISABLE | NON (t=13, Thread3) | FAISABLE |
| RM | NON FAISABLE | NON (t=13, Thread3) | FAISABLE |
| DM | NON FAISABLE | NON (t=13, Thread3) | FAISABLE |
| EDF | FAISABLE | FAISABLE | FAISABLE |

Le test des temps de réponse du cours ne vaut que pour le préemptif. Le non préemptif est donc évalué par simulation. Ce verdict est exact pour ce système, où toutes les tâches sont activées ensemble à t = 0. Si les tâches pouvaient démarrer avec un décalage, le pire cas non préemptif pourrait être ailleurs : une tâche prioritaire arrive juste après le lancement d'une tâche longue moins prioritaire, et doit attendre qu'elle se termine (blocage).

Remarque : ici, le **non préemptif réussit alors que le préemptif échoue**. Thread3 n'est plus interrompue par Thread1 ; elle s'exécute d'un bloc de 5 à 10, et Thread1 attend sans rater son échéance. Ce n'est pas une règle générale. L'exemple `exemples/non_preemptif.txt` montre le cas inverse : une tâche longue et peu prioritaire **bloque** une tâche prioritaire, qui rate son échéance (t = 6) en non préemptif seulement. Aucun des deux modes ne domine l'autre.

---

## 4. Exercice 2 — Ordonnanceur EDF

### Q1. Simulateur EDF

À chaque **événement d'ordonnancement** (activation ou fin d'un job), le simulateur **parcourt la file d'attente** des jobs prêts. Il élit celui dont l'**échéance absolue** (date d'activation + D) est la plus proche. La priorité d'une tâche change donc d'une activation à l'autre : c'est une priorité dynamique. Avec `-v`, le journal affiche la file parcourue et la décision prise à chaque événement.

Choix d'implémentation :
- **Égalité d'échéance** : la tâche en cours garde le processeur, ce qui évite une préemption inutile. Sinon, on suit l'ordre du fichier. (Exception : dans les scénarios de la méthode de Spuri, la tâche étudiée perd les égalités, voir Q2.)
- **Job en retard** : il continue son exécution, et le job suivant de la même tâche attend derrière lui (file FIFO par tâche). Le dépassement est signalé à l'instant de l'échéance.
- **Durée de simulation** : PPCM des périodes (cours p.126). Toutes les situations possibles sont couvertes pour des tâches activées ensemble à t = 0.

Analyse théorique associée :
- U > 1 : non faisable ;
- période d'étude (busy period, cours p.129) : L = W(L) = Σ ⌈L/Tᵢ⌉ Cᵢ, en partant de t = 1. Pour l'exemple, elle vaut **39** ;
- **D = T et U ≤ 1 : faisable**, car EDF est optimal et la condition de charge est alors nécessaire **et** suffisante ;
- D < T : U ≤ 1 n'est plus que nécessaire. C'est la **méthode de Spuri** (cours p.133, détaillée en Q2) qui tranche : faisable si et seulement si Rᵢ ≤ Dᵢ pour toute tâche. Elle donne aussi les pires temps de réponse quand D = T.

### Q2. Vérification par la trace

**Sur l'exemple de l'énoncé** (`make exo2`) : U = 0,943 ≤ 1 et D = T, donc l'ensemble est faisable. La simulation sur 1001 unités ne trouve **aucune échéance ratée**. Pires temps de réponse observés : Thread1 = 5, Thread2 = 9, Thread3 = 10.

```
t          0    5    10   15   20   25
Thread1    ##.....---##..##.....-##....##
Thread2    --###......-##--#.....--###...
Thread3    -----#####...----#####....-#--
Depasse
```

Le moment clé, tel que le journal l'affiche (`-v`) :

```
  t=   7 : activation de Thread1 (échéance absolue 14)
  t=   7 : file parcourue : Thread1(éch. 14) Thread3(éch. 13)
  t=   7 : -> Thread3 garde le processeur (échéance absolue la plus proche)
```

Thread3 a une échéance à 13, plus proche que celle de Thread1 (14) : **EDF la laisse finir**. HPF, au contraire, la préemptait, et c'est ce qui la faisait échouer.

**Faisabilité et période active.** Pour des tâches activées ensemble à t = 0, avec D ≤ T et U ≤ 1 : si EDF rate une échéance, la première est ratée dans la **1re période active** [0, L] (Baruah, Rosier et Howell, 1990 ; Spuri, 1996). Simuler jusqu'à L suffit donc pour conclure. Le programme en tient compte : avec `-d`, un verdict EDF préemptif est concluant dès que la durée atteint L (voir § 6). Les tests aléatoires le vérifient aussi : la 1re échéance ratée tombe toujours au plus tard à t = L.

**Pire temps de réponse : méthode de Spuri (cours p.133).** Le cours p.125 prévient que, avec EDF, le pire temps de réponse n'est pas forcément obtenu à la 1ʳᵉ activation. La p.133 le définit comme le maximum de rᵢ(a) sur un ensemble A d'instants d'activation. C'est la méthode de Spuri (1996). Pour chaque tâche i :
- A_i = { k·Tⱼ + Dⱼ − Dᵢ ≥ 0, pour toute tâche j et tout k ≥ 0 } ∩ [0, L[ ;
- pour chaque a de A_i, on prend le scénario où **toutes les autres tâches sont activées à t = 0 et la tâche i à a** (ses jobs précédents à a − Tᵢ, a − 2Tᵢ… ≥ 0) ;
- rᵢ(a) = temps de réponse du job activé à a, et Rᵢ = max rᵢ(a).

Le programme calcule chaque rᵢ(a) avec **le même simulateur**. Chaque tâche a une date de 1re activation S, comme dans le modèle du cours p.93 : elle vaut 0 pour les tâches lues, et seul ce calcul la décale. Dans ces scénarios, la tâche étudiée **perd les égalités d'échéance**. C'est le cas le plus défavorable, celui que suppose l'analyse de Spuri : Rᵢ est ainsi un majorant sûr, quelle que soit la règle d'égalité. Avec la règle du simulateur (« la tâche en cours garde le processeur »), ce n'était pas le cas : sur des jeux aléatoires, la simulation trouvait parfois un job plus lent que ce maximum.

Sortie de `./ordo exemples/taches_tp.txt -p edf -v` :

```
  Thread1    a=0:r=2 a=4:r=2 a=6:r=4 a=7:r=5 a=14:r=2 a=15:r=2
             a=19:r=4 a=21:r=3 a=26:r=2 a=28:r=2 a=32:r=3 a=35:r=2
             a=37:r=2
             -> R = 5 (a = 7)
  Thread2    a=0:r=5 a=2:r=8 a=3:r=9 a=10:r=3 a=11:r=6 a=15:r=8
             a=17:r=8 a=22:r=5 a=24:r=4 a=28:r=8 a=31:r=7 a=33:r=6
             a=38:r=3
             -> R = 9 (a = 3)
  Thread3    a=0:r=10 a=1:r=11 a=8:r=6 a=9:r=10 a=13:r=9 a=15:r=9
             a=20:r=10 a=22:r=11 a=26:r=8 a=29:r=6 a=31:r=10 a=36:r=9
             -> R = 11 (a = 1)
```

| Tâche | D | R (Spuri) | Pire cas observé sur l'hyperpériode |
|---|---|---|---|
| Thread1 | 7 | 5 | 5 |
| Thread2 | 11 | 9 | 9 |
| Thread3 | 13 | 11 | 10 |

Ce qu'on en retire :
- **Le pire cas n'est pas à la 1ʳᵉ activation** : pour Thread1, r = 5 à a = 7, contre 2 à a = 0 (cours p.125).
- **Thread2 : R = 9 pour a = 3**, c'est-à-dire Thread1 et Thread3 activées à 0 et Thread2 à 3. Ce scénario n'existe pas dans la 1re période active du déroulement synchrone, où le maximum de Thread2 n'est que 6. Il réapparaît plus loin dans ce déroulement : le processeur est libre à t = 908–909, puis Thread1 et Thread3 sont activées ensemble à t = 910 (= 130 × 7 = 70 × 13), et Thread2 à t = 913 (= 83 × 11). C'est exactement le décalage a = 3, d'où le r = 9 observé à a = 913. La période active suffit donc bien à trouver le pire temps de réponse, **à condition d'examiner les scénarios décalés de Spuri**, et pas seulement le déroulement synchrone.
- **Thread3 : R = 11 pour a = 1, alors que la simulation observe au plus 10.** Dans ce scénario, Thread3 (activée à 1, échéance 14) s'exécute de 5 à 7. À t = 7, Thread1 est activée avec la même échéance 14. Si Thread3 perd l'égalité, elle finit à 12 (r = 11). Avec la règle du simulateur, elle garde le processeur et finit à 10 (r = 9). **Les temps de réponse dépendent donc de la règle d'égalité, pas le verdict.**
- Le programme vérifie automatiquement que R (Spuri) ≥ pire cas observé sur l'hyperpériode, et signale une égalité ou un écart (`Contrôle Spuri`). Ici, toutes les valeurs restent sous les échéances : 5 ≤ 7, 9 ≤ 11, 11 ≤ 13.

Le programme affiche aussi les temps de réponse du **déroulement synchrone** dans sa 1re période active. Ils viennent de la simulation EDF préemptive et ne constituent pas l'ensemble A de Spuri :

```
Déroulement synchrone (simulation EDF préemptive) : temps de réponse
des activations dans [0, 39[ (1re période active) :
  Thread1    a=0:r=2 a=7:r=5 a=14:r=2 a=21:r=3 a=28:r=2 a=35:r=2  -> max = 5
  Thread2    a=0:r=5 a=11:r=6 a=22:r=5 a=33:r=6  -> max = 6  (plus tard : r=9 pour a=913)
  Thread3    a=0:r=10 a=13:r=9 a=26:r=8  -> max = 10
```

**Sur l'exemple du cours p.134**, τ1(2,4,4) et τ2(3,7,7) : le journal donne pour τ2 les temps de réponse **5, 5, 5, 4** aux activations 0, 7, 14 et 21, exactement comme dans le cours. Le cours déroule toute l'hyperpériode (28), alors que la période active ne vaut que 7. Le tableau du déroulement synchrone ne contient donc, pour τ2, que a = 0 ; les autres valeurs sont dans le journal. La méthode de Spuri donne R = 3 pour τ1 et **R = 6 pour τ2** (a = 1, puis égalité d'échéance perdue à t = 4 face à τ1). C'est plus que le 5 observé, pour la même raison que Thread3 ci-dessus. Extrait du journal :

```
  t=   0 : activation de tau1 (échéance absolue 4)
  t=   0 : activation de tau2 (échéance absolue 7)
  t=   0 : file parcourue : tau1(éch. 4) tau2(éch. 7)
  t=   0 : -> tau1 élue (échéance absolue la plus proche)
  t=   2 : fin de tau1 (temps de réponse 2)
  t=   2 : file parcourue : tau2(éch. 7)
  t=   2 : -> tau2 élue (échéance absolue la plus proche)
  ...
  t=   8 : activation de tau1 (échéance absolue 12)
  t=   8 : file parcourue : tau1(éch. 12) tau2(éch. 14)
  t=   8 : -> tau1 élue (échéance absolue la plus proche)
  t=   8 : tau2 préemptée par tau1
```

### Conclusion : priorités fixes ou EDF

Le même ensemble de tâches (U = 0,943) **n'est pas faisable avec HPF, RM ou DM, mais l'est avec EDF**. EDF exploite le processeur jusqu'à U = 1, alors que les priorités fixes peuvent échouer bien avant. En contrepartie, EDF demande de recalculer les priorités à chaque événement. Son comportement en surcharge est aussi moins prévisible : on ne sait pas à l'avance quelle tâche ratera son échéance.

---

## 5. Validation

| Jeu de tâches | Attendu (cours ou calcul à la main) | Obtenu |
|---|---|---|
| `taches_tp.txt` | R = 2, 5, 17 → HPF/RM/DM non faisables ; EDF faisable, Spuri : R = 5, 9, 11 ≥ observés 5, 9, 10 | ✅ |
| `cours_cas2.txt` (p.123) | R₃ = 17 ≤ 17 → faisable | ✅ |
| `cours_optimalite.txt` (p.136) | RM échoue, DM et EDF réussissent (EDF, D < T : tranché par Spuri, R = 3 et 5) | ✅ |
| `cours_edf_p134.txt` (p.134) | temps de réponse de τ2 : 5, 5, 5, 4 ; Spuri : R = 3 et 6 ≥ observés 3 et 5 | ✅ |
| `priorites_inversees.txt` | HPF : Thread1 R = 10 > 7 | ✅ |
| `non_preemptif.txt` | préemptif OK, non préemptif rate à t = 6 | ✅ |
| `surcharge.txt` | U = 1,25 > 1 → non faisable partout | ✅ |
| `charge_exactement_1.txt` | U = 1 exactement → faisable (piège d'arrondi) | ✅ |

Tout se relance avec **`make test`** :
- `tests/test_exemples.sh` compare le tableau récapitulatif de chaque exemple au résultat attendu. Il vérifie aussi R₃ = 17, la suite 5, 5, 5, 4 du cours p.134 et les valeurs de Spuri (5, 9, 11 et 3, 6), ainsi que les pires cas observés.
- `tests/test_aleatoire.py` tire 300 jeux de tâches au hasard, **tous avec U ≤ 1** : les autres sont rejetés, puisque U > 1 donne un verdict trivial. La graine est fixe, donc le résultat est reproductible. Pour chaque jeu, il vérifie que :
  - la théorie et la simulation donnent le même verdict ;
  - pour EDF, la 1re échéance ratée tombe dans la période active ;
  - R (Spuri) ≥ pire cas observé.

  Résultat : **257 jeux non triviaux** (la charge seule ne permet pas de conclure) et **0 erreur sur 1 200 cas**. Il y a aussi 0 erreur sur 4 000 cas avec `python3 tests/test_aleatoire.py 1000 5`.
- **Contrôles intégrés au programme** : pour les priorités fixes en préemptif, les temps de réponse simulés doivent être égaux aux temps de réponse calculés ; pour EDF, R (Spuri) doit être ≥ au pire cas observé.
- **`make sanitize`** recompile avec `-fsanitize=address,undefined` et rejoue tous les exemples avec plusieurs jeux d'options : aucune erreur.

---

## 6. Hypothèses et limites

- Monoprocesseur, tâches **indépendantes** (pas de ressource partagée), coût système négligé (cours p.117).
- Tâches **périodiques, toutes activées à t = 0**. C'est l'instant critique, donc le pire cas pour les priorités fixes préemptives.
- D ≤ T. Les formules de temps de réponse utilisées ne sont valables que dans ce cas.
- Le test des temps de réponse ne s'applique qu'au mode **préemptif**. Le non préemptif est évalué par simulation.
- Temps discret : les valeurs sont entières, ce qui est le cas dans l'énoncé.
- Durée de simulation : par défaut l'hyperpériode H, limitée à 10⁶. Une échéance ratée prouve toujours la non-faisabilité. L'absence d'échec prouve la faisabilité si la durée atteint :
  - **max D** pour les priorités fixes en préemptif : d'après l'instant critique, le 1er job de chaque tâche a le pire temps de réponse ;
  - **la période active L** pour EDF en préemptif (voir Ex2 Q2) ;
  - **H** en non préemptif, faute de résultat aussi simple : si tout est terminé à H, le déroulement se répète à l'identique.

  Sinon, le verdict est marqué `(*)` (non concluant) dans le récapitulatif.
- Méthode de Spuri : si la période active est très longue, le calcul est abandonné (plus de 2·10⁸ unités de temps simulées). L'EDF avec D < T reste alors indécis, et la simulation tranche.

### Pourquoi un simulateur plutôt que de vrais threads ?

L'exercice 2 demande explicitement un « simulateur », et l'exercice 1 demande de « vérifier la faisabilité », c'est-à-dire de faire une analyse. On aurait pu lancer trois vrais threads POSIX (`pthread_create`, politique `SCHED_FIFO`, cours PMT C) qui calculent (attente active) pendant 2, 3 et 5 secondes, mais :
- le résultat dépendrait de l'ordonnanceur de la machine. macOS n'est pas temps réel, et `SCHED_FIFO` demande les droits administrateur sous Linux ;
- le temps d'une vraie exécution n'est jamais exactement de 2 s : les mesures seraient bruitées et différentes à chaque lancement ;
- vérifier une hyperpériode de 1001 s prendrait près de 17 minutes.

Le simulateur en temps discret donne un résultat **exact, reproductible et instantané**, directement comparable aux calculs du cours.
