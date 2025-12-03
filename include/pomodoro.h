#pragma once

#include <stdbool.h>
#include <stdint.h>

enum pomodoro_state {
    POMODORO_STATE_IDLE = 0,
    POMODORO_STATE_WORK,
    POMODORO_STATE_BREAK,
    POMODORO_STATE_PAUSED,
};

#define POMODORO_DEFAULT_WORK_SECONDS (25 * 60)
#define POMODORO_DEFAULT_BREAK_SECONDS (5 * 60)
#define POMODORO_MAX_SESSIONS 4

enum pomodoro_action {
    POMODORO_ACTION_START = 0,
    POMODORO_ACTION_PAUSE,
    POMODORO_ACTION_STOP,
    POMODORO_ACTION_SMART,
    POMODORO_ACTION_RESUME,
    POMODORO_ACTION_BREAK_EXTEND,
    POMODORO_ACTION_BREAK_SKIP,
};

struct pomodoro_status {
    enum pomodoro_state state;
    uint8_t session;
    uint8_t max_sessions;
    bool on_break;
    bool paused;
    uint32_t remaining_seconds;
    uint32_t phase_total_seconds;
    bool resume_on_any_key;
};

int pomodoro_start(void);
int pomodoro_pause(void);
int pomodoro_stop(void);
int pomodoro_smart(void);
int pomodoro_resume(void);
int pomodoro_break_extend(void);
int pomodoro_break_skip(void);

struct pomodoro_status pomodoro_current_status(void);
