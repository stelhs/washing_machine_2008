#include <stdio.h>
#include "../lib.h"
#include "../messages.h"
#include "../os.h"
#include "../../../lib/lib_liveos.h"
#include "../config.h"
#include "config.h"
#include "../lcd_buffer.h"
#include "proc_lib.h"
#include "../../../lib/lib_menu.h"

extern struct time Main_current_time; // Часы реального времени
extern struct machine_states Machine_states; // Состояние всей машины


enum selected_cells
{
	HOUR,
	MIN,
	DAY,
	MONTH,
	YEAR,
	DAY_OF_WEEK,
};

struct
{
	uint8_t current_cell : 3;  //Текущий номер ячейки редактирования (часы или минуты)
	uint16_t blink_cell; // Таймер для мигания активной ячейки
	uint8_t blink_state : 1; // Бит состояния мигания выбранного для редактирования разряда
}Set_time_states;


static void hour_up(void)
{
	if(Main_current_time.hour < 23)
		Main_current_time.hour ++;
	else
		Main_current_time.hour = 0;
}

static void hour_down(void)
{
	if(Main_current_time.hour > 0)
		Main_current_time.hour --;
	else
		Main_current_time.hour = 23;
}


static void min_up(void)
{
	if(Main_current_time.min < 59)
		Main_current_time.min ++;
	else
		Main_current_time.min = 0;
}

static void min_down(void)
{
	if(Main_current_time.min > 1)
		Main_current_time.min --;
	else
		Main_current_time.min = 59;
}


static void day_up(void)
{
	if(Main_current_time.day_of_month < 31)
		Main_current_time.day_of_month ++;
	else
		Main_current_time.day_of_month = 1;
}

static void day_down(void)
{
	if(Main_current_time.day_of_month > 1)
		Main_current_time.day_of_month --;
	else
		Main_current_time.day_of_month = 31;
}


static void month_up(void)
{
	if(Main_current_time.month < 12)
		Main_current_time.month ++;
	else
		Main_current_time.month = 1;
}

static void month_down(void)
{
	if(Main_current_time.month > 1)
		Main_current_time.month --;
	else
		Main_current_time.month = 12;
}


static void year_up(void)
{
	if(Main_current_time.year > 1)
		Main_current_time.year --;
	else
		Main_current_time.year = 99;
}

static void year_down(void)
{
	if(Main_current_time.year < 99)
		Main_current_time.year ++;
	else
		Main_current_time.year = 0;
}


static void day_of_week_up(void)
{
	if(Main_current_time.day_of_week < 7)
		Main_current_time.day_of_week ++;
	else
		Main_current_time.day_of_week = 1;
}

static void day_of_week_down(void)
{
	if(Main_current_time.day_of_week > 1)
		Main_current_time.day_of_week --;
	else
		Main_current_time.day_of_week = 7;
}


CALLBACK_NAME(PROCESS_SET_TIME, init)(void) // Инициализация процесса установки времени
{
	Set_time_states.current_cell = HOUR;
	Set_time_states.blink_state = 0;
}

CALLBACK_NAME(PROCESS_SET_TIME, run)(char *params)
{
	Set_time_states.current_cell = HOUR;
	Set_time_states.blink_state = 0;
	Set_time_states.blink_cell = BLINK_TIME;
}

CALLBACK_NAME(PROCESS_SET_TIME, active)(struct task_info *from)
{
	clr_lcd_buf();
}

CALLBACK_NAME(PROCESS_SET_TIME, main_loop)(void)
{
	if(Machine_states.change_current_time) // Отрисовываем время
	{
		draw_current_time(&Main_current_time, 1);
		Machine_states.change_current_time = 0;
	}

	if(Set_time_states.blink_cell == 1)
	{
		Set_time_states.blink_cell = BLINK_TIME;
		struct time t = Main_current_time;
		if(!Set_time_states.blink_state)
		{
			Set_time_states.blink_state = 1;
			switch(Set_time_states.current_cell) //Выставляем запредельные значения для выбранной ячейки часов, для того чтобы выбранная ячейка не отображалась в данный момент
			{
				case HOUR:
					t.hour = 25;
				break;

				case MIN:
					t.min = 60;
				break;

				case DAY:
					t.day_of_month = 32;
				break;

				case MONTH:
					t.month = 13;
				break;

				case YEAR:
					t.year = 100;
				break;

				case DAY_OF_WEEK:
					t.day_of_week = 8;
				break;
			}
		}
		else
			Set_time_states.blink_state = 0;

		draw_current_time(&t, 1);
		draw_current_date(&t, 1);
	}
}

CALLBACK_NAME(PROCESS_SET_TIME, click_up)(void)
{
	switch(Set_time_states.current_cell)
	{
		case HOUR:
			hour_down();
		break;

		case MIN:
			min_down();
		break;

		case DAY:
			day_down();
		break;

		case MONTH:
			month_down();
		break;

		case YEAR:
			year_down();
		break;

		case DAY_OF_WEEK:
			day_of_week_down();
		break;
	}

	write_datetime(&Main_current_time);
	draw_current_time(&Main_current_time, 1);
	draw_current_date(&Main_current_time, 1);
}

CALLBACK_NAME(PROCESS_SET_TIME, click_down)(void)
{
	switch(Set_time_states.current_cell)
	{
		case HOUR:
			hour_up();
		break;

		case MIN:
			min_up();
		break;

		case DAY:
			day_up();
		break;

		case MONTH:
			month_up();
		break;

		case YEAR:
			year_up();
		break;

		case DAY_OF_WEEK:
			day_of_week_up();
		break;
	}

	write_datetime(&Main_current_time);
	draw_current_time(&Main_current_time, 1);
	draw_current_date(&Main_current_time, 1);
}

CALLBACK_NAME(PROCESS_SET_TIME, click_enter)(void)
{
	Set_time_states.current_cell++;
	if(Set_time_states.current_cell > DAY_OF_WEEK)
		Set_time_states.current_cell = HOUR;
}


CALLBACK_NAME(PROCESS_SET_TIME, click_esc)(void)
{
	stop_process(PROCESS_SET_TIME); // завершаем работу
	switch_process(PROCESS_MENU); // вход в меню
}

CALLBACK_NAME(PROCESS_SET_TIME, timer2)(void)
{
	if(Set_time_states.blink_cell > 1)
		Set_time_states.blink_cell--;
}

CALLBACK_NAME(PROCESS_SET_TIME, console)(char input_byte)
{
	switch(input_byte)
	{
		case '7':
			stop_process(PROCESS_SET_TIME); // завершаем работу
			switch_process(PROCESS_MENU); //Вход в меню
		break;
	}
}
