# ESP32 Timelapse Box

[简体中文](README.zh-CN.md)

Snapmaker Orca can trigger an external ESP32 camera through the Klipper macro
`ESP_TIMELAPSE_SHOT`. Install that macro before enabling timelapse on a printer
profile that declares ESP32 support.

Minimal Klipper macro contract:

```ini
[gcode_macro ESP_TIMELAPSE_SHOT]
variable_seq: 0
gcode:
    SET_GCODE_VARIABLE MACRO=ESP_TIMELAPSE_SHOT VARIABLE=seq VALUE={printer["gcode_macro ESP_TIMELAPSE_SHOT"].seq + 1}
```

The ESP32 box polls `gcode_macro ESP_TIMELAPSE_SHOT.seq`; Snapmaker Orca does
not connect to the camera, ESP32 serial port, or Moonraker API directly.

The printer preset owns these six settings:

- `supports_esp32_timelapse`
- `esp32_timelapse_gcode`
- `esp32_timelapse_park_x`
- `esp32_timelapse_park_y`
- `esp32_timelapse_travel_speed`
- `esp32_timelapse_dwell_ms`

The dwell defaults to 2000 ms and must not be lower. Larger values remain valid.

- **Off** emits neither native nor ESP32 timelapse commands.
- **Traditional** keeps native capture and adds the configured ESP32 command after
  each completed layer using `M400`, the configured command, and the configured
  dwell. It does not force a prime tower.
- **Smooth** keeps native capture, uses one shared Orca prime tower, and safely
  lifts and parks before the ESP32 command. It adds material and print time.

Standard Snapmaker U1 profiles enable this capability and retain
`TIMELAPSE_TAKE_FRAME`, so native and ESP32 capture can run together. Generic
Klipper profiles may enable it by explicitly setting the six fields above. Profiles
without the macro or box must keep `supports_esp32_timelapse=false`.

This work remains licensed under the repository's AGPL-3.0 license.

## Use And Verification

1. Install the macro and restart Klipper.
2. Confirm `ESP_TIMELAPSE_SHOT` increments `seq` from the printer console.
3. Select **Off**, **Traditional**, or **Smooth** under Process > Other >
   Special mode > Timelapse.
4. Export G-code before the first print. Off must contain no
   `ESP_TIMELAPSE_SHOT`; enabled modes should contain one command per printed
   layer. Smooth must also show one shared prime/stabilization tower.
5. Start with a 10-20 layer single-material model and watch the first capture
   cycle before attempting a long or multi-material print.

The mixed-nozzle U1 firmware patch is unrelated and is not required for ESP32
timelapse. To disable the box, select Off or use a printer profile with
`supports_esp32_timelapse=false`. To restore the official slicer, reinstall a
release from `Snapmaker/OrcaSlicer`; printer and ESP32 firmware are not changed
by installing or removing the slicer.

This is an experimental community integration, not an official Snapmaker,
Klipper, Sony, or Bambu Lab feature.
