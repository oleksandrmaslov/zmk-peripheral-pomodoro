zephyr_include_directories(include)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ZMK_POMODORO src/pomodoro.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_POMODORO src/pomodoro_behaviors.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_POMODORO_DISPLAY src/pomodoro_display.c)