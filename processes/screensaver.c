#include <stdio.h>
#include "../lib.h"
#include "../messages.h"
#include "../os.h"
#include "../../../lib/lib_liveos.h"
#include "../../../lib/lib_menu.h"
#include "../config.h"
#include "../drivers.h"
#include "config.h"
#include "../lcd_buffer.h"
#include "proc_lib.h"

extern struct time Main_current_time; // Часы реального времени
extern struct machine_states Machine_states; // Состояние всей машины
extern struct machine_settings Machine_settings; // Глобальные настройки стиральной машины

struct
{
	uint8_t need_result_draw : 1; // Если бит выставлен в 1 , значит надо отрисовать результаты стирки или замачивания
	uint8_t disable_draw_date : 1; // Если данный бит выставлен в 1 то дата не отрисовывается
	uint16_t shutdown_timer : 14; //Таймер автовыключения машины при активации данного модуля
}Screensaver_state;



static void draw_welcom(void) //Отрисовать приветствие
{
	const char *str1, *str2;
	static uint8_t welcom_message_num = 0;

	switch(welcom_message_num)
	{
		case 0:
			str1 = PSTR("Чтото будем");
			str2 = PSTR("стирать?");
		break;

		case 1:
			str1 = PSTR("Что на этот раз");
			str2 = PSTR("от меня надо?");
		break;

		case 2:
			str1 = PSTR("Что будем");
			str2 = PSTR("делать?");
		break;

		case 3:
			str1 = PSTR("Опять стирать, Да?");
			str2 = PSTR("");
		break;

		case 4:
			str1 = PSTR("Да, я сейчас хочу");
			str2 = PSTR("что-то постирать!");
		break;

		case 5:
			str1 = PSTR("Может в другой");
			str2 = PSTR("раз будем стирать?");
		break;

		case 6:
			str1 = PSTR("УРА! Наконецто мы");
			str2 = PSTR("будем стирать!");
		break;
	}

	clear_line_buf(2);
	clear_line_buf(3);
	gotoxy_lcd_buf(get_center(strlen_P(str1)), 2); print_lcd_buf_P(str1);
	gotoxy_lcd_buf(get_center(strlen_P(str2)), 3); print_lcd_buf_P(str2);
	welcom_message_num++; // Увеличиваем номер следующего выводимого сообщения

	if(welcom_message_num > 6) // Если дошли до последнего сообщения то обнуляем счетчик
		welcom_message_num = 0;

	Screensaver_state.disable_draw_date = 1;
}


CALLBACK_NAME(PROCESS_SCREENSAVER, init)(void) // Инициализация процесса стирки
{
	Screensaver_state.need_result_draw = 0;
	Screensaver_state.disable_draw_date = 0;
}

CALLBACK_NAME(PROCESS_SCREENSAVER, run)(char *params) // Запуск процесса
{
}

CALLBACK_NAME(PROCESS_SCREENSAVER, active)(struct task_info *from)
{
	clr_lcd_buf();
	Screensaver_state.disable_draw_date = 0;
	Screensaver_state.shutdown_timer = AUTO_POWER_OFF_TIMEOUT;

	draw_current_time(&Main_current_time, 1);
	if(Screensaver_state.need_result_draw) // если нужно отобразить результаты стирки
	{
		draw_calculate_time(NULL, 1);
		draw_prog_name(NULL, 1);
		draw_step_name(NULL, 0, 0, 1);
		draw_operation_name(NULL, 1);
		Screensaver_state.need_result_draw = 0;
		Screensaver_state.disable_draw_date = 1;
	}
	else
	{
		draw_current_date(&Main_current_time, 1);
		Machine_states.current_machine_state = LED_OFF; // Выключаем все светодиоды
	}
}

CALLBACK_NAME(PROCESS_SCREENSAVER, main_loop)(void)
{
	if(Machine_states.change_current_time) // Если изменилось время, то перерисовываем текущее время
	{
		Machine_states.change_current_time = 0;
		draw_current_time(&Main_current_time, 1);
		if(!Screensaver_state.disable_draw_date)
			draw_current_date(&Main_current_time, 1);
	}

	if(Screensaver_state.shutdown_timer == 1) //Если прошло время свободного простоя, то выключаем машину
	{
		Screensaver_state.shutdown_timer = 0;
		Machine_states.current_machine_state = LED_OFF;
		Screensaver_state.disable_draw_date = 0;
		power_off_system();
	}
}

CALLBACK_NAME(PROCESS_SCREENSAVER, click_up)(void)
{
	Screensaver_state.shutdown_timer = 0;
	if(Machine_states.main_power)
		switch_process(PROCESS_MENU); //Вход в меню
}

CALLBACK_NAME(PROCESS_SCREENSAVER, click_down)(void)
{
	Screensaver_state.shutdown_timer = 0;
	if(Machine_states.main_power)
		switch_process(PROCESS_MENU); //Вход в меню
}

CALLBACK_NAME(PROCESS_SCREENSAVER, click_enter)(void)
{
	Screensaver_state.shutdown_timer = 0;
	if(Machine_states.main_power)
		switch_process(PROCESS_MENU); //Вход в меню
}

CALLBACK_NAME(PROCESS_SCREENSAVER, long_enter)(void) // при долгом удерживании включаем или выключаем стиральную машину
{
	if(!Machine_states.main_power)
	{
		power_on_system();
		draw_welcom();
	}
	else
	{
		Screensaver_state.disable_draw_date = 0;
		power_off_system();
	}
}

CALLBACK_NAME(PROCESS_SCREENSAVER, click_esc)(void)
{
	Screensaver_state.shutdown_timer = 0;
	if(Machine_states.main_power)
		switch_process(PROCESS_MENU); //Вход в меню
}

int t=0;
int t1=0;
int t2=0;

CALLBACK_NAME(PROCESS_SCREENSAVER, console)(char input_byte)
{
	switch(input_byte)
	{
		case '7':
			switch_process(PROCESS_MENU); //Вход в меню
		break;

		case 'z':
			turn_pump(t);
			t = !t;
		break;

		case 'x':
			turn_valve(0, t1);
			t1 = !t1;
		break;

		case 'c':
			turn_valve(1, t2);
			t2 = !t2;
		break;
	}
}

CALLBACK_NAME(PROCESS_SCREENSAVER, signal)(uint8_t signal) // Обработчик входящего сигнала
{
	switch(signal)
	{
		case 3:  // Сигнал означающий что надо вывести результаты стирки
			Screensaver_state.need_result_draw = 1;
			if(Machine_settings.sound)
				turn_blink_speaker(ON, 1000, 300, SPEAKER_SOUND_FREQ, 3); //Пищим три раза
		break;
	}
}

CALLBACK_NAME(PROCESS_SCREENSAVER, timer0)(void)
{
	if(!Machine_states.main_power) // Если машина выключенна
	{
		Screensaver_state.shutdown_timer = 0;
		return;
	}

	//Если следующие процессы запущенны то автовыключение питания неработает
	if(	get_process_status(PROCESS_WASHING) == RUNNING
		|| get_process_status(PROCESS_BEDABBLE) == RUNNING
		|| get_process_status(PROCESS_RINSE) == RUNNING
		|| get_process_status(PROCESS_WRING) == RUNNING)
	{
		return;
	}

	if(Screensaver_state.shutdown_timer > 1) //Таймер для обратного отсчета автовыключения питания
		Screensaver_state.shutdown_timer --;
}

CALLBACK_NAME(PROCESS_SCREENSAVER, stop)(void)
{
	clr_lcd_buf();
}

