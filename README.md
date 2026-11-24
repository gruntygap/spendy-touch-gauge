# Spendy-touch-gauge

Yet another gauge. Follow up from [this](https://github.com/gruntygap/cheapo-gauge).

# Components

| Item | Link |
| :--- | :---: |
| Can Bus Breakout for Seeed Studio XIAO | https://www.seeedstudio.com/Seeed-Studio-CAN-Bus-Breakout-Board-for-XIAO-and-QT-Py-p-5702.html?srsltid=AYCX9vL3NXZgqkbd9bWJNzHg8lQg5UtjEVwLIye2lbi1JMX22183kmvi |
| XIAO Seeed Studio ESP32-S3 Plus | https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32S3-Plus-p-6361.html?srsltid=AYCX9vLRxjwKGE0vFwK-PI5mTr27Ex2W9yBosw3iMPDqbFukuc2bD4dt |
| Waveshare 1.85" Touch Screen LCD | https://www.waveshare.com/1.85inch-touch-lcd-module.htm?srsltid=AYCX9vLjHqOh5nIDhFPLy6QjbOgcW0eNV872x1K6F92SzpCkN1hFMgaB |

TODO:

- Figure out if it's worth it to go lvgl pro?

Sensors to get on a screen:
- RPM?
- Ethanol%
- Voltage
- TPS
- MAT / CLT
- Barometer / MAP KPA (boost)
- IDLE: Ign Adv Idle, clIdleTarget (rpm?, ), warmCor (warm up enrichment %-age), iacStep (pwm?)
- AFR: (3 pieces of data: AEM can, AEMCAN -> MS2 -> back as can, AEM Can O2%)
- Advance: ignAdv, ignAdvBase? (not sure the diff), ignAdvIdle

Rational ways to group the above:
- System: voltage, barometer, MAP, ethanol
- engine: rpm, afr, tps, MAT, MAP
- Fuel: Ethanol, TPS, AFR, (PW or VE if wanted)
- Idle: RPM, Target RPM, IAC, WUE, CLT
- IGN: Actual, base, correction
