#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <string.h>
#include <stdio.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>

#include <lvgl.h>

#include "pomodoro.h"
#include "pomodoro_display.h"

LOG_MODULE_DECLARE(pomodoro, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_POMODORO_DISPLAY)

#define POMODORO_WORK_SECONDS POMODORO_DEFAULT_WORK_SECONDS

static lv_obj_t *screen;
static lv_obj_t *status_label;
static lv_obj_t *session_label;
static lv_obj_t *time_label;
static lv_obj_t *hint_label;
static lv_obj_t *progress_bar;

static struct pomodoro_status cached_state;
static bool cached_force;
static bool has_cached_state;
static struct pomodoro_status last_drawn;
static bool has_drawn;

K_MUTEX_DEFINE(display_state_mutex);
static void apply_state(struct pomodoro_status state, bool force);

static void pomodoro_display_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!screen || !zmk_display_is_initialized()) {
        return;
    }

    struct pomodoro_status state;
    bool force;

    k_mutex_lock(&display_state_mutex, K_FOREVER);
    if (!has_cached_state) {
        k_mutex_unlock(&display_state_mutex);
        return;
    }
    state = cached_state;
    force = cached_force;
    cached_force = false;
    k_mutex_unlock(&display_state_mutex);

    apply_state(state, force);
}

K_WORK_DEFINE(pomodoro_display_work, pomodoro_display_work_handler);

static void request_draw(bool force) {
    k_mutex_lock(&display_state_mutex, K_FOREVER);
    cached_force |= force;
    has_cached_state = true;
    k_mutex_unlock(&display_state_mutex);

    if (screen) {
        k_work_submit_to_queue(zmk_display_work_q(), &pomodoro_display_work);
    }
}

void pomodoro_display_update(const struct pomodoro_status *status, bool force) {
    if (status == NULL) {
        return;
    }

    k_mutex_lock(&display_state_mutex, K_FOREVER);
    cached_state = *status;
    k_mutex_unlock(&display_state_mutex);
    request_draw(force);
}

void pomodoro_display_bootstrap(const struct pomodoro_status *status) {
    pomodoro_display_update(status, true);
}

static void set_label_text(lv_obj_t *label, const char *text) {
    if (!label) {
        return;
    }
    lv_label_set_text(label, text);
}

static void apply_state(struct pomodoro_status state, bool force) {
    if (!screen) {
        return;
    }

    bool state_changed = !has_drawn || state.state != last_drawn.state ||
                         state.session != last_drawn.session || state.on_break != last_drawn.on_break;
    bool time_changed =
        !has_drawn || state.remaining_seconds != last_drawn.remaining_seconds ||
        state.phase_total_seconds != last_drawn.phase_total_seconds || state.paused != last_drawn.paused;

    if (!force && !state_changed && !time_changed) {
        return;
    }

    char status_text[8] = "Idle";
    char session_text[12];
    char time_text[8];
    char hint_text[20] = "";

    switch (state.state) {
    case POMODORO_STATE_WORK:
        strcpy(status_text, "Work");
        break;
    case POMODORO_STATE_BREAK:
        strcpy(status_text, "Break");
        strcpy(hint_text, "Resume=Skip");
        break;
    case POMODORO_STATE_PAUSED:
        strcpy(status_text, "Paused");
        strcpy(hint_text, "Resume/Play");
        break;
    case POMODORO_STATE_IDLE:
    default:
        strcpy(status_text, "Idle");
        strcpy(hint_text, "Press Start");
        break;
    }

    if (state.resume_on_any_key && (state.state == POMODORO_STATE_BREAK || state.state == POMODORO_STATE_PAUSED)) {
        strcpy(hint_text, "Any key resumes");
    }

    uint8_t session = state.session;
    if (state.state == POMODORO_STATE_IDLE) {
        session = 0;
    }
    snprintf(session_text, sizeof(session_text), "Sess %u/%u", session, state.max_sessions);

    bool is_idle = state.state == POMODORO_STATE_IDLE;
    uint32_t total = state.phase_total_seconds ? state.phase_total_seconds : POMODORO_WORK_SECONDS;
    uint32_t remaining_display = is_idle ? 0 : state.remaining_seconds;
    uint32_t remaining_for_progress = is_idle ? total : MIN(state.remaining_seconds, total);
    uint32_t elapsed = (remaining_for_progress > total) ? 0 : total - remaining_for_progress;

    snprintf(time_text, sizeof(time_text), "%02u:%02u", remaining_display / 60,
             remaining_display % 60);

    if (state_changed || force) {
        set_label_text(status_label, status_text);
        set_label_text(session_label, session_text);
    }

    if (time_changed || force) {
        set_label_text(time_label, time_text);
        if (lv_bar_get_max_value(progress_bar) != total) {
            lv_bar_set_range(progress_bar, 0, total);
        }
        lv_bar_set_value(progress_bar, elapsed, LV_ANIM_OFF);
    }

    if (hint_text[0] != '\0') {
        set_label_text(hint_label, hint_text);
        lv_obj_clear_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        set_label_text(hint_label, "");
        lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    }

    has_drawn = true;
    last_drawn = state;
}

static void create_ui(lv_obj_t *parent) {
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    status_label = lv_label_create(parent);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 0, 0);

    session_label = lv_label_create(parent);
    lv_obj_align(session_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(time_label, lv_theme_get_font_large(parent), LV_PART_MAIN);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -4);

    hint_label = lv_label_create(parent);
    lv_obj_set_style_text_font(hint_label, lv_theme_get_font_small(parent), LV_PART_MAIN);
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -2);

    progress_bar = lv_bar_create(parent);
    lv_coord_t width = lv_obj_get_width(parent);
    if (width == 0) {
        lv_disp_t *disp = lv_disp_get_default();
        width = lv_disp_get_hor_res(disp);
    }
    lv_obj_set_size(progress_bar, width - 8, 8);
    lv_obj_align(progress_bar, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_bar_set_range(progress_bar, 0, POMODORO_WORK_SECONDS);
}

lv_obj_t *zmk_display_status_screen(void) {
    screen = lv_obj_create(NULL);
    create_ui(screen);

    if (has_cached_state) {
        k_work_submit_to_queue(zmk_display_work_q(), &pomodoro_display_work);
    } else {
        struct pomodoro_status idle_state = {
            .state = POMODORO_STATE_IDLE,
            .session = 0,
            .max_sessions = POMODORO_MAX_SESSIONS,
            .on_break = false,
            .paused = false,
            .remaining_seconds = 0,
            .phase_total_seconds = POMODORO_WORK_SECONDS,
            .resume_on_any_key = IS_ENABLED(CONFIG_ZMK_POMODORO_RESUME_ON_ANY_KEY),
        };
        pomodoro_display_update(&idle_state, true);
    }

    return screen;
}

#endif
