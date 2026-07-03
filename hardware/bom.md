# Bill of Materials

## Kitchen tester (on hand)

| Item | Qty used / on hand | Notes |
|---|---|---|
| DORHEA TT motor, 1:48, 3–6V | 4 / 4 | Kit's 69mm plastic wheels unused |
| Eujgoov 48mm mecanum wheels | 4 / 4 | 2 left + 2 right; split pairs across diagonals |
| BOJACK L298N dual H-bridge | 2 / 4 | Spares cover a blown board |
| URGENEX 2S LiPo 7.4V 1000mAh 35C | 1 / 2 | JST; second pack for swap during long tests |
| ESP32 WROOM-32 38-pin dev board | 2 / stash | Robot + transmitter |

## Kitchen tester (to buy / find in stash)

| Item | Qty | Notes |
|---|---|---|
| 470–1000µF ≥16V electrolytic cap | 1–2 | Bulk cap across motor rail (brownout fix) |
| 100kΩ + 33kΩ resistors, 1% | 1 ea | Battery sense divider → GPIO 34 |
| M3 screws (×30 for motors, ×8 for brackets) + nuts | ~20 | TT motor mounts |
| Dupont jumpers, 20cm | ~20 | Logic wiring |
| 18–20 AWG silicone wire | 1m | Battery → drivers |
| JST connector pigtail | 1 | Match the URGENEX pack |
| Velcro strap | 1 | Battery retention |
| Chassis: 1/4" plywood offcut ~200mm sq | 1 | Or print `chassis-plate` in PETG |

## Production build (deferred — do not order until kitchen tests pass)

| Item | Qty (per robot × 2) | Notes |
|---|---|---|
| JGB37-520 12V gearmotor w/ Hall encoder | 4 | Torque spec TBD by shelf weight measurement |
| BTS7960 43A driver | 4 | One per motor for closed-loop |
| Mecanum wheels, 97mm+ steel hub | 4 | Roller compound TBD by floor surface |
| NEMA 17 stepper + lead screw (Tr8x8) | 1 | Lift mechanism |
| Stepper driver (TMC2209 or DM542) | 1 | |
| MPU-6050 IMU | 1 | Tilt-abort |
| 4S 18650 pack + BMS | 1 | Capacity TBD from Test 4 baseline |
| ESP32 WROOM-32 | 1 | Same firmware family as tester |
| Steel square tube for shelf skid | — | H-frame per shelf |
