# LKSS Hackathon (Day 5) - LVGL samples, skeletons and solutions

The userspace side of the hackathon, built on:

- `/dev/fb0` - the `st7789fb.ko` framebuffer driver (Lab 5)
- `/dev/hackpad` - the `hackpad.ko` buttons/LEDs driver (Lab 5)
- BMP280 sysfs attributes - the Lab 4 `bmp280.ko` driver
- [LVGL](https://lvgl.io) v9.3 for the graphics

See `doc/2026/day5.rst` in the lkss-main repository for the full
hackathon guide.

## Layout

Each app lives in `<name>/<name>.c` and builds to `build/<name>`, so the
binary keeps a suggestive name when copied to the board.

Warm-up samples (start here, in order). Each demonstrates one building
block used by the projects:

| Directory   | Sample                                            | Needs      |
|-------------|---------------------------------------------------|------------|
| `hello/`    | text: labels, fonts, colors, timer updates        | display    |
| `colors/`   | paints the screen with successive colors          | display    |
| `flag/`     | simple rectangles: 6 full-height color stripes    | display    |
| `dot/`      | move a dot on a grid, one cell per press (snake)  | + buttons  |
| `paddle/`   | hold SW1/SW2 to slide a paddle (pong, breakout)   | + buttons  |
| `bounce/`   | ball bouncing off the walls (game physics)        | display    |
| `jump/`     | 4 balls; each button makes its ball jump          | + buttons  |
| `blink/`    | press = screen color change + LED blink           | + buttons  |
| `leds/`     | LED control panel: buttons toggle the three LEDs  | + buttons  |
| `bar/`      | progress bar + LED level meter (sysmon gauges)    | + buttons  |
| `menu/`     | 3 screens navigated with SW1/SW2 (watch, weather) | + buttons  |
| `chart/`    | live scrolling plot fed by a timer (weather)      | display    |

Project skeletons - the UI is built, the logic is `TODO`s for you to
fill in. Each file starts with an API quick reference for exactly the
HAL and LVGL calls you will need:

| Directory   | Project                                          |
|-------------|--------------------------------------------------|
| `common/`   | shared HAL: display, buttons, LEDs, sensor       |
| `pong/`     | retro Pong vs. a software-controlled paddle      |
| `snake/`    | classic Snake with LED milestones                |
| `breakout/` | brick breaker, lives shown on the LEDs           |
| `tetris/`   | falling blocks, next-piece preview, LED lines    |
| `watch/`    | wristwatch, 3 faces, BMP280 temp/pressure        |
| `weather/`  | weather station with menus and history charts    |
| `sysmon/`   | retro terminal system monitor (procfs)           |

## Build (cross-compile on the host)

```bash
cd repos/lkss-linux/drivers/lkss/lab5/demos
git clone --depth 1 --branch v9.3.0 https://github.com/lvgl/lvgl.git
make -j$(nproc)
```

The binaries land in `build/` (one per demo). To build a single demo:
`make pong`. For a native build: `make CROSS_COMPILE=`.

## Run (on the board)

```bash
# once: load the framework drivers
modprobe st7789fb
modprobe hackpad
modprobe bmp280        # used by watch + weather (watch falls back to
                       # random values without it)

./colors               # first display test (works without hackpad)
./jump                 # first button test
./pong                 # or snake, breakout, tetris, watch, weather,
                       # sysmon
```

Copy the binaries to the rootfs with `scripts/lkss.py` or `scp`.
