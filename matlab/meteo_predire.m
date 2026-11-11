function [classe, proba] = meteo_predire(x_brut, M)
%METEO_PREDIRE  Inference du reseau 14 -> 32 -> 16 -> 3 en calcul matriciel pur.
%  Meme resultat que le modele ONNX (verifie par test_meteo_matlab.m), mais
%  ne demande AUCUNE toolbox : c'est ce qui permet de le faire tourner dans
%  "MATLAB Analysis" de ThingSpeak, ou la Deep Learning Toolbox n'existe pas.
%
%  x_brut : 1 x 14 (sortie de meteo_features), M : struct de meteo_modele()
%  classe : indice 1..3 (M.classes{classe}), proba : 1 x 3
  x = (x_brut - M.mu) ./ M.sigma;              % normalisation (hors du modele)
  a = max(0, x * M.W{1} + M.b{1});             % ReLU
  a = max(0, a * M.W{2} + M.b{2});             % ReLU
  z = a * M.W{3} + M.b{3};
  e = exp(z - max(z));                         % softmax numeriquement stable
  proba = e / sum(e);
  [~, classe] = max(proba);
end
