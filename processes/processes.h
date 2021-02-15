
CALLBACK_NAME(PROCESS_BEDABBLE, init)(void); // Инициализация процесса замачивания
CALLBACK_NAME(PROCESS_BEDABBLE, run)(char *params); // Запуск процесса замачивания
CALLBACK_NAME(PROCESS_BEDABBLE, active)(struct task_info *from);
CALLBACK_NAME(PROCESS_BEDABBLE, main_loop)(void);
CALLBACK_NAME(PROCESS_BEDABBLE, background)(uint8_t is_active);
CALLBACK_NAME(PROCESS_BEDABBLE, click_esc)(void);
CALLBACK_NAME(PROCESS_BEDABBLE, click_enter)(void);
CALLBACK_NAME(PROCESS_BEDABBLE, console)(char input_byte);
CALLBACK_NAME(PROCESS_BEDABBLE, timer0)(void); // Обработчик таймера с частотой 1на секунда
CALLBACK_NAME(PROCESS_BEDABBLE, signal)(uint8_t signal);
CALLBACK_NAME(PROCESS_BEDABBLE, stop)(void);




CALLBACK_NAME(PROCESS_DISPLAY_ERROR, init)(void);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, run)(char *params);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, active)(struct task_info *from);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, main_loop)(void);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, click_up)(void);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, click_down)(void);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, click_esc)(void);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, click_enter)(void);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, long_enter)(void);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, click_ext1)(void);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, timer2)(void);
CALLBACK_NAME(PROCESS_DISPLAY_ERROR, background)(uint8_t is_active);




CALLBACK_NAME(PROCESS_MENU, init)(void);
CALLBACK_NAME(PROCESS_MENU, run)(char *params);
CALLBACK_NAME(PROCESS_MENU, active)(struct task_info *from);
CALLBACK_NAME(PROCESS_MENU, main_loop)(void);
CALLBACK_NAME(PROCESS_MENU, background)(uint8_t is_active);
CALLBACK_NAME(PROCESS_MENU, click_up)(void);
CALLBACK_NAME(PROCESS_MENU, click_down)(void);
CALLBACK_NAME(PROCESS_MENU, long_up)(void);
CALLBACK_NAME(PROCESS_MENU, long_down)(void);
CALLBACK_NAME(PROCESS_MENU, click_esc)(void);
CALLBACK_NAME(PROCESS_MENU, click_enter)(void);
CALLBACK_NAME(PROCESS_MENU, click_ext1)(void);
CALLBACK_NAME(PROCESS_MENU, timer2)(void);
CALLBACK_NAME(PROCESS_MENU, console)(char ch);




CALLBACK_NAME(PROCESS_RINSE, init)(void); // Инициализация процесса полоскания
CALLBACK_NAME(PROCESS_RINSE, run)(char *params);
CALLBACK_NAME(PROCESS_RINSE, active)(struct task_info *from);
CALLBACK_NAME(PROCESS_RINSE, main_loop)(void);
CALLBACK_NAME(PROCESS_RINSE, background)(uint8_t is_active);
CALLBACK_NAME(PROCESS_RINSE, click_esc)(void);
CALLBACK_NAME(PROCESS_RINSE, click_enter)(void);
CALLBACK_NAME(PROCESS_RINSE, timer0)(void);
CALLBACK_NAME(PROCESS_RINSE, signal)(uint8_t signal);
CALLBACK_NAME(PROCESS_RINSE, console)(char input_byte);
CALLBACK_NAME(PROCESS_RINSE, stop)(void);




CALLBACK_NAME(PROCESS_SCREENSAVER, init)(void);
CALLBACK_NAME(PROCESS_SCREENSAVER, run)(char *params);
CALLBACK_NAME(PROCESS_SCREENSAVER, stop)(void);
CALLBACK_NAME(PROCESS_SCREENSAVER, active)(struct task_info *from);
CALLBACK_NAME(PROCESS_SCREENSAVER, main_loop)(void);
CALLBACK_NAME(PROCESS_SCREENSAVER, background)(uint8_t is_active);
CALLBACK_NAME(PROCESS_SCREENSAVER, click_up)(void);
CALLBACK_NAME(PROCESS_SCREENSAVER, click_down)(void);
CALLBACK_NAME(PROCESS_SCREENSAVER, click_esc)(void);
CALLBACK_NAME(PROCESS_SCREENSAVER, click_enter)(void);
CALLBACK_NAME(PROCESS_SCREENSAVER, long_enter)(void);
CALLBACK_NAME(PROCESS_SCREENSAVER, timer0)(void);
CALLBACK_NAME(PROCESS_SCREENSAVER, console)(char input_byte);
CALLBACK_NAME(PROCESS_SCREENSAVER, signal)(uint8_t signal);




CALLBACK_NAME(PROCESS_SET_TIME, init)(void);
CALLBACK_NAME(PROCESS_SET_TIME, run)(char *params);
CALLBACK_NAME(PROCESS_SET_TIME, active)(struct task_info *from);
CALLBACK_NAME(PROCESS_SET_TIME, main_loop)(void);
CALLBACK_NAME(PROCESS_SET_TIME, click_esc)(void);
CALLBACK_NAME(PROCESS_SET_TIME, click_up)(void);
CALLBACK_NAME(PROCESS_SET_TIME, click_down)(void);
CALLBACK_NAME(PROCESS_SET_TIME, click_enter)(void);
CALLBACK_NAME(PROCESS_SET_TIME, long_esc)(void);
CALLBACK_NAME(PROCESS_SET_TIME, long_up)(void);
CALLBACK_NAME(PROCESS_SET_TIME, long_down)(void);
CALLBACK_NAME(PROCESS_SET_TIME, long_enter)(void);
CALLBACK_NAME(PROCESS_SET_TIME, press_ext1)(void);
CALLBACK_NAME(PROCESS_SET_TIME, timer2)(void);
CALLBACK_NAME(PROCESS_SET_TIME, console)(char input_byte);




CALLBACK_NAME(PROCESS_WASHING, init)(void); // Инициализация процесса стирки
CALLBACK_NAME(PROCESS_WASHING, run)(char *params); // Запуск процесса стирки
CALLBACK_NAME(PROCESS_WASHING, active)(struct task_info *from);
CALLBACK_NAME(PROCESS_WASHING, main_loop)(void);
CALLBACK_NAME(PROCESS_WASHING, background)(uint8_t is_active);
CALLBACK_NAME(PROCESS_WASHING, click_esc)(void);
CALLBACK_NAME(PROCESS_WASHING, click_enter)(void);
CALLBACK_NAME(PROCESS_WASHING, console)(char input_byte);
CALLBACK_NAME(PROCESS_WASHING, signal)(uint8_t signal); // Обработчик входящего сигнала
CALLBACK_NAME(PROCESS_WASHING, timer0)(void);
CALLBACK_NAME(PROCESS_WASHING, stop)(void);




CALLBACK_NAME(PROCESS_WATER_CONTROL, init)(void);
CALLBACK_NAME(PROCESS_WATER_CONTROL, run)(char *params);
CALLBACK_NAME(PROCESS_WATER_CONTROL, active)(struct task_info *from);
CALLBACK_NAME(PROCESS_WATER_CONTROL, main_loop)(void);
CALLBACK_NAME(PROCESS_WATER_CONTROL, click_esc)(void);
CALLBACK_NAME(PROCESS_WATER_CONTROL, press_ext1)(void);
CALLBACK_NAME(PROCESS_WATER_CONTROL, press_ext2)(void);
CALLBACK_NAME(PROCESS_WATER_CONTROL, press_ext3)(void);
CALLBACK_NAME(PROCESS_WATER_CONTROL, press_ext4)(void);
CALLBACK_NAME(PROCESS_WATER_CONTROL, click_enter)(void);
CALLBACK_NAME(PROCESS_WATER_CONTROL, timer2)(void);
CALLBACK_NAME(PROCESS_WATER_CONTROL, stop)(void);
CALLBACK_NAME(PROCESS_WATER_CONTROL, console)(char ch);



CALLBACK_NAME(PROCESS_WRING, init)(void);
CALLBACK_NAME(PROCESS_WRING, run)(char *params);
CALLBACK_NAME(PROCESS_WRING, active)(struct task_info *from);
CALLBACK_NAME(PROCESS_WRING, main_loop)(void);
CALLBACK_NAME(PROCESS_WRING, background)(uint8_t is_active);
CALLBACK_NAME(PROCESS_WRING, click_up)(void);
CALLBACK_NAME(PROCESS_WRING, click_down)(void);
CALLBACK_NAME(PROCESS_WRING, click_esc)(void);
CALLBACK_NAME(PROCESS_WRING, click_enter)(void);
CALLBACK_NAME(PROCESS_WRING, click_ext1)(void);
CALLBACK_NAME(PROCESS_WRING, timer0)(void);
CALLBACK_NAME(PROCESS_WRING, signal)(uint8_t signal);
CALLBACK_NAME(PROCESS_WRING, console)(char input_byte);
CALLBACK_NAME(PROCESS_WRING, stop)(void);



CALLBACK_NAME(PROCESS_POWER_MANAGER, init)(void);
CALLBACK_NAME(PROCESS_POWER_MANAGER, run)(char *params);
CALLBACK_NAME(PROCESS_POWER_MANAGER, background)(uint8_t is_active);
CALLBACK_NAME(PROCESS_POWER_MANAGER, signal)(uint8_t signal);

