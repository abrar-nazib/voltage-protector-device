# AC Voltage Protector Device (FSMBD Recruitment Phase 1)

An Arduino Nano (ATmega328P) based voltage protector: it continuously measures AC mains
voltage and load current, disconnects the protected appliance (output on `D2`) on a
voltage surge/sag or over-current fault, and exposes its configuration over UART so a
computer can read and change the trip settings at runtime.

> Submission scope note: configuration is implemented **via computer (UART) only**.
> On-device configuration (LCD + buttons) is not implemented in this phase.

## Repository layout

```
├── src/                firmware source (PlatformIO project)
├── include/            firmware headers
├── schematics/         schematic / block diagram of the product
├── test_evidence/      serial logs, screenshots, timing captures
├── project_files/      Proteus simulation project
├── design_document.pdf one-page design report (see note below)
└── readme.md           this file
```

## Hardware / signal chain

| Signal | Device pin | Sensor |
|---|---|---|
| AC voltage sense | `A0` | scaled/biased mains sample (mid-rail centered, `VOLTAGE_CAL = 187`) |
| Load current | `A1` | ACS712-05B Hall sensor (mid-rail at 2.5 V, `185 mV/A`) |
| Appliance disconnect (relay driver) | `D2` | HIGH = connected, LOW = disconnected |
| UART config/status | `D0`/`D1` | 9600 8N1 |

## Measurement math

Both ADC channels are read with the 10-bit ADC (0–1023 counts, 5 V reference,
≈4.887 mV/count). The AC signals are biased to mid-rail so the waveform swings
symmetrically around ~512 counts.

**Voltage (true RMS sampling):** a digital low-pass filter tracks the DC bias,
`offset += (raw − offset)/1024`, each sample is de-biased, squared and accumulated
over a 200 ms window (10 mains cycles at 50 Hz):

```
Vrms = VCAL × (Vref/1000 / 1024) × sqrt( Σ(filtered²) / N )      VCAL = 187
```

**Current (peak-hold):** the loop tracks the largest raw count in each 200 ms window,
converts it to sensor millivolts, subtracts the zero-current offset and divides by the
sensitivity, then converts peak → RMS:

```
Ipeak = ( (peakCounts/1024 × 5000 mV) − 2542 mV ) / 185 mV/A
Irms  = Ipeak / √2
```

## Surge detection — dual gate

The trip thresholds are derived from two runtime settings per channel: **base** (nominal
RMS) and **tolerance %**.

```
V_upper = V_base × (1 + tol/100)        V_lower = V_base × (1 − tol/100)
I_upper = I_base × (1 + tol/100)        (over-current only — no under-current fault)
```

1. **Fast gate (peak, every ~440 µs loop):** each instantaneous sample is compared
   against the ADC-count equivalent of the upper RMS limit (limit × √2 → counts).
   10 consecutive over-threshold samples (~4.4 ms) trip the device. The peak gate
   deliberately watches only the upper edge: a sine wave dips below mid-rail every
   negative half-cycle, so an instantaneous "too low" test is meaningless — the lower
   edge is owned by the RMS gate.
2. **Slow gate (RMS, once per 200 ms window):** trips when `V < V_lower`, `V > V_upper`
   or `I > I_upper`. It is armed only after the first complete window so boot-time
   readings cannot false-trip.
3. **Latch and recovery:** both gates set one boolean state (`tripped`) that alone drives
   `D2`. The state holds for 2 s of continuously normal readings before reconnecting, so
   a flickering fault cannot chatter the relay.

All thresholds are integers (ADC counts) pre-derived from the double-precision settings
whenever the settings change, so the hot loop only does integer compares.

## UART protocol (9600 8N1, lines terminated by `\n`)

| Command | Meaning | Reply |
|---|---|---|
| `SVB <volts>` | set voltage base | `OK SVB <volts>` |
| `SVT <percent>` | set voltage tolerance | `OK SVT <percent>` |
| `SCB <amps>` | set current base | `OK SCB <amps>` |
| `SCT <percent>` | set current tolerance | `OK SCT <percent>` |
| `GVB` / `GVT` | get voltage base / tolerance | `GVB <v>` / `GVT <p>` |
| `GCB` / `GCT` | get current base / tolerance | `GCB <a>` / `GCT <p>` |

Validation: `0 < base ≤ 500 V / 100 A`, `0 < tolerance ≤ 50 %`. Invalid input replies
`ERR BAD_VALUE`; unknown commands reply `ERR UNKNOWN_CMD`. Parsing is non-blocking —
characters are buffered in the main loop and the protection logic never stops running.

Example session:

```
> GVB
GVB 230.0
> SVB 220
OK SVB 220.0
> SVT 3
OK SVT 3.0
> GVB
GVB 220.0
```

Every 200 ms the device also prints a status line, e.g.
`V=229.8 I=0.42 D2=HIGH vOver=0 iOver=0` (readings, relay state and the live
over-threshold counters for both fast gates).

## Building and running

- **Hardware:** `pio run -t upload`, then open a 9600-baud serial monitor
  (`pio device monitor`).
- **Proteus simulation:** attach `.pio/build/nanoatmega328/firmware.hex` to the simulated
  Arduino Nano; wire the Virtual Terminal RXD → Arduino TXD (pin 1) and TXD → RXD (pin 0)
  at 9600 baud. The simulation project is in `project_files/`.

## Known limitations

- No on-device (LCD/button) configuration path — settings changes require the UART link.
- The fast gate checks instantaneous peak values, so a burst of spikes above the peak
  threshold trips even if true RMS stays inside the band (10-sample debounce mitigates
  this); the true-RMS verdict arrives once per 200 ms window.
- The ACS712 zero-current offset (2542 mV) and voltage calibration (VCAL = 187) are
  compile-time constants, not runtime-calibrated.
- Power/energy integration is not implemented in this phase.
