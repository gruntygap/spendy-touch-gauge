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
