#include <string.h>
#include <stdio.h>
#include "drivers.h"
#include "messages.h"
#include "../../lib/lib_liveos.h"
#include "menu.h"
#include "lib.h"
#include "os.h"
#include "lcd_buffer.h"
#include "../../lib/lib_menu.h" //�������� ���������� ��� ������ � ����

extern struct washing_settings Washing_settings; // �������� ������������ � ����
extern struct machine_settings Machine_settings; // ���������� ��������� ���������� ������

static PGM_P const PROGMEM Select_switch[] = {GET_MESS_POINTER(MENU_SELECT_SWITCH_1), GET_MESS_POINTER(MENU_SELECT_SWITCH_2)}; // �������� ������ ��������� ��� ����������
static PGM_P const PROGMEM Select_bedabble_time[] = {GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_1), GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_2), GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_3), GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_4), GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_5), GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_6)}; // �������� ������ ������� �����������
PGM_P const PROGMEM Select_washing_presetes[] = {GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_1), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_2), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_3), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_4), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_5), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_6), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_7), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_8)}; // ������� ���������� ������
static PGM_P const PROGMEM Select_washing_mode[] = {GET_MESS_POINTER(MENU_SELECT_WASHING_MODE_NORMAL), GET_MESS_POINTER(MENU_SELECT_WASHING_MODE_DELICATE)}; // ����� ������
static PGM_P const PROGMEM Select_temperatures[] = {GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_1), GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_2), GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_3), GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_4), GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_5), GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_6)}; // �������� ������ �����������




static void clear_screen(void) //��������� �������
{
    clr_lcd_buf();
}

void print_header(const char *header) // ���������� ��������� ����
{
	fill_line_buf(0, '-');

	gotoxy_lcd_buf(0, 0);
	putchar_lcd_buf(' ');

	gotoxy_lcd_buf(get_center(strlen_P(header)) - 1, 0);
	putchar_lcd_buf(' ');

    gotoxy_lcd_buf(get_center(strlen_P(header)), 0);
    print_lcd_buf_P(header);

    gotoxy_lcd_buf(get_center(strlen_P(header)) + strlen_P(header), 0);
	putchar_lcd_buf(' ');

	gotoxy_lcd_buf(DISPLAY_WIDTH - 1, 0);
	putchar_lcd_buf(' ');
}

// ���������� ������� ����
// num_row - ����� ������
// pointer_type - ��� ��������� �� ������
// name - ������ � �������
// is_active - ������� �������� ��� ���
static void print_item(uint8_t num_row, uint8_t pointer_type, void *name, uint8_t is_active)
{
    clear_line_buf(num_row + HEADER_COUNT_ROWS);

    if(!name)
        return;

	switch(pointer_type)
	{
		case PROGMEM_POINTER:
			if(is_active) // ���� ������� ���� ��������
			{
				gotoxy_lcd_buf(get_center(strlen_P(name)) - 2, num_row + HEADER_COUNT_ROWS);
				print_lcd_buf(">");
			}

			gotoxy_lcd_buf(get_center(strlen_P(name)), num_row + HEADER_COUNT_ROWS);
			print_lcd_buf_P(name);

			if(is_active)
			{
				gotoxy_lcd_buf(get_center(strlen_P(name)) + strlen_P(name) + 1, num_row + HEADER_COUNT_ROWS);
				print_lcd_buf("<");
			}
		break;

		case MEMORY_POINTER:
			if(is_active)
			{
				gotoxy_lcd_buf(get_center(strlen(name)) - 2, num_row + HEADER_COUNT_ROWS);
				print_lcd_buf(">");
			}

			gotoxy_lcd_buf(get_center(strlen(name)), num_row + HEADER_COUNT_ROWS);
			print_lcd_buf(name);

			if(is_active)
			{
				gotoxy_lcd_buf(get_center(strlen(name)) + strlen(name) + 1, num_row + HEADER_COUNT_ROWS);
				print_lcd_buf("<");
			}
		break;
	}
}

static void menu_run_process(uint8_t num) // ��������� ��� ��������� �������
{
    run_process(num, NULL);
}

static void menu_switch_process(uint8_t num) // ��������� ��� ������������ � ������ �������
{
    if(switch_process(num)) //���� ��������� ������������� � ��������� ������� �� ������������� � ������� �� ���������
		switch_process(DEFAULT_PROCESS);
}

static void change_access_to_washing(void) // ������� callback , ��������� ��������� ������ � ����������� �� �������
{
	Washing_settings.bedabble_water_soure = 1; // �������� ������� ��� �����������
	Washing_settings.prev_washing_water_soure = 1; // �������� ������� ��� ��������������� ������
	Washing_settings.washing_water_soure = 0; // �������� ������� ��� ������

	switch(Washing_settings.washing_preset) // � ����������� �� ���������� ������ ������, ������������� ����������� ��������� ��������� ������
	{
		case 0: // ������ 95
			Washing_settings.enable_prev_washing = 1; // ��������������� ������
			Washing_settings.washing_time_part1 = 40; // 30 ����� ����� ������ ���� ����
			Washing_settings.temperature_part1 = 3; // 60 �������� ���� ����
			Washing_settings.enable_washing_part2	= 1; //�������� ���� ���
			Washing_settings.washing_time_part2 = 30; // 30 ����� ����� ������ ���� ���
			Washing_settings.temperature_part2 = 5; // 95 �������� ���� ���
			Washing_settings.count_rinses = 4; // ���������� ����������
			Washing_settings.wring_speed = 1200; // �������� ������
			Washing_settings.wring_time = 7; // ����� ������ 7 �����
			Washing_settings.washing_mode = 0; // ����� ������� ������
			Washing_settings.washing_water_level = 5; //���������� ����
		break;

		case 1: // ������ 60
			Washing_settings.enable_prev_washing = 1;
			Washing_settings.washing_time_part1 = 60; // ����� ������
			Washing_settings.temperature_part1 = 3; // 60 ��������
			Washing_settings.enable_washing_part2	= 0; //��������� ���� ���
			Washing_settings.count_rinses = 4; // ���������� ����������
			Washing_settings.wring_speed = 1200; // �������� ������
			Washing_settings.wring_time = 7; // ����� ������
			Washing_settings.washing_mode = 0; // ����� ������� ������
			Washing_settings.washing_water_level = 4; //���������� ����
		break;

		case 2: // ��������� 70
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 20; // ����� ������
			Washing_settings.temperature_part1 = 2; // 40 ��������
			Washing_settings.enable_washing_part2	= 1; //�������� ���� ���
			Washing_settings.washing_time_part2 = 30; // 30 ����� ����� ������ ���� ���
			Washing_settings.temperature_part2 = 4; // 70 �������� ���� ���
			Washing_settings.count_rinses = 3; // ���������� ����������
			Washing_settings.wring_speed = 1000; // �������� ������
			Washing_settings.wring_time = 5; // ����� ������
			Washing_settings.washing_mode = 0; // ����� ������� ������
			Washing_settings.washing_water_level = 5; //���������� ����
		break;

		case 3: // ��������� 40
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 40; // ����� ������
			Washing_settings.temperature_part1 = 2; // 40 ��������
			Washing_settings.enable_washing_part2	= 0; //��������� ���� ���
			Washing_settings.count_rinses = 3; // ���������� ����������
			Washing_settings.wring_speed = 900; // �������� ������
			Washing_settings.wring_time = 5; // ����� ������
			Washing_settings.washing_mode = 0; // ����� ������� ������
			Washing_settings.washing_water_level = 4; //���������� ����
		break;

		case 4: // ������
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 30; // ����� ������
			Washing_settings.temperature_part1 = 2; // 40 ��������
			Washing_settings.enable_washing_part2	= 0; //��������� ���� ���
			Washing_settings.count_rinses = 3; // ���������� ����������
			Washing_settings.wring_speed = 1200; // �������� ������
			Washing_settings.wring_time = 7; // ����� ������
			Washing_settings.washing_mode = 0; // ����� ������� ������
			Washing_settings.washing_water_level = 4; //���������� ����
		break;

		case 5: // ��������
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 30; // ����� ������
			Washing_settings.temperature_part1 = 2; // 40 ��������
			Washing_settings.enable_washing_part2	= 0; //��������� ���� ���
			Washing_settings.count_rinses = 3; // ���������� ����������
			Washing_settings.wring_speed = 600; // �������� ������
			Washing_settings.wring_time = 5; // ����� ������
			Washing_settings.washing_mode = 1; // ����� ���������� ������
			Washing_settings.washing_water_level = 5; //���������� ����
		break;

		case 6: // ������
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 30; // ����� ������
			Washing_settings.temperature_part1 = 1; // 30 ��������
			Washing_settings.enable_washing_part2	= 0; //��������� ���� ���
			Washing_settings.count_rinses = 5; // ���������� ����������
			Washing_settings.wring_speed = 400; // �������� ������
			Washing_settings.wring_time = 3; // ����� ������
			Washing_settings.washing_mode = 1; // ����� ���������� ������
			Washing_settings.washing_water_level = 6; //���������� ����
		break;

		case 7: // �����
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 40; // ����� ������
			Washing_settings.temperature_part1 = 3; // 60 ��������
			Washing_settings.enable_washing_part2	= 0; //��������� ���� ���
			Washing_settings.count_rinses = 3; // ���������� ����������
			Washing_settings.wring_speed = 900; // �������� ������
			Washing_settings.wring_time = 5; // ����� ������
			Washing_settings.washing_mode = 0; // ����� ���������� ������
			Washing_settings.washing_water_level = 3; //���������� ����
		break;
	}

	Washing_settings.bedabble_water_soure = 2; // �������� ������� ��� �����������
	Washing_settings.prev_washing_water_soure = 2; // �������� ������� ��� ��������������� ������
	Washing_settings.washing_water_soure = 1; // �������� ������� ��� ������

	refresh_menu(1); //�������� ������ ����
}

static void change_access_to_bedabble(void) // ������� callback ��� ���������� ��� ������������� ����� ���� ����������� � �����������
{
	if(Washing_settings.bedabble_time) //���� ������� ����� �����������, �� �������� ����� "��������"
	{
		enable_menu_item(4);
		enable_menu_item(5);
	}
	else
	{
		disable_menu_item(4);
		disable_menu_item(5);
	}
}

static void bedabble_stop(void)
{
	send_signal(PROCESS_BEDABBLE, 5); // �������� ������ 5 �������� "�����������", ����� ��� �����������
}

static void washing_stop(void) // ������� ����������� �� ���� "���������� ������"
{
	send_signal(PROCESS_WASHING, 5); // �������� ������ 5 �������� "������", ����� ��� �����������
}

static void change_access_to_prev_washing(void)
{
	if(Washing_settings.washing_mode) //���� ������ ���������� ����� �� ��������� ����� ��������������� ������ ,� ���������� ��������� �
	{
		Washing_settings.enable_prev_washing = 0; //��������� ��������������� ������
		disable_menu_item(11);
	}
	else
		enable_menu_item(11);

}


static void change_access_prev_source_water(void)
{
	if(Washing_settings.enable_prev_washing) // ���� ��������� ��������������� ������, �� ���������� ����� ��������� ������� ����� ����� ����� �������
		enable_menu_item(13);
	else
		disable_menu_item(13);
}

static void change_access_washing_part2(void)
{
	if(Washing_settings.enable_washing_part2) // ���� ��������� ������ ���� ������ �� �������� ��� ��������� ��������� � ���
	{
		enable_menu_item(34);
		enable_menu_item(35);
	}
	else
	{
		disable_menu_item(34);
		disable_menu_item(35);
	}
}

static void rinse_stop(void) // ������� ����������� �� ���� "���������� ����������"
{
	send_signal(PROCESS_RINSE, 5); // �������� ������ 5 �������� "����������", ����� ��� �����������
}

static void wring_stop(void) // ������� ����������� �� ���� "���������� �����"
{
	printf("send_signal(PROCESS_WRING, 5)");
	send_signal(PROCESS_WRING, 5); // �������� ������ 5 �������� "����������", ����� ��� �����������
}


static struct menu_item Menu[] =
{
	{
		.id = 0, // �������� ������� ����
		.parent_id = 0,
		.name = GET_MESS_POINTER(MENU_ITEM_MAIN),  //����
		.type = GROUP,
		.visible = 1,
		.on_click = NULL,
	},
	{
				.id = 1,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_WASHING_PRESET),  // ����� ������
				.type = SELECT_PARAM,
				.value = &Washing_settings.washing_preset,
				.select_variants = Select_washing_presetes,
				.count = sizeof(Select_washing_presetes) / sizeof(PGM_P),
				.visible = 1,
				.on_click = change_access_to_washing,
	},
	{
				.id = 2,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_WASHING_BEDABBLE),  // �����������
				.type = GROUP,
				.visible = 1,
				.on_click = NULL,
	},
	{
						.id = 3,
						.parent_id = 2,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_BEDABBLE_TIME),  // ����� ������� �����������
						.type = SELECT_PARAM,
						.value = &Washing_settings.bedabble_time,
						.select_variants = Select_bedabble_time,
						.count = sizeof(Select_bedabble_time) / sizeof(PGM_P),
						.visible = 1,
						.on_click = change_access_to_bedabble,
	},
	{
						.id = 4,
						.parent_id = 2,
						.name = GET_MESS_POINTER(MENU_ITEM_WATER_SOURCE),  // �������� ������� (��� �����������)
						.type = CHANGE_NUMBER,
						.value = &Washing_settings.bedabble_water_soure,
						.min = 1,
						.count = 2,
						.step = 1,
						.postfix = "�����",
						.visible = 0,
						.on_click = NULL,
	},
	{
						.id = 5,
						.parent_id = 2,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_BEDABBLE_START_NOW),  // �������� ������
						.type = RUN_PROCESS,
						.process_id = PROCESS_BEDABBLE,
						.visible = 0,
						.on_click = NULL,
	},
	{
						.id = 6,
						.parent_id = 2,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_BEDABBLE_STOP_NOW),  // ���������� �����������
						.type = RUN_FUNCTION,
						.visible = 0,
						.on_click = bedabble_stop,
	},
	{
				.id = 9,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_WASHING_PARAMS),  // ��������� ������
				.type = GROUP,
				.visible = 1,
				.on_click = NULL,
	},
	{
						.id = 10,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_MODE),  // ����� ������
						.type = SELECT_PARAM,
						.value = &Washing_settings.washing_mode,
						.select_variants = Select_washing_mode,
						.count = sizeof(Select_switch) / sizeof(PGM_P),
						.visible = 1,
						.on_click = change_access_to_prev_washing,
	},
	{
						.id = 11,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_PREVIOUS_WASHING),  // ��������������� ������
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 12,
								.parent_id = 11,
								.name = GET_MESS_POINTER(MENU_ITEM_SWITCH_TEXT),  // ���./����.
								.type = SELECT_PARAM,
								.value = &Washing_settings.enable_prev_washing,
								.select_variants = Select_switch,
								.count = sizeof(Select_switch) / sizeof(PGM_P),
								.visible = 1,
								.on_click = change_access_prev_source_water,
	},
	{
								.id = 13,
								.parent_id = 11,
								.name = GET_MESS_POINTER(MENU_ITEM_WATER_SOURCE),  // �������� ������� (��� ��������������� ������)
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.prev_washing_water_soure,
								.min = 1,
								.count = 2,
								.step = 1,
								.postfix = "�����",
								.visible = 0,
								.on_click = NULL,
	},
	{
						.id = 31,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_PART1),  // 1�� ���� ������
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 14,
								.parent_id = 31,
								.name = GET_MESS_POINTER(MENU_ITEM_TEMPERATURE),  // �����������
								.type = SELECT_PARAM,
								.value = &Washing_settings.temperature_part1,
								.select_variants = Select_temperatures,
								.count = sizeof(Select_temperatures) / sizeof(PGM_P),
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 15,
								.parent_id = 31,
								.name = GET_MESS_POINTER(MENU_ITEM_WASHING_TIME),  // ����� ������
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.washing_time_part1,
								.min = 5,
								.count = 10,
								.step = 5,
								.postfix = "���.",
								.visible = 1,
								.on_click = NULL,
	},
	{
						.id = 32,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_PART2),  // 2�� ���� ������
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 33,
								.parent_id = 32,
								.name = GET_MESS_POINTER(MENU_ITEM_SWITCH_TEXT),  // ���./����.
								.type = SELECT_PARAM,
								.value = &Washing_settings.enable_washing_part2,
								.select_variants = Select_switch,
								.count = sizeof(Select_switch) / sizeof(PGM_P),
								.visible = 1,
								.on_click = change_access_washing_part2,
	},
	{
								.id = 34,
								.parent_id = 32,
								.name = GET_MESS_POINTER(MENU_ITEM_TEMPERATURE),  // �����������
								.type = SELECT_PARAM,
								.value = &Washing_settings.temperature_part2,
								.select_variants = Select_temperatures,
								.count = sizeof(Select_temperatures) / sizeof(PGM_P),
								.visible = 0,
								.on_click = NULL,
	},
	{
								.id = 35,
								.parent_id = 32,
								.name = GET_MESS_POINTER(MENU_ITEM_WASHING_TIME),  // ����� ������
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.washing_time_part2,
								.min = 5,
								.count = 10,
								.step = 5,
								.postfix = "���.",
								.visible = 0,
								.on_click = NULL,
	},
	{
						.id = 16,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WATER_SOURCE),  // �������� ������� (��� �������� ������)
						.type = CHANGE_NUMBER,
						.value = &Washing_settings.washing_water_soure,
						.min = 1,
						.count = 2,
						.step = 1,
						.postfix = "�����",
						.visible = 1,
						.on_click = NULL,
	},
	{
						.id = 17,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_WATER_LEVEL),  // ���-�� ����
						.type = CHANGE_NUMBER,
						.value = &Washing_settings.washing_water_level,
						.min = 1,
						.count = 7,
						.step = 1,
						.postfix = "�������",
						.visible = 1,
						.on_click = NULL,
	},
	{
						.id = 18,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_COUNT_RINSE),  // ���-�� ����������
						.type = CHANGE_NUMBER,
						.value = &Washing_settings.count_rinses,
						.min = 3,
						.count = 6,
						.step = 1,
						.postfix = NULL,
						.visible = 1,
						.on_click = NULL,
	},
	{
						.id = 19,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WRING),  // �����
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 20,
								.parent_id = 19,
								.name = GET_MESS_POINTER(MENU_ITEM_WRING_SPEED),  // ������������ ��������
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.wring_speed,
								.min = 200,
								.count = 10,
								.step = 100,
								.postfix = NULL,
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 21,
								.parent_id = 19,
								.name = GET_MESS_POINTER(MENU_ITEM_WRING_TIME),  // ����� ������
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.wring_time,
								.min = 2,
								.count = 5,
								.step = 2,
								.postfix = "���.",
								.visible = 1,
								.on_click = NULL,
	},
	{
						.id = 22,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_SWILL_SIGNAL),  // ������ �������������
						.type = SELECT_PARAM,
						.value = &Washing_settings.signal_swill,
						.select_variants = Select_switch,
						.count = sizeof(Select_switch) / sizeof(PGM_P),
						.visible = 1,
						.on_click = NULL,
	},
	{
				.id = 7,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_RUN_WASHING),  // ������ ������
				.type = RUN_PROCESS,
				.process_id = PROCESS_WASHING,
				.visible = 1,
				.on_click = NULL,
	},
	{
				.id = 8,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_STOP_WASHING),  // ���������� ������
				.type = RUN_FUNCTION,
				.visible = 0,
				.on_click = washing_stop,
	},
	{
				.id = 23,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_OPERATIONS),  // ������ ����������
				.type = GROUP,
				.visible = 1,
				.on_click = NULL,
	},
	{
						.id = 24,
						.parent_id = 23,
						.name = GET_MESS_POINTER(MENU_ITEM_WATER_CONTROL),  // ���������� �����
						.type = RUN_PROCESS,
						.process_id = PROCESS_WATER_CONTROL,
						.visible = 1,
						.on_click = NULL,
	},
	{
						.id = 25,
						.parent_id = 23,
						.name = GET_MESS_POINTER(RINSE_MESSAGE), // ����������
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 36,
								.parent_id = 25,
								.name = GET_MESS_POINTER(MENU_ITEM_COUNT_RINSE),  // ���-�� ����������
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.rinse_count,
								.min = 1,
								.count = 5,
								.step = 1,
								.postfix = NULL,
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 37,
								.parent_id = 25,
								.name = GET_MESS_POINTER(MENU_ITEM_RINSE_MODE),  // ����� ����������
								.type = SELECT_PARAM,
								.value = &Washing_settings.rinse_mode,
								.select_variants = Select_washing_mode,
								.count = sizeof(Select_switch) / sizeof(PGM_P),
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 45,
								.parent_id = 25,
								.name = GET_MESS_POINTER(MENU_ITEM_WATER_SOURCE),  // �������� ����
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.rinse_water_soure,
								.min = 1,
								.count = 2,
								.step = 1,
								.postfix = "�����",
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 38,
								.parent_id = 25,
								.name = GET_MESS_POINTER(MENU_ITEM_RUN_RINSE),  // ���������
								.type = RUN_PROCESS,
								.process_id = PROCESS_RINSE,
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 39,
								.parent_id = 25,
								.name = GET_MESS_POINTER(MENU_ITEM_STOP_RINSE),  // ���������� ����������
								.type = RUN_FUNCTION,
								.visible = 0,
								.on_click = rinse_stop,
	},
	{
						.id = 40,
						.parent_id = 23,
						.name = GET_MESS_POINTER(MENU_ITEM_WRING), // �����
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 41,
								.parent_id = 40,
								.name = GET_MESS_POINTER(MENU_ITEM_WRING_SPEED),  // ������������ ��������
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.wring_wring_speed,
								.min = 200,
								.count = 10,
								.step = 100,
								.postfix = NULL,
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 42,
								.parent_id = 40,
								.name = GET_MESS_POINTER(MENU_ITEM_WRING_TIME),  // ����� ������
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.wring_wring_time,
								.min = 2,
								.count = 5,
								.step = 2,
								.postfix = "���.",
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 43,
								.parent_id = 40,
								.name = GET_MESS_POINTER(MENU_ITEM_RUN_WRING),  // ������
								.type = RUN_PROCESS,
								.process_id = PROCESS_WRING,
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 44,
								.parent_id = 40,
								.name = GET_MESS_POINTER(MENU_ITEM_STOP_WRING),  // ���������� �����
								.type = RUN_FUNCTION,
								.visible = 0,
								.on_click = wring_stop,
	},
	{
				.id = 27,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_SETTINGS),  // ����� ���������
				.type = GROUP,
				.visible = 1,
				.on_click = NULL,
	},
	{
						.id = 28,
						.parent_id = 27,
						.name = GET_MESS_POINTER(MENU_ITEM_SETTINGS_SOUND),  // ����
						.type = SELECT_PARAM,
						.value = &Machine_settings.sound,
						.select_variants = Select_switch,
						.count = sizeof(Select_switch) / sizeof(PGM_P),
						.visible = 1,
						.on_click = save_all_settings,
	},
	{
						.id = 29,
						.parent_id = 27,
						.name = GET_MESS_POINTER(SET_TIME_MENU_ITEM),  // ��������� �������
						.type = RUN_PROCESS,
						.process_id = PROCESS_SET_TIME,
						.visible = 1,
						.on_click = save_all_settings,
	},
};


struct lib_menu Lib_menu = //������� ������ ���������������� lib_menu
{
	.tree = Menu, // ������ ����
	.count_items = (sizeof(Menu) / sizeof(struct menu_item)), // ���������� ��������� � ������ ����
	.display_width = DISPLAY_WIDTH, //������ ������ � ��������
	.max_width_str = 18, // ������������ ������ ������ � ����
	.middle_row_num = ((DISPLAY_HEIGHT - HEADER_COUNT_ROWS - 1) / 2), // ����� ������ ��� ��������� ��������
	.count_rows = (DISPLAY_HEIGHT - HEADER_COUNT_ROWS), // ���������� ����� ��������� ��� ����
	.clear_screen = clear_screen, // ������� ��� �������� ������
	.print_header = print_header, // ������� ��� ��������� ���������� ����
	.print_item = print_item, // ���������� ������� ����
	.run_process = menu_run_process, //������� ��������� �������
//	.switch_process = menu_switch_process, //������� ����������� �������� �������
};
