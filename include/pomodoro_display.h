#pragma once

#include <zephyr/sys/util.h>

#include "pomodoro.h"

#if defined(CONFIG_ZMK_POMODORO_DISPLAY)
void pomodoro_display_update(const struct pomodoro_status *status, bool force);
void pomodoro_display_bootstrap(const struct pomodoro_status *status);
#else
static inline void pomodoro_display_update(const struct pomodoro_status *status, bool force) {
    ARG_UNUSED(status);
    ARG_UNUSED(force);
}

static inline void pomodoro_display_bootstrap(const struct pomodoro_status *status) {
    ARG_UNUSED(status);
}
#endif
