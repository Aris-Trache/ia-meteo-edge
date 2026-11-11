function x = meteo_features(T, RH, P_mer, hist)
%METEO_FEATURES  Les 14 entrees du modele meteo, IDENTIQUES au code C de la carte
%  (firmware/.../meteo/meteo_features.c) et au Python (notebooks/meteo_features.py).
%
%  T, RH, P_mer : mesure courante [degC], [%], [hPa niveau de la mer]
%  hist         : matrice n x 3 [T RH P_mer], ligne 1 = heure t-1, ligne 2 = t-2, ...
%                 (n = 0..6 heures disponibles ; vide au demarrage)
%  x            : vecteur ligne 1 x 14, ordre = models/meteo_k3_mlp_meta.json
%
%  Heure manquante -> tendance = 0 ; fenetres 6 h calculees sur ce qui existe
%  (convention "demarrage a froid" utilisee a l'entrainement).
  n = size(hist, 1);
  spread = T - point_de_rosee(T, RH);

  dp3  = tendance(hist, P_mer, 3, 3);  dp1  = tendance(hist, P_mer, 1, 3);
  dt3  = tendance(hist, T,     3, 1);  drh3 = tendance(hist, RH,    3, 2);
  dp6  = tendance(hist, P_mer, 6, 3);  dt1  = tendance(hist, T,     1, 1);
  drh1 = tendance(hist, RH,    1, 2);

  % fenetre 6 h = heure courante + jusqu'a 5 heures passees
  k = min(n, 5);
  rh_win = [RH; hist(1:k, 2)];
  sp_win = [spread; hist(1:k, 1) - point_de_rosee(hist(1:k, 1), hist(1:k, 2))];

  x = [T, RH, P_mer, spread, dp3, dp1, dt3, drh3, dp6, dt1, drh1, ...
       max(rh_win), min(rh_win), mean(sp_win)];
end

function d = tendance(hist, actuel, k, col)
% actuel - valeur de l'heure t-k ; 0 si cette heure n'existe pas encore
  if k <= size(hist, 1), d = actuel - hist(k, col); else, d = 0; end
end

function td = point_de_rosee(T, RH)
% Formule de Magnus (a = 17,62 ; b = 243,12 degC), comme sur la carte
  a = 17.62; b = 243.12;
  RH = min(max(RH, 1), 100);
  g = log(RH / 100) + a .* T ./ (b + T);
  td = b .* g ./ (a - g);
end
