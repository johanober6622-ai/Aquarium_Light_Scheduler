# Aquarium

PlatformIO starter project for an ESP32 using the Arduino framework.

## Build and upload

Open this folder in VS Code with the PlatformIO extension installed, then run:

```text
pio run
pio run --target upload
pio device monitor
```

Change `board` in `platformio.ini` if your ESP32 board uses a different PlatformIO board ID.
