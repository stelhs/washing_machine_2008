#ifndef OS_H
#define OS_H

enum all_proceses // список всех процессов в ОС
{
	PROCESS_SCREENSAVER = 1,
	PROCESS_MENU,
	PROCESS_WASHING,
	PROCESS_BEDABBLE,
	PROCESS_DISPLAY_ERROR,
	PROCESS_RINSE,
	PROCESS_WRING,
	PROCESS_WATER_CONTROL,
	PROCESS_SET_TIME,
	PROCESS_POWER_MANAGER,
};

struct external_callbacks
{
			// Встраивание в обработчик таймера 2
	void (*on_timer0)(void); 
	void (*on_timer2)(void); 
	
		// События по нажатию на кнопки
	void (*on_click_up)(void); 
	void (*on_click_down)(void);
	void (*on_click_esc)(void);
	void (*on_click_enter)(void);
	void (*on_click_ext1)(void);
	void (*on_long_esc)(void); 
	void (*on_long_enter)(void);
	void (*on_press_ext1)(void);
	void (*on_press_ext2)(void);
	void (*on_press_ext3)(void);
	void (*on_press_ext4)(void);
};

void sys_call_timer0(void); // Функция встраивается в системный таймер 0

void sys_call_timer2(void); // Функция встраивается в системный таймер 2

#endif //OS_H
