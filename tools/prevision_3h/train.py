import sys, json, time, numpy as np, pandas as pd
sys.path.insert(0, 'notebooks')
import meteo_features as mf
import tensorflow as tf
from sklearn.metrics import accuracy_score, balanced_accuracy_score, f1_score, confusion_matrix
H = int(sys.argv[1]) if len(sys.argv) > 1 else 3
D = mf.load_dataset('data/meteostat_alpes_2017_2025.csv.gz')
M = mf.GROUPINGS['K3']['map']; F = mf.FEATURES_FINAL
D['y0'] = D['coco'].map(M)
t = D[['station', 'y0']].copy(); t.index = t.index - pd.Timedelta(hours=H); t = t.rename(columns={'y0': 'yH'})
D = D.reset_index().rename(columns={'index': 'time'}).merge(t.reset_index().rename(columns={'index': 'time'}), on=['time', 'station']).set_index('time')
yr = D.index.year; TR, VA, TE = D[yr <= 2022], D[yr == 2023], D[yr >= 2024]
hdr=open('firmware/TP2_Capteurs/FSBL/Core/Src/meteo/meteo_model_params.h').read()
import re as _r
g=lambda n: np.array([float(v.strip().rstrip('f')) for v in _r.search(n+r'\[METEO_NB_FEATURES\]\s*=\s*\{([^}]*)\}',hdr).group(1).split(',')],dtype='float32')
mu=g('METEO_MEAN'); sd=g('METEO_STD')
tr = TR.copy(); idx = np.random.default_rng(0).random(len(tr)) < 0.15; tr.loc[idx, F] = mf.cold_start(tr.loc[idx])[F]
X = lambda d: (d[F].to_numpy('float32') - mu) / sd
ytr, yva, yte = tr.yH.to_numpy(), VA.yH.to_numpy(), TE.yH.to_numpy()
cnt = np.bincount(ytr, minlength=3); w = (cnt.sum() / (3 * cnt)) ** 0.5
tf.keras.utils.set_random_seed(0)
m = tf.keras.Sequential([tf.keras.Input((14,)), tf.keras.layers.Dense(32, activation='relu'), tf.keras.layers.Dense(16, activation='relu'), tf.keras.layers.Dense(3, activation='softmax')])
m.compile(optimizer=tf.keras.optimizers.Adam(2e-3), loss='sparse_categorical_crossentropy')
m.fit(X(tr), ytr, validation_data=(X(VA), yva), epochs=60, batch_size=512, class_weight={i: float(w[i]) for i in range(3)}, verbose=0,
      callbacks=[tf.keras.callbacks.EarlyStopping(patience=5, restore_best_weights=True)])
p = m.predict(X(TE), verbose=0).argmax(1)
pers = TE.y0.to_numpy()
now = tf.keras.models.load_model('models/meteo_k3_mlp.keras').predict(X(TE), verbose=0).argmax(1)  # persistance : "dans 3 h comme maintenant"
def sc(y, q): return dict(acc=round(accuracy_score(y, q), 3), bacc=round(balanced_accuracy_score(y, q), 3), f1=round(f1_score(y, q, average='macro'), 3))
from sklearn.metrics import precision_score, recall_score
res = dict(horizon_h=H, n_test=int(len(TE)), modele=sc(yte, p), persistance=sc(yte, pers), ia_actuelle_comme_prevision=sc(yte, now),
           rappel_pluie=round(recall_score(yte, p, labels=[2], average='macro'), 3), precision_pluie=round(precision_score(yte, p, labels=[2], average='macro'), 3),
           rappel_pluie_persist=round(recall_score(yte, pers, labels=[2], average='macro'), 3),
           mu_identique=bool(np.allclose(mu, np.array([12.86761, 72.5664, 1017.785, 5.568298, 0.003072298, 0.002579116, -0.01523066, 0.08090879, -0.002201457, -0.004690567, 0.06489313, 79.95618, 64.6916, 5.593514]), rtol=2e-3, atol=2e-3)))
print(json.dumps(res))
json.dump(res, open(f'tools/prevision_3h/res_{H}h.json', 'w'))
m.save(f'tools/prevision_3h/meteo_k3_prev{H}h.keras')
np.save(f'tools/prevision_3h/mu.npy', mu); np.save(f'tools/prevision_3h/sd.npy', sd)
xv = TE.sample(8, random_state=1); pv = m.predict(X(xv), verbose=0)
json.dump([{'x_norm': X(xv)[i].tolist(), 'proba': pv[i].tolist()} for i in range(8)], open(f'tools/prevision_3h/vect_{H}h.json', 'w'))
