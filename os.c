#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <avr/eeprom.h>
#include "config.h"
#include "messages.h"
#include "../../lib/button.h"
#include "../../lib/encoders.h"
#include "os.h"
#include "../../lib/lib_liveos.h"
#include "../../lib/i2c.h"
#include "processes/processes.h"
#include "drivers.h"
#include "lib.h"
#include "lcd_buffer.h"

char Temp_buf[CLI_LENGTH]; // Временный буфер для работы консольных команд

extern struct button Buttons[]; // проинициализированно в drivers.c
extern struct encoder Encoders[]; // проинициализированно в drivers.c
extern struct machine_states Machine_states; // Состояние всей машины
extern struct machine_timers Machine_timers;  // Состояния всех таймеров
extern struct washing_settings Washing_settings; // Нстройки регулируемые с меню
extern struct time Main_current_time; //Часы реального времени
extern int16_t Current_temperature; // Текущая температура воды
extern uint16_t Rotating_speed; // Текущая скорость вращения барабана
extern int16_t Motor_power; // Требуемая выходная мощность на мотор
extern int16_t Current_vibration; //Текущий уровень вибрации

// Подключаем процессы
CREATE_PGM_STR(PROCESS_SCREENSAVER_NAME, "screensaver");
CREATE_PGM_STR(PROCESS_MENU_NAME, "menu");
CREATE_PGM_STR(PROCESS_WASHING_NAME, "washing");
CREATE_PGM_STR(PROCESS_BEDABBLE_NAME, "bedabble");
CREATE_PGM_STR(PROCESS_DISPLAY_ERROR_NAME, "display_error");
CREATE_PGM_STR(PROCESS_RINSE_NAME, "rinse");
CREATE_PGM_STR(PROCESS_WRING_NAME, "wring");
CREATE_PGM_STR(PROCESS_WATER_CONTROL_NAME, "water_control");
CREATE_PGM_STR(PROCESS_SET_TIME_NAME, "set_time");
CREATE_PGM_STR(PROCESS_POWER_MANAGER_NAME, "power_manager");

static struct task Processes[] =  // Заполняем функции всех процессов
{
	{ // Обработчики действий для screensaver
		.name = GET_MESS_POINTER(PROCESS_SCREENSAVER_NAME),
		SET_CALLBACK(PROCESS_SCREENSAVER, init),
		SET_CALLBACK(PROCESS_SCREENSAVER, stop),
		SET_CALLBACK(PROCESS_SCREENSAVER, run),
		.on_stop = NULL,
		SET_CALLBACK(PROCESS_SCREENSAVER, active),
		SET_CALLBACK(PROCESS_SCREENSAVER, main_loop),
		SET_CALLBACK(PROCESS_SCREENSAVER, signal),
		SET_CALLBACK(PROCESS_SCREENSAVER, console),
	},

	{ // Обработчики действий для menu
		.name = GET_MESS_POINTER(PROCESS_MENU_NAME),
		SET_CALLBACK(PROCESS_MENU, init),
		.on_stop = NULL,
		SET_CALLBACK(PROCESS_MENU, run),
		SET_CALLBACK(PROCESS_MENU, active),
		SET_CALLBACK(PROCESS_MENU, main_loop),
		SET_CALLBACK(PROCESS_MENU, background),
		SET_CALLBACK(PROCESS_MENU, console),
		.on_signal = NULL,
	},
	{ // Обработчики действий для washing
		.name = GET_MESS_POINTER(PROCESS_WASHING_NAME),
		SET_CALLBACK(PROCESS_WASHING, init),
		SET_CALLBACK(PROCESS_WASHING, run),
		SET_CALLBACK(PROCESS_WASHING, stop),
		SET_CALLBACK(PROCESS_WASHING, active),
		SET_CALLBACK(PROCESS_WASHING, main_loop),
		SET_CALLBACK(PROCESS_WASHING, background),
		SET_CALLBACK(PROCESS_WASHING, signal),
		SET_CALLBACK(PROCESS_WASHING, console),
	},
	{ // Обработчики действий для bedabble
		.name = GET_MESS_POINTER(PROCESS_BEDABBLE_NAME),
		SET_CALLBACK(PROCESS_BEDABBLE, init),
		SET_CALLBACK(PROCESS_BEDABBLE, run),
		SET_CALLBACK(PROCESS_BEDABBLE, stop),
		SET_CALLBACK(PROCESS_BEDABBLE, active),
		SET_CALLBACK(PROCESS_BEDABBLE, main_loop),
		SET_CALLBACK(PROCESS_BEDABBLE, background),
		SET_CALLBACK(PROCESS_BEDABBLE, signal),
		SET_CALLBACK(PROCESS_BEDABBLE, console),
	},
	{ // Обработчики действий для display error
		.name = GET_MESS_POINTER(PROCESS_DISPLAY_ERROR_NAME),
		SET_CALLBACK(PROCESS_DISPLAY_ERROR, init),
		SET_CALLBACK(PROCESS_DISPLAY_ERROR, run),
		.on_stop = NULL,
		.on_console = NULL,
		SET_CALLBACK(PROCESS_DISPLAY_ERROR, active),
		SET_CALLBACK(PROCESS_DISPLAY_ERROR, main_loop),
		SET_CALLBACK(PROCESS_DISPLAY_ERROR, background),
		.on_signal = NULL,
	},
	{ // Обработчики действий для rinse
		.name = GET_MESS_POINTER(PROCESS_RINSE_NAME),
		SET_CALLBACK(PROCESS_RINSE, init),
		SET_CALLBACK(PROCESS_RINSE, run),
		SET_CALLBACK(PROCESS_RINSE, stop),
		SET_CALLBACK(PROCESS_RINSE, active),
		SET_CALLBACK(PROCESS_RINSE, main_loop),
		SET_CALLBACK(PROCESS_RINSE, background),
		SET_CALLBACK(PROCESS_RINSE, signal),
		SET_CALLBACK(PROCESS_RINSE, console),
	},
	{ // Обработчики действий для wring
		.name = GET_MESS_POINTER(PROCESS_WRING_NAME),
		SET_CALLBACK(PROCESS_WRING, init),
		SET_CALLBACK(PROCESS_WRING, run),
		SET_CALLBACK(PROCESS_WRING, stop),
		SET_CALLBACK(PROCESS_WRING, active),
		SET_CALLBACK(PROCESS_WRING, main_loop),
		SET_CALLBACK(PROCESS_WRING, background),
		SET_CALLBACK(PROCESS_WRING, signal),
		SET_CALLBACK(PROCESS_WRING, console),
	},
	{ // Обработчики действий для water_control
		.name = GET_MESS_POINTER(PROCESS_WATER_CONTROL_NAME),
		SET_CALLBACK(PROCESS_WATER_CONTROL, init),
		SET_CALLBACK(PROCESS_WATER_CONTROL, run),
		SET_CALLBACK(PROCESS_WATER_CONTROL, stop),
		SET_CALLBACK(PROCESS_WATER_CONTROL, active),
		SET_CALLBACK(PROCESS_WATER_CONTROL, main_loop),
		.on_background = NULL,
		.on_signal = NULL,
		SET_CALLBACK(PROCESS_WATER_CONTROL, console),
	},
	{ // Обработчики действий для set_time
		.name = GET_MESS_POINTER(PROCESS_SET_TIME_NAME),
		SET_CALLBACK(PROCESS_SET_TIME, init),
		SET_CALLBACK(PROCESS_SET_TIME, run),
		.on_stop = NULL,
		SET_CALLBACK(PROCESS_SET_TIME, active),
		SET_CALLBACK(PROCESS_SET_TIME, main_loop),
		.on_background = NULL,
		.on_signal = NULL,
		SET_CALLBACK(PROCESS_SET_TIME, console),
	},
	{ // Обработчики действий для power_manager
		.name = GET_MESS_POINTER(PROCESS_POWER_MANAGER_NAME),
		SET_CALLBACK(PROCESS_POWER_MANAGER, init),
		SET_CALLBACK(PROCESS_POWER_MANAGER, run),
		SET_CALLBACK(PROCESS_POWER_MANAGER, background),
		SET_CALLBACK(PROCESS_POWER_MANAGER, signal),
		.on_active = NULL,
		.on_stop = NULL,
		.on_main_loop = NULL,
		.on_console = NULL,
	},
};

static struct external_callbacks Ext_callbacks[] =
{
	{// Внешние обработчики действий для screensaver
		SET_CALLBACK(PROCESS_SCREENSAVER, timer0),
		.on_timer2 = NULL,
		SET_CALLBACK(PROCESS_SCREENSAVER, click_up),
		SET_CALLBACK(PROCESS_SCREENSAVER, click_down),
		SET_CALLBACK(PROCESS_SCREENSAVER, click_esc),
		SET_CALLBACK(PROCESS_SCREENSAVER, click_enter),
		.on_click_ext1 = NULL,
		.on_long_esc = NULL,
		SET_CALLBACK(PROCESS_SCREENSAVER, long_enter),
		.on_press_ext1 = NULL,
		.on_press_ext2 = NULL,
		.on_press_ext3 = NULL,
		.on_press_ext4 = NULL,
	},
	{// Внешние обработчики действий для menu
		.on_timer0 = NULL,
		SET_CALLBACK(PROCESS_MENU, timer2),
		SET_CALLBACK(PROCESS_MENU, click_up),
		SET_CALLBACK(PROCESS_MENU, click_down),
		SET_CALLBACK(PROCESS_MENU, click_esc),
		SET_CALLBACK(PROCESS_MENU, click_enter),
		SET_CALLBACK(PROCESS_MENU, click_ext1),
		.on_long_esc = NULL,
		.on_long_enter = NULL,
		.on_press_ext1 = NULL,
		.on_press_ext2 = NULL,
		.on_press_ext3 = NULL,
		.on_press_ext4 = NULL,
	},
	{ // Внешние обработчики действий для washing
		SET_CALLBACK(PROCESS_WASHING, timer0),
		.on_timer2 = NULL,
		.on_click_up = NULL,
		.on_click_down = NULL,
		SET_CALLBACK(PROCESS_WASHING, click_esc),
		SET_CALLBACK(PROCESS_WASHING, click_enter),
		.on_click_ext1 = NULL,
		.on_long_esc = NULL,
		.on_long_enter = NULL,
		.on_press_ext1 = NULL,
		.on_press_ext2 = NULL,
		.on_press_ext3 = NULL,
		.on_press_ext4 = NULL,
	},
	{// Внешние обработчики действий для bedabble
		SET_CALLBACK(PROCESS_BEDABBLE, timer0),
		.on_timer2 = NULL,
		.on_click_up = NULL,
		.on_click_down = NULL,
		SET_CALLBACK(PROCESS_BEDABBLE, click_esc),
		SET_CALLBACK(PROCESS_BEDABBLE, click_enter),
		.on_click_ext1 = NULL,
		.on_long_esc = NULL,
		.on_long_enter = NULL,
		.on_press_ext1 = NULL,
		.on_press_ext2 = NULL,
		.on_press_ext3 = NULL,
		.on_press_ext4 = NULL,
	},
	{// Внешние обработчики действий для display error
		.on_timer0 = NULL,
		SET_CALLBACK(PROCESS_DISPLAY_ERROR, timer2),
		.on_click_up = NULL,
		.on_click_down = NULL,
		SET_CALLBACK(PROCESS_DISPLAY_ERROR, click_esc),
		SET_CALLBACK(PROCESS_DISPLAY_ERROR, click_enter),
		.on_click_ext1 = NULL,
		.on_long_esc = NULL,
		SET_CALLBACK(PROCESS_DISPLAY_ERROR, long_enter),
		.on_press_ext1 = NULL,
		.on_press_ext2 = NULL,
		.on_press_ext3 = NULL,
		.on_press_ext4 = NULL,
	},
	{// Внешние обработчики действий для rinse
		SET_CALLBACK(PROCESS_RINSE, timer0),
		.on_timer2 = NULL,
		.on_click_up = NULL,
		.on_click_down = NULL,
		SET_CALLBACK(PROCESS_RINSE, click_esc),
		SET_CALLBACK(PROCESS_RINSE, click_enter),
		.on_click_ext1 = NULL,
		.on_long_esc = NULL,
		.on_long_enter = NULL,
		.on_press_ext1 = NULL,
		.on_press_ext2 = NULL,
		.on_press_ext3 = NULL,
		.on_press_ext4 = NULL,
	},
	{// Внешние обработчики действий для wring
		SET_CALLBACK(PROCESS_WRING, timer0),
		.on_timer2 = NULL,
		.on_click_up = NULL,
		.on_click_down = NULL,
		SET_CALLBACK(PROCESS_WRING, click_esc),
		SET_CALLBACK(PROCESS_WRING, click_enter),
		.on_click_ext1 = NULL,
		.on_long_esc = NULL,
		.on_long_enter = NULL,
		.on_press_ext1 = NULL,
		.on_press_ext2 = NULL,
		.on_press_ext3 = NULL,
		.on_press_ext4 = NULL,
	},
	{// Внешние обработчики действий для water_control
		.on_timer0 = NULL,
		SET_CALLBACK(PROCESS_WATER_CONTROL, timer2),
		.on_click_up = NULL,
		.on_click_down = NULL,
		SET_CALLBACK(PROCESS_WATER_CONTROL, click_esc),
		SET_CALLBACK(PROCESS_WATER_CONTROL, click_enter),
		.on_click_ext1 = NULL,
		.on_long_esc = NULL,
		.on_long_enter = NULL,
		SET_CALLBACK(PROCESS_WATER_CONTROL, press_ext1),
		SET_CALLBACK(PROCESS_WATER_CONTROL, press_ext2),
		SET_CALLBACK(PROCESS_WATER_CONTROL, press_ext3),
		SET_CALLBACK(PROCESS_WATER_CONTROL, press_ext4),
	},
	{// Внешние обработчики действий для set_time
		.on_timer0 = NULL,
		SET_CALLBACK(PROCESS_SET_TIME, timer2),
		SET_CALLBACK(PROCESS_SET_TIME, click_up),
		SET_CALLBACK(PROCESS_SET_TIME, click_down),
		SET_CALLBACK(PROCESS_SET_TIME, click_esc),
		SET_CALLBACK(PROCESS_SET_TIME, click_enter),
		.on_click_ext1 = NULL,
		.on_long_esc = NULL,
		.on_long_enter = NULL,
		.on_press_ext1 = NULL,
		.on_press_ext2 = NULL,
		.on_press_ext3 = NULL,
		.on_press_ext4 = NULL,
	},
	{// Внешние обработчики действий для power_manager
		.on_timer0 = NULL,
		.on_timer2 = NULL,
		.on_click_up = NULL,
		.on_click_down = NULL,
		.on_click_esc = NULL,
		.on_click_enter = NULL,
		.on_click_ext1 = NULL,
		.on_long_esc = NULL,
		.on_long_enter = NULL,
		.on_press_ext1 = NULL,
		.on_press_ext2 = NULL,
		.on_press_ext3 = NULL,
		.on_press_ext4 = NULL,
	},
};

static void autoexec(void) //Автозагрузка при включении устройства
{
	load_all_settings();
//	run_bg_process(PROCESS_POWER_MANAGER, NULL); // Запускаем процесс управления питанием в фоне
	run_bg_process(PROCESS_MENU, NULL); // Запускаем процесс меню в фоне
	run_bg_process(PROCESS_DISPLAY_ERROR, NULL); //Запускаем процесс обработчика ошибок в фоне
	run_process(PROCESS_SCREENSAVER, NULL); //Запсукаем данный процесс как активный
//	send_signal(PROCESS_POWER_MANAGER, 2); //Посылаем сигнал менеджеру питания чтобы тот проверил и если надо возобновил прерванные процессы
}

// Объявляем имена внешних консольных команд
CREATE_PGM_STR(CMD_ENV, "env");
CREATE_PGM_STR(CMD_ENV_HELP, "Print or set system variable");

CREATE_PGM_STR(CMD_POWER, "power");
CREATE_PGM_STR(CMD_POWER_HELP, "Turn on/off device power.\r\n\t\tFormat: power (on|off) (main|valve1|valve2|pomp|heater|door|speaker)\r\n"
																				"\t\tParameter 2 detail info:\r\n"
																				"\t\t\tmain\t - main system power\r\n"
																				"\t\t\tvalve1\t - turn on/off water input from valve1\r\n"
																				"\t\t\tvalve2\t - turn on/off water input from valve2\r\n"
																				"\t\t\tpomp\t - turn on/off water out\r\n"
																				"\t\t\theater\t - turn on/off heater\r\n"
																				"\t\t\tdoor\t - lock/unlock door\r\n"
																				"\t\t\tspeaker\t - turn on/off sound signal and set frequency\r\n"
																				);

CREATE_PGM_STR(CMD_MOTOR, "motor");
CREATE_PGM_STR(CMD_MOTOR_HELP, "Turn on or off motor.\r\n\t\tFormat: motor (on|off) (forward|backward) (low|high) (0..500)\r\n"
																	"\t\tparametr1 - turn on/off power motor\r\n"
																	"\t\tparametr2 - direction rotate motor\r\n"
																	"\t\tparametr3 - transmission motor\r\n"
																	"\t\tparametr4 - rotate speed. May be from 0 to 500\r\n"
																	);

CREATE_PGM_STR(CMD_TIME, "time");
CREATE_PGM_STR(CMD_TIME_HELP, "Print or set RTC. \r\n\t\tFormat: time (print|set|start|stop)\r\n"
																"\t\ttime set <HH> <MM> <DD> <MM> <YY> <day of week 1..7>\r\n"
																);

CREATE_PGM_STR(CMD_TEST, "test");
CREATE_PGM_STR(CMD_TEST_HELP, "Temporary command run cmd_test() function");

static void cmd_env(char *params) //Команда позволяет вывести имена и значения переменных а также изменить их значение
{
	int8_t set = -1;
	int8_t all = 0;
	uint8_t i;
	uint16_t set_val = 0;

	if(!params)
	{
		console_out_str_P(PSTR("Parameter 1 not recognized. Parameter 1 must be 'set' or 'print'.\r\n"));
		return;
	}

	memset(Temp_buf, 0, sizeof(Temp_buf));
	params = copy_cli_parameter(params, Temp_buf);
	// Проверяем какой параметр передан
	if(!strcasecmp_P(Temp_buf, PSTR("set")))   // Если передан set
		set = 1;

	if(!strcasecmp_P(Temp_buf, PSTR("print"))) // Если передан print
		set = 0;

	if(set == -1) // Если параметр 1 непонятен
	{
		console_out_str_P(PSTR("Parameter 1 not recognized. Parameter 1 must be 'set' or 'print'.\r\n"));
		return;
	}

	params = copy_cli_parameter(params, Temp_buf);
	if(params == NULL)
	{
		if(set) //Если второй параметр ненайден ,однако было сказанно 'set', то выводим ошибку
		{
			console_out_str_P(PSTR("Parameter 2 not recognized. Parameter 2 must be name variable if parameter 1 = 'set'.\r\n"));
			return;
		}

		all = 1; //Если был указан параметр 'print' но второй параметр отсутсвует, значит надо вывести все переменные
	}

	if(set == 1) //Если был установлен режим 'set' то нужно распознать значение в которое необходимо выставить переменную
	{
		i = get_start_word(params); //Чтобы незапортить введеное имя переменной в Temp_buf, получим значение последнего аргумента путем движения по строке params
		if(i == strlen(params))
		{
			console_out_str_P(PSTR("Parameter 3 not recognized. Parameter 3 must be value variable if parameter 1 = 'set' and parameter 2 = variable name.\r\n"));
			return;
		}

		set_val = atoi(params + i); //Распазнаем значение третьего параметра
	}

	i = 0; // В данный момент эта переменная будет определять была ли найденна введеная во второй параметр переменная

	char itoa_buffer[7];
	memset(itoa_buffer, 0, sizeof(itoa_buffer));

	#define SET_GET_VAR_VAL(str) if((strcmp_P(Temp_buf, PSTR(#str)) == 0) && set)str = set_val; if(strcmp_P(Temp_buf, PSTR(#str)) == 0 || all){ print_var(str) i = 1; asm("wdr"); }
	#include "env_print.d"  // Подгружаем текстовый файл со списком переменных которые нужно иметь возможность вывести или изменить данной командой

	if(!i) //Если небыло найденно неодной переменной с таким именем
		console_out_str_P(PSTR("Variable not found\r\n"));
}

static void cmd_power(char *params) // Команда позволяет включить или выключить питание различных узлов стиральной машины
{
	int8_t power = -1;

	if(!params)
	{
		console_out_str_P(PSTR("Parameter 1 is not set. Parameter 1 may be 'on' or 'off'\r\n"));
		return;
	}

	memset(Temp_buf, 0, sizeof(Temp_buf));
	params = copy_cli_parameter(params, Temp_buf);

	if(!strcasecmp_P(Temp_buf, PSTR("on")))   // Если передан параметр 'on'
		power = 1;

	if(!strcasecmp_P(Temp_buf, PSTR("off")))   // Если передан параметр 'off'
		power = 0;

	if(power == -1) //Если параметр 1 некоррекктный
	{
		console_out_str_P(PSTR("Parameter 1 is not correct. Parameter 1 may be 'on' or 'off'\r\n"));
		return;
	}

	params = copy_cli_parameter(params, Temp_buf); //Находим начало второго параметра
	if(params == NULL) //Если ненайден второй параметр
	{
		console_out_str_P(PSTR("Parameter 2 is not found. view 'help' for more details\r\n"));
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("main")))   // Если передан второй параметр 'main'
	{
		if(power) //Включить или выключить питание системы
			power_on_system();
		else
			power_off_system();
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("valve1")))   // Если передан второй параметр 'valve1'
	{
		turn_valve(VALVE1, power);
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("valve2")))   // Если передан второй параметр 'valve2'
	{
		turn_valve(VALVE2, power);
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("pomp")))   // Если передан второй параметр 'pomp'
	{
		turn_pump(power);
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("heater")))   // Если передан второй параметр 'heater'
	{
		turn_heater(power);
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("door")))   // Если передан второй параметр 'door'
	{
		block_door(power);
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("speaker")))   // Если передан второй параметр 'speaker'
	{
		if(!power) //Если надо выключить динамик
		{
			turn_speaker(OFF);
			return;
		}

		params = copy_cli_parameter(params, Temp_buf); //Определяем требуемую частоту сигнала
		if(params == NULL)
		{
			console_out_str_P(PSTR("Parameter 3 must be number signal frequency\r\n"));
			return;
		}

		turn_speaker(atoi(Temp_buf)); //Включаем звуковой сигнал с указанной частотой
		return;
	}

	// если неодно условие невыполнилось значит второй параметр введен некорректный
	console_out_str_P(PSTR("Parameter 2 is not correct. view 'help' for more details\r\n"));
}

static void cmd_motor(char *params) // Команда позволяет включить или выключить вращение барабана
{
	int8_t power = -1;
	int8_t reverse = -1;
	int8_t mode = -1;
	int16_t speed = -1;

	if(!params)
	{
		console_out_str_P(PSTR("Parameter 1 is not set. Parameter 1 may be 'on' or 'off'\r\n"));
		return;
	}

	memset(Temp_buf, 0, sizeof(Temp_buf));
	params = copy_cli_parameter(params, Temp_buf);

	if(!strcasecmp_P(Temp_buf, PSTR("on")))   // Если передан параметр 'on'
		power = 1;

	if(!strcasecmp_P(Temp_buf, PSTR("off")))   // Если передан параметр 'off'
	{
		disable_rotate_drum(); //Останавливаем вращение и выходим
		return;
	}

	if(power == -1) //Если параметр 1 некоррекктный
	{
		console_out_str_P(PSTR("Parameter 1 is not correct. Parameter 1 may be 'on' or 'off'\r\n"));
		return;
	}



	params = copy_cli_parameter(params, Temp_buf);
	if(params == NULL) //Если ненайден второй параметр
	{
		console_out_str_P(PSTR("Parameter 2 is not found. Parameter 2 may be 'forward' or 'backward'\r\n"));
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("forward")))   // Если передан второй параметр 'forward'
		reverse = 0;

	if(!strcasecmp_P(Temp_buf, PSTR("backward")))   // Если передан второй параметр 'backward'
		reverse = 1;

	if(reverse == -1) //Если параметр 2 некоррекктный
	{
		console_out_str_P(PSTR("Parameter 2 is not correct. Parameter 2 may be 'forward' or 'backward'\r\n"));
		return;
	}


	params = copy_cli_parameter(params, Temp_buf);
	if(params == NULL) //Если ненайден третий параметр
	{
		console_out_str_P(PSTR("Parameter 3 is not found. Parameter 3 may be 'low' or 'high'\r\n"));
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("low")))   // Если передан третий параметр 'low'
		mode = 0;

	if(!strcasecmp_P(Temp_buf, PSTR("high")))   // Если передан третий параметр 'high'
		mode = 1;

	if(mode == -1) //Если параметр 3 некоррекктный
	{
		console_out_str_P(PSTR("Parameter 3 is not correct. Parameter 3 may be 'low' or 'high'\r\n"));
		return;
	}


	params = copy_cli_parameter(params, Temp_buf);
	if(params == NULL) //Если ненайден четвертый параметр
	{
		console_out_str_P(PSTR("Parameter 4 is not found. Parameter 4 may be number from 0 to 500\r\n"));
		return;
	}

	speed = atoi(Temp_buf); //Распазнаем значение четвертого параметра

	if(speed < 0 || speed > 500) //Если параметр 4 некоррекктный
	{
		console_out_str_P(PSTR("Parameter 4 is not correct. Parameter 4 may be number from 0 to 500\r\n"));
		return;
	}

	enable_rotate_drum(mode, speed, reverse);
}

static void cmd_time(char *params) //Команда для вывода и установки даты и времени
{
	uint8_t i;
	uint8_t sec, set = 0;
	struct time set_time;

	params = copy_cli_parameter(params, Temp_buf);

	if(!strcasecmp_P(Temp_buf, PSTR("start"))) //Запустить часы
	{
		i = i2c_recv_data(I2C_ADDR_CLOCK, 0, &sec, 1); //Читаем первый байт с DS1307
		if(i < 1)
		{
			printf_P(PSTR("Can't read first byte from DS1307\r\n"));
			return;
		}

		sec &= ((1 << 7) - 1); //Опускаем 7ой бит
		i = i2c_send_data(I2C_ADDR_CLOCK, 0, &sec, 1); //Записываем байт в DS1307
		if(i < 1)
		{
			printf_P(PSTR("Can't write first byte to DS1307\r\n"));
			return;
		}
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("stop"))) //Остановить часы
	{
		i = i2c_recv_data(I2C_ADDR_CLOCK, 0, &sec, 1); //Читаем первый байт с DS1307
		if(i < 1)
		{
			printf_P(PSTR("Can't read first byte from DS1307\r\n"));
			return;
		}

		sec |= (1 << 7) ; //Поднимаем 7ой бит
		i = i2c_send_data(I2C_ADDR_CLOCK, 0, &sec, 1); //Записываем байт в DS1307
		if(i < 1)
		{
			printf_P(PSTR("Can't write first byte to DS1307\r\n"));
			return;
		}
		return;
	}

	if(!strcasecmp_P(Temp_buf, PSTR("set"))) //Установить время
	{
		params = copy_cli_parameter(params, Temp_buf);
		if(params == NULL) //Если ненайден параметр
			goto error_set;
		set_time.hour = atoi(Temp_buf);

		params = copy_cli_parameter(params, Temp_buf);
		if(params == NULL) //Если ненайден параметр
			goto error_set;
		set_time.min = atoi(Temp_buf);

		params = copy_cli_parameter(params, Temp_buf);
		if(params == NULL) //Если ненайден параметр
			goto error_set;
		set_time.day_of_month = atoi(Temp_buf);

		params = copy_cli_parameter(params, Temp_buf);
		if(params == NULL) //Если ненайден параметр
			goto error_set;
		set_time.month = atoi(Temp_buf);

		params = copy_cli_parameter(params, Temp_buf);
		if(params == NULL) //Если ненайден параметр
			goto error_set;
		set_time.year = atoi(Temp_buf);

		params = copy_cli_parameter(params, Temp_buf);
		if(params == NULL) //Если ненайден параметр
			goto error_set;
		set_time.day_of_week = atoi(Temp_buf);

		set_time.sec = 0;
		write_datetime(&set_time); //Устанавливаем введенное время
		Main_current_time = set_time;
		set = 1; //Выставляем данный флаг в 1 чтобы отобразить установленное время
	}

	if(!strcasecmp_P(Temp_buf, PSTR("print")) || set) //Отобразить текущее время
	{
		printf_P(PSTR("\t%.2d:%.2d:%.2d %.2d.%.2d.20%.2d, day of week: %d\r\n"), Main_current_time.hour,
																																Main_current_time.min,
																																Main_current_time.sec,
																																Main_current_time.day_of_month,
																																Main_current_time.month,
																																Main_current_time.year,
																																Main_current_time.day_of_week
																																);
		return;
	}

	console_out_str_P(PSTR("Incorrect parameter1\r\n\tParameter1 may be (print|set|start|stop)\r\n"));
	return;

	error_set:
		console_out_str_P(PSTR("Incorrect format parameters\r\n\tFormat: time set <HH> <MM> <DD> <MM> <YY> <day of week 1..7>\r\n\tExample: time set 9 15 23 2 10 2\r\n\t\tThis command set 09:15:00 23.02.2010 thursday\r\n"));
}

extern uint16_t T;

static void cmd_test(char *params) //Команда для тестирования разного рода кода. Сюда можно записать все что угодно по желанию
{
	uint8_t i;
	params = copy_cli_parameter(params, Temp_buf);

	if(!strcasecmp_P(Temp_buf, PSTR("r")))
	{
		uint8_t buf[9];
		memset(buf, 0, 9);
		read_current_datetime((struct time *)buf);
		for(i = 0; i < 8; i++)
			printf("buf = %d\r\n", buf[i]);
	}

	if(!strcasecmp_P(Temp_buf, PSTR("init_adc")))
	{
		printf_P(PSTR("run init_adc()\r\n"));
		init_adc();
	}

	if(!strcasecmp_P(Temp_buf, PSTR("start_mr")))
	{
		printf_P(PSTR("run start_mode_rotating()\r\n"));
		start_mode_rotating(INTENSIVE_ROTATING);
	}

	if(!strcasecmp_P(Temp_buf, PSTR("stop_mr")))
	{
		printf_P(PSTR("run stop_mode_rotating()\r\n"));
		stop_mode_rotating();
	}

	if(!strcasecmp_P(Temp_buf, PSTR("rotate")))
	{
		params = copy_cli_parameter(params, Temp_buf);

		Motor_power = atoi(Temp_buf);
		turn_motor(ON, ROTATION_BACKWARD, LOW_SPEED_ROTATION);
	}

}

struct cmd_command External_commands[] = //Создаем собственные консольные команды
{
	{
		.cmd_name = GET_MESS_POINTER(CMD_ENV),
		.cmd_callback = cmd_env,
		.cmd_help = GET_MESS_POINTER(CMD_ENV_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_POWER),
		.cmd_callback = cmd_power,
		.cmd_help = GET_MESS_POINTER(CMD_POWER_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_MOTOR),
		.cmd_callback = cmd_motor,
		.cmd_help = GET_MESS_POINTER(CMD_MOTOR_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_TIME),
		.cmd_callback = cmd_time,
		.cmd_help = GET_MESS_POINTER(CMD_TIME_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_TEST),
		.cmd_callback = cmd_test,
		.cmd_help = GET_MESS_POINTER(CMD_TEST_HELP),
	}
};

static void generate_actions(void) //Функция которая следит за состоянием всего и генерирует события
{
    // Опрашиваем кнопки
	get_buttons();
	scan_code_button();

	//Генерируем событие опроса кнопок и/или энкодера
	if(Encoders[0].state == LEFT_ROTATING && Ext_callbacks[get_active_proc_num() - 1].on_click_up)
	{
		Ext_callbacks[get_active_proc_num() - 1].on_click_up();
		Encoders[0].state = NO_ROTATING;
	}

	if(Encoders[0].state == RIGHT_ROTATING && Ext_callbacks[get_active_proc_num() - 1].on_click_down)
	{
		Ext_callbacks[get_active_proc_num() - 1].on_click_down();
		Encoders[0].state = NO_ROTATING;
	}

	if(Buttons[BUTT_ESC].state.one_click && Ext_callbacks[get_active_proc_num() - 1].on_click_esc)
		Ext_callbacks[get_active_proc_num() - 1].on_click_esc();

	if(Buttons[BUTT_ENTER].state.one_click && Ext_callbacks[get_active_proc_num() - 1].on_click_enter)
		Ext_callbacks[get_active_proc_num() - 1].on_click_enter();

	if(Buttons[BUTT_EXT1].state.one_click && Ext_callbacks[get_active_proc_num() - 1].on_click_ext1)
		Ext_callbacks[get_active_proc_num() - 1].on_click_ext1();

	if(Buttons[BUTT_ESC].state.hold && Ext_callbacks[get_active_proc_num() - 1].on_long_esc)
		Ext_callbacks[get_active_proc_num() - 1].on_long_esc();

	if(Buttons[BUTT_ENTER].state.hold && !Buttons[BUTT_ENTER].state.prev_hold && Ext_callbacks[get_active_proc_num() - 1].on_long_enter)
		Ext_callbacks[get_active_proc_num() - 1].on_long_enter();


	if(Buttons[BUTT_EXT1].state.state_button && Ext_callbacks[get_active_proc_num() - 1].on_press_ext1)
		Ext_callbacks[get_active_proc_num() - 1].on_press_ext1();

	if(Buttons[BUTT_EXT2].state.state_button && Ext_callbacks[get_active_proc_num() - 1].on_press_ext2)
		Ext_callbacks[get_active_proc_num() - 1].on_press_ext2();

	if(Buttons[BUTT_EXT3].state.state_button && Ext_callbacks[get_active_proc_num() - 1].on_press_ext3)
		Ext_callbacks[get_active_proc_num() - 1].on_press_ext3();

	if(Buttons[BUTT_EXT4].state.state_button && Ext_callbacks[get_active_proc_num() - 1].on_press_ext4)
		Ext_callbacks[get_active_proc_num() - 1].on_press_ext4();


	clear_unused_key_code();
}

struct lib_liveos Lib_liveos =  //Создаем объект инициализирующий lib_os
{
	.all_tasks = Processes,
	.count_tasks = sizeof(Processes) / sizeof(struct task),
	.generate_actions = generate_actions,
	.console_out_ch = console_put_ch,
	.console_out_str = console_out_str,
	.console_out_str_P = console_out_str_P,
	.external_cmd = External_commands,
	.external_cmd_count = (sizeof(External_commands) / sizeof(struct cmd_command)),
	.reboot = reboot,
	.autoexec = autoexec,
};


void sys_call_timer0(void) // Функция встраивается в системный таймер 0
{
	for(int i = 0; i < get_count_processes(); i++) // Запуск  on_timer2 во всех процессах
		if(Ext_callbacks[i].on_timer0 && get_process_status(i + 1) == RUNNING) //Если в данном процессе on_timer2 определен и процесс запущен
			Ext_callbacks[i].on_timer0();
}

void sys_call_timer2(void) // Функция встраивается в системный таймер 2
{
	for(int i = 0; i < get_count_processes(); i++) // Запуск  on_timer2 во всех процессах
		if(Ext_callbacks[i].on_timer2 && get_process_status(i + 1) == RUNNING) //Если в данном процессе on_timer2 определен и процесс запущен
			Ext_callbacks[i].on_timer2();
}

