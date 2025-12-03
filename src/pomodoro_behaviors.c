#define DT_DRV_COMPAT zmk_behavior_pomodoro

#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>

#include "pomodoro.h"

LOG_MODULE_DECLARE(pomodoro, CONFIG_ZMK_LOG_LEVEL);

struct pomodoro_behavior_config {
    enum pomodoro_action action;
};

static int pomodoro_behavior_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct pomodoro_behavior_config *cfg = dev->config;

    switch (cfg->action) {
    case POMODORO_ACTION_START:
        return pomodoro_start();
    case POMODORO_ACTION_PAUSE:
        return pomodoro_pause();
    case POMODORO_ACTION_STOP:
        return pomodoro_stop();
    case POMODORO_ACTION_SMART:
        return pomodoro_smart();
    case POMODORO_ACTION_RESUME:
        return pomodoro_resume();
    case POMODORO_ACTION_BREAK_EXTEND:
        return pomodoro_break_extend();
    case POMODORO_ACTION_BREAK_SKIP:
        return pomodoro_break_skip();
    default:
        return ZMK_BEHAVIOR_OPAQUE;
    }
}

static int pomodoro_behavior_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api pomodoro_behavior_driver_api = {
    .binding_pressed = pomodoro_behavior_pressed,
    .binding_released = pomodoro_behavior_released,
};

static int pomodoro_behavior_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define POMODORO_CFG(inst)                                                                        \
    static const struct pomodoro_behavior_config pomodoro_behavior_config_##inst = {              \
        .action = DT_INST_ENUM_IDX(inst, pomo_action),                                            \
    };

#define POMODORO_INST(inst)                                                                       \
    POMODORO_CFG(inst)                                                                            \
    BEHAVIOR_DT_INST_DEFINE(inst, pomodoro_behavior_init, NULL, NULL,                             \
                            &pomodoro_behavior_config_##inst, POST_KERNEL,                        \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &pomodoro_behavior_driver_api);

DT_INST_FOREACH_STATUS_OKAY(POMODORO_INST)
