Du bist jetzt deutlich näher am Ziel. In deinem **letzten Run** sind die zwei wichtigsten Verbesserungen sichtbar:

* **Keine dauernden „RTC Discontinuity off by -1 second(s)”** mehr (das war vorher der Killer für Drift-Auswertung).
* Die **RTC-Drift-Werte stabilisieren sich im ppm-Bereich** (typisch um ~±10 ppm, Mittelwert bei dir zuletzt grob um −1 ppm).

Was noch klar auffällt (und der verbleibende Haupthebel ist):

## 1) Das „second boundary race” passiert (fast) jedes Mal

Beispiel:

```
offset_from_edge=55149 ns
✓ Detected second boundary race: RTC read before edge
RTC=... Edge(TAI)=... → Incrementing RTC second
```

Wenn du **konsequent** nur ~30–150 µs nach der PPS-Flanke bist, aber trotzdem ständig „RTC read before edge“ triggert, ist das in der Praxis fast immer **kein echter RTC-Fehler**, sondern ein **Read/Fetch-Ordering-Problem**:

* Du liest die RTC-Sekunden **vor** der Flanke (oder sehr knapp davor),
* holst danach den PPS-Timestamp, der **die bereits vergangene** Flanke repräsentiert,
* daraus entsteht künstlich „RTC ist 1 Sekunde hinten“.

### Konsequenz

Deine Drift-Messung wird zwar „gerettet“ durch deine Korrektur, aber:

* du **kontaminierst** Baselines (erste Drift-Samples),
* und du erzwingst „Reset drift buffer“ Situationen (z. B. dein 100001 ppm Ausreißer war genau so ein Artefakt).

## 2) Konkreter Fix: PPS-Flanke als Trigger verwenden, dann RTC lesen

Der robuste Ablauf ist:

1. **Auf die nächste PPS-Flanke warten** (`time_pps_fetch()` *mit Timeout*, nicht „poll last edge“).
2. **Unmittelbar danach** RTC-Sekunden lesen.
3. Sekunden+Nanosekunden konsistent zusammenbauen.

Pseudo-Flow:

```c
// 1) wait for next edge
time_pps_fetch(pps_fd, PPS_TSFMT_TSPEC, &info, &timeout);
edge_utc = info.assert_timestamp; // kernel timestamp for that edge

// 2) read RTC seconds immediately after the edge
rtc_sec = read_ds3231_seconds();  // your I2C/rtc read

// 3) convert scales consistently (UTC<->TAI) and combine
tai_utc = adjtimex().tai;
edge_tai_sec = edge_utc.tv_sec + tai_utc;
...
```

Damit verschwindet das „read before edge“ praktisch vollständig, weil du die RTC **nach** der echten Flanke liest.

## 3) Dein TAI-UTC Offset ist weiterhin korrekt: 37 Sekunden

Du druckst `TAI-UTC=37` – das ist auch **Stand jetzt** korrekt. Die IANA/IERS Leap-Seconds-Liste zeigt weiterhin **DTAI=37 seit 1 Jan 2017** und keine zusätzlichen Leap Seconds bis mindestens zur Dateigültigkeit der Liste (läuft bis 28. Juni 2026). ([IANA][1])

Das heißt: die beobachteten ±1s Effekte kommen **nicht** von einem „falschen TAI-UTC“, sondern von Logik/Sequencing.

## 4) Nebenbefunde, die du bereinigen solltest (klein, aber sinnvoll)

### a) `errno=11` bei `PPS_FETCH returned 0`

Du loggst `errno` auch dann, wenn der Returncode 0 ist. `errno` ist dann schlicht ein Altwert und irreführend.

Empfehlung: `errno` nur ausgeben, wenn `rc < 0`.

### b) „Skip 1–3 PPS cycles after PHC calibration“ auch für RTC-Drift anwenden

Du hast diesen Hinweis bereits im Output. In der Praxis solltest du nach:

* PHC-Calibration completion **und**
* RTC-Sync (Zeitdiskontinuität)

für z. B. **3–5 PPS** die Drift-Samples hart ignorieren, bevor du Baseline setzt. Das eliminiert genau diese ersten „100001 ppm“ Artefakte.

## 5) Schnelle Verifikation am System (ohne Codeänderung)

Damit du die Hypothese „Ordering/Scheduling“ sofort bestätigt bekommst:

1. PPS-Timestamp-Stabilität auf `/dev/pps1`:

```bash
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ sudo ppstest /dev/pps1
trying PPS source "/dev/pps1"
found PPS source "/dev/pps1"
ok, found 1 source(s), now start fetching data...
source 0 - assert 1768223277.095453202, sequence: 10529 - clear  0.000000000, sequence: 0
source 0 - assert 1768223278.095458317, sequence: 10530 - clear  0.000000000, sequence: 0
source 0 - assert 1768223279.095463339, sequence: 10531 - clear  0.000000000, sequence: 0
source 0 - assert 1768223280.095467102, sequence: 10532 - clear  0.000000000, sequence: 0
source 0 - assert 1768223281.095472105, sequence: 10533 - clear  0.000000000, sequence: 0
source 0 - assert 1768223282.095479164, sequence: 10534 - clear  0.000000000, sequence: 0
source 0 - assert 1768223283.095482853, sequence: 10535 - clear  0.000000000, sequence: 0

```

2. RTC vs System vs erwartete TAI-Sekunde (nur zur Plausibilisierung):

```bash

zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ date -u +%s
sudo hwclock -r -f /dev/rtc1 --utc
adjtimex --print | grep tai
1768223289
2026-01-12 14:08:46.573122+01:00
-bash: adjtimex: command not found

```

Wenn dein RTC in TAI läuft (wie dein Code es offenbar behandelt), dann erwartest du grob:

```
rtc_unix_seconds ≈ utc_unix_seconds + tai
```

und nicht ±1 Sekunde „springen“.

---

## Was ich aus deinem letzten Log konkret ableite

* **PHC calibration ist sauber** (Final drift 7.5 ppm akzeptiert).
* **RTC drift ist plausibel** (ppm-Bereich, keine dauerhafte −1s Discontinuity mehr).
* **Das dominante Restproblem ist die Sampling-Reihenfolge** (RTC-Read vs PPS-Fetch), die dir die ständigen Race-Korrekturen und gelegentlich extreme Drift-Outlier erzeugt.

Wenn du willst, kann ich dir als nächsten Schritt eine **präzise Patch-Skizze** für `rtc_adapter.cpp` formulieren (welche Funktionen/Abschnitte umzustellen sind, inkl. „wait for next edge“-Variante und Baseline/Skip-Logik).

[1]: https://data.iana.org/time-zones/tzdb/leap-seconds.list "data.iana.org"
