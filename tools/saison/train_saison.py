import sys, json, numpy as np, pandas as pd, re
sys.path.insert(0, 'notebooks')
import meteo_features as mf
import tensorflow as tf
from sklearn.metrics import accuracy_score, f1_score, recall_score, balanced_accuracy_score
H = int(sys.argv[1])          # 0 = maintenant, 3 = dans 3 h
F14 = mf.FEATURES_FINAL; F = F14 + ['dsin', 'dcos']
D = mf.load_dataset('data/meteostat_alpes_2017_2025.csv.gz')
M = mf.GROUPINGS['K3']['map']; D['y0'] = D['coco'].map(M)
if H:
    t = D[['station', 'y0']].copy(); t.index = t.index - pd.Timedelta(hours=H); t = t.rename(columns={'y0': 'yH'})
    D = D.reset_index().rename(columns={'index': 'time'}).merge(t.reset_index().rename(columns={'index': 'time'}), on=['time', 'station']).set_index('time')
else:
    D['yH'] = D['y0']
yr = D.index.year; TR, VA, TE = D[yr <= 2022], D[yr == 2023], D[yr >= 2024]
hdr = open('firmware/TP2_Capteurs/FSBL/Core/Src/meteo/meteo_model_params.h').read()
g = lambda n: [float(v.strip().rstrip('f')) for v in re.search(n + r'\[METEO_NB_FEATURES\]\s*=\s*\{([^}]*)\}', hdr).group(1).split(',')]
mu = np.array(g('METEO_MEAN') + [float(TR.dsin.mean()), float(TR.dcos.mean())], dtype='float32')
sd = np.array(g('METEO_STD') + [float(TR.dsin.std()), float(TR.dcos.std())], dtype='float32')
tr = TR.copy(); idx = np.random.default_rng(0).random(len(tr)) < 0.15; tr.loc[idx, F14] = mf.cold_start(tr.loc[idx])[F14]
X = lambda d, f=F: (d[f].to_numpy('float32') - mu[:len(f)]) / sd[:len(f)]
ytr = tr.yH.to_numpy(); cnt = np.bincount(ytr, minlength=3); w = (cnt.sum() / (3 * cnt)) ** 0.5
def fit(f, seed=0):
    tf.keras.utils.set_random_seed(seed)
    m = tf.keras.Sequential([tf.keras.Input((len(f),)), tf.keras.layers.Dense(32, activation='relu'), tf.keras.layers.Dense(16, activation='relu'), tf.keras.layers.Dense(3, activation='softmax')])
    m.compile(optimizer=tf.keras.optimizers.Adam(2e-3), loss='sparse_categorical_crossentropy')
    m.fit(X(tr, f), ytr, validation_data=(X(VA, f), VA.yH.to_numpy()), epochs=60, batch_size=512, class_weight={i: float(w[i]) for i in range(3)}, verbose=0,
          callbacks=[tf.keras.callbacks.EarlyStopping(patience=5, restore_best_weights=True)])
    return m
yte = TE.yH.to_numpy()
def sc(m, f):
    p = m.predict(X(TE, f), verbose=0).argmax(1)
    return dict(acc=round(accuracy_score(yte, p), 3), bacc=round(balanced_accuracy_score(yte, p), 3), f1=round(f1_score(yte, p, average='macro'), 3), rappel_pluie=round(recall_score(yte, p, labels=[2], average='macro'), 3))
m16 = fit(F); m14 = fit(F14)
# demarrage a froid
Xc = TE.copy(); Xc.loc[:, F14] = mf.cold_start(TE)[F14]
pc16 = m16.predict(X(Xc, F), verbose=0).argmax(1); pc14 = m14.predict(X(Xc, F14), verbose=0).argmax(1)
res = dict(horizon_h=H, avec_saison=sc(m16, F), sans_saison=sc(m14, F14), froid_avec=round(f1_score(yte, pc16, average='macro'), 3), froid_sans=round(f1_score(yte, pc14, average='macro'), 3),
           mu_saison=mu[14:].tolist(), sd_saison=sd[14:].tolist())
print(json.dumps(res)); json.dump(res, open(f'tools/saison/res_saison_{H}h.json', 'w'))
m16.save(f'tools/saison/meteo_k3_saison_{H}h.keras')
xv = TE.sample(8, random_state=1); pv = m16.predict(X(xv, F), verbose=0)
json.dump([{'x_norm': X(xv, F)[i].tolist(), 'proba': pv[i].tolist()} for i in range(8)], open(f'tools/saison/vect_saison_{H}h.json', 'w'))
