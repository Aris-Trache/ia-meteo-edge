/**
 * Modele pour les cles ThingSpeak (TP2.3 / TP3 partie Cloud).
 *
 * 1. Copier ce fichier en "secrets_thingspeak.h" (meme dossier).
 * 2. Remplacer les valeurs par celles de VOTRE compte ThingSpeak :
 *      canal "Capteurs" > API Keys > Write API Key
 *      Apps > TalkBack > votre TalkBack > TalkBack ID et API Key
 *      canal "Capteurs" > Channel ID et API Keys > Read API Key (historique de la page web)
 * 3. secrets_thingspeak.h est ignore par git (.gitignore) : la cle ne part
 *    jamais sur GitHub.
 */
#ifndef SECRETS_THINGSPEAK_H
#define SECRETS_THINGSPEAK_H

#define THINGSPEAK_CLE_ECRITURE   "XXXXXXXXXXXXXXXX"
#define THINGSPEAK_TALKBACK_ID    ""      /* ex. "52345" ; vide = pas de TalkBack */
#define THINGSPEAK_TALKBACK_CLE   ""
#define THINGSPEAK_CANAL_ID       ""      /* ex. "3123456" ; vide = historique garde par la carte seulement */
#define THINGSPEAK_CLE_LECTURE    ""      /* Read API Key (vide si le canal est public) */

#endif
