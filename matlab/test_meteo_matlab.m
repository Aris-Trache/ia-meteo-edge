%TEST_METEO_MATLAB  Verifie que la version MATLAB donne EXACTEMENT les resultats
% du Python / Keras (et donc de la carte). Fonctionne aussi sous GNU Octave.
% Si la Deep Learning Toolbox est installee, compare aussi avec le modele ONNX.
M = meteo_modele();
D = donnees_test_meteo();

% 1) Features sur 48 h reelles (meme protocole que firmware/tests_hote/test_meteo.c)
hist = zeros(0, 3); err_max = 0; nb = 0;
for t = 1:size(D.SEQ, 1)
  x = meteo_features(D.SEQ(t,1), D.SEQ(t,2), D.SEQ(t,3), hist);
  if t == 1
    ok_froid = max(abs(x - D.COLD0)) < 1e-3;
  end
  if t > 6
    e = abs(x - D.EXP(t,:)); err_max = max(err_max, max(e)); nb = nb + numel(e);
  end
  hist = [D.SEQ(t,:); hist(1:min(end,5), :)];     % la plus recente en tete, 6 max
end
fprintf('features : %d valeurs comparees, ecart max MATLAB/Python = %.2e | froid %d\n', ...
        nb, err_max, ok_froid);

% 2) Reseau : memes probabilites que Keras
dec = 0; pm = 0;
for v = 1:size(D.VX, 1)
  [c, p] = meteo_predire(D.VX(v,:), M);
  [~, c_ref] = max(D.VP(v,:));
  dec = dec + (c == c_ref); pm = max(pm, max(abs(p - D.VP(v,:))));
end
fprintf('reseau   : %d/%d decisions identiques a Keras, ecart max proba = %.2e\n', ...
        dec, size(D.VX,1), pm);

% 3) Historique horaire reconstruit depuis des mesures brutes (comme sur ThingSpeak)
t0 = 1.7e9; t_s = []; T = []; RH = []; P = [];
for h = 1:size(D.SEQ, 1)                    % 1 mesure / 10 min, constante dans l'heure
  for m = 0:5
    t_s(end+1,1) = t0 + (h-1)*3600 + m*600; T(end+1,1) = D.SEQ(h,1);
    RH(end+1,1) = D.SEQ(h,2);                P(end+1,1) = D.SEQ(h,3);
  end
end
sel = t_s <= t0 + 47*3600;                  % jusqu'a la 1re mesure de l'heure 48
[c_cloud, p_cloud, x_cloud, n_h] = meteo_cloud_etape(t_s(sel), T(sel), RH(sel), P(sel), M);
e_cloud = max(abs(x_cloud - D.EXP(48,:)));
fprintf('cloud    : historique %d h, ecart features vs Python = %.2e -> %s (%.0f%%)\n', ...
        n_h, e_cloud, M.classes{c_cloud}, 100*max(p_cloud));

ok = err_max < 1e-3 && ok_froid && dec == size(D.VX,1) && pm < 1e-4 && n_h == 6 && e_cloud < 1e-3;

% 4) Option : modele ONNX (MATLAB + Deep Learning Toolbox uniquement, non requis)
if exist('importNetworkFromONNX') == 2
  try
    net = importNetworkFromONNX(fullfile('..', 'models', 'meteo_k3_mlp.onnx'));
    xn  = dlarray(single((D.VX - M.mu) ./ M.sigma)', 'CB');   % 14 x N, format "canaux x lot"
    p_onnx = double(extractdata(predict(net, xn)))';           % N x 3
    p_mat = zeros(size(D.VX,1), 3);
    for v = 1:size(D.VX, 1), [~, p_mat(v,:)] = meteo_predire(D.VX(v,:), M); end
    fprintf('onnx     : ecart max proba ONNX / MATLAB pur = %.2e\n', max(abs(p_onnx(:) - p_mat(:))));
  catch err
    fprintf('onnx     : comparaison non faite (%s)\n', err.message);
  end
end

if ok, disp('TEST MATLAB : OK'); else, disp('TEST MATLAB : ECHEC'); end
