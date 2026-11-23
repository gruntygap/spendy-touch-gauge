# spendy-touch-gauge (LVGL Pro Project)

This directory contains the declarative **LVGL Pro XML** recreation of the `spendy-touch-gauge` automotive cluster UI designed for the 360×360 round display on Seeed XIAO ESP32-S3.

---

## Architecture Overview

Instead of 1,000+ lines of procedural C++ widget creation and coordinate math, the UI is modularized into declarative XML components, clean styling rules, and reactive LVGL Subjects (`lv_subject_t`).

```text
lvgl_pro/
├── project.xml              # Project target: 360x360 round display, LVGL 9.6.0
├── globals.xml              # Color palette, constants, and live telemetry subjects
├── screens/
│   └── screen_gauges.xml    # Top-level screen hosting the 5-tile horizontal snap carousel
└── components/
    ├── gauge_boost.xml      # Tile 0: Boost scale gauge (-10.0 to +20.0 PSI)
    ├── gauge_coolant.xml    # Tile 1: 270° dynamic arc coolant gauge (100°F - 260°F)
    ├── card_fan_control.xml # Tile 2: Primary & secondary cooling fan toggles + mini coolant bar
    ├── card_table_switch.xml# Tile 3: Fuel VE & Ignition advance table 1/2 selectors
    ├── card_power_fuel.xml  # Tile 4: System voltage (lightning icon) & Ethanol % (corn icon)
    ├── icon_fan.xml         # Reusable vector cooling fan icon
    ├── icon_lightning.xml   # Reusable vector lightning bolt icon
    ├── icon_corn.xml        # Reusable vector corn cob & husk icon
    ├── page_dots.xml        # 5-dot page indicator matching active tile
    └── drawer_settings.xml  # Slide-up round settings drawer with brightness slider
```

---

## Reactive Subjects (Data Model)

All data binding in the UI connects to subjects declared in [`globals.xml`](file:///Users/grant/Documents/Arduino/spendy-touch-gauge/lvgl_pro/globals.xml):

| Subject | Type | Range | Description |
| :--- | :--- | :--- | :--- |
| `subject_boost_psi_tenths` | `int` | `-100` to `200` | Boost pressure in tenths (-10.0 to +20.0 PSI) |
| `subject_coolant_temp_tenths` | `int` | `1000` to `2600` | Coolant temp in tenths (100.0 to 260.0 °F) |
| `subject_battery_volts_tenths`| `int` | `100` to `180` | Voltage in tenths (10.0 to 18.0 V) |
| `subject_ethanol_pct` | `int` | `0` to `100` | Ethanol fuel percentage (0 to 100%) |
| `subject_fan_primary` | `int` | `0` or `1` | Primary fan override state (0=Auto, 1=ON) |
| `subject_fan_secondary` | `int` | `0` or `1` | Secondary fan override state (0=Auto, 1=ON) |
| `subject_fuel_table_2` | `int` | `0` or `1` | Active Fuel VE table (0=Table 1, 1=Table 2) |
| `subject_ign_table_2` | `int` | `0` or `1` | Active Ignition table (0=Table 1, 1=Table 2) |
| `subject_brightness` | `int` | `5` to `100` | Display brightness percentage |
| `subject_active_tile` | `int` | `0` to `4` | Currently active tileview page index |
| `subject_drawer_open` | `int` | `0` or `1` | Slide-up settings drawer state |

When telemetry updates on the microcontroller, you update the subject value:
```c
lv_subject_set_int(subject_boost_psi_tenths, current_boost_tenths);
```
All observing widgets (arcs, labels, colors) update automatically without manual setter code.
