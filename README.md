# Aquarium Light Controller

ESP32 aquarium light controller built with PlatformIO and the Arduino framework.
The controller drives a PWM light, follows an eight-slot daily schedule, and
provides a browser-based control panel over Wi-Fi.

## Features

- Eight configurable schedule slots
- Ramp-up, ramp-down, and fixed-intensity slot modes
- Manual `On`, `Off`, and `Auto` override modes
- Five-second ramp for manual on/off changes
- Schedule and override settings stored in ESP32 non-volatile preferences
- Automatic time synchronization through NTP
- Responsive web interface for status, manual control, and schedule editing
- Serial commands for basic manual control

## Hardware

- ESP32 development board
- PWM-compatible aquarium light or LED driver
- Light control signal connected to GPIO 21

The PWM output uses channel 0, 1 kHz frequency, and 8-bit resolution. The
output is non-inverted: higher intensity produces a higher PWM duty value. Use
an appropriate transistor, MOSFET, or LED driver between the ESP32 and the
light; do not connect a high-power light directly to an ESP32 GPIO.

## Configuration

1. Open `src/secrets.h`.
2. Set `ssid` and `password` to the Wi-Fi network used by the controller.
3. Keep `src/secrets.h` private and do not commit real credentials to a public
	repository.

The main firmware settings are near the top of `src/main.cpp`, including the
PWM pin, schedule slot count, ramp step, timezone, and NTP interval. The
default timezone is `SAST-2` (UTC+2).

## Build and upload

Install VS Code with the PlatformIO extension, open this project folder, and
connect the ESP32 over USB. From the PlatformIO terminal, run:

```text
pio run
pio run --target upload
pio device monitor
```

The serial monitor uses `115200` baud. The firmware prints the assigned IP
address after a successful Wi-Fi connection.

## Web interface

Open the printed address in a browser:

```text
http://<esp32-ip-address>/
```

The main page shows the current intensity, time, active slot, and override
mode. Edit the schedule directly in the table on the main page. A slot with
both start and end time set to `00:00` is disabled.

The schedule page supports times that cross midnight, such as `20:00` to
`02:00`. Changes are saved in the ESP32 preferences and survive restarts.

## Serial commands

Enter one of these characters in the serial monitor:

| Command | Action |
| --- | --- |
| `1` | Manual On, ramp to 100% |
| `0` | Manual Off, ramp to 0% |
| `a` or `A` | Return to automatic schedule mode |

## Project structure

```text
platformio.ini  PlatformIO build and board configuration
src/main.cpp   Firmware, web pages, schedule logic, and web server
src/secrets.h  Local Wi-Fi credentials
```

## Board configuration

The project currently targets the `esp32dev` board with the Arduino
framework. To use another supported ESP32 board, change the `board` value in
`platformio.ini` to the appropriate PlatformIO board ID.
