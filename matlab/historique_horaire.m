function hist = historique_horaire(t_s, T, RH, P_mer, t_now)
%HISTORIQUE_HORAIRE  Moyennes horaires passees, comme le buffer de la carte.
%  t_s    : instants des mesures [s, temps POSIX] (vecteur)
%  t_now  : instant de la mesure courante [s]
%  hist   : n x 3 [T RH P_mer] ; ligne k = moyenne de [t_now - k h, t_now - (k-1) h[
%           (k = 1..6). On s'arrete a la premiere heure SANS mesure, comme la carte
%           apres un redemarrage : jamais de tendance calculee a travers un trou.
  hist = zeros(0, 3);
  for k = 1:6
    % intervalle semi-ouvert : une mesure pile sur une limite n'est comptee qu'une
    % fois, et la mesure courante (t = t_now) n'est jamais dans le passe
    sel = (t_s >= t_now - k*3600) & (t_s < t_now - (k-1)*3600);
    ok = sel & ~isnan(T) & ~isnan(RH) & ~isnan(P_mer);
    if ~any(ok), break; end
    hist(k, :) = [mean(T(ok)), mean(RH(ok)), mean(P_mer(ok))];
  end
end
