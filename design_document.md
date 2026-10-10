---
fontsize: 12pt
geometry: margin=0.55in
mainfont: "Tinos"
mathfont: "TeX Gyre Termes Math"
header-includes:
  - \usepackage{titlesec}
  - \linespread{0.94}
  - \titleformat{\section}{\normalfont\fontsize{14}{17}\selectfont\bfseries}{}{0em}{}
  - \titleformat{\subsection}{\normalfont\fontsize{14}{17}\selectfont\bfseries}{}{0em}{}
  - \titlespacing*{\section}{0pt}{8pt}{4pt}
  - \titlespacing*{\subsection}{0pt}{6pt}{3pt}
  - \AtBeginDocument{\setlength{\abovedisplayskip}{3pt}\setlength{\belowdisplayskip}{3pt}\renewcommand{\arraystretch}{0.92}}
---

# Design Report on FSMB RECRUITMENT ASSIGNMENT - Nazib Abrar

An Arduino Nano (ATmega328P) based voltage protector. It measures AC mains voltage and load current and drives a relay output on D2, dropping the appliance when the voltage leaves its band or the current crosses its limit. Settings can be read or changed over UART at runtime; on-device (LCD and buttons) configuration is not part of this phase.

## Measurement and trip logic

The voltage sense sits on A0, scaled and biased to mid-rail; current comes from an ACS712-05B on A1 (185 mV/A, 2.5 V at zero current). One ADC count is 4.887 mV and both signals swing around 512 counts.

Voltage is sampled as true RMS: a slow low-pass filter tracks the DC bias, $\text{offset} \leftarrow \text{offset} + (v_{\text{raw}} - \text{offset})/1024$, and each 200 ms window (10 cycles at 50 Hz) accumulates the squared, de-biased samples:

$$V_{\text{rms}} = V_{\text{CAL}} \cdot \frac{V_{\text{ref}}/1000}{1024} \cdot \sqrt{\frac{\sum_{n=1}^{N} v_{\text{filt},n}^2}{N}}, \qquad V_{\text{CAL}} = 187$$

Current uses the window peak: the largest raw count becomes sensor millivolts, the zero-current offset is removed, and the result is divided by the sensitivity and by $\sqrt{2}$:

$$I_{\text{peak}} = \frac{\dfrac{\text{peak}}{1024} \cdot 5000 - 2542}{185}, \qquad I_{\text{rms}} = \frac{I_{\text{peak}}}{\sqrt{2}}$$

Every channel has two runtime settings, a base and a tolerance percentage. The band and the fast-gate threshold in ADC counts follow from them:

$$V_{\text{upper}} = V_{\text{base}}\left(1 + \tfrac{t}{100}\right), \quad V_{\text{lower}} = V_{\text{base}}\left(1 - \tfrac{t}{100}\right), \quad I_{\text{upper}} = I_{\text{base}}\left(1 + \tfrac{t}{100}\right)$$

$$c_{\text{thr}} = 512 + \frac{V_{\text{upper}}\,\sqrt{2}}{V_{\text{CAL}} \cdot \left(V_{\text{ref}}/1000/1024\right)}, \qquad c_{\text{thr}} = \frac{2542}{4.887} + \frac{I_{\text{upper}}\,\sqrt{2} \cdot 185}{4.887}$$

The fast gate runs every loop iteration and compares each instantaneous sample against the threshold counts; ten consecutive over-threshold samples, about 4.4 ms, trip the device. It watches only the upper edge, since the sine dips below mid-rail every negative half-cycle. The slow gate runs once per window, trips when $V < V_{\text{lower}}$, $V > V_{\text{upper}}$ or $I > I_{\text{upper}}$, and arms after the first window so boot readings cannot false-trip the relay. Current has no lower bound; an unplugged load is not a fault. Both gates set one latched boolean that alone drives D2; it releases after 2 s of continuously normal readings. Thresholds are re-derived as integers on every settings change, so the hot loop does integer compares only.

## UART protocol

| Command | Effect | Reply |
|---|---|---|
| `SVB <volts>` / `SVT <pct>` | set voltage base / tolerance | `OK` with the applied value |
| `SCB <amps>` / `SCT <pct>` | set current base / tolerance | `OK` with the applied value |
| `GVB` / `GVT` | read voltage base / tolerance | value |
| `GCB` / `GCT` | read current base / tolerance | value |

Commands are ASCII lines ending in `\n` at 9600 8N1; values outside $0 < \text{base} \le 500$ V (or 100 A) and $0 < \text{tolerance} \le 50$ % get `ERR BAD_VALUE`, unknown ones `ERR UNKNOWN_CMD`. Parsing is non-blocking and never delays protection.
