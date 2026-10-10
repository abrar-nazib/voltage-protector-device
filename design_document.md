---
fontsize: 12pt
geometry: margin=0.45in
mainfont: "Tinos"
mathfont: "TeX Gyre Termes Math"
header-includes:
  - \usepackage{titlesec}
  - \linespread{0.90}
  - \titleformat{\section}{\normalfont\fontsize{14}{17}\selectfont\bfseries}{}{0em}{}
  - \titleformat{\subsection}{\normalfont\fontsize{14}{17}\selectfont\bfseries}{}{0em}{}
  - \titlespacing*{\section}{0pt}{6pt}{3pt}
  - \titlespacing*{\subsection}{0pt}{5pt}{2pt}
  - \AtBeginDocument{\setlength{\abovedisplayskip}{2pt}\setlength{\belowdisplayskip}{2pt}\renewcommand{\arraystretch}{0.80}}
---

# Design Report on FSMB RECRUITMENT ASSIGNMENT - Nazib Abrar

This project is a mains voltage protector built around an Arduino Nano (ATmega328P). It measures AC voltage and load current and keeps a relay output on D2 high while both readings stay inside their bands; outside a band, D2 drops and the appliance disconnects. I did not implement the on-device configuration path (LCD and buttons) in this phase; settings change over UART while the device runs.

## Measurement and trip logic

The voltage signal arrives on A0, scaled down and biased to mid-rail; current comes from an ACS712-05B on A1 (185 mV per amp, 2.5 V with no load). One ADC count is 4.887 mV, and both AC signals swing around 512 counts.

I measure voltage as true RMS: a slow digital low-pass filter tracks the DC bias, $\text{offset} \leftarrow \text{offset} + (v_{\text{raw}} - \text{offset})/1024$, and every 200 ms window (ten cycles at 50 Hz) collects the squared, de-biased samples, which turn into volts through the calibration constant:

$$V_{\text{rms}} = V_{\text{CAL}} \cdot \frac{V_{\text{ref}}/1000}{1024} \cdot \sqrt{\frac{\sum_{n=1}^{N} v_{\text{filt},n}^2}{N}}, \qquad V_{\text{CAL}} = 187$$

Current comes from the window peak: the largest raw count seen becomes sensor millivolts, the 2542 mV zero-current offset comes off, and the rest is divided by the sensitivity and by $\sqrt{2}$:

$$I_{\text{peak}} = \frac{\tfrac{\text{peak}}{1024} \cdot 5000 - 2542}{185}, \qquad I_{\text{rms}} = \frac{I_{\text{peak}}}{\sqrt{2}}$$

The peak shortcut is cheap on an AVR but trusts the real peak to appear among the window's ~450 samples, and the offset stays a fixed constant.

Each channel has a base and a tolerance setting; the trip band and fast-gate threshold in ADC counts follow:

$$V_{\text{upper}} = V_{\text{base}}\left(1 + \tfrac{t}{100}\right), \quad V_{\text{lower}} = V_{\text{base}}\left(1 - \tfrac{t}{100}\right), \quad I_{\text{upper}} = I_{\text{base}}\left(1 + \tfrac{t}{100}\right)$$

$$c_{\text{thr}} = 512 + \frac{V_{\text{upper}}\,\sqrt{2}}{V_{\text{CAL}} \cdot \left(V_{\text{ref}}/1000/1024\right)}, \qquad c_{\text{thr}} = \frac{2542}{4.887} + \frac{I_{\text{upper}}\,\sqrt{2} \cdot 185}{4.887}$$

The fast gate runs on every loop iteration: it compares each instantaneous sample against the threshold counts and counts consecutive hits; ten in a row, about 4.4 ms, trip the device. This gate watches only the upper edge, since the sine dips below mid-rail every negative half-cycle. The slow gate runs once per window and trips when $V < V_{\text{lower}}$, $V > V_{\text{upper}}$ or $I > I_{\text{upper}}$; it stays unarmed through the first window so the unsettled boot readings cannot trip the relay. I gave current no lower bound on purpose: an unplugged load is not a fault.

Both gates write the same latched boolean that alone drives D2; it comes back high only after 2 s of continuously normal readings. Settings changes re-derive all thresholds as integers, so the hot loop never touches floating point.

## UART protocol

Configuration happens over UART at 9600 8N1, one ASCII command per line ending in `\n`.

| Command | Effect | Reply |
|---|---|---|
| `SVB <volts>` / `SVT <pct>` | set voltage base / tolerance | `OK` with the applied value |
| `SCB <amps>` / `SCT <pct>` | set current base / tolerance | `OK` with the applied value |
| `GVB` / `GVT` | read voltage base / tolerance | value |
| `GCB` / `GCT` | read current base / tolerance | value |

Values outside $0 < \text{base} \le 500$ V (or 100 A) and $0 < \text{tolerance} \le 50$ % get `ERR BAD_VALUE` back, and unknown commands get `ERR UNKNOWN_CMD`. The parser is non-blocking, so typing never slows the protection logic.
