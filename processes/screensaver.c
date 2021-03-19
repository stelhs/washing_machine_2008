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

extern struct time Main_current_time; // ���� ��������� �������
extern struct machine_states Machine_states; // ��������� ���� ������
extern struct machine_settings Machine_settings; // ���������� ��������� ���������� ������

struct
{
	uint8_t need_result_draw : 1; // ���� ��� ��������� � 1 , ������ ���� ���������� ���������� ������ ��� �����������
	uint8_t disable_draw_date : 1; // ���� ������ ��� ��������� � 1 �� ���� �� ��������������
	uint16_t shutdown_timer : 14; //������ �������������� ������ ��� ��������� ������� ������
}Screensaver_state;



static void draw_welcom(void) //���������� �����������
{
	const char *str1, *str2;
	static uint8_t welcom_message_num = 0;

	switch(welcom_message_num)
	{
		case 0:
			str1 = PSTR("���-�� �����");
			str2 = PSTR("�������?");
		break;

		case 1:
			str1 = PSTR("��� �� ���� ���");
			str2 = PSTR("�� ���� ����?");
		break;

		case 2:
			str1 = PSTR("��� �����");
			str2 = PSTR("������?");
		break;

		case 3:
			str1 = PSTR("����� �������, ��?");
			str2 = PSTR("");
		break;

		case 4:
			str1 = PSTR("��, � ������ ����");
			str2 = PSTR("���-�� ���������!");
		break;

		case 5:
			str1 = PSTR("����� � ������");
			str2 = PSTR("��� ����� �������?");
		break;

		case 6:
			str1 = PSTR("���! �������-�� ��");
			str2 = PSTR("����� �������!");
		break;
	}

	clear_line_buf(2);
	clear_line_buf(3);
	gotoxy_lcd_buf(get_center(strlen_P(str1)), 2); print_lcd_buf_P(str1);
	gotoxy_lcd_buf(get_center(strlen_P(str2)), 3); print_lcd_buf_P(str2);
	welcom_message_num++; // ����������� ����� ���������� ���������� ���������

	if(welcom_message_num > 6) // ���� ����� �� ���������� ��������� �� �������� �������
		welcom_message_num = 0;

	Screensaver_state.disable_draw_date = 1;
}


CALLBACK_NAME(PROCESS_SCREENSAVER, init)(void) // ������������� �������� ������
{
	Screensaver_state.need_result_draw = 0;
	Screensaver_state.disable_draw_date = 0;
}

CALLBACK_NAME(PROCESS_SCREENSAVER, run)(char *params) // ������ ��������
{
}

CALLBACK_NAME(PROCESS_SCREENSAVER, active)(struct task_info *from)
{
	clr_lcd_buf();
	Screensaver_state.disable_draw_date = 0;
	Screensaver_state.shutdown_timer = AUTO_POWER_OFF_TIMEOUT;

	draw_current_time(&Main_current_time, 1);
	if(Screensaver_state.need_result_draw) // ���� ����� ���������� ���������� ������
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
		Machine_states.current_machine_state = LED_OFF; // ��������� ��� ����������
	}
}

CALLBACK_NAME(PROCESS_SCREENSAVER, main_loop)(void)
{
	if(Machine_states.change_current_time) // ���� ���������� �����, �� �������������� ������� �����
	{
		Machine_states.change_current_time = 0;
		draw_current_time(&Main_current_time, 1);
		if(!Screensaver_state.disable_draw_date)
			draw_current_date(&Main_current_time, 1);
	}

	if(Screensaver_state.shutdown_timer == 1) //���� ������ ����� ���������� �������, �� ��������� ������
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
		switch_process(PROCESS_MENU); //���� � ����
}

CALLBACK_NAME(PROCESS_SCREENSAVER, click_down)(void)
{
	Screensaver_state.shutdown_timer = 0;
	if(Machine_states.main_power)
		switch_process(PROCESS_MENU); //���� � ����
}

CALLBACK_NAME(PROCESS_SCREENSAVER, click_enter)(void)
{
	Screensaver_state.shutdown_timer = 0;
	if(Machine_states.main_power)
		switch_process(PROCESS_MENU); //���� � ����
}

CALLBACK_NAME(PROCESS_SCREENSAVER, long_enter)(void) // ��� ������ ����������� �������� ��� ��������� ���������� ������
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
		switch_process(PROCESS_MENU); //���� � ����
}

int t=0;
int t1=0;
int t2=0;

CALLBACK_NAME(PROCESS_SCREENSAVER, console)(char input_byte)
{
	switch(input_byte)
	{
		case '7':
			switch_process(PROCESS_MENU); //���� � ����
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

CALLBACK_NAME(PROCESS_SCREENSAVER, signal)(uint8_t signal) // ���������� ��������� �������
{
	switch(signal)
	{
		case 3:  // ������ ���������� ��� ���� ������� ���������� ������
			Screensaver_state.need_result_draw = 1;
			if(Machine_settings.sound)
				turn_blink_speaker(ON, 1000, 300, SPEAKER_SOUND_FREQ, 3); //����� ��� ����
		break;
	}
}

CALLBACK_NAME(PROCESS_SCREENSAVER, timer0)(void)
{
	if(!Machine_states.main_power) // ���� ������ ����������
	{
		Screensaver_state.shutdown_timer = 0;
		return;
	}

	//���� ��������� �������� ��������� �� �������������� ������� ����������
	if(	get_process_status(PROCESS_WASHING) == RUNNING
		|| get_process_status(PROCESS_BEDABBLE) == RUNNING
		|| get_process_status(PROCESS_RINSE) == RUNNING
		|| get_process_status(PROCESS_WRING) == RUNNING)
	{
		return;
	}

	if(Screensaver_state.shutdown_timer > 1) //������ ��� ��������� ������� �������������� �������
		Screensaver_state.shutdown_timer --;
}

CALLBACK_NAME(PROCESS_SCREENSAVER, stop)(void)
{
	clr_lcd_buf();
}

