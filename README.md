# Geometry Dash — M5Stack Cardputer

A small Geometry-Dash-style auto-runner for the [M5Stack Cardputer](https://docs.m5stack.com/en/core/Cardputer).
The cube runs forward on its own — you only control the jump. Dodge the spikes,
ride the blocks, survive as long as you can.

![240x135 TFT](https://img.shields.io/badge/display-240x135-blue)

## Gameplay

- The cube auto-runs; the world scrolls toward it and speeds up over time.
- **Spikes** (red triangles) are lethal on any touch.
- **Blocks** (blue squares) can be landed on top of — but hitting one from the
  side kills you.
- A clean hitbox-friendly margin makes near-misses feel fair.
- Your best score is saved to flash (NVS) and survives a reboot.

## Controls

| Action            | Input                                   |
|-------------------|-----------------------------------------|
| Jump              | Any key / `SPACE` (hold to keep jumping) |
| Jump (alt)        | Front button (`G0` / `BtnA`)             |
| Start / Retry     | Any key on the menu or game-over screen  |

## Build & flash

This is a [PlatformIO](https://platformio.org/) project.

```bash
# from the project root
pio run                 # compile
pio run -t upload       # compile + flash over USB-C
pio device monitor      # optional: serial log @ 115200
```

The `M5Cardputer` library (and its `M5Unified` / `M5GFX` dependencies) is pulled
in automatically via `lib_deps` in [platformio.ini](platformio.ini).

### Arduino IDE

If you prefer the Arduino IDE: install the **M5Cardputer** library from the
Library Manager, select the **M5Stack-StampS3** board, and open
[src/main.cpp](src/main.cpp) as a `.ino` sketch (rename or copy it into a folder
of the same name).

## How it works

Everything lives in [src/main.cpp](src/main.cpp), one translation unit:

- **Rendering** is double-buffered through an `M5Canvas` sprite (240×135, 16-bit)
  pushed once per frame — no flicker.
- **Physics** is a fixed-step loop paced to ~60 FPS: constant gravity, a single
  jump impulse, and the cube spinning in the air then snapping to a clean face
  on landing.
- **Obstacles** are spawned procedurally as small "patterns" (spike rows, low or
  tall blocks, ride-across platforms, block-then-spike combos) with gaps tuned
  so every pattern is clearable with one jump.
- **Collision** uses slightly inset axis-aligned boxes; blocks resolve as
  landings when approached from above, and as death when hit from the side.

## Tuning

The constants at the top of [src/main.cpp](src/main.cpp) are the knobs:
`GRAVITY`, `JUMP_VEL`, `SPEED_MIN` / `SPEED_MAX`, `ROT_SPEED`, and the obstacle
sizes. Adjust to taste.
