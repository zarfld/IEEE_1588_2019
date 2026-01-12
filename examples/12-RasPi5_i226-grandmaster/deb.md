Das sieht insgesamt deutlich besser aus. Aus dem Log kann man drei klare Punkte ableiten: **(A) TAI/UTC-Bezug ist jetzt konsistent**, **(B) die Driftmessung arbeitet plausibel**, **(C) ein paar “kosmetische”/Robustheits-Themen bleiben (Returncode/Logging/Initial-Sync).**

---

## A) TAI/UTC ist jetzt sauber – keine “-1s” Discontinuities mehr

Sie haben jetzt fortlaufend:

* `PPS edge sec=...` (UTC/`CLOCK_REALTIME` aus `/dev/pps1`)
* `RTC seconds=...` ist **TAI** und liegt **konstant +37s** darüber
* `TAI-UTC=37` wird korrekt angezeigt

Beispiel:

```
RTC seconds=1768224707 ... PPS edge sec=1768224670 ... TAI-UTC=37
```

Das ist exakt die erwartete Beziehung (DS3231 läuft in TAI, PPS-Timestamp ist UTC). Damit ist die Hauptursache der früheren ±1s-Slips im Kern beseitigt.

---

## B) Driftmessung: Werte sind plausibel, und Ihre “post-discontinuity transient skip” wirkt

Nach der RTC-Synchronisierung (die ja eine Zeitdiskontinuität ist) skippen Sie korrekt einige Samples, dann:

* Baseline: `1646028 ns`
* Drift: im Bereich **±0.3 ppm**
* Moving average pendelt nahe **0 ppm**
* “Error” liegt bei ca. **1.6 ms**

Das ist als Ergebnisbild stimmig: Die DS3231-SQW-Flanke liegt “irgendwo” im Sekundenfenster (bei Ihnen ~106 ms nach der Sekunde, siehe `nsec=1060...`), und Ihre **gemessene “Error” ist primär die Userspace-Latenz** zwischen PPS-Edge und dem Zeitpunkt, wo Sie auswerten (nicht die absolute SQW-Phasenlage).

---

## C) Offene Themen / Verbesserungen

### 1) `PPS_FETCH returned 3` ganz am Anfang

Das ist kein “normales” Erfolgsbild. Wichtig: je nachdem ob Sie `time_pps_fetch()` aus `timepps.h` verwenden, ist die Semantik **nicht** “0 oder -1”, sondern oft:

* `0` = OK
* `!=0` = **Fehlercode (errno-Wert)**

`3` entspräche dann z.B. `ESRCH`. Auffällig ist aber: Sie bekommen trotzdem direkt danach valide Daten. Das spricht für **falsches Logging der Rückgabe** (oder Sie loggen den Returncode eines vorgelagerten Calls).

Empfehlung (robust + eindeutig):

* Beim Fetch immer beides loggen: `ret` und `strerror(ret)` (wenn `ret != 0`)
* Wenn Sie im Code noch `errno` loggen: **nur** wenn ein API wirklich `-1` liefert.

Beispiel-Pattern (prinzipiell):

```c
int ret = time_pps_fetch(...);
if (ret != 0) {
  LOG("PPS fetch error ret=%d (%s)", ret, strerror(ret));
  ...
}
```

### 2) “Same sequence, using cached timestamp” in Non-Blocking Mode

Das ist an sich OK (Sie rufen mehrmals pro Sekunde ab). Zwei Hinweise:

* Wenn Sie das sauber halten wollen: **pro Sekunde nur einmal fetchen** (blocking) und ansonsten ausschließlich Cache nutzen.
* Oder: Non-blocking beibehalten, aber das Log-Level für “Same sequence” stark runterdrehen (sonst spammt es und erschwert Fehlersuche).

### 3) Log-Interleaving / kaputte GPS Raw Zeile

Sie haben:

```
[GPS Raw] 420 bytes: $G0P (non-blocking mode, fd=10)
VTG,,T,,M,...
```

Das ist klassisches **Thread-Interleaving** (mehrere Threads schreiben gleichzeitig auf stdout). Das ist funktional nicht kritisch, aber es macht Debugging unnötig schwer.

Minimalmaßnahme: eine globale `std::mutex` um den Logger, oder konsequent `fprintf`/`write` in *einem* Logger-Thread.

### 4) “adjtimex command not found” ist unabhängig von Ihrem Programm

Ihr Programm kann den **Kernel-`adjtimex` Syscall** nutzen, ohne dass das Userland-Tool `adjtimex` installiert ist. Darum sehen Sie im Programm korrekt `TAI-UTC=37`, aber in der Shell:

`-bash: adjtimex: command not found`

Wenn Sie das CLI-Tool haben wollen:

```bash
sudo apt update
sudo apt install -y adjtimex
adjtimex --print | grep -i tai
```

---

## Nächste sinnvolle Checks (kurz, zielgerichtet)

### 1) Verifizieren, dass DS3231 weiterhin stabil +37s gegen UTC ist

```bash
date -u +%s
sudo hwclock --show --rtc=/dev/rtc1 --utc
```


```bash
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ date -u +%s
sudo hwclock --show --rtc=/dev/rtc1 --utc
1768225118
2026-01-12 14:39:16.083698+01:00
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $

```

Wenn die Differenz weiterhin **~37 s** ist, ist die RTC weiterhin effektiv TAI.

### 2) PPS-Phasenlage grob im Blick behalten

`ppstest /dev/pps1` zeigt weiterhin ~0.1s Phase (erwartbar). Das ist keine Drift.

---

## Bewertung

* **Ihr aktueller Lauf ist inhaltlich “gesund”:** keine ±1s Discontinuities, Driftwerte plausibel, Skipping nach Sync wirkt.
* Die verbleibenden Punkte sind überwiegend **Robustheit/Logging/Returncode-Interpretation** und kein grundlegendes Zeitmodell-Problem.

Wenn Sie möchten, kann ich Ihnen aus genau diesem Log ein kleines “Acceptance Criteria”-Set ableiten (welche Logzeilen in welcher Reihenfolge auftreten müssen und welche numerischen Grenzen sinnvoll sind), damit Sie das bei jedem Lauf automatisch validieren können.
