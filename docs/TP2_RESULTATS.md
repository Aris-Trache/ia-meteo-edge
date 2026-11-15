# TP2 — Résultats (partie 3 : FreeRTOS + LwIP)

Carte NUCLEO-N657X0-Q + X-NUCLEO-IKS4A1, firmware `firmware/TP2_Capteurs`, mode `TP2_PARTIE 4`.

## Montage réseau
La carte est reliée **directement au PC** par un câble Ethernet ; le PC partage sa connexion
Wi-Fi (« Partage de connexion Internet » de Windows). Windows joue alors le rôle de box :
serveur DHCP et DNS en `192.168.137.1`.

## Architecture logicielle
| Tâche FreeRTOS | Priorité | Rôle |
|---|---|---|
| `capteurs_ia` | 3 | toutes les 2 s (`vTaskDelayUntil`) : capteurs I²C + IA embarquée (float + int8) |
| `reseau` | 3 | DHCP, puis toutes les 20 s : ping google.com + envoi ThingSpeak |
| `echo` | 2 | serveur TCP echo, port 7 (`socket/bind/listen/accept`) |
| `tcpip`, `eth_rx`, `eth_lien` | 5 / 6 / 2 | pile LwIP, réception Ethernet, surveillance du câble |

- **FreeRTOS V11.1.0** (port `ARM_CM55_NTZ`) et **LwIP 2.2.1** ajoutés à la main : CubeMX ne les
  propose pas pour le STM32N6. Configuration Ethernet reprise de l'exemple ST `Nx_TCP_Echo_Client`
  (PHY LAN8742 en RMII, DMA multi-canaux).
- **Mémoire du DMA Ethernet** : 64 Ko non cacheables (MPU) en fin d'AXISRAM2, sinon le cache
  du Cortex-M55 et le DMA ne verraient pas les mêmes données.
- **Sécurité RIF** : le DMA Ethernet est déclaré maître sécurisé/privilégié, sinon il n'a pas
  accès à la RAM du FSBL.
- **Console partagée** : un mutex évite que les messages des deux tâches se mélangent.
- **LEDs** : rouge = lecture capteurs, bleu = communication réseau, vert = attente.
- **API sockets BSD** de LwIP pour tout (ping = socket RAW ICMP, HTTP = socket TCP).

## Résultats sur la carte
```
FreeRTOS : demarrage de l'ordonnanceur (tas libre 48736 octets)
<Communication Reseau> initialisation LwIP + Ethernet
            [eth] PHY LAN8742 detecte (adresse MDIO 0)
  [eth] cable branche : 100 Mbit/s, full duplex
<Communication Reseau> DHCP OK :  IP 192.168.137.112  masque 255.255.255.0  passerelle 192.168.137.1  DNS 192.168.137.1
<Communication Reseau> ping google.com
  ping google.com (172.217.16.238) : reponse en 23 ms
  ping google.com (172.217.16.238) : reponse en 22 ms
  ...
<Communication Reseau>
  ping google.com (172.217.16.238) : reponse en 19 ms
  ThingSpeak : mesure enregistree (entree n. 13)
  bilan : ping 6/6 OK, ThingSpeak 2 envois, trames rx 20 tx 26, pile reseau libre 1560 mots
```
- Ping google.com : **100 % de réponses, 19–23 ms**.
- Les mesures (T, HR, P niveau mer) et la prédiction de l'IA embarquée arrivent sur le canal
  ThingSpeak toutes les 20 s (la version gratuite impose ≥ 15 s entre deux envois).
- L'IA embarquée continue à tourner pendant le réseau : float32 14,7 µs, int8 8,2 µs, 100 %
  d'accord float/int8.
- Messages LwIP affichés sur la console (DHCP, ICMP) : `TRACES_LWIP` dans `lwipopts.h`.

## Problèmes rencontrés (et corrigés)
| Symptôme | Cause | Correction |
|---|---|---|
| Pas d'adresse, LwIP affiche `netif is not up, old style port?` | `dhcp_start()` appelé avant que l'interface soit active | `netif_set_up()` avant `dhcp_start()` ; le câble est géré à part (`netif_set_link_up/down`) |
| ThingSpeak répond `200 OK` mais le numéro d'entrée est vide | les en-têtes HTTP de ThingSpeak (~700 o) dépassaient le tampon de 512 o | tampon de 2048 o, requête en HTTP/1.0 (pas de « chunked ») |
| CubeIDE ignore les nouveaux fichiers HAL Ethernet | les fichiers HAL sont des *liens* du projet, que CubeIDE ne recharge pas | pilotes Ethernet copiés dans `FSBL/Middlewares/ST/Pilotes_Ethernet` (dossier source) |
| Clé ThingSpeak collée dans le fichier modèle (suivi par git) | confusion entre les deux fichiers | clé déplacée dans `secrets_thingspeak.h` (ignoré par git) avant le push |

## Taille du programme
Code + constantes ≈ 235 Ko sur 255 Ko (compilation -O0 Debug) ; RAM : 192 Ko + 64 Ko réseau.
