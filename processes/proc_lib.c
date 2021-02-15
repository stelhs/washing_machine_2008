#include <string.h>
#include "../drivers.h"
#include "../messages.h"
#include "proc_lib.h"
#include "../lcd_buffer.h"
#include "lib_menu.h"

uint16_t conv_temperature_to_adc_val(uint8_t temperature) //Функция конвертации температуры в значение с ADC преобразователя
{
	switch(temperature) // Все эти цифры тщательно подогнанны опытным путем(методом замера реальной температуры).
	{
		case 0:
		case 10:
			return 0;

		case 30:
			return 415 ;

		case 40:
			return 575;

		case 60:
			return 790;

		case 70:
			return 855;

		case 80:
			return 905;

		case 95:
			return 950;
	}

	return 0;
}

void draw_current_time(struct time *t, uint8_t need_redraw) //Отрисовать реальное время
{
	static char time[9];
	if(t != NULL)
	{
		memset(time, 0, sizeof(time));

		if(t -> hour > 24)
			snprintf(time, sizeof(time), "  :");
		else
			snprintf(time, sizeof(time), "%.2d:", t -> hour);

		if(t -> min > 59)
			snprintf(time + strlen(time), sizeof(time) - strlen(time), "  :");
		else
			snprintf(time + strlen(time), sizeof(time) - strlen(time), "%.2d:", t -> min);

		if(t -> sec > 59)
			snprintf(time + strlen(time), sizeof(time) - strlen(time), "  ");
		else
			snprintf(time + strlen(time), sizeof(time) - strlen(time), "%.2d", t -> sec);
	}
	if(need_redraw)
	{
		gotoxy_lcd_buf(0, 0);
		print_lcd_buf(time);
	}
}

void draw_current_date(struct time *t, uint8_t need_redraw) //Отрисовать текущую дату
{
	static char date[13];
	static uint8_t day_of_week = 1;

	if(t != NULL)
	{
		memset(date, 0, sizeof(date));

		if(t -> day_of_month > 31)
			snprintf(date, sizeof(date), "  .");
		else
			snprintf(date, sizeof(date), "%.2d.", t -> day_of_month);

		if(t -> month > 12)
			snprintf(date + strlen(date), sizeof(date) - strlen(date), "  .");
		else
			snprintf(date + strlen(date), sizeof(date) - strlen(date), "%.2d.", t -> month);

		if(t -> year > 99)
			snprintf(date + strlen(date), sizeof(date) - strlen(date), "20  г.");
		else
			snprintf(date + strlen(date), sizeof(date) - strlen(date), "20%.2dг.", t -> year);

		day_of_week = t -> day_of_week;
	}

	if(need_redraw)
	{
		clear_line_buf(1);
		gotoxy_lcd_buf(get_center(strlen(date)), 1);
		print_lcd_buf(date);

		clear_line_buf(2);
		gotoxy_lcd_buf(get_center(strlen_P(  (const char *)pgm_read_word(&List_names_day_of_week[day_of_week - 1])  )), 2);
		if(day_of_week > 7 || day_of_week < 1)
			print_lcd_buf_P(PSTR(" "));
		else
			print_lcd_buf_P((const char *)pgm_read_word(&List_names_day_of_week[day_of_week - 1]));
	}
}

void draw_work_time(struct time *t, uint8_t need_redraw) //Отрисовать время работы
{
	static char time[9];
	if(t != NULL)
		snprintf(time, sizeof(time), "%.2d:%.2d:%.2d", t -> hour, t -> min, t -> sec);

	if(need_redraw)
	{
		gotoxy_lcd_buf(12, 1);
		print_lcd_buf(time);
	}
}


void draw_center_pointer(void) // Отрисовать стрелочку в центре
{
	gotoxy_lcd_buf(9, 0);
	print_lcd_buf("->");
}

void draw_calculate_time(struct time *t, uint8_t need_redraw) //Отрисовать расчетное время
{
	static char time[12];
	if(t != NULL)
		snprintf(time, sizeof(time), "%.2d:%.2d:%.2d", t -> hour, t -> min, t -> sec);

	if(need_redraw)
	{
		gotoxy_lcd_buf(12, 0);
		print_lcd_buf(time);
	}
}

void draw_prog_name(const char *name, uint8_t need_redraw) //Отрисовать название программы
{
	static const char *prev_name = NULL;

	if(name == NULL && prev_name == NULL)
		return;

	if(name == NULL)
		name = prev_name;

	if(need_redraw)
	{
		gotoxy_lcd_buf(0, 1);
		print_lcd_buf_P(name);
	}
	prev_name = name;
}

void draw_step_name(const char *name, uint8_t step_num, uint8_t count_step, uint8_t need_redraw) //Отрисовать название и номер текущего шага выполнения
{
	char buffer[DISPLAY_WIDTH]; // Создаем буфер
	uint8_t name_len = 0;
	static const char *prev_name = NULL;
	static uint8_t prev_step_num = 0;
	static uint8_t prev_count_step = 0;

	if(name == NULL && prev_name == NULL)
		return;

	if(name == NULL) // Если передаем NULL то восстанвливаем отрисовываем предыдущие данные сохраненные в static
	{
		name = prev_name;
		step_num = prev_step_num;
		count_step = prev_count_step;
	}

	strcpy_P(buffer, name);
	name_len = strlen(buffer);

	snprintf(buffer + name_len, DISPLAY_WIDTH - name_len, ": %d/%d", step_num, count_step);

	if(need_redraw)
	{
		clear_line_buf(2);
		gotoxy_lcd_buf(get_center(strlen(buffer)), 2);
		print_lcd_buf(buffer);
	}

	prev_name = name;
	prev_step_num = step_num;
	prev_count_step = count_step;
}

void draw_operation_name(const char *name, uint8_t need_redraw) //Отрисовать название текущей операции
{
	static const char *prev_name = NULL;

	if(name == NULL && prev_name == NULL)
		return;

	if(name == NULL)
		name = prev_name;

	if(need_redraw)
	{
		clear_line_buf(3);
		gotoxy_lcd_buf(get_center(strlen_P(name)), 3);
		print_lcd_buf_P(name);
	}
	prev_name = name;
}

void draw_pause(uint8_t need_redraw) //Отрисовать текст паузы
{
	if(need_redraw)
	{
		clear_line_buf(3);
		gotoxy_lcd_buf(get_center(strlen_P(GET_MESS_POINTER(PAUSE_MESSAGE))), 3);
		print_lcd_buf_P(GET_MESS_POINTER(PAUSE_MESSAGE));
	}
}

void draw_close_door(uint8_t need_redraw) //Отрисовать текст ожидания закрытия люка
{
	if(need_redraw)
	{
		clear_line_buf(2);
		gotoxy_lcd_buf(get_center(strlen_P(GET_MESS_POINTER(PLEASE_CLOSE_DOOR))), 2);
		print_lcd_buf_P(GET_MESS_POINTER(PLEASE_CLOSE_DOOR));

		clear_line_buf(3);
		gotoxy_lcd_buf(get_center(strlen_P(GET_MESS_POINTER(NOW_WAITING))), 3);
		print_lcd_buf_P(GET_MESS_POINTER(NOW_WAITING));
	}
}

void triggered_menu_items(uint8_t *list, uint8_t count) // Инвертировать состояние элементов меню id которых перечисленны в массиве list
{
	uint8_t i;
	for(i = 0; i < count; i++)
		invert_menu_item(list[i]);
}


