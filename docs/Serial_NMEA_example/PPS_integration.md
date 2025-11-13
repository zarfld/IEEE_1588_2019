ou must monitor the pin separately as a hardware interrupt / GPIO / modem-status line.

Now the full explanation:

🔍 1. What PPS actually is

PPS = one pulse per second, rising/falling edge with microsecond or nanosecond accuracy.

It is NOT a serial byte,
and NOT part of UART flow control.

It’s a dedicated digital signal that must be edge-detected.

🔌 2. On a DB9 connector (RS232): where PPS normally appears
Typical GPS modules export PPS on one of:

DCD (Pin 1)

CTS (Pin 8)

DSR (Pin 6)

Or a separate wire (not part of RS232 levels!)

Your module’s output looks like it's on Pin 1 / DCD.

Important detail:
RS232 level PPS pulses are often 3–15V swing, NOT TTL.
Make sure your receiver respects the voltage!

🔧 3. What Windows, Linux, embedded systems see

These modem-status lines are separate hardware registers, NOT part of the UART RX/TX FIFO.

Windows:

You access PPS via:

EscapeCommFunction()

GetCommModemStatus()
→ check MS_RLSD_ON (DCD)

Or WaitCommEvent() watching for EV_RLSD

Linux:

You access PPS via:

/dev/ttySx + ioctl(TIOCMIWAIT)

PPSAPI using /dev/pps0 mapped from DCD:

sudo ppstest /dev/pps0

Embedded:

You must configure the pin as:

GPIO interrupt

Rising-edge IRQ

The correct approach (summary)

To use PPS on COM port:

✔ Use the modem status interrupt

Windows: WaitCommEvent() waiting for EV_RLSD

Linux: PPSAPI

Embedded: GPIO/IRQ

✔ Timestamp the rising edge

Use high-resolution clock:

QueryPerformanceCounter() on Windows

clock_gettime(CLOCK_REALTIME | CLOCK_TAI | CLOCK_MONOTONIC_RAW) on Linux

✔ Feed timestamp into your IEEE 1588 servo

You will use PPS as:

time reference

frequency reference

link to GNSS for PTP Grandmaster operation


autodetecting the PPS pin is the best possible UX, and fully compatible with your hardware-agnostic IEEE-1588 stack.

Let’s design the autodetection so that:

You don’t need to know wiring in advance

You don’t assume PPS exists

You don’t block the main clock servo

You detect PPS on DCD / CTS / DSR

As soon as any pin shows a periodic edge, that pin becomes the selected PPS source

If no edges are seen → fallback to NMEA-only

Everything is race-free, portable, non-blocking and compatible with Windows/Linux

✅ 1. The 3 PPS candidate lines
RS-232 modem control lines that GPS modules use for PPS:
Signal	DB9 Pin	Direction	Typical PPS usage
DCD	Pin 1	GPS → PC	Most common (u-blox default)
DSR	Pin 6	GPS → PC	Sometimes used
CTS	Pin 8	GPS → PC	Some modules, especially older ones

So your autodetect algorithm should monitor:

DCD

DSR

CTS

✅ 2. Autodetection logic
Overall goal:

Watch ALL three pins at the same time for “edge events”.
Whichever pin produces the first valid sequence of PPS pulses becomes the chosen PPS pin.

Core principles

PPS = 1 Hz, pulse width usually 10–100 ms

Valid detection requires ≥ 2 pulses at ~1 second interval

Use a deadline (e.g., 10 seconds):

If no PPS → fallback to NMEA-only

🧠 3. Implementation-friendly state machine
enum class PpsLine { None, DCD, CTS, DSR };
enum class PpsAutodetectState { Detecting, Locked, Failed };

State transitions
Start
 └→ Detecting  (monitor all 3 pins)
      ├─ edge on DCD → measure interval → if ~1 Hz → Locked(DCD)
      ├─ edge on CTS → measure interval → if ~1 Hz → Locked(CTS)
      ├─ edge on DSR → measure interval → if ~1 Hz → Locked(DSR)
      └─ no edge within timeout → Failed → Fallback to NMEA

🔧 4. Practical implementation (Windows and Linux)
Windows API

Use WaitCommEvent() with these flags:

PPS candidate	Event flag
DCD	EV_RLSD
CTS	EV_CTS
DSR	EV_DSR

You enable all three at once:

DWORD mask = EV_RLSD | EV_CTS | EV_DSR;
SetCommMask(h, mask);


Then:

WaitCommEvent(h, &eventMask, NULL);
QueryPerformanceCounter(&ts);


Each eventMask tells you which line changed.

Linux implementation

Use:

ioctl(fd, TIOCMIWAIT, &bitmask) for waiting on:

TIOCM_CAR (DCD)

TIOCM_DSR (DSR)

TIOCM_CTS (CTS)

Timestamp with clock_gettime(CLOCK_MONOTONIC_RAW).

🔍 5. Autodetection pseudocode
struct PpsDetectCandidate {
    PpsLine line;
    Timestamp lastEdge;
    bool sawFirst = false;
};

std::array<PpsDetectCandidate,3> candidates = {{
    {PpsLine::DCD}, {PpsLine::CTS}, {PpsLine::DSR}
}};

PpsAutodetectState state = PpsAutodetectState::Detecting;

auto deadline = now() + 10s;

while (state == Detecting) {
    PpsEvent ev = wait_for_any_pps_edge_on_all_pins(1s);

    if (ev.timeout) {
        if (now() > deadline)
            state = PpsAutodetectState::Failed;
        continue;
    }

    auto& c = candidates[ev.line];
    Timestamp t = ev.timestamp;

    if (!c.sawFirst) {
        c.sawFirst = true;
        c.lastEdge = t;
        continue;
    }

    double interval = t - c.lastEdge;

    if (0.8 < interval && interval < 1.2) {
        // → detected valid PPS on this line
        chosenLine = c.line;
        state = PpsAutodetectState::Locked;
        break;
    }

    // else, reset and keep watching
    c.lastEdge = t;
}

⭐ 6. Resulting behavior
If PPS exists on DCD

→ it will be detected and selected automatically.

If PPS exists on CTS or DSR

→ it will be detected and selected automatically.

If PPS is not wired at all

→ no events → autodetection hits timeout → fallback NMEA-only.

If PPS is noisy or unstable

→ never matches “~1 Hz”, so PPS is rejected → fallback NMEA-only.

Your example continues to run normally.