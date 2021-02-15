#include <stdio.h>
#include "../drivers.h"
#include "../lib.h"
#include "../messages.h"
#include "../os.h"
#include "lib_liveos.h"
#include "../config.h"
#include "config.h"
#include "../lcd_buffer.h"
#include "proc_lib.h"

extern struct machine_states Machine_states; // Состояние всей машины

struct // Структура состояния процесса
{
	uint8_t scroll_pos; // позиция скроллера прокрутки
	uint8_t width_message; // длинна сообщения
	uint16_t scroll_timer; // Счетчик для отмера времени между перерисовкой бегущей строки
	uint8_t from; // Хранит номер процесс с которого произошол вход. Откого пришли, к тому и надо уйти.
}Display_error;

static void draw_scroll_text(void)
{
	if(Error_message)
	{
		clear_line_buf(3);
		gotoxy_lcd_buf(0, 3); 
		print_lcd_buf_P(Error_message + Display_error.scroll_pos);
	}
}

static void exit_display_error(void)
{
	Machine_states.error = 0;
	switch_process(Display_error.from); //Переключаемся туда откуда пришли
}

CALLBACK_NAME(PROCESS_DISPLAY_ERROR, init)(void) // Инициализация процесса стирки
{
	Display_error.scroll_pos = 0;
	Display_error.width_message = 0;
	Display_error.scroll_timer = 0;
}

CALLBACK_NAME(PROCESS_DISPLAY_ERROR, run)(char *params) // Запуск процесса стирки
{
}

CALLBACK_NAME(PROCESS_DISPLAY_ERROR, active)(struct task_info *from)
{
	Display_error.from = from -> from_task_id;
	
	if(!Machine_states.error)
		return;
		
	Display_error.scroll_pos = 0;

	Display_error.width_message = strlen_P(Error_message);
	
	if(Display_error.width_message <= DISPLAY_WIDTH)
		draw_scroll_text();
	else
		Display_error.scroll_timer = SCROLL_TEXT_START_SPEED;
	
	
	switch(Machine_states.error)
	{
		case HARDWARE_ERROR_1:
			stop_process(PROCESS_WASHING);
			stop_process(PROCESS_WRING);
			stop_process(PROCESS_BEDABBLE);
			stop_process(PROCESS_RINSE);
			stop_fill_tank_and_stir_powder();
			stop_fill_tank();
			stop_rinse();
			stop_wring();
			stop_mode_rotating();
			disable_rotate_drum();
			pour_out_water();
			Machine_states.lock = 1;
		break;

		case HARDWARE_ERROR_2:
			stop_process(PROCESS_WASHING);
			stop_process(PROCESS_WRING);
			stop_process(PROCESS_BEDABBLE);
			stop_process(PROCESS_RINSE);
			stop_fill_tank_and_stir_powder();
			stop_fill_tank();
			stop_rinse();
			stop_wring();
			stop_mode_rotating();
			disable_rotate_drum();
			Machine_states.lock = 1;
		break;

		case EXTERNAL_ERROR:
			stop_process(PROCESS_WASHING);
			stop_process(PROCESS_WRING);
			stop_process(PROCESS_BEDABBLE);
			stop_process(PROCESS_RINSE);
		break;

		case ERROR:
			Machine_states.lock = 0;
		break;
	}

	clear_line_buf(2);
	gotoxy_lcd_buf(1, 2); // Вывод сообщения "Обнаруженна ошибка"
	print_lcd_buf_P(GET_MESS_POINTER(ERROR_WARNING));
	draw_scroll_text();
}

CALLBACK_NAME(PROCESS_DISPLAY_ERROR, main_loop)(void)
{
	if(!Machine_states.error)
		return;
		
	if(Display_error.scroll_timer == 1)
	{
		Display_error.scroll_timer = SCROLL_TEXT_SPEED;
		Display_error.scroll_pos ++;
		if(Display_error.scroll_pos >= Display_error.width_message) //Если прокрутили строку до конца то ожидаем чуть больше и начинаем повтор бегущей строки
		{
			Display_error.scroll_pos = 0;
			Display_error.scroll_timer = SCROLL_TEXT_START_SPEED;
		}
		draw_scroll_text();
	}
}

CALLBACK_NAME(PROCESS_DISPLAY_ERROR, background)(uint8_t is_active)
{
	if(Machine_states.error && !is_active) //Если обнаруженна ошибка, то активизируемся
		switch_process(PROCESS_DISPLAY_ERROR);
}

CALLBACK_NAME(PROCESS_DISPLAY_ERROR, click_esc)(void)
{
	if(Machine_states.lock)
		return;
		
	exit_display_error();
}

CALLBACK_NAME(PROCESS_DISPLAY_ERROR, click_enter)(void)
{
	if(Machine_states.lock)
		return;
		
	exit_display_error();
}

CALLBACK_NAME(PROCESS_DISPLAY_ERROR, long_enter)(void)
{
	if(Machine_states.main_power) 
	{
		power_off_system();
		Machine_states.lock = 0; // Когда машина выключается, флаг блокировки всегда снимается независимо от того был он установлен или нет
		Machine_states.error = 0;
		switch_process(PROCESS_SCREENSAVER);
	}
}

CALLBACK_NAME(PROCESS_DISPLAY_ERROR, timer2)(void)
{
	if(Display_error.scroll_timer > 1)
		Display_error.scroll_timer--;
}

