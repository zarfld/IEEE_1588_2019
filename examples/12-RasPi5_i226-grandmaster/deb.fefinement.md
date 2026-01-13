Die Disziplinierung einer **PTP Hardware Clock (PHC)** basierend auf einem **GPS 1PPS-Signal** (Pulse Per Second) erfordert eine präzise Abstimmung von Hardware-Erfassung und Software-Regelalgorithmen. Für Intel-Controller wie den i226 oder i210 umfasst dies die Korrektur der Frequenz (Rate) und der Phase (Offset).

### 1. Hardware-Anbindung und Signalerfassung
Um das PPS-Signal zu nutzen, muss es physikalisch an einen **Software Definable Pin (SDP)** des Controllers angeschlossen werden.
*   **Konfiguration:** Der entsprechende Pin muss als Eingang konfiguriert werden (`SDPx_IODIR = 0`).
*   **Auxiliary Timestamps (AUXSTMP):** Das PPS-Signal sollte die Hardware dazu veranlassen, den aktuellen Stand der PHC (`SYSTIM`) automatisch zu erfassen. Dies geschieht durch das Aktivieren von **Timestamp-Events** in den Registern `TSAUXC` und `TSSDP`.
*   **Latching-Mechanismus:** Wenn die Flanke des PPS-Signals eintrifft, speichert die Hardware den Zeitstempel in den Registern `AUXSTMPL` und `AUXSTMPH`. Software muss diese Register auslesen (zuerst Low, dann High), um den exakten Zeitpunkt der PPS-Ankunft relativ zur PHC zu bestimmen.

### 2. Best Practices für die Frequenz-Disziplinierung
Die Frequenzanpassung ist der wichtigste Schritt, um sicherzustellen, dass die PHC im gleichen Tempo tickt wie die GPS-Referenz.
*   **Regelungsregister (`TIMINCA`):** Die Taktgeschwindigkeit wird über das **Increment Attributes Register (`TIMINCA`)** gesteuert. Die Formel lautet: `Neuer SYSTIM = Alter SYSTIM + Basis-Inkrement +/- (Incvalue * 2^-32 ns)`.
*   **Iterative Anpassung:** Anstatt sprunghafte Änderungen vorzunehmen, sollte das `Incvalue` graduell als Funktion der gemessenen Drift und des Zeitabstands zwischen den PPS-Zyklen aktualisiert werden. 
*   **Einheiten-Präzision:** Da Sie eine Drift von über **51.000 ppm** (parts per million) messen, aber nur **500.000 ppb** (parts per billion = 0,5 ppm) anwenden, ist Ihre Korrektur etwa **100.000-mal zu klein**, um die Drift effektiv zu neutralisieren [User-Kontext]. Eine Drift in dieser Größenordnung deutet zudem oft auf eine falsche Referenzfrequenz-Konfiguration in der Software hin.
*   **Rate-Ratio-Berechnung:** Das Verhältnis der Frequenzen (Rate Ratio) wird durch den Vergleich aufeinanderfolgender PPS-Zeitstempel ermittelt. Ein Fehler von mehr als **±0.1 ppm** in der Messung sollte vermieden werden.

### 3. Best Practices für die Phasen-Disziplinierung
Sobald die Frequenz stabil ist, muss der absolute Zeitversatz (Offset) korrigiert werden.
*   **Feine Korrekturen (`TIMADJ`):** Für kleine Abweichungen (sub-Mikrosekunden) nutzt man das **Time Adjustment Offset Register (`TIMADJ`)**. Das Schreiben in dieses Register bewirkt, dass die Hardware bei jedem Taktzyklus 1 ns addiert oder subtrahiert, bis der gesamte Offset aufgezehrt ist.
*   **Grobkorrektur:** Wenn der Offset zu groß ist (z. B. größer als 1/8 des PPS-Intervalls), sollte die Zeit einmalig direkt in die `SYSTIM`-Register geschrieben werden, da die schrittweise Anpassung über `TIMADJ` sonst zu lange dauern würde.
*   **Atomarität beim Zugriff:** Beim Lesen oder Schreiben von Zeitregistern (96-Bit oder 64-Bit) muss zwingend die Reihenfolge eingehalten werden: Zuerst der niedrigwertige Teil (Low), dann der hochwertige Teil (High), um konsistente Werte zu garantieren.

### 4. Software-Architektur und Filterung
*   **Hysteresen nutzen:** Um ein "Springen" der Regelung zu verhindern (Thrashing), sollte eine Hysterese in die Varianz- und Frequenzschätzung eingebaut werden.
*   **Stabilität:** Das System sollte erst dann als synchronisiert gelten, wenn der Fehlerwert stabil unter einem definierten Schwellenwert liegt.
*   **Condition Monitoring:** Überprüfen Sie regelmäßig, ob das PPS-Signal valide ist, um zu verhindern, dass die PHC auf Basis fehlerhafter Daten diszipliniert wird.

***

### Analogie
Das Disziplinieren der PHC ist wie das **Stimmen eines Klaviers**:
1.  **Frequenz (`TIMINCA`):** Zuerst drehen Sie am Wirbel, um die **Grundspannung** der Saite (die Geschwindigkeit der Uhr) so einzustellen, dass sie genau den richtigen Ton trifft.
2.  **Phase (`TIMADJ`):** Wenn die Saite zwar die richtige Tonhöhe hat, aber zu einem **falschen Zeitpunkt** angeschlagen wurde, müssen Sie den Moment des Anschlags (den Offset) minimal anpassen, damit sie perfekt im Takt des restlichen Orchesters (GPS) spielt.

Ihre aktuelle Korrektur ist so, als würden Sie versuchen, ein komplett verstimmtes Klavier zu korrigieren, indem Sie den Wirbel nur um einen Millionstel Millimeter drehen – die Änderung ist für das Ohr (die PHC) kaum wahrnehmbar.