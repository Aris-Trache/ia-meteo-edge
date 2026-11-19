# Démo Edge : site web dans la carte, mode hors ligne, test du souffle

Tout a été testé sur la carte le 19/11/2026 (branche `npu`).

## 1. La carte sert sa propre page web
- Serveur HTTP sur le port 80 (socket BSD LwIP, tâche FreeRTOS `web`) : `GET /` renvoie la page,
  `GET /api` renvoie l'état en JSON (mesures, probabilités, temps d'inférence, état Internet, Cloud).
- Page autonome (`FSBL/Core/Src/reseau/page_web.h`, 6 Ko) : sans CDN ni police externe, graphiques en SVG,
  rafraîchie toutes les 2 s. Elle marche **sans Internet** : il suffit du câble carte ↔ PC.
- L'adresse est affichée dans la console : `<Serveur web> tableau de bord en direct : http://192.168.137.xx/`.

## 2. Mode hors ligne (store & forward)
- Si ThingSpeak est injoignable, l'IA continue sur la carte et chaque mesure est gardée en mémoire
  (file de 90 mesures = 30 min).
- Au retour du réseau, les mesures sont renvoyées une par une toutes les 16 s (limite ThingSpeak gratuit : 15 s),
  **avec leur vraie date** (`created_at`). L'heure UTC est lue dans l'en-tête `Date:` des réponses ThingSpeak,
  sans RTC ni NTP.
- Testé : Wi-Fi du PC coupé → badge « Internet coupé · 2 mesures gardées », l'IA continue (468 prédictions) ;
  Wi-Fi rallumé → « Internet OK », 3 mesures rattrapées.

## 3. Le souffle fait basculer la prédiction
Réponse du modèle (vérifiée sur PC avec le code C de la carte), pression 999 hPa, sans historique :

| Humidité | Prédiction |
|---|---|
| 43 % | Nuageux-Couvert (57 %) |
| 60 % | Précipitations (58 %) |
| 75–95 % | Précipitations (78–87 %) |

Souffler sur le SHT40 fait monter l'humidité au-delà de 60 % : la prédiction passe à « Précipitations ».

## Mise en place pour la démo
PC portable connecté au partage de connexion du téléphone → partage Windows (ICS) vers l'Ethernet de la carte
→ CubeIDE F11 puis F8 → ouvrir l'adresse de la console dans Chrome. Pour la coupure : couper le partage du téléphone.
