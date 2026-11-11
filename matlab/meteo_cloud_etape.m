function [classe, proba, x, n_hist] = meteo_cloud_etape(t_s, T, RH, P_mer, M)
%METEO_CLOUD_ETAPE  Une prediction "Cloud" a partir de l'historique du canal ThingSpeak.
%  t_s, T, RH, P_mer : vecteurs des mesures envoyees par la carte (temps POSIX [s])
%  La mesure la plus recente joue le role de "mesure courante", les moyennes
%  horaires precedentes celui du buffer de 6 h de la carte : meme entree que l'Edge.
  ok = ~isnan(t_s) & ~isnan(T) & ~isnan(RH) & ~isnan(P_mer);
  t_s = t_s(ok); T = T(ok); RH = RH(ok); P_mer = P_mer(ok);
  if isempty(t_s)
    error('meteo:vide', 'Aucune mesure exploitable dans le canal');
  end
  [t_now, i] = max(t_s);
  hist = historique_horaire(t_s, T, RH, P_mer, t_now);
  n_hist = size(hist, 1);
  x = meteo_features(T(i), RH(i), P_mer(i), hist);
  [classe, proba] = meteo_predire(x, M);
end
