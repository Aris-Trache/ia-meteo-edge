# TP3.2 — IA météo dans le Cloud (MATLAB + ThingSpeak)

Le **même réseau** que sur la carte (mêmes poids que `models/meteo_k3_mlp.onnx`), mais exécuté dans le Cloud.
Intérêt pour la comparaison Edge / Cloud : la carte et le Cloud prédisent sur les **mêmes mesures**,
le canal résultats enregistre s'ils sont d'accord (`field5`).

```
carte --(LwIP, HTTP)--> canal CAPTEURS --(MATLAB Analysis, /10 min)--> canal RESULTATS
                                                     \------------------> TalkBack --> carte
```

## Fichiers
| Fichier | Rôle |
|---|---|
| `thingspeak_meteo_cloud.m` | **Script autonome à coller dans ThingSpeak** (le réseau est inclus dedans) |
| `meteo_features.m`, `historique_horaire.m`, `meteo_predire.m`, `meteo_cloud_etape.m` | Les briques, identiques au code C |
| `meteo_modele.m`, `donnees_test_meteo.m` | Générés par `tools/export_matlab.py` (ne pas éditer) |
| `test_meteo_matlab.m` | Vérification : MATLAB == Python == carte |

**Pourquoi un réseau « à la main » et pas `importNetworkFromONNX` sur ThingSpeak ?** La Deep Learning Toolbox
n'est pas disponible dans MATLAB Analysis. Un MLP, c'est 3 produits matriciels : ça tient en 10 lignes et c'est
vérifié contre Keras. Sur un MATLAB de bureau avec la toolbox, `test_meteo_matlab.m` compare aussi avec l'ONNX.

## Vérifier (MATLAB de bureau ou GNU Octave)
```matlab
cd matlab
test_meteo_matlab
```
Attendu : écart features < 1e-6, 8/8 décisions identiques à Keras, `TEST MATLAB : OK`.

## Mise en place sur ThingSpeak (compte MathWorks gratuit)
1. **Channels → New Channel « Meteo capteurs »** : field1 `T`, field2 `HR`, field3 `P_mer`, field4 `classe_edge`.
2. **New Channel « Meteo resultats »** : field1 `classe_cloud`, field2 `p_degage`, field3 `p_nuageux`,
   field4 `p_precip`, field5 `accord_edge_cloud`, field6 `heures_historique`.
3. **Apps → TalkBack → New TalkBack** (la carte viendra y lire `METEO:<classe>:<p_precip>`).
4. **Apps → MATLAB Analysis → New → Custom** : coller `thingspeak_meteo_cloud.m`, remplir le bloc
   *Configuration* (IDs et clés), **Save and Run**.
5. **Apps → TimeControl** : lancer l'analyse toutes les 10 minutes.

Les clés API restent sur ThingSpeak : **ne jamais les commiter**.

Tant que la carte n'envoie rien (TP2.3, LwIP), on peut tester le flux en écrivant des mesures à la main :
`https://api.thingspeak.com/update?api_key=<WRITE_KEY>&field1=12.5&field2=80&field3=1008&field4=2`
