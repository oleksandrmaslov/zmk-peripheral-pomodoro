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
