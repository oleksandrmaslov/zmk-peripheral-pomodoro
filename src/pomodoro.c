#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/display.h>

#include "pomodoro.h"
#include "pomodoro_display.h"

LOG_MODULE_REGISTER(pomodoro, CONFIG_ZMK_LOG_LEVEL);

#define POMODORO_WORK_SECONDS POMODORO_DEFAULT_WORK_SECONDS
#define POMODORO_BREAK_SECONDS POMODORO_DEFAULT_BREAK_SECONDS
#define POMODORO_MINUTE_CHUNK 60

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

int pomodoro_start(void) { return 0; }
int pomodoro_pause(void) { return 0; }
int pomodoro_stop(void) { return 0; }
int pomodoro_smart(void) { return 0; }
int pomodoro_resume(void) { return 0; }
int pomodoro_break_extend(void) { return 0; }
int pomodoro_break_skip(void) { return 0; }

struct pomodoro_status pomodoro_current_status(void) {
    return (struct pomodoro_status){
        .state = POMODORO_STATE_IDLE,
        .session = 0,
        .max_sessions = POMODORO_MAX_SESSIONS,
        .on_break = false,
        .paused = false,
        .remaining_seconds = 0,
        .phase_total_seconds = POMODORO_WORK_SECONDS,
        .resume_on_any_key = IS_ENABLED(CONFIG_ZMK_POMODORO_RESUME_ON_ANY_KEY),
    };
}

#else

enum pomodoro_phase {
    POMODORO_PHASE_NONE = 0,
    POMODORO_PHASE_WORK,
    POMODORO_PHASE_BREAK,
};

struct pomodoro_context {
    struct k_mutex lock;
    enum pomodoro_state state;
    enum pomodoro_phase phase;
    uint8_t session;
    uint32_t phase_length_s;
    uint32_t elapsed_s;
    int64_t phase_started_ms;
    bool ui_timer_running;
};

static struct pomodoro_context ctx = {
    .lock = Z_MUTEX_INITIALIZER(ctx.lock),
    .state = POMODORO_STATE_IDLE,
    .phase = POMODORO_PHASE_NONE,
    .session = 0,
    .phase_length_s = POMODORO_WORK_SECONDS,
    .elapsed_s = 0,
    .phase_started_ms = 0,
    .ui_timer_running = false,
};

static struct pomodoro_status snapshot_locked(void);
static void schedule_minute_tick_locked(void);
static void cancel_timers_locked(void);
static void refresh_display_locked(bool force);
static void start_ui_timer_locked(void);

static struct k_work_delayable minute_tick_work;
static void minute_tick_cb(struct k_work *work);

static void ui_timer_cb(struct k_timer *timer);
K_TIMER_DEFINE(ui_timer, ui_timer_cb, NULL);

static inline bool is_running(void) {
    return ctx.state == POMODORO_STATE_WORK || ctx.state == POMODORO_STATE_BREAK;
}

static inline bool is_break_phase(void) { return ctx.phase == POMODORO_PHASE_BREAK; }

static inline uint32_t current_elapsed_locked(void) {
    if (!is_running()) {
        return ctx.elapsed_s;
    }

    int64_t now = k_uptime_get();
    int64_t delta_ms = now - ctx.phase_started_ms;
    uint32_t delta_s = delta_ms > 0 ? delta_ms / 1000 : 0;
    uint32_t total = ctx.elapsed_s + delta_s;
    return MIN(total, ctx.phase_length_s);
}

static inline uint32_t remaining_locked(void) {
    uint32_t elapsed = current_elapsed_locked();
    if (elapsed >= ctx.phase_length_s) {
        return 0;
    }

    return ctx.phase_length_s - elapsed;
}

static void start_ui_timer_locked(void) {
    if (!ctx.ui_timer_running) {
        k_timer_start(&ui_timer, K_SECONDS(1), K_SECONDS(1));
        ctx.ui_timer_running = true;
    }
}

static void stop_ui_timer_locked(void) {
    if (ctx.ui_timer_running) {
        k_timer_stop(&ui_timer);
        ctx.ui_timer_running = false;
    }
}

static void refresh_display_locked(bool force) {
    struct pomodoro_status status = snapshot_locked();
    k_mutex_unlock(&ctx.lock);
    pomodoro_display_update(&status, force);
    k_mutex_lock(&ctx.lock, K_FOREVER);
}

static void reset_phase_timing_locked(void) {
    ctx.elapsed_s = 0;
    ctx.phase_started_ms = k_uptime_get();
}

static void start_phase_locked(enum pomodoro_phase phase, bool reset_session) {
    ctx.phase = phase;
    ctx.state = (phase == POMODORO_PHASE_WORK) ? POMODORO_STATE_WORK : POMODORO_STATE_BREAK;
    ctx.phase_length_s =
        (phase == POMODORO_PHASE_WORK) ? POMODORO_WORK_SECONDS : POMODORO_BREAK_SECONDS;

    if (reset_session) {
        ctx.session = 1;
    } else if (ctx.session == 0) {
        ctx.session = 1;
    }

    reset_phase_timing_locked();
    start_ui_timer_locked();
    schedule_minute_tick_locked();
}

static void schedule_minute_tick_locked(void) {
    if (!is_running()) {
        return;
    }

    uint32_t remaining = remaining_locked();
    uint32_t delay_s = remaining > POMODORO_MINUTE_CHUNK ? POMODORO_MINUTE_CHUNK : remaining;

    k_work_reschedule(&minute_tick_work, K_SECONDS(MAX(delay_s, 1)));
}

static void complete_work_locked(void) {
    ctx.elapsed_s = 0;
    ctx.phase_started_ms = k_uptime_get();
    ctx.phase = POMODORO_PHASE_BREAK;
    ctx.state = POMODORO_STATE_BREAK;
    ctx.phase_length_s = POMODORO_BREAK_SECONDS;

    start_ui_timer_locked();
    schedule_minute_tick_locked();
}

static void complete_break_locked(void) {
    ctx.session++;

    if (ctx.session > POMODORO_MAX_SESSIONS) {
        ctx.session = 0;
        ctx.state = POMODORO_STATE_IDLE;
        ctx.phase = POMODORO_PHASE_NONE;
        ctx.elapsed_s = 0;
        ctx.phase_started_ms = 0;
        ctx.phase_length_s = POMODORO_WORK_SECONDS;
        cancel_timers_locked();
        return;
    }

    ctx.phase = POMODORO_PHASE_WORK;
    ctx.state = POMODORO_STATE_WORK;
    ctx.phase_length_s = POMODORO_WORK_SECONDS;
    reset_phase_timing_locked();
    start_ui_timer_locked();
    schedule_minute_tick_locked();
}

static void minute_tick_cb(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&ctx.lock, K_FOREVER);

    if (!is_running()) {
        k_mutex_unlock(&ctx.lock);
        return;
    }

    ctx.elapsed_s = current_elapsed_locked();

    if (remaining_locked() == 0) {
        if (ctx.phase == POMODORO_PHASE_WORK) {
            complete_work_locked();
        } else {
            complete_break_locked();
        }
        refresh_display_locked(true);
        k_mutex_unlock(&ctx.lock);
        return;
    }

    schedule_minute_tick_locked();
    refresh_display_locked(false);
    k_mutex_unlock(&ctx.lock);
}

static void cancel_timers_locked(void) {
    k_work_cancel_delayable(&minute_tick_work);
    stop_ui_timer_locked();
}

static void stop_locked(void) {
    ctx.state = POMODORO_STATE_IDLE;
    ctx.phase = POMODORO_PHASE_NONE;
    ctx.session = 0;
    ctx.elapsed_s = 0;
    ctx.phase_started_ms = 0;
    ctx.phase_length_s = POMODORO_WORK_SECONDS;
    cancel_timers_locked();
}

static void ui_timer_cb(struct k_timer *timer) {
    ARG_UNUSED(timer);
    k_mutex_lock(&ctx.lock, K_FOREVER);

    if (is_running()) {
        ctx.elapsed_s = current_elapsed_locked();
        if (remaining_locked() == 0) {
            if (ctx.phase == POMODORO_PHASE_WORK) {
                complete_work_locked();
            } else {
                complete_break_locked();
            }
        }
    }

    refresh_display_locked(false);
    k_mutex_unlock(&ctx.lock);
}

static struct pomodoro_status snapshot_locked(void) {
    uint32_t remaining = remaining_locked();

    return (struct pomodoro_status){
        .state = ctx.state,
        .session = ctx.session,
        .max_sessions = POMODORO_MAX_SESSIONS,
        .on_break = ctx.phase == POMODORO_PHASE_BREAK,
        .paused = ctx.state == POMODORO_STATE_PAUSED,
        .remaining_seconds = remaining,
        .phase_total_seconds = ctx.phase_length_s,
        .resume_on_any_key = IS_ENABLED(CONFIG_ZMK_POMODORO_RESUME_ON_ANY_KEY),
    };
}

int pomodoro_start(void) {
    k_mutex_lock(&ctx.lock, K_FOREVER);
    if (is_running()) {
        k_mutex_unlock(&ctx.lock);
        return 0;
    }

    start_phase_locked(POMODORO_PHASE_WORK, true);
    refresh_display_locked(true);
    k_mutex_unlock(&ctx.lock);
    return 0;
}

int pomodoro_pause(void) {
    k_mutex_lock(&ctx.lock, K_FOREVER);

    if (is_running()) {
        ctx.elapsed_s = current_elapsed_locked();
        ctx.state = POMODORO_STATE_PAUSED;
        stop_ui_timer_locked();
        k_work_cancel_delayable(&minute_tick_work);
        refresh_display_locked(true);
        k_mutex_unlock(&ctx.lock);
        return 0;
    }

    if (ctx.state == POMODORO_STATE_PAUSED) {
        if (is_break_phase()) {
            complete_break_locked();
        } else {
            ctx.state = POMODORO_STATE_WORK;
            ctx.phase = POMODORO_PHASE_WORK;
            ctx.phase_started_ms = k_uptime_get();
            start_ui_timer_locked();
            schedule_minute_tick_locked();
        }
        refresh_display_locked(true);
    }

    k_mutex_unlock(&ctx.lock);
    return 0;
}

int pomodoro_resume(void) {
    k_mutex_lock(&ctx.lock, K_FOREVER);

    if (ctx.state == POMODORO_STATE_PAUSED) {
        if (is_break_phase()) {
            complete_break_locked();
        } else {
            ctx.state = POMODORO_STATE_WORK;
            ctx.phase = POMODORO_PHASE_WORK;
            ctx.phase_started_ms = k_uptime_get();
            start_ui_timer_locked();
            schedule_minute_tick_locked();
        }
        refresh_display_locked(true);
    } else if (ctx.state == POMODORO_STATE_BREAK) {
        complete_break_locked();
        refresh_display_locked(true);
    }

    k_mutex_unlock(&ctx.lock);
    return 0;
}

int pomodoro_stop(void) {
    k_mutex_lock(&ctx.lock, K_FOREVER);
    stop_locked();
    refresh_display_locked(true);
    k_mutex_unlock(&ctx.lock);
    return 0;
}

int pomodoro_smart(void) {
    k_mutex_lock(&ctx.lock, K_FOREVER);

    if (ctx.state == POMODORO_STATE_IDLE) {
        start_phase_locked(POMODORO_PHASE_WORK, true);
    } else if (ctx.state == POMODORO_STATE_PAUSED) {
        if (is_break_phase()) {
            complete_break_locked();
        } else {
            ctx.state = POMODORO_STATE_WORK;
            ctx.phase = POMODORO_PHASE_WORK;
            ctx.phase_started_ms = k_uptime_get();
            start_ui_timer_locked();
            schedule_minute_tick_locked();
        }
    } else if (ctx.state == POMODORO_STATE_BREAK) {
        complete_break_locked();
    } else {
        ctx.elapsed_s = current_elapsed_locked();
        ctx.state = POMODORO_STATE_PAUSED;
        stop_ui_timer_locked();
        k_work_cancel_delayable(&minute_tick_work);
    }

    refresh_display_locked(true);
    k_mutex_unlock(&ctx.lock);
    return 0;
}

int pomodoro_break_extend(void) {
    k_mutex_lock(&ctx.lock, K_FOREVER);

    if (ctx.phase == POMODORO_PHASE_BREAK &&
        ctx.phase_length_s < CONFIG_ZMK_POMODORO_BREAK_EXTEND_LIMIT_MINUTES * 60) {
        ctx.phase_length_s =
            MIN(ctx.phase_length_s + 60, CONFIG_ZMK_POMODORO_BREAK_EXTEND_LIMIT_MINUTES * 60);
        if (is_running()) {
            schedule_minute_tick_locked();
        }
        refresh_display_locked(true);
    }

    k_mutex_unlock(&ctx.lock);
    return 0;
}

int pomodoro_break_skip(void) {
    k_mutex_lock(&ctx.lock, K_FOREVER);

    if (ctx.phase == POMODORO_PHASE_BREAK) {
        complete_break_locked();
        refresh_display_locked(true);
    }

    k_mutex_unlock(&ctx.lock);
    return 0;
}

struct pomodoro_status pomodoro_current_status(void) {
    k_mutex_lock(&ctx.lock, K_FOREVER);
    struct pomodoro_status status = snapshot_locked();
    k_mutex_unlock(&ctx.lock);
    return status;
}

static int pomodoro_any_key_handler(const zmk_event_t *eh) {
    if (!IS_ENABLED(CONFIG_ZMK_POMODORO_RESUME_ON_ANY_KEY)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    k_mutex_lock(&ctx.lock, K_FOREVER);
    bool in_break = ctx.state == POMODORO_STATE_BREAK || (ctx.state == POMODORO_STATE_PAUSED && is_break_phase());
    bool in_pause = ctx.state == POMODORO_STATE_PAUSED;
    k_mutex_unlock(&ctx.lock);

    if (in_break) {
        pomodoro_break_skip();
    } else if (in_pause) {
        pomodoro_resume();
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(pomodoro_any_key, pomodoro_any_key_handler);
ZMK_SUBSCRIPTION(pomodoro_any_key, zmk_position_state_changed);

static int pomodoro_init(void) {
    k_work_init_delayable(&minute_tick_work, minute_tick_cb);
    pomodoro_display_bootstrap(&pomodoro_current_status());
    return 0;
}

SYS_INIT(pomodoro_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif
