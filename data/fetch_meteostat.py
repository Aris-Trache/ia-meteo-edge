"""Recupere les donnees horaires Meteostat utilisees au TP3 (reproductibilite).

    python fetch_meteostat.py            -> meteostat_alpes_2017_2025.csv.gz

Source : Meteostat bulk data (https://dev.meteostat.net/bulk/), licence CC BY-NC 4.0,
donnees issues principalement des services meteorologiques nationaux (Meteo-France, MeteoSuisse...).
Le fichier livre dans ce depot a ete genere le 11/11/2026 avec exactement cette logique.
"""
import csv, gzip, io, urllib.request

STATIONS = {"07491": "Chambery / Aix-les-Bains (site du projet, 235 m)",
            "07481": "Lyon / Saint-Exupery (248 m)",
            "07486": "Grenoble / Saint-Geoirs (384 m)",
            "06700": "Geneve-Cointrin (420 m)",
            "LFLP0": "Annecy / Meythet (463 m)",
            "07577": "Montelimar (73 m)"}
COLS = ["date", "hour", "temp", "dwpt", "rhum", "prcp", "snow", "wdir", "wspd", "wpgt", "pres", "tsun", "coco"]


def main(out="meteostat_alpes_2017_2025.csv.gz", start="2017-01-01"):
    with gzip.open(out, "wt", newline="") as fo:
        w = csv.writer(fo)
        w.writerow(["station", "ts", "temp", "dwpt", "rhum", "prcp", "snow", "wdir", "wspd", "wpgt", "pres", "coco"])
        for sid in STATIONS:
            url = f"https://bulk.meteostat.net/v2/hourly/{sid}.csv.gz"
            raw = gzip.decompress(urllib.request.urlopen(url, timeout=60).read()).decode()
            n = 0
            for row in csv.reader(io.StringIO(raw)):
                r = dict(zip(COLS, row))
                if r["date"] < start or "" in (r["temp"], r["rhum"], r["pres"]):
                    continue
                ts = r["date"].replace("-", "") + r["hour"].zfill(2)
                w.writerow([sid, ts] + [r[c] for c in COLS[2:11]] + [r["coco"]])
                n += 1
            print(f"{sid} {STATIONS[sid]} : {n} heures")


if __name__ == "__main__":
    main()
