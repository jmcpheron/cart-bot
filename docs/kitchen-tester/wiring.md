# Kitchen Tester — Wiring

## Power tree

```
7.4V 2S LiPo (+) ──┬──────────────► L298N Board A  12V/VIN terminal
                   ├──────────────► L298N Board B  12V/VIN terminal
                   ├──[470–1000µF electrolytic to GND, near the drivers]
                   └──[100kΩ]──┬──[33kΩ]──► GND        (battery sense divider)
                               └──────────► ESP32 GPIO 34

L298N Board A 5V out ─────────────► ESP32 VIN (5V pin)
LiPo (−) / both L298N GND / ESP32 GND ── all common ground (star at Board A)
```

- **Keep the 5V-regulator jumper ON** on Board A (it powers the ESP32). Board B's
  jumper can stay on too; just don't wire Board B's 5V out to anything.
- **Never power the ESP32 from USB and the L298N 5V at the same time** unless your
  dev board has a protection diode on VIN (most 38-pin WROOM boards do — verify
  yours before the first combined USB + battery session, or just unplug the LiPo
  while flashing).
- The bulk capacitor matters: four stalled TT motors sag the rail hard enough to
  brown out the ESP32 mid-drive. Observed symptom is the robot "rebooting" when it
  hits a wall.

## Battery sense divider

8.4V full pack → 8.4 × 33/133 ≈ 2.08V at GPIO 34 — inside the ADC's usable range
with 11dB attenuation. Firmware multiplies the millivolt reading by 133/33.
1% resistors preferred; calibration constant lives in `src/robot/battery.h`.

## Pin map (revised — avoids all strap pins 0, 2, 5, 12, 15)

> The original draft used GPIO 12 (MTDI strap). If it reads high at boot the chip
> selects 1.8V flash voltage and won't boot. All pins below are safe at boot and
> LEDC/PWM-capable.

| Motor | L298N board / channel | IN1 | IN2 | EN (PWM) |
|---|---|---|---|---|
| FL (front-left)  | A / OUT1-2 | 25 | 26 | 27 |
| FR (front-right) | A / OUT3-4 | 16 | 17 | 4  |
| RL (rear-left)   | B / OUT1-2 | 33 | 32 | 23 |
| RR (rear-right)  | B / OUT3-4 | 19 | 18 | 21 |

| Other | GPIO |
|---|---|
| Battery sense (ADC1_CH6) | 34 (input-only) |
| Free for future use | 13, 14, 22, 35 (in-only), 36 (in-only), 39 (in-only) |

Remove the plastic jumpers on the L298N ENA/ENB pins — those tie enable high; we
drive them with PWM instead.

## Motor polarity

Wire every motor the same way (red to OUT-odd, black to OUT-even), then fix any
backwards wheel in `firmware/src/robot/pins.h` by swapping that motor's IN1/IN2
numbers — no re-soldering. Test 1 in the test plan verifies each wheel's direction
individually before assembly.
