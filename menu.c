#include <string.h>
#include <stdio.h>
#include "drivers.h"
#include "messages.h"
#include "../../lib/lib_liveos.h"
#include "menu.h"
#include "lib.h"
#include "os.h"
#include "lcd_buffer.h"
#include "../../lib/lib_menu.h" //Описание библиотеки для работы с меню

extern struct washing_settings Washing_settings; // Нстройки регулируемые с меню
extern struct machine_settings Machine_settings; // Глобальные настройки стиральной машины

static PGM_P const PROGMEM Select_switch[] = {GET_MESS_POINTER(MENU_SELECT_SWITCH_1), GET_MESS_POINTER(MENU_SELECT_SWITCH_2)}; // Варианты выбора включенно или выключенно
static PGM_P const PROGMEM Select_bedabble_time[] = {GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_1), GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_2), GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_3), GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_4), GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_5), GET_MESS_POINTER(MENU_SELECT_BEDABBLE_TIME_6)}; // Варианты выбора времени замачивания
PGM_P const PROGMEM Select_washing_presetes[] = {GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_1), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_2), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_3), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_4), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_5), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_6), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_7), GET_MESS_POINTER(MENU_SELECT_WASHING_PRESET_8)}; // Пресеты параметров стирки
static PGM_P const PROGMEM Select_washing_mode[] = {GET_MESS_POINTER(MENU_SELECT_WASHING_MODE_NORMAL), GET_MESS_POINTER(MENU_SELECT_WASHING_MODE_DELICATE)}; // Режим стирки
static PGM_P const PROGMEM Select_temperatures[] = {GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_1), GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_2), GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_3), GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_4), GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_5), GET_MESS_POINTER(MENU_SELECT_TEMPERATURE_6)}; // Варианты выбора температуры




static void clear_screen(void) //Отчистить дисплей
{
    clr_lcd_buf();
}

void print_header(const char *header) // Отрисовать заголовок меню
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

// Отрисовать элемент меню
// num_row - Номер строки
// pointer_type - тип указателя на строку
// name - строка с текстом
// is_active - Элемент активный или нет
static void print_item(uint8_t num_row, uint8_t pointer_type, void *name, uint8_t is_active)
{
    clear_line_buf(num_row + HEADER_COUNT_ROWS);

    if(!name)
        return;

	switch(pointer_type)
	{
		case PROGMEM_POINTER:
			if(is_active) // Если элемент меню активный
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

static void menu_run_process(uint8_t num) // Описываем как запустить процесс
{
    run_process(num, NULL);
}

static void menu_switch_process(uint8_t num) // Описываем как переключится в другой процесс
{
    if(switch_process(num)) //Если неужалось переключиться в указанный процесс то переключаемся в процесс по умолчанию
		switch_process(DEFAULT_PROCESS);
}

static void change_access_to_washing(void) // Функция callback , подбирает параметры стирки в зависимости от пресета
{
	Washing_settings.bedabble_water_soure = 1; // Источник порошка для замачивания
	Washing_settings.prev_washing_water_soure = 1; // Источник порошка для предварительной стирки
	Washing_settings.washing_water_soure = 0; // Источник порошка для стирки

	switch(Washing_settings.washing_preset) // В зависимости от выбранного режима стирки, автоматически настраиваем различные параметры стирки
	{
		case 0: // Хлопок 95
			Washing_settings.enable_prev_washing = 1; // Предварительную стирку
			Washing_settings.washing_time_part1 = 40; // 30 минут время стирки фазы один
			Washing_settings.temperature_part1 = 3; // 60 градусов фазы один
			Washing_settings.enable_washing_part2	= 1; //Включаем фазу два
			Washing_settings.washing_time_part2 = 30; // 30 минут время стирки фазы два
			Washing_settings.temperature_part2 = 5; // 95 градусов фазы два
			Washing_settings.count_rinses = 4; // Количество полосканий
			Washing_settings.wring_speed = 1200; // Скорость отжима
			Washing_settings.wring_time = 7; // Время отжима 7 минут
			Washing_settings.washing_mode = 0; // Режим обычной стирки
			Washing_settings.washing_water_level = 5; //Количество воды
		break;

		case 1: // Хлопок 60
			Washing_settings.enable_prev_washing = 1;
			Washing_settings.washing_time_part1 = 60; // время стирки
			Washing_settings.temperature_part1 = 3; // 60 градусов
			Washing_settings.enable_washing_part2	= 0; //Выключаем фазу два
			Washing_settings.count_rinses = 4; // Количество полосканий
			Washing_settings.wring_speed = 1200; // Скорость отжима
			Washing_settings.wring_time = 7; // Время отжима
			Washing_settings.washing_mode = 0; // Режим обычной стирки
			Washing_settings.washing_water_level = 4; //Количество воды
		break;

		case 2: // Синтетика 70
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 20; // время стирки
			Washing_settings.temperature_part1 = 2; // 40 градусов
			Washing_settings.enable_washing_part2	= 1; //Включаем фазу два
			Washing_settings.washing_time_part2 = 30; // 30 минут время стирки фазы два
			Washing_settings.temperature_part2 = 4; // 70 градусов фазы два
			Washing_settings.count_rinses = 3; // Количество полосканий
			Washing_settings.wring_speed = 1000; // Скорость отжима
			Washing_settings.wring_time = 5; // Время отжима
			Washing_settings.washing_mode = 0; // Режим обычной стирки
			Washing_settings.washing_water_level = 5; //Количество воды
		break;

		case 3: // Синтетика 40
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 40; // время стирки
			Washing_settings.temperature_part1 = 2; // 40 градусов
			Washing_settings.enable_washing_part2	= 0; //Выключаем фазу два
			Washing_settings.count_rinses = 3; // Количество полосканий
			Washing_settings.wring_speed = 900; // Скорость отжима
			Washing_settings.wring_time = 5; // Время отжима
			Washing_settings.washing_mode = 0; // Режим обычной стирки
			Washing_settings.washing_water_level = 4; //Количество воды
		break;

		case 4: // Джинсы
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 30; // время стирки
			Washing_settings.temperature_part1 = 2; // 40 градусов
			Washing_settings.enable_washing_part2	= 0; //Выключаем фазу два
			Washing_settings.count_rinses = 3; // Количество полосканий
			Washing_settings.wring_speed = 1200; // Скорость отжима
			Washing_settings.wring_time = 7; // Время отжима
			Washing_settings.washing_mode = 0; // Режим обычной стирки
			Washing_settings.washing_water_level = 4; //Количество воды
		break;

		case 5: // Бережная
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 30; // время стирки
			Washing_settings.temperature_part1 = 2; // 40 градусов
			Washing_settings.enable_washing_part2	= 0; //Выключаем фазу два
			Washing_settings.count_rinses = 3; // Количество полосканий
			Washing_settings.wring_speed = 600; // Скорость отжима
			Washing_settings.wring_time = 5; // Время отжима
			Washing_settings.washing_mode = 1; // Режим деликатной стирки
			Washing_settings.washing_water_level = 5; //Количество воды
		break;

		case 6: // Шерсть
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 30; // время стирки
			Washing_settings.temperature_part1 = 1; // 30 градусов
			Washing_settings.enable_washing_part2	= 0; //Выключаем фазу два
			Washing_settings.count_rinses = 5; // Количество полосканий
			Washing_settings.wring_speed = 400; // Скорость отжима
			Washing_settings.wring_time = 3; // Время отжима
			Washing_settings.washing_mode = 1; // Режим деликатной стирки
			Washing_settings.washing_water_level = 6; //Количество воды
		break;

		case 7: // Носки
			Washing_settings.enable_prev_washing = 0;
			Washing_settings.washing_time_part1 = 40; // время стирки
			Washing_settings.temperature_part1 = 3; // 60 градусов
			Washing_settings.enable_washing_part2	= 0; //Выключаем фазу два
			Washing_settings.count_rinses = 3; // Количество полосканий
			Washing_settings.wring_speed = 900; // Скорость отжима
			Washing_settings.wring_time = 5; // Время отжима
			Washing_settings.washing_mode = 0; // Режим деликатной стирки
			Washing_settings.washing_water_level = 3; //Количество воды
		break;
	}

	Washing_settings.bedabble_water_soure = 2; // Источник порошка для замачивания
	Washing_settings.prev_washing_water_soure = 2; // Источник порошка для предварительной стирки
	Washing_settings.washing_water_soure = 1; // Источник порошка для стирки

	refresh_menu(1); //Обновить дерево меню
}

static void change_access_to_bedabble(void) // Функция callback для блокировки или разблокировки веток меню относящихся к замачиванию
{
	if(Washing_settings.bedabble_time) //Если выбрано время замачивания, то включаем пункт "замочить"
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
	send_signal(PROCESS_BEDABBLE, 5); // Посылаем сигнал 5 процессу "замачивание", чтобы тот остановился
}

static void washing_stop(void) // Функция запускается из меню "остановить стирку"
{
	send_signal(PROCESS_WASHING, 5); // Посылаем сигнал 5 процессу "стирка", чтобы тот остановился
}

static void change_access_to_prev_washing(void)
{
	if(Washing_settings.washing_mode) //Если выбран деликатный режим то блокируем выбор предварительной стирки ,и разумеется выключаем её
	{
		Washing_settings.enable_prev_washing = 0; //Выключаем предварительную стирку
		disable_menu_item(11);
	}
	else
		enable_menu_item(11);

}


static void change_access_prev_source_water(void)
{
	if(Washing_settings.enable_prev_washing) // Если включенна предварительная стирка, то предлогаем выбор источника порошка иначе такой выбор убираем
		enable_menu_item(13);
	else
		disable_menu_item(13);
}

static void change_access_washing_part2(void)
{
	if(Washing_settings.enable_washing_part2) // Если включенна вторая фаза стирки то включаем все настройки связанные с ней
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

static void rinse_stop(void) // Функция запускается из меню "остановить полоскание"
{
	send_signal(PROCESS_RINSE, 5); // Посылаем сигнал 5 процессу "полоскание", чтобы тот остановился
}

static void wring_stop(void) // Функция запускается из меню "остановить отжим"
{
	printf("send_signal(PROCESS_WRING, 5)");
	send_signal(PROCESS_WRING, 5); // Посылаем сигнал 5 процессу "полоскание", чтобы тот остановился
}


static struct menu_item Menu[] =
{
	{
		.id = 0, // Корневой элемент меню
		.parent_id = 0,
		.name = GET_MESS_POINTER(MENU_ITEM_MAIN),  //Меню
		.type = GROUP,
		.visible = 1,
		.on_click = NULL,
	},
	{
				.id = 1,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_WASHING_PRESET),  // Метод стирки
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
				.name = GET_MESS_POINTER(MENU_ITEM_WASHING_BEDABBLE),  // Замачивание
				.type = GROUP,
				.visible = 1,
				.on_click = NULL,
	},
	{
						.id = 3,
						.parent_id = 2,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_BEDABBLE_TIME),  // Выбор времени замачивания
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
						.name = GET_MESS_POINTER(MENU_ITEM_WATER_SOURCE),  // Источник порошка (для замачивания)
						.type = CHANGE_NUMBER,
						.value = &Washing_settings.bedabble_water_soure,
						.min = 1,
						.count = 2,
						.step = 1,
						.postfix = "отсек",
						.visible = 0,
						.on_click = NULL,
	},
	{
						.id = 5,
						.parent_id = 2,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_BEDABBLE_START_NOW),  // Замочить сейчас
						.type = RUN_PROCESS,
						.process_id = PROCESS_BEDABBLE,
						.visible = 0,
						.on_click = NULL,
	},
	{
						.id = 6,
						.parent_id = 2,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_BEDABBLE_STOP_NOW),  // Остановить замачивание
						.type = RUN_FUNCTION,
						.visible = 0,
						.on_click = bedabble_stop,
	},
	{
				.id = 9,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_WASHING_PARAMS),  // Параметры стирки
				.type = GROUP,
				.visible = 1,
				.on_click = NULL,
	},
	{
						.id = 10,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_MODE),  // Режим стирки
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
						.name = GET_MESS_POINTER(MENU_ITEM_PREVIOUS_WASHING),  // Предварительная стирка
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 12,
								.parent_id = 11,
								.name = GET_MESS_POINTER(MENU_ITEM_SWITCH_TEXT),  // вкл./выкл.
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
								.name = GET_MESS_POINTER(MENU_ITEM_WATER_SOURCE),  // Источник порошка (для предварительной стирки)
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.prev_washing_water_soure,
								.min = 1,
								.count = 2,
								.step = 1,
								.postfix = "отсек",
								.visible = 0,
								.on_click = NULL,
	},
	{
						.id = 31,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_PART1),  // 1ая фаза стирки
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 14,
								.parent_id = 31,
								.name = GET_MESS_POINTER(MENU_ITEM_TEMPERATURE),  // Температура
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
								.name = GET_MESS_POINTER(MENU_ITEM_WASHING_TIME),  // Время стирки
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.washing_time_part1,
								.min = 5,
								.count = 10,
								.step = 5,
								.postfix = "мин.",
								.visible = 1,
								.on_click = NULL,
	},
	{
						.id = 32,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_PART2),  // 2ая фаза стирки
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 33,
								.parent_id = 32,
								.name = GET_MESS_POINTER(MENU_ITEM_SWITCH_TEXT),  // вкл./выкл.
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
								.name = GET_MESS_POINTER(MENU_ITEM_TEMPERATURE),  // Температура
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
								.name = GET_MESS_POINTER(MENU_ITEM_WASHING_TIME),  // Время стирки
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.washing_time_part2,
								.min = 5,
								.count = 10,
								.step = 5,
								.postfix = "мин.",
								.visible = 0,
								.on_click = NULL,
	},
	{
						.id = 16,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WATER_SOURCE),  // Источник порошка (для основной стирки)
						.type = CHANGE_NUMBER,
						.value = &Washing_settings.washing_water_soure,
						.min = 1,
						.count = 2,
						.step = 1,
						.postfix = "отсек",
						.visible = 1,
						.on_click = NULL,
	},
	{
						.id = 17,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_WATER_LEVEL),  // Кол-во воды
						.type = CHANGE_NUMBER,
						.value = &Washing_settings.washing_water_level,
						.min = 1,
						.count = 7,
						.step = 1,
						.postfix = "уровень",
						.visible = 1,
						.on_click = NULL,
	},
	{
						.id = 18,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_COUNT_RINSE),  // Кол-во полосканий
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
						.name = GET_MESS_POINTER(MENU_ITEM_WRING),  // Отжим
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 20,
								.parent_id = 19,
								.name = GET_MESS_POINTER(MENU_ITEM_WRING_SPEED),  // Максимальная скорость
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
								.name = GET_MESS_POINTER(MENU_ITEM_WRING_TIME),  // Время отжима
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.wring_time,
								.min = 2,
								.count = 5,
								.step = 2,
								.postfix = "мин.",
								.visible = 1,
								.on_click = NULL,
	},
	{
						.id = 22,
						.parent_id = 9,
						.name = GET_MESS_POINTER(MENU_ITEM_WASHING_SWILL_SIGNAL),  // Сигнал ополаскивания
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
				.name = GET_MESS_POINTER(MENU_ITEM_RUN_WASHING),  // Запуск стирки
				.type = RUN_PROCESS,
				.process_id = PROCESS_WASHING,
				.visible = 1,
				.on_click = NULL,
	},
	{
				.id = 8,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_STOP_WASHING),  // Остановить стирку
				.type = RUN_FUNCTION,
				.visible = 0,
				.on_click = washing_stop,
	},
	{
				.id = 23,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_OPERATIONS),  // Ручное управление
				.type = GROUP,
				.visible = 1,
				.on_click = NULL,
	},
	{
						.id = 24,
						.parent_id = 23,
						.name = GET_MESS_POINTER(MENU_ITEM_WATER_CONTROL),  // Управление водой
						.type = RUN_PROCESS,
						.process_id = PROCESS_WATER_CONTROL,
						.visible = 1,
						.on_click = NULL,
	},
	{
						.id = 25,
						.parent_id = 23,
						.name = GET_MESS_POINTER(RINSE_MESSAGE), // Полоскание
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 36,
								.parent_id = 25,
								.name = GET_MESS_POINTER(MENU_ITEM_COUNT_RINSE),  // Кол-во полосканий
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
								.name = GET_MESS_POINTER(MENU_ITEM_RINSE_MODE),  // Режим полоскания
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
								.name = GET_MESS_POINTER(MENU_ITEM_WATER_SOURCE),  // Источник воды
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.rinse_water_soure,
								.min = 1,
								.count = 2,
								.step = 1,
								.postfix = "отсек",
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 38,
								.parent_id = 25,
								.name = GET_MESS_POINTER(MENU_ITEM_RUN_RINSE),  // Полоскать
								.type = RUN_PROCESS,
								.process_id = PROCESS_RINSE,
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 39,
								.parent_id = 25,
								.name = GET_MESS_POINTER(MENU_ITEM_STOP_RINSE),  // Остановить полоскание
								.type = RUN_FUNCTION,
								.visible = 0,
								.on_click = rinse_stop,
	},
	{
						.id = 40,
						.parent_id = 23,
						.name = GET_MESS_POINTER(MENU_ITEM_WRING), // Отжим
						.type = GROUP,
						.visible = 1,
						.on_click = NULL,
	},
	{
								.id = 41,
								.parent_id = 40,
								.name = GET_MESS_POINTER(MENU_ITEM_WRING_SPEED),  // Максимальная скорость
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
								.name = GET_MESS_POINTER(MENU_ITEM_WRING_TIME),  // Время отжима
								.type = CHANGE_NUMBER,
								.value = &Washing_settings.wring_wring_time,
								.min = 2,
								.count = 5,
								.step = 2,
								.postfix = "мин.",
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 43,
								.parent_id = 40,
								.name = GET_MESS_POINTER(MENU_ITEM_RUN_WRING),  // Отжать
								.type = RUN_PROCESS,
								.process_id = PROCESS_WRING,
								.visible = 1,
								.on_click = NULL,
	},
	{
								.id = 44,
								.parent_id = 40,
								.name = GET_MESS_POINTER(MENU_ITEM_STOP_WRING),  // Остановить отжим
								.type = RUN_FUNCTION,
								.visible = 0,
								.on_click = wring_stop,
	},
	{
				.id = 27,
				.parent_id = 0,
				.name = GET_MESS_POINTER(MENU_ITEM_SETTINGS),  // Общие настройки
				.type = GROUP,
				.visible = 1,
				.on_click = NULL,
	},
	{
						.id = 28,
						.parent_id = 27,
						.name = GET_MESS_POINTER(MENU_ITEM_SETTINGS_SOUND),  // Звук
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
						.name = GET_MESS_POINTER(SET_TIME_MENU_ITEM),  // Установка времени
						.type = RUN_PROCESS,
						.process_id = PROCESS_SET_TIME,
						.visible = 1,
						.on_click = save_all_settings,
	},
};


struct lib_menu Lib_menu = //Создаем объект инициализирующий lib_menu
{
	.tree = Menu, // Дерево меню
	.count_items = (sizeof(Menu) / sizeof(struct menu_item)), // Количество элементов в дереве меню
	.display_width = DISPLAY_WIDTH, //Ширина экрана в символах
	.max_width_str = 18, // Максимальная длинна строки в меню
	.middle_row_num = ((DISPLAY_HEIGHT - HEADER_COUNT_ROWS - 1) / 2), // Номер строки для активного элемента
	.count_rows = (DISPLAY_HEIGHT - HEADER_COUNT_ROWS), // Количество строк отводимых под меню
	.clear_screen = clear_screen, // Функция для отчистки экрана
	.print_header = print_header, // Функция для отрисовки заголовока меню
	.print_item = print_item, // Отрисовать элемент меню
	.run_process = menu_run_process, //Функция запускает процесс
//	.switch_process = menu_switch_process, //Функция переключает активный процесс
};
