#include <stdio.h>
#include "../drivers.h"
#include "../lib.h"
#include "../messages.h"
#include "../os.h"
#include "../../../lib/lib_liveos.h"
#include "../config.h"
#include "config.h"
#include "../lcd_buffer.h"
#include "proc_lib.h"

extern struct machine_states Machine_states; // Состояние всей машины
extern struct time Main_current_time; // Часы реального времени

static PGM_P const PROGMEM Water_control_operations[] = {GET_MESS_POINTER(SOURCE1_MESSAGE), GET_MESS_POINTER(SOURCE2_MESSAGE), GET_MESS_POINTER(SOURCE3_MESSAGE), GET_MESS_POINTER(POUR_OUT_MESSAGE)};

struct
{
	uint16_t time_counter; // Счетчик времени
	uint16_t key_up_delay; // Счетчик времени отпускания кнопки
	uint8_t state_blink : 1; // Мигалка то (1 то 0) с периодичностью BLINK_PRESSED_BUTTON_SPEED
	uint8_t prev_state_blink : 1;
	uint8_t need_redraw : 1; // Если 1 то нужно перерисовать экран
	uint8_t source1 : 1; // Состояние первого источника воды
	uint8_t source2 : 1; // Состояние второго источника воды
	uint8_t source3 : 1; // Состояние третьего источника воды
	uint8_t pour_out : 1; // Состояние слива воды
	uint8_t water_type : 1; //Тип воды, горячая или холодная
}Water_control_states;

static void draw_water_control(void) //Отрисовать название "Управление водой"
{
	gotoxy_lcd_buf(0, 1); print_lcd_buf_P(GET_MESS_POINTER(CONTROL_MESSAGE)); // "Управление"

	gotoxy_lcd_buf(0, 2); print_lcd_buf("         ");

	gotoxy_lcd_buf(1, 2);
	if(Water_control_states.water_type)
		print_lcd_buf_P(GET_MESS_POINTER(BY_HOT_MESSAGE)); // "горячей"
	else
		print_lcd_buf_P(GET_MESS_POINTER(BY_COLD_MESSAGE)); // "холодной"

	gotoxy_lcd_buf(0, 3); print_lcd_buf("         ");
	gotoxy_lcd_buf(2, 3); print_lcd_buf_P(GET_MESS_POINTER(BY_WATER_MESSAGE)); // "водой"
}

static void draw_button_action(uint8_t button, uint8_t visible) // Отрисовать строчку с сответсвующие описание указанной кнопки
{
	char buffer[11];

	snprintf(buffer, sizeof(buffer) - 1, "%d-       ", button);
	gotoxy_lcd_buf(11, button - 1); print_lcd_buf(buffer);

	if(visible)
		gotoxy_lcd_buf(13, button - 1); print_lcd_buf_P((const char *)pgm_read_word(&Water_control_operations[button - 1]));
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, init)(void) // Инициализация процесса полоскания
{
	Water_control_states.source1 = 0;
	Water_control_states.source2 = 0;
	Water_control_states.source3 = 0;
	Water_control_states.pour_out = 0;
	Water_control_states.water_type = 0;
	Water_control_states.need_redraw = 1;
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, run)(char *params)
{
	Water_control_states.time_counter = BLINK_PRESSED_BUTTON_SPEED;
	power_on_system(); //Включаем основное питание системы
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, active)(struct task_info *from)
{
	clr_lcd_buf();
	draw_water_control();
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, main_loop)(void)
{
	uint8_t i;
	if(Machine_states.change_current_time) // Отрисовываем время
	{
		draw_current_time(&Main_current_time, 1);
		Machine_states.change_current_time = 0;
	}

	if(Water_control_states.state_blink != Water_control_states.prev_state_blink)
	{
		if(Water_control_states.source1)
			draw_button_action(1, Water_control_states.state_blink);

		if(Water_control_states.source2)
			draw_button_action(2, Water_control_states.state_blink);

		if(Water_control_states.source3)
			draw_button_action(3, Water_control_states.state_blink);

		if(Water_control_states.pour_out)
			draw_button_action(4, Water_control_states.state_blink);

		Water_control_states.prev_state_blink = Water_control_states.state_blink;
	}

	if(Water_control_states.need_redraw) //Если надо перерисовать
	{
		Water_control_states.need_redraw = 0;
		for(i = 1; i < 5; i++)
			draw_button_action(i, 1);
	}

	if(Water_control_states.source1)
		turn_water_source(WATER_SOURCE1, ON);

	if(Water_control_states.source2)
		turn_water_source(WATER_SOURCE2, ON);

	if(Water_control_states.source3)
		turn_water_source(WATER_SOURCE3, ON);

	if(Water_control_states.pour_out)
		turn_pump(ON);
	else
		turn_pump(OFF);

	if(!(Water_control_states.source1 || Water_control_states.source2 || Water_control_states.source3))
		turn_water_source(0, 0);

}

CALLBACK_NAME(PROCESS_WATER_CONTROL, click_enter)(void) //Если нажали ENTER то переключаем режим управления горячей или холодной водой
{
	Water_control_states.water_type = !Water_control_states.water_type;
	draw_water_control();
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, press_ext1)(void)
{
	Water_control_states.key_up_delay = KEY_UP_DELAY;
	Water_control_states.source1 = 1;
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, press_ext2)(void)
{
	Water_control_states.key_up_delay = KEY_UP_DELAY;
	Water_control_states.source2 = 1;
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, press_ext3)(void)
{
	Water_control_states.key_up_delay = KEY_UP_DELAY;
	Water_control_states.source3 = 1;
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, press_ext4)(void)
{
	Water_control_states.key_up_delay = KEY_UP_DELAY;
	Water_control_states.pour_out = 1;
}


CALLBACK_NAME(PROCESS_WATER_CONTROL, click_esc)(void)
{
	stop_process(PROCESS_WATER_CONTROL); // завершаем работу
	switch_process(PROCESS_MENU); // вход в меню
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, timer2)(void)
{
	if(Water_control_states.time_counter > 1)
		Water_control_states.time_counter--;
	else
	{
		Water_control_states.state_blink = !Water_control_states.state_blink;
		Water_control_states.time_counter = BLINK_PRESSED_BUTTON_SPEED;
	}

	if(Water_control_states.key_up_delay > 1)
		Water_control_states.key_up_delay--;
	else	// Если истекло время после отпускания любой кнопки
	{
		Water_control_states.source1 = 0;
		Water_control_states.source2 = 0;
		Water_control_states.source3 = 0;
		Water_control_states.pour_out = 0;
		Water_control_states.key_up_delay = 0;
		Water_control_states.need_redraw = 1;
	}
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, stop)(void)
{
	turn_water_source(0, 0);
	turn_pump(OFF);
}

CALLBACK_NAME(PROCESS_WATER_CONTROL, console)(char ch)
{
/*	switch(ch)
	{
		case '1':
			printf("Water_control_states.key_up_delay = %d\r\n", Water_control_states.key_up_delay);
			printf("Water_control_states.time_counter = %d\r\n", Water_control_states.time_counter);
			printf("Water_control_states.pour_out = %d\r\n", Water_control_states.pour_out);
		break;
	}*/
}


