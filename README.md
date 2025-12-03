# ZMK Pomodoro Module

Peripheral-only Pomodoro timer for ZMK with nice!view (ls01xx) UI, 4× (25 min work + 5 min break),
and behaviors for start/pause/stop/smart/resume/extend/skip. Central builds only carry the
behaviors; timers and UI are no-ops.

## Features

- States: IDLE → WORK → BREAK → repeat, PAUSED anywhere; after the 4th break → IDLE.
- Timers run on the peripheral via k_work_delayable minute ticks plus 1s UI ticks.
- nice!view UI: countdown, progress bar, session indicator, and status text (Idle/Work/Break/Paused).
- Smart button: Play/Resume/Pause/Skip logic, resume-on-any-key option, extend break +1:00 (capped).

## Devicetree behaviors

Include `dts/overlay/pomodoro.dtsi` and bind the provided behaviors in your keymap:

```
#include "pomodoro.dtsi"

&keymap {
    compatible = "zmk,keymap";
    default_layer {
        bindings = <
            &pomo_smart  &pomo_pause  &pomo_stop
            &pomo_start  &pomo_resume &pomo_break_skip
            &pomo_break_extend
        >;
    };
};
```

<<<<<<< HEAD
Available nodes: `&pomo_start`, `&pomo_pause`, `&pomo_stop`, `&pomo_smart`, `&pomo_resume`,
`&pomo_break_extend`, `&pomo_break_skip`.

## Configuration knobs

- `CONFIG_ZMK_POMODORO` (default y): enable the module logic.
- `CONFIG_ZMK_POMODORO_DISPLAY` (default y, peripheral only): enable the nice!view UI.
- `CONFIG_ZMK_POMODORO_RESUME_ON_ANY_KEY`: resume/skip when any key is pressed during break or
  paused states.
- `CONFIG_ZMK_POMODORO_BREAK_EXTEND_LIMIT_MINUTES` (default 10): cap the break after extend presses.

UI hints:
- Idle shows “Press Start/Any key”, session 0/4, empty progress.
- Work/Break: MM:SS countdown, “Sess X/4”, progress per phase, 1 Hz refresh.
- Paused: “Paused” with frozen time/progress and Resume/Play hint.
=======
## Configuration Options

**Visualisation of these settings here: https://pointing.streamlit.app/**

The acceleration processor provides several settings to customize how your pointing device behaves. Here's a detailed explanation of each option:

### Basic Settings

- `min-factor`: (Default: 1000)
  - Controls how slow movements are handled
  - Values below 1000 will make slow movements even slower for precision
  - Values are in thousandths (e.g., 800 = 0.8x speed)
  - Example: `min-factor = <800>` makes slow movements 20% slower

- `max-factor`: (Default: 3500)
  - Controls maximum acceleration at high speeds
  - Values are in thousandths (e.g., 3500 = 3.5x speed)
  - Example: `max-factor = <3000>` means fast movements are up to 3x faster

### Speed Settings

- `speed-threshold`: (Default: 1000)
  - Speed at which acceleration starts
  - Measured in counts per second
  - Below this speed, min-factor is applied
  - Above this speed, acceleration begins
  - Example: `speed-threshold = <1200>` means acceleration starts at moderate speeds

- `speed-max`: (Default: 6000)
  - Speed at which maximum acceleration is reached
  - Measured in counts per second
  - At this speed and above, max-factor is applied
  - Example: `speed-max = <6000>` means you reach max acceleration at high speeds

### Acceleration Behavior

- `acceleration-exponent`: (Default: 1)
  - Controls how quickly acceleration increases
  - 1 = Linear (smooth, gradual acceleration)
  - 2 = Quadratic (acceleration increases more rapidly)
  - 3 = Cubic (very rapid acceleration increase)
  - Example: `acceleration-exponent = <2>` for a more aggressive acceleration curve

### Advanced Options

- `track-remainders`: (Default: disabled)
  - Enables tracking of fractional movements
  - Improves precision by accumulating small movements
  - Enable with `track-remainders;` in your config


### Visual Examples

Here's how different configurations affect pointer movement:

```
Slow Speed │  Medium Speed  │  High Speed
───────────┼────────────────┼────────────
0.8x      →│      1.0x     →│     3.0x     (Balanced)
0.9x      →│      1.0x     →│     2.0x     (Light)
0.7x      →│      1.0x     →│     4.0x     (Heavy)
0.5x      →│      1.0x     →│     1.5x     (Precision)
```



## Share Your Settings
### App for easy configuration visualisation: https://pointing.streamlit.app/
The configurations under are just starting points - every person's perfect pointer settings are as unique as they are) I'd love to see what works best for you.

### Why Share?
- Help others find their ideal setup
- Contribute to the community knowledge
- Get feedback and suggestions
- Inspire new configuration ideas

### How to Share
- Drop your config in a GitHub issue
- Share on Discord ZMK or my DM (with a quick note about your use case)
- Comment on what worked/didn't work for you

>  **Remember**: These examples were primarily tested with Cirque trackpads. If you're using other pointing devices (like trackballs or trackpoints), your mileage may vary - and that's why sharing your experience is so valuable
 

### General Use:
```devicetree
&pointer_accel {
    input-type = <INPUT_EV_REL>;
    min-factor = <800>;        // Slight slowdown for precision
    max-factor = <3000>;       // Good acceleration for large movements
    speed-threshold = <1200>;  // Balanced acceleration point
    speed-max = <6000>;
    acceleration-exponent = <2>; // Smooth quadratic curve
    track-remainders;         // Track fractional movements
};
```
### Light Acceleration
```devicetree
&pointer_accel {
    input-type = <INPUT_EV_REL>;
    min-factor = <900>;        // 0.9x minimum
    max-factor = <2000>;       // 2.0x maximum
    speed-threshold = <1500>;  // Start accelerating later
    speed-max = <5000>;         // 6000 counts/sec for max accel
    acceleration-exponent = <1>; // Linear acceleration
    track-remainders;          // Track fractional movements
};
```

### Heavy Acceleration
```devicetree
&pointer_accel {
    input-type = <INPUT_EV_REL>;
    min-factor = <700>;        // 0.7x minimum
    max-factor = <4000>;       // 4.0x maximum
    speed-threshold = <1000>;  // Start accelerating earlier
    speed-max = <6000>;          // 6000 counts/sec for max accel
    acceleration-exponent = <3>; // Cubic acceleration curve
    track-remainders;          // Track fractional movements
};
```

### Precision Mode
```devicetree
&pointer_accel {
    input-type = <INPUT_EV_REL>;
    min-factor = <500>;        // 0.5x for fine control
    max-factor = <1500>;       // 1.5x maximum
    speed-threshold = <2000>;  // High threshold for stability
    speed-max = <7000>;          // 6000 counts/sec for max accel
    acceleration-exponent = <1>; // Linear response
    track-remainders;          // Track fractional movements
};
```
>>>>>>> 0da8f97f8f2abb18fcc9d488b25357511922cf32
