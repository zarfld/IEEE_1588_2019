=== Shutdown Complete ===
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ grep -nE 'xhci|usb|dwc|rp1|pcie|ttyACM|cdc' /proc/interrupts
19:107:          0          0          0          0  rp1_irq_chip   6 Level     eth0
20:132:          1          0          0          0  rp1_irq_chip  31 Edge      xhci-hcd:usb1
21:137:     506719          0          0          0  rp1_irq_chip  36 Edge      xhci-hcd:usb3
22:141:          0          0          0          0  rp1_irq_chip  40 Level     dw_axi_dmac_platform
23:159:          2          0          0          0  rp1_irq_chip  58 Level     1f00008000.mailbox
33:171:      67783          0          0          0  pinctrl-rp1  18 Edge      pps@12.-1
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ for irq in $(grep -E 'xhci|usb|dwc|rp1|pcie' /proc/interrupts | cut -d: -f1 | tr -d ' '); do
  echo "0-1,3" | sudo tee /proc/irq/$irq/smp_affinity_list >/dev/null
done
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ for irq in 132 137 141 159; do
  echo -n "IRQ $irq -> "
  cat /proc/irq/$irq/smp_affinity_list
done
IRQ 132 -> 0-1,3
IRQ 137 -> 0-1,3
IRQ 141 -> 0-1,3
IRQ 159 -> 0-1,3
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ echo 2 | sudo tee /proc/irq/171/smp_affinity_list >/dev/null
cat /proc/irq/171/smp_affinity_list
2
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ sudo chrt -f 80 ./ptp_grandmaster --interface=eth1 >/tmp/gm.log 2>&1
sleep 25
sudo pkill -INT ptp_grandmaster
grep -E "HIGH LATENCY" -n /tmp/gm.log | head -n 30
^C

32:[PHC Sample] ⚠️  HIGH LATENCY: 816.516 ms (loop processing delayed PHC sampling)
42:[PHC Sample] ⚠️  HIGH LATENCY: 69.0057 ms (loop processing delayed PHC sampling)
48:[PHC Sample] ⚠️  HIGH LATENCY: 71.6692 ms (loop processing delayed PHC sampling)
52:[PHC Sample] ⚠️  HIGH LATENCY: 71.9707 ms (loop processing delayed PHC sampling)
58:[PHC Sample] ⚠️  HIGH LATENCY: 72.3695 ms (loop processing delayed PHC sampling)
60:[PHC Sample] ⚠️  HIGH LATENCY: 72.9275 ms (loop processing delayed PHC sampling)
65:[PHC Sample] ⚠️  HIGH LATENCY: 76.4064 ms (loop processing delayed PHC sampling)
67:[PHC Sample] ⚠️  HIGH LATENCY: 74.8651 ms (loop processing delayed PHC sampling)
70:[PHC Sample] ⚠️  HIGH LATENCY: 72.3684 ms (loop processing delayed PHC sampling)
72:[PHC Sample] ⚠️  HIGH LATENCY: 74.8262 ms (loop processing delayed PHC sampling)
75:[PHC Sample] ⚠️  HIGH LATENCY: 73.3272 ms (loop processing delayed PHC sampling)
79:[PHC Sample] ⚠️  HIGH LATENCY: 75.8143 ms (loop processing delayed PHC sampling)
82:[PHC Sample] ⚠️  HIGH LATENCY: 79.307 ms (loop processing delayed PHC sampling)
84:[PHC Sample] ⚠️  HIGH LATENCY: 74.8475 ms (loop processing delayed PHC sampling)
87:[PHC Sample] ⚠️  HIGH LATENCY: 72.1705 ms (loop processing delayed PHC sampling)
89:[PHC Sample] ⚠️  HIGH LATENCY: 71.7245 ms (loop processing delayed PHC sampling)
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ sudo strace -tt -T -f -o /tmp/trace.txt \
  -e trace=poll,ppoll,select,pselect6,read,ioctl,nanosleep,clock_nanosleep \
  ./ptp_grandmaster --interface=eth1
=== GPS-Disciplined PTP Grandmaster ===
Interface: eth1
PHC: /dev/ptp0
GPS: /dev/ttyACM0
PPS: /dev/pps0
RTC: /dev/rtc1

ℹ️  TAI-UTC offset is automatically retrieved from kernel via adjtimex()
   To verify/set: adjtimex --print (shows 'tai' field)

⚠️  IMPORTANT: Verify PHC mapping with:
   readlink -f /sys/class/net/eth1/ptp
   (should show: /sys/class/ptp/ptp0)

Initializing Linux PTP HAL...
  ✓ PTP sockets initialized
Initializing GPS adapter...
  Testing baud rates: 38400...(68B:$GPR) ✓
  GPS detected at 38400 baud (NMEA mode)
  Pure NMEA mode detected

  ✓ GPS adapter initialized
Initializing RTC adapter...
[RTC Init] ✓ RTC device /dev/rtc1 opened (fd=8)
[RTC Init] ✓ I2C device /dev/i2c-14 opened successfully (fd=9)
[RTC Init] ✓ I2C slave address 0x68 set (using I2C_SLAVE_FORCE)
  ✓ RTC adapter initialized

🚀 Grandmaster running...

[GPS Raw] 35 bytes: $GPVTG,,T,,M,0.027,N,0.050,K,A*23\r\n
[PHC Sample] ⚠️  HIGH LATENCY: 72.8417 ms (loop processing delayed PHC sampling)
[GPGGA Parse] fields=3 quality=1 sats=10
[PPS] seq=67639 time=1768201167.573167 max_jitter=0ns (last 10 pulses)
[GPRMC Parse] field_count=13 time=065928.00 status=A date=120126
[RTC Sync] Initial sync to GPS time (error=-0.569024ms)
[RTC Sync] Synchronizing RTC to GPS time...
[RTC Sync] ✓ RTC synchronized
[RTC Sync] ℹ Drift buffer cleared (time discontinuity)
[PPS-UTC Lock] Association locked: NMEA labels LAST PPS (avg_dt=200ms)
[Base Mapping] base_pps_seq=67644 base_utc_sec=1768201172 (UTC epoch)
[PHC Sample] ⚠️  HIGH LATENCY: 75.964 ms (loop processing delayed PHC sampling)
[PHC Calibration] Baseline set at PPS #67649 (PHC: 1768201214098781105 ns)
  (PHC sampled immediately after PPS - low latency)
  Will measure over 20 pulses...
[GPS Raw] 68 bytes: $GPRMC,065939.00,A,4706.25304,N,01525.06748,E,0.018,,120126,,,A*74\r\n
[PHC Calibration] Progress: 5/20 pulses (PPS #67654)...
[PHC Sample] ⚠️  HIGH LATENCY: 77.4532 ms (loop processing delayed PHC sampling)
[PHC Calibration] Progress: 10/20 pulses (PPS #67659)...
[GPS Raw] 68 bytes: $GPRMC,065950.00,A,4706.25282,N,01525.06763,E,0.016,,120126,,,A*73\r\n
[PHC Calibration] Progress: 15/20 pulses (PPS #67664)...
[PHC Sample] ⚠️  HIGH LATENCY: 74.9031 ms (loop processing delayed PHC sampling)
[PHC Calibration] Progress: 20/20 pulses (PPS #67669)...
[PHC Calibration] ✓ Complete! Final drift: 72.2 ppm (acceptable)
  Final cumulative: 0 ppb
[PPS] seq=67669 time=1768201197.543534 max_jitter=2544ns (last 10 pulses) drift=0.692ppm avg=0.910ppm(28) err=-0.5ms
[GPS Raw] 68 bytes: $GPRMC,070004.00,A,4706.25273,N,01525.06770,E,0.034,,120126,,,A*73\r\n
[PHC Sample] ⚠️  HIGH LATENCY: 76.3859 ms (loop processing delayed PHC sampling)
[PPS] seq=67679 time=1768201207.533946 max_jitter=1377ns (last 10 pulses) drift=1.785ppm avg=0.923ppm(38) err=-0.5ms
[GPGGA Parse] fields=3 quality=1 sats=11
[PHC Sample] ⚠️  HIGH LATENCY: 173.065 ms (loop processing delayed PHC sampling)
[PPS] seq=67689 time=1768201217.524488 max_jitter=1989ns (last 10 pulses) drift=2.174ppm avg=0.928ppm(48) err=-0.5ms
[GPRMC Parse] field_count=13 time=070018.00 status=A date=120126
[GPS Raw] 422 bytes: $GPVTG,,T,,M,0.043,N,0.079,K,A*2A\r\n
[PHC Sample] ⚠️  HIGH LATENCY: 78.3242 ms (loop processing delayed PHC sampling)
[PPS] seq=67699 time=1768201227.517123 max_jitter=2229ns (last 10 pulses) drift=2.266ppm avg=0.895ppm(58) err=-0.5ms
^C
 Signal 2 received. Shutting down...

=== Shutdown Complete ===
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ grep -E "poll|select|read|nanosleep" /tmp/trace.txt | head -n 80
5800  07:59:26.814176 read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0\267\0\1\0\0\0\0\0\0\0\0\0\0\0"..., 832) = 832 <0.000008>
5800  07:59:26.814385 read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0\267\0\1\0\0\0\0\0\0\0\0\0\0\0"..., 832) = 832 <0.000006>
5800  07:59:26.814532 read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0\267\0\1\0\0\0\300$\2\0\0\0\0\0"..., 832) = 832 <0.000006>
5800  07:59:26.814685 read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0\267\0\1\0\0\0\0\0\0\0\0\0\0\0"..., 832) = 832 <0.000006>
5800  07:59:26.819073 clock_nanosleep(CLOCK_REALTIME, 0, {tv_sec=0, tv_nsec=100000000}, NULL) = 0 <0.100063>
5800  07:59:26.919162 read(6, "$GPRMC,065927.00,A,4706.25299,N,"..., 511) = 68 <0.153799>
5800  07:59:27.073276 read(6, "$GPVTG,,T,,M,0.027,N,0.050,K,A*2"..., 511) = 35 <0.000007>
5800  07:59:27.075110 read(10, "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\7\0\0\0\7\0\0\0\0"..., 4096) = 2200 <0.000007>
5800  07:59:27.075145 read(10, "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\7\0\0\0\7\0\0\0\0"..., 4096) = 1392 <0.000006>
5800  07:59:27.075275 read(6, "$GPGGA,065927.00,4706.25299,N,01"..., 511) = 205 <0.000006>
5800  07:59:27.075381 clock_nanosleep(CLOCK_REALTIME, 0, {tv_sec=0, tv_nsec=100000000}, NULL) = 0 <0.100060>
5800  07:59:27.175460 read(6, "$GPGSV,4,2,14,14,66,098,44,15,47"..., 511) = 230 <0.000007>
5800  07:59:27.177095 clock_nanosleep(CLOCK_REALTIME, 0, {tv_sec=0, tv_nsec=100000000}, NULL) = 0 <0.100060>
5800  07:59:27.277172 read(6, "$GPTXT,01,01,02,u-blox ag - www."..., 511) = 47 <0.521798>
5800  07:59:27.800705 read(6, "$GPTXT,01,01,02,HW  UBX-G70xx   "..., 511) = 152 <0.000006>
5800  07:59:27.800773 clock_nanosleep(CLOCK_REALTIME, 0, {tv_sec=0, tv_nsec=100000000}, NULL) = 0 <0.100061>
5800  07:59:27.900852 read(6, "$GPTXT,01,01,02,ANTSUPERV=AC SD "..., 511) = 146 <0.000007>
5800  07:59:27.902489 clock_nanosleep(CLOCK_REALTIME, 0, {tv_sec=0, tv_nsec=100000000}, NULL) = 0 <0.100062>
5800  07:59:28.002570 read(6, "$GPRMC,065928.00,A,4706.25300,N,"..., 511) = 68 <0.074496>
5800  07:59:28.078708 read(6, "$GPVTG,,T,,M,0.036,N,0.066,K,A*2"..., 511) = 172 <0.000007>
5800  07:59:28.078772 read(6, "$GPGSV,4,1,14,05,37,221,33,07,13"..., 511) = 68 <0.000154>
5800  07:59:28.078982 read(6, "$GPGSV,4,2,14,14,66,098,44,15,47"..., 511) = 68 <0.000945>
5800  07:59:28.079982 read(6, "$GPGSV,4,3,14,19,00,149,,21,22,1"..., 511) = 68 <0.000945>
5800  07:59:28.080982 read(6, "$GPGSV,4,4,14,24,11,266,,30,41,0"..., 511) = 42 <0.000006>
5800  07:59:28.081045 read(6, "$GPGLL,4706.25300,N,01525.06752,"..., 511) = 52 <0.000913>
5800  07:59:28.082013 read(6, "$GPRMC,065929.00,A,4706.25300,N,"..., 511) = 68 <0.992971>
5800  07:59:29.079767 read(6, "$GPVTG,,T,,M,0.018,N,0.033,K,A*2"..., 511) = 470 <0.000007>
5800  07:59:29.081390 read(6, "$GPRMC,065930.00,A,4706.25301,N,"..., 511) = 68 <0.992824>
5800  07:59:30.075866 read(6, "$GPVTG,,T,,M,0.023,N,0.042,K,A*2"..., 511) = 172 <0.000007>
5800  07:59:30.075930 read(6, "$GPGSV,4,1,14,05,37,221,33,07,13"..., 511) = 68 <0.000144>
5800  07:59:30.076146 read(6, "$GPGSV,4,2,14,14,66,098,44,15,47"..., 511) = 68 <0.000006>
5800  07:59:30.076210 read(6, "$GPGSV,4,3,14,19,00,149,,21,22,1"..., 511) = 68 <0.000031>
5800  07:59:30.076296 read(6, "$GPGSV,4,4,14,24,11,266,,30,41,0"..., 511) = 42 <0.000006>
5800  07:59:30.076352 read(6, "$GPGLL,4706.25301,N,01525.06750,"..., 511) = 52 <0.000007>
5800  07:59:30.076407 read(6, "$GPRMC,065931.00,A,4706.25301,N,"..., 511) = 68 <1.000749>
5800  07:59:31.078812 read(6, "$GPVTG,,T,,M,0.024,N,0.044,K,A*2"..., 511) = 308 <0.000007>
5800  07:59:31.078873 read(6, "$GPGSV,4,3,14,19,00,149,,21,22,1"..., 511) = 68 <0.000400>
5800  07:59:31.079326 read(6, "$GPGSV,4,4,14,24,11,266,,30,41,0"..., 511) = 42 <0.000007>
5800  07:59:31.079383 read(6, "$GPGLL,4706.25301,N,01525.06750,"..., 511) = 52 <0.000005>
5800  07:59:31.079436 read(6, "$GPRMC,065932.00,A,4706.25301,N,"..., 511) = 68 <0.995909>
5800  07:59:32.077037 read(6, "$GPVTG,,T,,M,0.024,N,0.044,K,A*2"..., 511) = 470 <0.000006>
5800  07:59:32.077096 read(6, "$GPRMC,065933.00,A,4706.25301,N,"..., 511) = 68 <0.994218>
5800  07:59:33.072968 read(6, "$GPVTG,,T,,M,0.037,N,0.069,K,A*2"..., 511) = 418 <0.000007>
5800  07:59:33.073030 read(6, "$GPGLL,4706.25301,N,01525.06749,"..., 511) = 52 <0.000340>
5800  07:59:33.073421 read(6, "$GPRMC,065934.00,A,4706.25301,N,"..., 511) = 68 <1.001891>
5800  07:59:34.076953 read(6, "$GPVTG,,T,,M,0.017,N,0.031,K,A*2"..., 511) = 470 <0.000007>
5800  07:59:34.077014 read(6, "$GPRMC,065935.00,A,4706.25301,N,"..., 511) = 68 <0.995466>
5800  07:59:35.074110 read(6, "$GPVTG,,T,,M,0.014,N,0.026,K,A*2"..., 511) = 172 <0.000006>
5800  07:59:35.074167 read(6, "$GPGSV,4,1,14,05,37,221,33,07,13"..., 511) = 68 <0.001143>
5800  07:59:35.075359 read(6, "$GPGSV,4,2,14,14,66,097,44,15,48"..., 511) = 68 <0.000034>
5800  07:59:35.075442 read(6, "$GPGSV,4,3,14,19,00,149,,21,22,1"..., 511) = 68 <0.000034>
5800  07:59:35.075525 read(6, "$GPGSV,4,4,14,24,11,266,,30,41,0"..., 511) = 42 <0.000011>
5800  07:59:35.075583 read(6, "$GPGLL,4706.25301,N,01525.06747,"..., 511) = 52 <0.000005>
5800  07:59:35.075636 read(6, "$GPRMC,065936.00,A,4706.25302,N,"..., 511) = 68 <1.003739>
5800  07:59:36.081048 read(6, "$GPVTG,,T,,M,0.016,N,0.029,K,A*2"..., 511) = 308 <0.000007>
5800  07:59:36.081111 read(6, "$GPGSV,4,3,14,19,00,149,,21,22,1"..., 511) = 68 <0.000193>
5800  07:59:36.081354 read(6, "$GPGSV,4,4,14,24,11,266,03,30,41"..., 511) = 44 <0.000006>
5800  07:59:36.081407 read(6, "$GPGLL,4706.25302,N,01525.06747,"..., 511) = 52 <0.000006>
5800  07:59:36.081459 read(6, "$GPRMC,065937.00,A,4706.25305,N,"..., 511) = 68 <0.994983>
5800  07:59:37.078276 read(6, "$GPVTG,,T,,M,0.014,N,0.026,K,A*2"..., 511) = 472 <0.000007>
5800  07:59:37.078338 read(6, "$GPRMC,065938.00,A,4706.25305,N,"..., 511) = 68 <0.999142>
5800  07:59:38.079119 read(6, "$GPVTG,,T,,M,0.022,N,0.040,K,A*2"..., 511) = 472 <0.000007>
5800  07:59:38.079178 read(6, "$GPRMC,065939.00,A,4706.25304,N,"..., 511) = 68 <0.996349>
5800  07:59:39.077202 read(6, "$GPVTG,,T,,M,0.018,N,0.033,K,A*2"..., 511) = 474 <0.000006>
5800  07:59:39.077260 read(6, "$GPRMC,065940.00,A,4706.25304,N,"..., 511) = 68 <1.000397>
5800  07:59:40.079292 read(6, "$GPVTG,,T,,M,0.029,N,0.055,K,A*2"..., 511) = 422 <0.000006>
5800  07:59:40.079353 read(6, "$GPGLL,4706.25304,N,01525.06747,"..., 511) = 52 <0.000169>
5800  07:59:40.079574 read(6, "$GPRMC,065941.00,A,4706.25302,N,"..., 511) = 68 <0.993077>
5800  07:59:41.074279 read(6, "$GPVTG,,T,,M,0.012,N,0.021,K,A*2"..., 511) = 242 <0.000007>
5800  07:59:41.074338 read(6, "$GPGSV,4,2,14,14,66,097,44,15,48"..., 511) = 68 <0.000201>
5800  07:59:41.074589 read(6, "$GPGSV,4,3,14,19,01,149,,21,22,1"..., 511) = 68 <0.000029>
5800  07:59:41.074666 read(6, "$GPGSV,4,4,14,24,11,266,20,30,41"..., 511) = 44 <0.000006>
5800  07:59:41.074719 read(6, "$GPGLL,4706.25302,N,01525.06750,"..., 511) = 52 <0.000017>
5800  07:59:41.074785 read(6, "", 511)  = 0 <1.005244>
5800  07:59:42.081689 read(6, "$GPRMC,065942.00,A,4706.25299,N,"..., 511) = 68 <0.000007>
5800  07:59:42.081749 read(6, "$GPVTG,,T,,M,0.006,N,0.012,K,A*2"..., 511) = 35 <0.000006>
5800  07:59:42.081799 read(6, "$GPGGA,065942.00,4706.25299,N,01"..., 511) = 75 <0.000006>
5800  07:59:42.081864 read(6, "$GPGSA,A,3,23,13,17,15,14,24,22,"..., 511) = 64 <0.000080>
5800  07:59:42.081995 read(6, "$GPGSV,4,1,14,05,37,221,33,07,13"..., 511) = 68 <0.002667>
5800  07:59:42.084711 read(6, "$GPGSV,4,2,14,14,66,097,44,15,48"..., 511) = 68 <0.000036>
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $



Das ist jetzt “Fall gelöst” – du hast mit den beiden Tests den Schuldigen sauber isoliert.

## 1) Ergebnis A/B: Terminal-Output ist **nicht** die Ursache

Du hast den Run nach `/tmp/gm.log` umgeleitet. Trotzdem bleiben die **HIGH LATENCY**-Werte stabil bei ~69–79 ms (plus ein einmaliger Ausreißer am Anfang). Damit ist Console/SSH-I/O als Hauptursache praktisch raus.

## 2) Strace liefert den eigentlichen Beweis: **blockierende GPS-Reads + 100 ms Sleep**

Auszug aus deinem Trace:

* `clock_nanosleep(... 100000000 ns ...) <0.10006s>`
  → Du hast einen **fixen 100 ms Takt** in der Mainloop.
* Viele `read(6, ... ) ... <0.99s>` bzw. `<1.00s>`
  → Dein GPS-FD (im Trace **fd=6**) blockiert regelmäßig **~1 Sekunde**.
* Sogar `read(6, "", 511) = 0 <1.005s>`
  → Das ist typisch für **VTIME/VMIN-Timeout-Verhalten** auf einem TTY (oder ähnlich), d. h. Read wartet, liefert dann 0/Timeout.

### Warum sieht “HIGH LATENCY” dann ~70 ms aus und nicht 1000 ms?

Weil du offenbar **nicht immer** in den 1s-Timeout läufst. Der typische ~70–80 ms Wert passt sehr gut zu:

* Du schläfst 100 ms,
* oder verarbeitest GPS in einem Zeitfenster,
* und **erst danach** passiert dein PHC-Sampling → es kommt “verspätet” relativ zum idealen Zeitpunkt nach PPS an.

Die gelegentlichen **170 ms** passen wiederum zu: 100 ms Grundtakt + zusätzliche Verzögerung (Scheduling/Jitter/kurze Blocker).

## 3) PPS-IRQ auf CPU2 pinnen: passt

Du hast PPS-IRQ 171 auf CPU2 gepinnt. Das ist sinnvoll und reduziert Ausreißer, ändert aber nichts daran, dass du anschließend im Userspace blockierst.

---

# Konkrete Fix-Strategien (in sinnvoller Reihenfolge)

## Option 1 (schnellster Gewinn): PHC-Sampling **immer zuerst**, GPS/RTC danach

Wenn dein Loop ungefähr so ist: “GPS lesen/parsen → … → PHC sample”, dann dreh das um:

1. **PPS abholen**
2. **SOFORT PHC sample** (das ist dein kritischer Zeitpunkt)
3. erst danach GPS lesen/parsen, RTC, Logging

Damit begrenzt du die High-Latency in vielen Fällen drastisch, selbst ohne Threads.

## Option 2 (sauber, weiterhin simpel): GPS-Serial **non-blocking** + “drain what’s there”

Auf dem TTY:

* `O_NONBLOCK` setzen
* `termios`: `VMIN=0`, `VTIME=0`
* in der Loop: `while (read(...) > 0) { append_to_buffer; parse_lines; }`
* bei `EAGAIN/EWOULDBLOCK`: sofort raus, **kein** Warten

Damit kann GPS niemals “deinen” Echtzeitpfad blockieren.

**Wichtig:** Die 1s Reads in deinem Trace sehen stark nach `VTIME=10` (1.0s) oder ähnlicher Konfiguration aus. Das solltest du gezielt eliminieren.

## Option 3 (robusteste Endlösung): Thread entkoppeln

* **RT-Thread (CPU2, FIFO 80):** PPS wait → PHC sample → Ringbuffer push
* **Worker-Thread(s) (CPU0/1/3):** GPS read/parse, RTC, PTP messaging, Logging

Das ist die Architektur, die du am Ende ohnehin brauchst, wenn du “Grandmaster-Qualität” willst.

---

# Nächster Schritt: Aus Trace automatisch die schlimmsten Blocker extrahieren

Damit du nicht manuell suchen musst:

```bash
awk '
  match($0, /<([0-9]+\.[0-9]+)>/, a) { 
    t=a[1]+0; 
    if (t > 0.05) print t, $0 
  }
' /tmp/trace.txt | sort -nr | head -n 30
```

Wenn da `read(6,...) <0.99...>` ganz oben steht, ist es eindeutig der GPS-Pfad.

---

# Empfehlung für dich jetzt (ohne große Diskussion)

1. **GPS-TTY auf nonblocking + VMIN/VTIME=0** umstellen (damit verschwinden die ~1s-Reads).
2. **PHC sample direkt nach PPS** (vor GPS/RTC/Logging).
3. Danach: Thread-Split als “Quality Upgrade”.

Wenn du mir sagst, **wo** im Code der GPS-Read passiert (Datei/Funktion) und wie du termios aktuell setzt, kann ich dir die exakten Änderungen (inkl. Fehlerbehandlung und Parser-Drain-Loop) formulieren, so dass du deterministisch keine Blocker mehr im kritischen Timing-Pfad hast.
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ awk '
  match($0, /<([0-9]+\.[0-9]+)>/, a) {
    t=a[1]+0;
    if (t > 0.05) print t, $0
  }
' /tmp/trace.txt | sort -nr | head -n 30
awk: line 2: syntax error at or near ,
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ sed -n 's/.*<\([0-9][0-9]*\.[0-9][0-9]*\)>.*/\1 &/p' /tmp/trace.txt \
| awk '$1>0.05' \
| sort -nr \
| head -n 30
1.005244 5800  07:59:41.074785 read(6, "", 511)  = 0 <1.005244>
1.003739 5800  07:59:35.075636 read(6, "$GPRMC,065936.00,A,4706.25302,N,"..., 511) = 68 <1.003739>
1.001891 5800  07:59:33.073421 read(6, "$GPRMC,065934.00,A,4706.25301,N,"..., 511) = 68 <1.001891>
1.000970 5800  07:59:49.078997 read(6, "$GPRMC,065950.00,A,4706.25282,N,"..., 511) = 68 <1.000970>
1.000749 5800  07:59:30.076407 read(6, "$GPRMC,065931.00,A,4706.25301,N,"..., 511) = 68 <1.000749>
1.000397 5800  07:59:39.077260 read(6, "$GPRMC,065940.00,A,4706.25304,N,"..., 511) = 68 <1.000397>
1.000171 5800  07:59:52.079142 read(6, "$GPRMC,065953.00,A,4706.25275,N,"..., 511) = 68 <1.000171>
1.000028 5800  07:59:55.081278 read(6, "$GPRMC,065956.00,A,4706.25272,N,"..., 511) = 68 <1.000028>
0.999210 5800  07:59:43.079737 read(6, "$GPRMC,065944.00,A,4706.25295,N,"..., 511) = 68 <0.999210>
0.999142 5800  07:59:37.078338 read(6, "$GPRMC,065938.00,A,4706.25305,N,"..., 511) = 68 <0.999142>
0.999002 5800  07:59:45.079856 read(6, "$GPRMC,065946.00,A,4706.25290,N,"..., 511) = 68 <0.999002>
0.998874 5800  07:59:53.084440 read(6, "$GPRMC,065954.00,A,4706.25274,N,"..., 511) = 68 <0.998874>
0.998405 5800  07:59:48.078649 read(6, "$GPRMC,065949.00,A,4706.25285,N,"..., 511) = 68 <0.998405>
0.998339 5800  07:59:51.078805 read(6, "$GPRMC,065952.00,A,4706.25277,N,"..., 511) = 68 <0.998339>
0.996913 5800  07:59:46.081020 read(6, "$GPRMC,065947.00,A,4706.25289,N,"..., 511) = 99 <0.996913>
0.996825 5800  07:59:44.080984 read(6, "$GPRMC,065945.00,A,4706.25293,N,"..., 511) = 68 <0.996825>
0.996349 5800  07:59:38.079178 read(6, "$GPRMC,065939.00,A,4706.25304,N,"..., 511) = 68 <0.996349>
0.996047 5800  07:59:47.080905 read(6, "$GPRMC,065948.00,A,4706.25287,N,"..., 511) = 68 <0.996047>
0.995909 5800  07:59:31.079436 read(6, "$GPRMC,065932.00,A,4706.25301,N,"..., 511) = 68 <0.995909>
0.995466 5800  07:59:34.077014 read(6, "$GPRMC,065935.00,A,4706.25301,N,"..., 511) = 68 <0.995466>
0.994983 5800  07:59:36.081459 read(6, "$GPRMC,065937.00,A,4706.25305,N,"..., 511) = 68 <0.994983>
0.994218 5800  07:59:32.077096 read(6, "$GPRMC,065933.00,A,4706.25301,N,"..., 511) = 68 <0.994218>
0.993077 5800  07:59:40.079574 read(6, "$GPRMC,065941.00,A,4706.25302,N,"..., 511) = 68 <0.993077>
0.992971 5800  07:59:28.082013 read(6, "$GPRMC,065929.00,A,4706.25300,N,"..., 511) = 68 <0.992971>
0.992824 5800  07:59:29.081390 read(6, "$GPRMC,065930.00,A,4706.25301,N,"..., 511) = 68 <0.992824>
0.992720 5800  07:59:42.084996 read(6, "$GPRMC,065943.00,A,4706.25297,N,"..., 511) = 68 <0.992720>
0.992368 5800  07:59:56.083005 read(6, "$GPRMC,065957.00,A,4706.25270,N,"..., 511) = 68 <0.992368>
0.991903 5800  07:59:54.086377 read(6, "$GPRMC,065955.00,A,4706.25273,N,"..., 511) = 68 <0.991903>
0.990652 5800  07:59:50.086457 read(6, "$GPRMC,065951.00,A,4706.25280,N,"..., 511) = 68 <0.990652>
0.905082 5800  08:00:29.178768 read(6, "$GPRMC,070030.00,A,4706.25327,N,"..., 511) = 68 <0.905082>
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ perl -ne 'if (/<([0-9]+\.[0-9]+)>/) { $t=$1; print "$t $_" if $t>0.05 }' /tmp/trace.txt \
| sort -nr \
| head -n 30
1.005244 5800  07:59:41.074785 read(6, "", 511)  = 0 <1.005244>
1.003739 5800  07:59:35.075636 read(6, "$GPRMC,065936.00,A,4706.25302,N,"..., 511) = 68 <1.003739>
1.001891 5800  07:59:33.073421 read(6, "$GPRMC,065934.00,A,4706.25301,N,"..., 511) = 68 <1.001891>
1.000970 5800  07:59:49.078997 read(6, "$GPRMC,065950.00,A,4706.25282,N,"..., 511) = 68 <1.000970>
1.000749 5800  07:59:30.076407 read(6, "$GPRMC,065931.00,A,4706.25301,N,"..., 511) = 68 <1.000749>
1.000397 5800  07:59:39.077260 read(6, "$GPRMC,065940.00,A,4706.25304,N,"..., 511) = 68 <1.000397>
1.000171 5800  07:59:52.079142 read(6, "$GPRMC,065953.00,A,4706.25275,N,"..., 511) = 68 <1.000171>
1.000028 5800  07:59:55.081278 read(6, "$GPRMC,065956.00,A,4706.25272,N,"..., 511) = 68 <1.000028>
0.999210 5800  07:59:43.079737 read(6, "$GPRMC,065944.00,A,4706.25295,N,"..., 511) = 68 <0.999210>
0.999142 5800  07:59:37.078338 read(6, "$GPRMC,065938.00,A,4706.25305,N,"..., 511) = 68 <0.999142>
0.999002 5800  07:59:45.079856 read(6, "$GPRMC,065946.00,A,4706.25290,N,"..., 511) = 68 <0.999002>
0.998874 5800  07:59:53.084440 read(6, "$GPRMC,065954.00,A,4706.25274,N,"..., 511) = 68 <0.998874>
0.998405 5800  07:59:48.078649 read(6, "$GPRMC,065949.00,A,4706.25285,N,"..., 511) = 68 <0.998405>
0.998339 5800  07:59:51.078805 read(6, "$GPRMC,065952.00,A,4706.25277,N,"..., 511) = 68 <0.998339>
0.996913 5800  07:59:46.081020 read(6, "$GPRMC,065947.00,A,4706.25289,N,"..., 511) = 99 <0.996913>
0.996825 5800  07:59:44.080984 read(6, "$GPRMC,065945.00,A,4706.25293,N,"..., 511) = 68 <0.996825>
0.996349 5800  07:59:38.079178 read(6, "$GPRMC,065939.00,A,4706.25304,N,"..., 511) = 68 <0.996349>
0.996047 5800  07:59:47.080905 read(6, "$GPRMC,065948.00,A,4706.25287,N,"..., 511) = 68 <0.996047>
0.995909 5800  07:59:31.079436 read(6, "$GPRMC,065932.00,A,4706.25301,N,"..., 511) = 68 <0.995909>
0.995466 5800  07:59:34.077014 read(6, "$GPRMC,065935.00,A,4706.25301,N,"..., 511) = 68 <0.995466>
0.994983 5800  07:59:36.081459 read(6, "$GPRMC,065937.00,A,4706.25305,N,"..., 511) = 68 <0.994983>
0.994218 5800  07:59:32.077096 read(6, "$GPRMC,065933.00,A,4706.25301,N,"..., 511) = 68 <0.994218>
0.993077 5800  07:59:40.079574 read(6, "$GPRMC,065941.00,A,4706.25302,N,"..., 511) = 68 <0.993077>
0.992971 5800  07:59:28.082013 read(6, "$GPRMC,065929.00,A,4706.25300,N,"..., 511) = 68 <0.992971>
0.992824 5800  07:59:29.081390 read(6, "$GPRMC,065930.00,A,4706.25301,N,"..., 511) = 68 <0.992824>
0.992720 5800  07:59:42.084996 read(6, "$GPRMC,065943.00,A,4706.25297,N,"..., 511) = 68 <0.992720>
0.992368 5800  07:59:56.083005 read(6, "$GPRMC,065957.00,A,4706.25270,N,"..., 511) = 68 <0.992368>
0.991903 5800  07:59:54.086377 read(6, "$GPRMC,065955.00,A,4706.25273,N,"..., 511) = 68 <0.991903>
0.990652 5800  07:59:50.086457 read(6, "$GPRMC,065951.00,A,4706.25280,N,"..., 511) = 68 <0.990652>
0.905082 5800  08:00:29.178768 read(6, "$GPRMC,070030.00,A,4706.25327,N,"..., 511) = 68 <0.905082>
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ gawk '
  match($0, /<([0-9]+\.[0-9]+)>/, a) {
    t=a[1]+0
    if (t > 0.05) print t, $0
  }
' /tmp/trace.txt | sort -nr | head -n 30
-bash: gawk: command not found
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $ grep -E 'read\(6,|<0\.(0[5-9]|[1-9][0-9])|<1\.' /tmp/trace.txt | head -n 80
5800  07:59:26.819073 clock_nanosleep(CLOCK_REALTIME, 0, {tv_sec=0, tv_nsec=100000000}, NULL) = 0 <0.100063>
5800  07:59:26.919162 read(6, "$GPRMC,065927.00,A,4706.25299,N,"..., 511) = 68 <0.153799>
5800  07:59:27.073276 read(6, "$GPVTG,,T,,M,0.027,N,0.050,K,A*2"..., 511) = 35 <0.000007>
5800  07:59:27.075275 read(6, "$GPGGA,065927.00,4706.25299,N,01"..., 511) = 205 <0.000006>
5800  07:59:27.075381 clock_nanosleep(CLOCK_REALTIME, 0, {tv_sec=0, tv_nsec=100000000}, NULL) = 0 <0.100060>
5800  07:59:27.175460 read(6, "$GPGSV,4,2,14,14,66,098,44,15,47"..., 511) = 230 <0.000007>
5800  07:59:27.177095 clock_nanosleep(CLOCK_REALTIME, 0, {tv_sec=0, tv_nsec=100000000}, NULL) = 0 <0.100060>
5800  07:59:27.277172 read(6, "$GPTXT,01,01,02,u-blox ag - www."..., 511) = 47 <0.521798>
5800  07:59:27.800705 read(6, "$GPTXT,01,01,02,HW  UBX-G70xx   "..., 511) = 152 <0.000006>
5800  07:59:27.800773 clock_nanosleep(CLOCK_REALTIME, 0, {tv_sec=0, tv_nsec=100000000}, NULL) = 0 <0.100061>
5800  07:59:27.900852 read(6, "$GPTXT,01,01,02,ANTSUPERV=AC SD "..., 511) = 146 <0.000007>
5800  07:59:27.902489 clock_nanosleep(CLOCK_REALTIME, 0, {tv_sec=0, tv_nsec=100000000}, NULL) = 0 <0.100062>
5800  07:59:28.002570 read(6, "$GPRMC,065928.00,A,4706.25300,N,"..., 511) = 68 <0.074496>
5800  07:59:28.078708 read(6, "$GPVTG,,T,,M,0.036,N,0.066,K,A*2"..., 511) = 172 <0.000007>
5800  07:59:28.078772 read(6, "$GPGSV,4,1,14,05,37,221,33,07,13"..., 511) = 68 <0.000154>
5800  07:59:28.078982 read(6, "$GPGSV,4,2,14,14,66,098,44,15,47"..., 511) = 68 <0.000945>
5800  07:59:28.079982 read(6, "$GPGSV,4,3,14,19,00,149,,21,22,1"..., 511) = 68 <0.000945>
5800  07:59:28.080982 read(6, "$GPGSV,4,4,14,24,11,266,,30,41,0"..., 511) = 42 <0.000006>
5800  07:59:28.081045 read(6, "$GPGLL,4706.25300,N,01525.06752,"..., 511) = 52 <0.000913>
5800  07:59:28.082013 read(6, "$GPRMC,065929.00,A,4706.25300,N,"..., 511) = 68 <0.992971>
5800  07:59:29.079767 read(6, "$GPVTG,,T,,M,0.018,N,0.033,K,A*2"..., 511) = 470 <0.000007>
5800  07:59:29.081390 read(6, "$GPRMC,065930.00,A,4706.25301,N,"..., 511) = 68 <0.992824>
5800  07:59:30.075866 read(6, "$GPVTG,,T,,M,0.023,N,0.042,K,A*2"..., 511) = 172 <0.000007>
5800  07:59:30.075930 read(6, "$GPGSV,4,1,14,05,37,221,33,07,13"..., 511) = 68 <0.000144>
5800  07:59:30.076146 read(6, "$GPGSV,4,2,14,14,66,098,44,15,47"..., 511) = 68 <0.000006>
5800  07:59:30.076210 read(6, "$GPGSV,4,3,14,19,00,149,,21,22,1"..., 511) = 68 <0.000031>
5800  07:59:30.076296 read(6, "$GPGSV,4,4,14,24,11,266,,30,41,0"..., 511) = 42 <0.000006>
5800  07:59:30.076352 read(6, "$GPGLL,4706.25301,N,01525.06750,"..., 511) = 52 <0.000007>
5800  07:59:30.076407 read(6, "$GPRMC,065931.00,A,4706.25301,N,"..., 511) = 68 <1.000749>
5800  07:59:31.078812 read(6, "$GPVTG,,T,,M,0.024,N,0.044,K,A*2"..., 511) = 308 <0.000007>
5800  07:59:31.078873 read(6, "$GPGSV,4,3,14,19,00,149,,21,22,1"..., 511) = 68 <0.000400>
5800  07:59:31.079326 read(6, "$GPGSV,4,4,14,24,11,266,,30,41,0"..., 511) = 42 <0.000007>
5800  07:59:31.079383 read(6, "$GPGLL,4706.25301,N,01525.06750,"..., 511) = 52 <0.000005>
5800  07:59:31.079436 read(6, "$GPRMC,065932.00,A,4706.25301,N,"..., 511) = 68 <0.995909>
5800  07:59:32.077037 read(6, "$GPVTG,,T,,M,0.024,N,0.044,K,A*2"..., 511) = 470 <0.000006>
5800  07:59:32.077096 read(6, "$GPRMC,065933.00,A,4706.25301,N,"..., 511) = 68 <0.994218>
5800  07:59:33.072968 read(6, "$GPVTG,,T,,M,0.037,N,0.069,K,A*2"..., 511) = 418 <0.000007>
5800  07:59:33.073030 read(6, "$GPGLL,4706.25301,N,01525.06749,"..., 511) = 52 <0.000340>
5800  07:59:33.073421 read(6, "$GPRMC,065934.00,A,4706.25301,N,"..., 511) = 68 <1.001891>
5800  07:59:34.076953 read(6, "$GPVTG,,T,,M,0.017,N,0.031,K,A*2"..., 511) = 470 <0.000007>
5800  07:59:34.077014 read(6, "$GPRMC,065935.00,A,4706.25301,N,"..., 511) = 68 <0.995466>
5800  07:59:35.074110 read(6, "$GPVTG,,T,,M,0.014,N,0.026,K,A*2"..., 511) = 172 <0.000006>
5800  07:59:35.074167 read(6, "$GPGSV,4,1,14,05,37,221,33,07,13"..., 511) = 68 <0.001143>
5800  07:59:35.075359 read(6, "$GPGSV,4,2,14,14,66,097,44,15,48"..., 511) = 68 <0.000034>
5800  07:59:35.075442 read(6, "$GPGSV,4,3,14,19,00,149,,21,22,1"..., 511) = 68 <0.000034>
5800  07:59:35.075525 read(6, "$GPGSV,4,4,14,24,11,266,,30,41,0"..., 511) = 42 <0.000011>
5800  07:59:35.075583 read(6, "$GPGLL,4706.25301,N,01525.06747,"..., 511) = 52 <0.000005>
5800  07:59:35.075636 read(6, "$GPRMC,065936.00,A,4706.25302,N,"..., 511) = 68 <1.003739>
5800  07:59:36.081048 read(6, "$GPVTG,,T,,M,0.016,N,0.029,K,A*2"..., 511) = 308 <0.000007>
5800  07:59:36.081111 read(6, "$GPGSV,4,3,14,19,00,149,,21,22,1"..., 511) = 68 <0.000193>
5800  07:59:36.081354 read(6, "$GPGSV,4,4,14,24,11,266,03,30,41"..., 511) = 44 <0.000006>
5800  07:59:36.081407 read(6, "$GPGLL,4706.25302,N,01525.06747,"..., 511) = 52 <0.000006>
5800  07:59:36.081459 read(6, "$GPRMC,065937.00,A,4706.25305,N,"..., 511) = 68 <0.994983>
5800  07:59:37.078276 read(6, "$GPVTG,,T,,M,0.014,N,0.026,K,A*2"..., 511) = 472 <0.000007>
5800  07:59:37.078338 read(6, "$GPRMC,065938.00,A,4706.25305,N,"..., 511) = 68 <0.999142>
5800  07:59:38.079119 read(6, "$GPVTG,,T,,M,0.022,N,0.040,K,A*2"..., 511) = 472 <0.000007>
5800  07:59:38.079178 read(6, "$GPRMC,065939.00,A,4706.25304,N,"..., 511) = 68 <0.996349>
5800  07:59:39.077202 read(6, "$GPVTG,,T,,M,0.018,N,0.033,K,A*2"..., 511) = 474 <0.000006>
5800  07:59:39.077260 read(6, "$GPRMC,065940.00,A,4706.25304,N,"..., 511) = 68 <1.000397>
5800  07:59:40.079292 read(6, "$GPVTG,,T,,M,0.029,N,0.055,K,A*2"..., 511) = 422 <0.000006>
5800  07:59:40.079353 read(6, "$GPGLL,4706.25304,N,01525.06747,"..., 511) = 52 <0.000169>
5800  07:59:40.079574 read(6, "$GPRMC,065941.00,A,4706.25302,N,"..., 511) = 68 <0.993077>
5800  07:59:41.074279 read(6, "$GPVTG,,T,,M,0.012,N,0.021,K,A*2"..., 511) = 242 <0.000007>
5800  07:59:41.074338 read(6, "$GPGSV,4,2,14,14,66,097,44,15,48"..., 511) = 68 <0.000201>
5800  07:59:41.074589 read(6, "$GPGSV,4,3,14,19,01,149,,21,22,1"..., 511) = 68 <0.000029>
5800  07:59:41.074666 read(6, "$GPGSV,4,4,14,24,11,266,20,30,41"..., 511) = 44 <0.000006>
5800  07:59:41.074719 read(6, "$GPGLL,4706.25302,N,01525.06750,"..., 511) = 52 <0.000017>
5800  07:59:41.074785 read(6, "", 511)  = 0 <1.005244>
5800  07:59:42.081689 read(6, "$GPRMC,065942.00,A,4706.25299,N,"..., 511) = 68 <0.000007>
5800  07:59:42.081749 read(6, "$GPVTG,,T,,M,0.006,N,0.012,K,A*2"..., 511) = 35 <0.000006>
5800  07:59:42.081799 read(6, "$GPGGA,065942.00,4706.25299,N,01"..., 511) = 75 <0.000006>
5800  07:59:42.081864 read(6, "$GPGSA,A,3,23,13,17,15,14,24,22,"..., 511) = 64 <0.000080>
5800  07:59:42.081995 read(6, "$GPGSV,4,1,14,05,37,221,33,07,13"..., 511) = 68 <0.002667>
5800  07:59:42.084711 read(6, "$GPGSV,4,2,14,14,66,097,44,15,48"..., 511) = 68 <0.000036>
5800  07:59:42.084795 read(6, "$GPGSV,4,3,14,19,01,149,,21,22,1"..., 511) = 68 <0.000036>
5800  07:59:42.084879 read(6, "$GPGSV,4,4,14,24,12,267,21,30,41"..., 511) = 44 <0.000006>
5800  07:59:42.084934 read(6, "$GPGLL,4706.25299,N,01525.06752,"..., 511) = 52 <0.000011>
5800  07:59:42.084996 read(6, "$GPRMC,065943.00,A,4706.25297,N,"..., 511) = 68 <0.992720>
5800  07:59:43.079350 read(6, "$GPVTG,,T,,M,0.039,N,0.073,K,A*2"..., 511) = 422 <0.000007>
5800  07:59:43.079410 read(6, "$GPGLL,4706.25297,N,01525.06754,"..., 511) = 52 <0.000274>
zarfld@GPSdisciplinedRTC:~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build $
