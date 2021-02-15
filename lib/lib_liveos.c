#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "lib_liveos.h"

#define CHAR_XON 0x11
#define CHAR_XOFF 0x13

#define CHAR_SPACE ' '
#define CHAR_BACKSPACE 0x8
#define CHAR_BACKSPACE_ALT 0x7f
#define CHAR_ENTER '\r'
#define CHAR_TAB '\t'

#define CONSOLE_BUF_LEN 10 // Длинна буфера UART. Нельзя устанавливать меньше 3х байт

#define CLOSE_REDIRECT_MODE_CHAR CHAR_ENTER // Символ принятый с консоли, который означает прекращения ретрансляции консоли в процесс


static struct lib_liveos *Lib_liveos;

struct system_states
{
	uint8_t current_task; //Номер активного таска
	uint8_t console_redirect_to_proc : 1; // Если данный бит выставлен в 1 то все данные с консоли переправляются в console_redirect_to указанный процесс
	uint8_t console_redirect_to; // Номер процесса в который необходимо перенаправить данные из консоли
};

static struct system_states System_states;
char Cli_buffer[CLI_LENGTH];
char Cli_temp[CLI_LENGTH];
char Console_buffer[CONSOLE_BUF_LEN];

char const Cli_nosuchcommand_str[] PROGMEM = "No such command. Type \'help\' for help.\r\n";
char const Cli_enter_str[] PROGMEM = "\r\n";
char const Cli_prompt_str[] PROGMEM = "# ";
char const Cli_backspace_str[] PROGMEM = "\x8\x20\x8";
char const Cli_proc_state_not_run_str[] PROGMEM = "\t - [not run]";
char const Cli_proc_state_stopped_str[] PROGMEM = "\t - [stopped]";
char const Cli_proc_state_active_str[] PROGMEM = "\t - [active]";
char const Cli_proc_state_running_str[] PROGMEM = "\t - [running]";
char const Cli_incorrect_str[] PROGMEM = "Incorrect process name\r\n";
char const Cli_process_not_found_str[] PROGMEM = "Entered process not found\r\n";
char const Cli_signal_not_recognized_str[] PROGMEM = "Signal number not recognized\r\n";
char const Cli_signal_not_received_str[] PROGMEM = "Signal not received\r\n";


CREATE_PGM_STR(CMD_HELP, "help");
CREATE_PGM_STR(CMD_HELP_HELP, "Display all commands with descriptions");

CREATE_PGM_STR(CMD_PS, "ps");
CREATE_PGM_STR(CMD_PS_HELP, "List all process and this states. Format: ps.");

CREATE_PGM_STR(CMD_SWITCHTO, "switch");
CREATE_PGM_STR(CMD_SWITCHTO_HELP, "Switch to process. Format: switch <process_name>.\r\n\t\t   Example: switch screensaver");

CREATE_PGM_STR(CMD_RUN, "run");
CREATE_PGM_STR(CMD_RUN_HELP, "Run process. Format: run <process_name>\r\n\t\t   Example: run screensaver");

CREATE_PGM_STR(CMD_RUNBG, "runbg");
CREATE_PGM_STR(CMD_RUNBG_HELP, "Run process on background mode.\r\n\t\tFormat: runbg <process_name>.\r\n\t\t   Example: run screensaver");

CREATE_PGM_STR(CMD_STOP, "stop");
CREATE_PGM_STR(CMD_STOP_HELP, "Stop process. Format: stop <process_name>.\r\n\t\t   Example: stop screensaver");

CREATE_PGM_STR(CMD_SIG, "signal");
CREATE_PGM_STR(CMD_SIG_HELP, "Send signal to process.\r\n\t\t   Format: signal <process_name> <number>.\r\n\t\t   Example: signal screensaver 5");

CREATE_PGM_STR(CMD_SEND, "send");
CREATE_PGM_STR(CMD_SEND_HELP, "Start redirect console. Format: send [<process_name>].\r\n\t\t   Starting console sending mode, type Enter key to exit from\r\n\t\t   sending mode.");

CREATE_PGM_STR(CMD_INIT, "init");
CREATE_PGM_STR(CMD_INIT_HELP, "Initializing process. Format: init <process_name>.\r\n\t\t   Example: init screensaver");

CREATE_PGM_STR(CMD_REBOOT, "reboot");
CREATE_PGM_STR(CMD_REBOOT_HELP, "Reboot system");

static void cmd_help(char *params);
static void cmd_ps(char *params);
static void cmd_switchto(char *params);
static void cmd_run(char *params);
static void cmd_runbg(char *params);
static void cmd_stop(char *params);
static void cmd_sig(char *params);
static void cmd_send(char *params);
static void cmd_init(char *params);
static void cmd_reboot(char *params);
static void send_ch_to_process(char ch, uint8_t task_id);
static void scan_stdin(char ch);

struct cmd_command Sys_commands[] = // Описание системных команд
{
	{
		.cmd_name = GET_MESS_POINTER(CMD_HELP),
		.cmd_callback = cmd_help,
		.cmd_help = GET_MESS_POINTER(CMD_HELP_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_PS),
		.cmd_callback = cmd_ps,
		.cmd_help = GET_MESS_POINTER(CMD_PS_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_SWITCHTO),
		.cmd_callback = cmd_switchto,
		.cmd_help = GET_MESS_POINTER(CMD_SWITCHTO_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_RUN),
		.cmd_callback = cmd_run,
		.cmd_help = GET_MESS_POINTER(CMD_RUN_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_RUNBG),
		.cmd_callback = cmd_runbg,
		.cmd_help = GET_MESS_POINTER(CMD_RUNBG_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_STOP),
		.cmd_callback = cmd_stop,
		.cmd_help = GET_MESS_POINTER(CMD_STOP_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_SIG),
		.cmd_callback = cmd_sig,
		.cmd_help = GET_MESS_POINTER(CMD_SIG_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_SEND),
		.cmd_callback = cmd_send,
		.cmd_help = GET_MESS_POINTER(CMD_SEND_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_INIT),
		.cmd_callback = cmd_init,
		.cmd_help = GET_MESS_POINTER(CMD_INIT_HELP),
	},
	{
		.cmd_name = GET_MESS_POINTER(CMD_REBOOT),
		.cmd_callback = cmd_reboot,
		.cmd_help = GET_MESS_POINTER(CMD_REBOOT_HELP),
	},
};

#define COUNT_SYS_COMMANDS (sizeof(Sys_commands) / sizeof(struct cmd_command))

static uint8_t push_queue(char ch, char *buffer, uint8_t buffer_len) // Положить данные в буффер
{
	if(strlen(buffer) >= buffer_len - 1) // Если буфер переполнен
		return buffer_len;

	buffer[strlen(buffer)] = ch;
	return strlen(buffer);
}

static uint8_t pop_queue(char *ch, char *buffer) // Извлеч дыннае из буфера
{
	uint8_t i;

	if(!strlen(buffer)) //Если буфер пуст
		return 0;

	*ch = buffer[0];
	for(i = 1; i < strlen(buffer); i++)
		buffer[i - 1] = buffer[i];

	buffer[strlen(buffer) - 1] = '\0';

	return strlen(buffer) + 1;
}

void init_process(uint8_t task_id) //Инициализация указанного процесса
{
	if(!task_id) //Если указали 0 значит необходимо проинициализировать все процессы
	{
		for(uint8_t i = 0; i < Lib_liveos -> count_tasks; i++) // Запуск  on_init во всех процессах
		{
			Lib_liveos -> all_tasks[i].status = NOT_RUN;
			if(Lib_liveos -> all_tasks[i].on_init)
				Lib_liveos -> all_tasks[i].on_init();
		}

		return;
	}

	Lib_liveos -> all_tasks[task_id - 1].status = NOT_RUN;
	if(Lib_liveos -> all_tasks[task_id - 1].on_init)
		Lib_liveos -> all_tasks[task_id - 1].on_init();
}

void run_process(uint8_t task_id, char *params) //Запуск указанного процесса
{
	if(!task_id) //Если указали нулевой процесс
		return;

	if(Lib_liveos -> all_tasks[task_id - 1].status == RUNNING)
	{
		switch_process(task_id);
		return;
	}

	if(Lib_liveos -> all_tasks[task_id - 1].on_run)
		Lib_liveos -> all_tasks[task_id - 1].on_run(params);
	Lib_liveos -> all_tasks[task_id - 1].status = RUNNING;
	switch_process(task_id);
}

int8_t run_bg_process(uint8_t task_id, char *params) //Запуск указанного процесса в фоне
{
	if(!task_id) //Если указали нулевой процесс
		return 1;

	if(Lib_liveos -> all_tasks[task_id - 1].status == RUNNING)
		return 1;

	if(Lib_liveos -> all_tasks[task_id - 1].on_run)
		Lib_liveos -> all_tasks[task_id - 1].on_run(params);
	Lib_liveos -> all_tasks[task_id - 1].status = RUNNING;

	return 0;
}

void init_liveos(struct lib_liveos *init) // Инициализация всех процессов, передается указатель на функцию генерации событий
{
	Lib_liveos = init;
	Lib_liveos -> console_out_str_P(PSTR("\r\nStarting liveOS v1.0..."));
	init_process(0); // Инициализация всех процессов

	System_states.current_task = DEFAULT_PROCESS;
	Cli_buffer[0] = 0;

	Lib_liveos -> console_out_str_P(PSTR("OK\r\n"));
	if(Lib_liveos -> autoexec) //если есть функция начальной загрузки то запускаем её
	{
		Lib_liveos -> console_out_str_P(PSTR("Run autoexec\r\n"));
		Lib_liveos -> autoexec();
	}

	Lib_liveos -> console_out_str_P(PSTR("liveOS started\r\n"));
	memset(Console_buffer, 0, sizeof(Console_buffer));
	Lib_liveos -> console_out_ch(CHAR_XON);
	Lib_liveos -> console_out_str_P(Cli_prompt_str);
}

void sys_input_console(void) //Данная функция встраивается в обработчик перывания входящих данных в UART
{
	char ch;
	uint8_t count;

	scanf("%c", &ch);

	count = push_queue(ch, Console_buffer, CONSOLE_BUF_LEN); // Полученный байт помещается в буфер
	if(count > CONSOLE_BUF_LEN - 4) //Если приближаемся к полному заполнению буфера, то останавливаем поток
		Lib_liveos -> console_out_ch(CHAR_XOFF); // Останавливаем поток, отправляет XOFF
}

void sys_main_loop(void) //Функция встраивается в основной цикл
{
	char in = 0;
	uint8_t count;

	if(Lib_liveos -> all_tasks[System_states.current_task - 1].status == RUNNING)
	{
		if(Lib_liveos -> all_tasks[System_states.current_task - 1].on_main_loop)
			Lib_liveos -> all_tasks[System_states.current_task - 1].on_main_loop(); //Запуск on_main_loop в текущем процессе
		Lib_liveos -> generate_actions(); //Запуск генератора событий
	}

	for(uint8_t i = 0; i < Lib_liveos -> count_tasks; i++) // Запуск  on_background во всех процессах
		if(Lib_liveos -> all_tasks[i].on_background && Lib_liveos -> all_tasks[i].status == RUNNING) //Если в данном процессе on_background определен и процесс запущен
			Lib_liveos -> all_tasks[i].on_background(((i + 1) == System_states.current_task) ? 1 : 0);


	count = pop_queue(&in, Console_buffer); //Извлекаем данные из буфера
	if(count) //Если в буфере консоли присутсвуют данные
	{
		if(count < 2) // Если в буфере осталось меньше трех байт
			Lib_liveos -> console_out_ch(CHAR_XON); // То включаем передачу

		if(System_states.console_redirect_to_proc) //Если поток данных необходимо перенапрвить в указанный процесс
		{
			if(in == CLOSE_REDIRECT_MODE_CHAR) // Если необходимо прекратить редирект консоли в процесс
			{
				System_states.console_redirect_to_proc = 0;
				Lib_liveos -> console_out_str_P(Cli_enter_str);
				Lib_liveos -> console_out_str_P(Cli_prompt_str);
			}
			else
				send_ch_to_process(in, System_states.console_redirect_to); // Перенаправляем все данные в указанный процесс
		}
		else //Если поток данных из консоли необходимо обрабатывать CLI
			scan_stdin(in);
	}
}

void stop_process(uint8_t task_id) //Остановить процесс
{
	if(!task_id) //Если указали 0 значит необходимо остановить все процессы
	{
		for(uint8_t i = 0; i < Lib_liveos -> count_tasks; i++)
		{
			if(Lib_liveos -> all_tasks[i].status == NOT_RUN || Lib_liveos -> all_tasks[i].status == STOPED)
				continue;

			if(Lib_liveos -> all_tasks[i].on_stop)
				Lib_liveos -> all_tasks[i].on_stop();
			Lib_liveos -> all_tasks[i].status = STOPED;
		}
		switch_process(DEFAULT_PROCESS);
		return;
	}

	if(Lib_liveos -> all_tasks[task_id - 1].status == NOT_RUN || Lib_liveos -> all_tasks[task_id - 1].status == STOPED)
		return;

	if(Lib_liveos -> all_tasks[task_id - 1].on_stop)
		Lib_liveos -> all_tasks[task_id - 1].on_stop();
	Lib_liveos -> all_tasks[task_id - 1].status = STOPED;
	switch_process(DEFAULT_PROCESS);
}


int8_t switch_process(uint8_t task_id) // Переключиться в указанный таск
{
	if(!task_id) //Нулевого процесса несуществует
		return 1;

	if(Lib_liveos -> all_tasks[task_id - 1].status != RUNNING)
		return 1;

	struct task_info ti;
	ti.from_task_id = System_states.current_task;
	System_states.current_task = task_id;
	Lib_liveos -> all_tasks[task_id - 1].on_active(&ti);

	return 0;
}

uint8_t get_active_proc_num(void) // Получить номер активной задачи
{
	return System_states.current_task;
}

int8_t get_process_status(uint8_t task_id) //Получить состояние процесса
{
	if(!task_id) //Нулевого процесса несуществует
		return -1;

	if(Lib_liveos -> all_tasks[task_id - 1].status == STOPED)
	{
		Lib_liveos -> all_tasks[task_id - 1].status = NOT_RUN;
		return STOPED;
	}

	return Lib_liveos -> all_tasks[task_id - 1].status;
}

uint8_t get_count_processes(void) //Получить общее количество процессов в системе
{
	return Lib_liveos -> count_tasks;
}

int send_signal(uint8_t task_id, uint8_t signal) // Послать сигнал указанному процессу
{
	if(!signal)
		return 1;

	if(!task_id) //Если указали 0 значит необходимо послать сигнал всем процессам
	{
		for(uint8_t i = 0; i < Lib_liveos -> count_tasks; i++)
		{
			if(Lib_liveos -> all_tasks[i].on_signal && Lib_liveos -> all_tasks[i].status == RUNNING) //Если обработчик сигнала определен в процессе получателе
				Lib_liveos -> all_tasks[i].on_signal(signal); // Запускаем обработчик
		}

		return 0;
	}

	if(Lib_liveos -> all_tasks[task_id - 1].on_signal && Lib_liveos -> all_tasks[task_id - 1].status == RUNNING) //Если обработчик сигнала определен в процессе получателе
		Lib_liveos -> all_tasks[task_id - 1].on_signal(signal); // Запускаем обработчик
	else
		return 1;

	return 0;
}

static void send_ch_to_process(char ch, uint8_t task_id) // Отправить символ в обработчик прибытия данных из консоли в указанный процесс
{
	if(task_id == 0) // Если процесс указан 0левой , значит отправить нужно текущему процессу
		task_id = System_states.current_task;

	if(Lib_liveos -> all_tasks[task_id - 1].on_console)
		Lib_liveos -> all_tasks[task_id - 1].on_console(ch);

	Lib_liveos -> console_out_ch(ch);
}


uint8_t get_end_word(char *str) // Функция возвращает указатель на конец строки, или 0 если конца строки ненайденно
{
	uint8_t i = 0;
	for(i = 0; i < CLI_LENGTH; i++)
		if(str[i] == CHAR_SPACE || str[i] == '\0')
			return i;

	return i;
}

uint8_t get_start_word(char *str) // Функция возвращает указатель на начало строки
{
	uint8_t i = 0;
	for(i = 0; i < CLI_LENGTH; i++)
		if(str[i] != CHAR_SPACE || str[i] == '\0')
			return i;

	return i;
}

static uint8_t parse_cli_command(void) // Попытка распознать введенную команду и запустить её
{
	uint8_t i;
	char *command;
	char *params;

	i = get_end_word(Cli_buffer); //Определяем есть ли параметры к команде и если есть то определяем конец строки с именем команды
	if(i < strlen(Cli_buffer)) //Если присутсвуют параметры
	{
		strncpy(Cli_temp, Cli_buffer, i); // Копируем команду в спец. буфер
		command = Cli_temp;
		params = Cli_buffer + i + get_start_word(Cli_buffer + i);
	}
	else // Если параметров нет
	{
		command = Cli_buffer;
		params = NULL;
	}

	for(i = 0; i < COUNT_SYS_COMMANDS ; i++) // Перебираем все команды
		if(!strcasecmp_P(command, Sys_commands[i].cmd_name)) // Если нашли подходящую пкоманду то запускаем её
		{
			Sys_commands[i].cmd_callback(params);
			return 0;
		}

	if(Lib_liveos -> external_cmd && Lib_liveos -> external_cmd_count) //Если определены внешние команды
	{
		for(i = 0; i < Lib_liveos -> external_cmd_count ; i++) // Перебираем все внешние команды
			if(!strcasecmp_P(command, Lib_liveos -> external_cmd[i].cmd_name)) // Если нашли подходящую пкоманду то запускаем её
			{
				Lib_liveos -> external_cmd[i].cmd_callback(params);
				return 0;
			}
	}

	return 1; //Если неодной команды найденно небыло, то возвращаем ошибку
}

static void scan_stdin(char ch) //Функция слушает консоль и осуществляет командно строчный интерфейс
{
	uint8_t cmd_len, i;
	switch(ch)
	{
		case CHAR_ENTER:
			Lib_liveos -> console_out_str_P(Cli_enter_str);
			if(Cli_buffer[0] && parse_cli_command())
				Lib_liveos -> console_out_str_P(Cli_nosuchcommand_str);

			memset(Cli_buffer, '\0', CLI_LENGTH);
			memset(Cli_temp, '\0', CLI_LENGTH);
			if(!System_states.console_redirect_to_proc) //Если неактивирован режим редиректа консоли на процесс
				Lib_liveos -> console_out_str_P(Cli_prompt_str);
		break;

		case CHAR_TAB: //Если нажали TAB, то подставляем в консоль подходяшую команду
			cmd_len = strlen(Cli_buffer);
			for(i = 0; i < COUNT_SYS_COMMANDS ; i++) // Перебираем все команды
				if(!strncmp_P(Cli_buffer, Sys_commands[i].cmd_name, cmd_len)) // если нашли совпадение с внутренней командой
				{
					strcpy_P(Cli_buffer, Sys_commands[i].cmd_name);
					for(i  = cmd_len; i < strlen(Cli_buffer); i++)
						Lib_liveos -> console_out_ch(Cli_buffer[i]);
					return;
				}

			if(Lib_liveos -> external_cmd && Lib_liveos -> external_cmd_count) //Если определены внешние команды
				for(i = 0; i < Lib_liveos -> external_cmd_count ; i++) // Перебираем все внешние команды
					if(!strncmp_P(Cli_buffer, Lib_liveos -> external_cmd[i].cmd_name, cmd_len)) // если нашли совпадение с внешний командой
					{
						strcpy_P(Cli_buffer, Lib_liveos -> external_cmd[i].cmd_name);
						for(i  = cmd_len; i < strlen(Cli_buffer); i++)
							Lib_liveos -> console_out_ch(Cli_buffer[i]);
						return;
					}
		break;

		case CHAR_BACKSPACE:
		case CHAR_BACKSPACE_ALT:
			if(strlen(Cli_buffer))
			{
				Cli_buffer[strlen(Cli_buffer) - 1] = 0;
				Lib_liveos -> console_out_str_P((char const *)&Cli_backspace_str);
			}
		break;

		default:
			if(strlen(Cli_buffer) < CLI_LENGTH)
			{
				Cli_buffer[strlen(Cli_buffer)] = ch;
				Lib_liveos -> console_out_ch(ch);
			}
	}
}

int8_t find_process_by_name(char *name) //Найти номер процесса по имени
{
	uint8_t i;
	for(i = 0; i < Lib_liveos -> count_tasks ; i++) // Перебираем все команды
		if(!strcasecmp_P(name, Lib_liveos -> all_tasks[i].name)) // Если нашли указанный в параметре процесс
			return i + 1;

	return -1;
}

char * copy_cli_parameter(char *params, char *parameter) //Функция копирует первый параметр из строки с параметрами params в буффер parameter. Функция возвращает указатель на следующий параметр.
{
	uint8_t i, p;
	i = get_start_word(params); //Находим начало параметра
	if(i >= strlen(params)) //Если ненайден параметр
		return NULL;

	p = i;
	i = get_end_word(params + p); //Определяем конец параметра
	strncpy(parameter, params + p, i); // Копируем параметр в во временный буфер
	parameter[i] = 0; //помечаем конец строки параметра
	p += i;

	return params + p;
}

// Внутренние комнады ОС
static void cmd_help(char *params) //Вывести памятку
{
	uint8_t i;

	params = copy_cli_parameter(params, Cli_temp);
	if(params) //Если передан параметр с именем интересующей команды
	{
		for(i = 0; i < COUNT_SYS_COMMANDS ; i++) // Перебираем все команды
		{
			asm("wdr");
			if(!strcasecmp_P(Cli_temp, Sys_commands[i].cmd_name)) //если нашли команду информацию о которой необходимом вывести
			{
				Lib_liveos -> console_out_str_P(Cli_enter_str);
				Lib_liveos -> console_out_str_P(Sys_commands[i].cmd_name);
				Lib_liveos -> console_out_str_P(PSTR(" - "));
				Lib_liveos -> console_out_str_P(Sys_commands[i].cmd_help);
				Lib_liveos -> console_out_str_P(Cli_enter_str);
				return;
			}
		}

		if(Lib_liveos -> external_cmd && Lib_liveos -> external_cmd_count) //Если определены внешние команды
		{
			for(i = 0; i < Lib_liveos -> external_cmd_count ; i++) // Перебираем все внешние команды
			{
				asm("wdr");
				if(!strcasecmp_P(Cli_temp, Lib_liveos -> external_cmd[i].cmd_name)) //если нашли команду информацию о которой необходимом вывести
				{
					Lib_liveos -> console_out_str_P(PSTR("\r\nExternal command: "));
					Lib_liveos -> console_out_str_P(Lib_liveos -> external_cmd[i].cmd_name);
					Lib_liveos -> console_out_str_P(PSTR(" - "));
					Lib_liveos -> console_out_str_P(Lib_liveos -> external_cmd[i].cmd_help);
					Lib_liveos -> console_out_str_P(Cli_enter_str);
					return;
				}
			}
		}
	}

	for(i = 0; i < COUNT_SYS_COMMANDS ; i++) // Перебираем все команды
	{
		asm("wdr");
		Lib_liveos -> console_out_str_P(Cli_enter_str);
		Lib_liveos -> console_out_str_P(PSTR("\t'"));
		Lib_liveos -> console_out_str_P(Sys_commands[i].cmd_name);
		Lib_liveos -> console_out_str_P(PSTR("'\t - "));
		Lib_liveos -> console_out_str_P(Sys_commands[i].cmd_help);
	}

	if(Lib_liveos -> external_cmd && Lib_liveos -> external_cmd_count) //Если определены внешние команды
	{
		asm("wdr");
		Lib_liveos -> console_out_str_P(PSTR("\r\n\r\nExternal commands: "));

		for(i = 0; i < Lib_liveos -> external_cmd_count ; i++) // Перебираем все внешние команды
		{
			Lib_liveos -> console_out_str_P(Cli_enter_str);
			Lib_liveos -> console_out_str_P(PSTR("\t'"));
			Lib_liveos -> console_out_str_P(Lib_liveos -> external_cmd[i].cmd_name);
			Lib_liveos -> console_out_str_P(PSTR("'\t - "));
			Lib_liveos -> console_out_str_P(Lib_liveos -> external_cmd[i].cmd_help);
		}
	}

	Lib_liveos -> console_out_str_P(Cli_enter_str);
}

static void cmd_ps(char *params) //Вывести список всех процессов и их состояние
{
	uint8_t i;

	Lib_liveos -> console_out_str_P(Cli_enter_str);
	for(i = 0; i < Lib_liveos -> count_tasks; i++) // Перебираем все процессы
	{
		asm("wdr");
		Lib_liveos -> console_out_str_P(Lib_liveos -> all_tasks[i].name);
		switch(Lib_liveos -> all_tasks[i].status)
		{
			case NOT_RUN:
				Lib_liveos -> console_out_str_P(Cli_proc_state_not_run_str);
			break;

			case STOPED:
				Lib_liveos -> console_out_str_P(Cli_proc_state_stopped_str);
			break;

			case RUNNING:
				if((i + 1) == System_states.current_task)
					Lib_liveos -> console_out_str_P(Cli_proc_state_active_str);
				else
					Lib_liveos -> console_out_str_P(Cli_proc_state_running_str);
			break;
		}

		Lib_liveos -> console_out_str_P(PSTR(" - "));

		if(Lib_liveos -> all_tasks[i].on_init)
			Lib_liveos -> console_out_str_P(PSTR("init"));

		Lib_liveos -> console_out_str_P(PSTR(":"));
		if(Lib_liveos -> all_tasks[i].on_run)
			Lib_liveos -> console_out_str_P(PSTR("run"));

		Lib_liveos -> console_out_str_P(PSTR(":"));
		if(Lib_liveos -> all_tasks[i].on_stop)
			Lib_liveos -> console_out_str_P(PSTR("stop"));

		Lib_liveos -> console_out_str_P(PSTR(":"));
		if(Lib_liveos -> all_tasks[i].on_active)
			Lib_liveos -> console_out_str_P(PSTR("active"));

		Lib_liveos -> console_out_str_P(PSTR(":"));
		if(Lib_liveos -> all_tasks[i].on_main_loop)
			Lib_liveos -> console_out_str_P(PSTR("main"));

		Lib_liveos -> console_out_str_P(PSTR(":"));
		if(Lib_liveos -> all_tasks[i].on_background)
			Lib_liveos -> console_out_str_P(PSTR("bg"));

		Lib_liveos -> console_out_str_P(PSTR(":"));
		if(Lib_liveos -> all_tasks[i].on_signal)
			Lib_liveos -> console_out_str_P(PSTR("sig"));

		Lib_liveos -> console_out_str_P(PSTR(":"));
		if(Lib_liveos -> all_tasks[i].on_console)
			Lib_liveos -> console_out_str_P(PSTR("console"));

		Lib_liveos -> console_out_str_P(Cli_enter_str);
	}
}

static void cmd_switchto(char *params)// Команда преключиться в процесс
{
	int8_t i;
	if(!params) //Если отсутсвуют параметры
	{
		Lib_liveos -> console_out_str_P(Cli_incorrect_str);
		return;
	}

	copy_cli_parameter(params, Cli_temp); // Копируем параметр в во временный буфер
	i = find_process_by_name(Cli_temp);
	if(i == -1)
	{
		Lib_liveos -> console_out_str_P(Cli_process_not_found_str);
		return;
	}

	switch_process(i);
}

static void cmd_run(char *params) // Команда запустиь процесс
{
	int8_t process_num;
	if(!params) //Если отсутсвуют параметры
	{
		Lib_liveos -> console_out_str_P(Cli_incorrect_str);
		return;
	}

	params = copy_cli_parameter(params, Cli_temp); // Копируем параметр в во временный буфер
	process_num = find_process_by_name(Cli_temp);
	if(process_num == -1)
	{
		Lib_liveos -> console_out_str_P(Cli_process_not_found_str);
		return;
	}

	run_process(process_num, params);
}

static void cmd_runbg(char *params) // Команда запустиь процесс  в фоне
{
	int8_t process_num;
	if(!params) //Если отсутсвуют параметры
	{
		Lib_liveos -> console_out_str_P(Cli_incorrect_str);
		return;
	}

	params = copy_cli_parameter(params, Cli_temp); // Копируем параметр в во временный буфер
	process_num = find_process_by_name(Cli_temp);
	if(process_num == -1)
	{
		Lib_liveos -> console_out_str_P(Cli_process_not_found_str);
		return;
	}

	copy_cli_parameter(params, Cli_temp); // Копируем параметр в во временный буфер
	run_bg_process(process_num, Cli_temp);
}

static void cmd_stop(char *params) // Команда остановить процесс
{
	int8_t i;
	if(!params) //Если отсутсвуют параметры
	{
		Lib_liveos -> console_out_str_P(Cli_incorrect_str);
		return;
	}

	params = copy_cli_parameter(params, Cli_temp); // Копируем параметр в во временный буфер
	i = find_process_by_name(Cli_temp);
	if(i == -1)
	{
		Lib_liveos -> console_out_str_P(Cli_process_not_found_str);
		return;
	}

	stop_process(i);
}

static void cmd_sig(char *params) // Команда послать сигнал процессу
{
	int8_t process_num;
	if(!params) //Если отсутсвуют параметры
	{
		Lib_liveos -> console_out_str_P(Cli_incorrect_str);
		return;
	}

	params = copy_cli_parameter(params, Cli_temp); // Копируем параметр в во временный буфер
	process_num = find_process_by_name(Cli_temp);
	if(process_num == -1)
	{
		Lib_liveos -> console_out_str_P(Cli_process_not_found_str);
		return;
	}

	params = copy_cli_parameter(params, Cli_temp); // Копируем параметр в во временный буфер
	if(params == NULL)
	{
		Lib_liveos -> console_out_str_P(Cli_signal_not_recognized_str);
		return;
	}

	if(send_signal(process_num, atoi(Cli_temp)))
		Lib_liveos -> console_out_str_P(Cli_signal_not_received_str);
}

static void cmd_send(char *params) // Команда перключить консоль в процесс
{
	int8_t process_num;
	System_states.console_redirect_to = 0;
	if(params) //Если присутсвует параметр
	{
		strncpy(Cli_temp, params, get_end_word(params)); // Копируем параметр в во временный буфер

		process_num = find_process_by_name(Cli_temp); // Пытаемся найти указанный процесс
		if(process_num == -1)
		{
			Lib_liveos -> console_out_str_P(Cli_process_not_found_str);
			return;
		}
		else
			System_states.console_redirect_to = process_num;
	}
	System_states.console_redirect_to_proc = 1;
}

static void cmd_init(char *params) // Команда "проинициализировать процесс"
{
	int8_t process_num;
	if(!params) //Если отсутсвуют параметры
	{
		Lib_liveos -> console_out_str_P(Cli_incorrect_str);
		return;
	}

	params = copy_cli_parameter(params, Cli_temp); // Копируем параметр в во временный буфер
	process_num = find_process_by_name(Cli_temp);
	if(process_num == -1)
	{
		Lib_liveos -> console_out_str_P(Cli_process_not_found_str);
		return;
	}

	init_process(process_num);
}

static void cmd_reboot(char *params) // Команда "перзагрузка"
{
	Lib_liveos -> console_out_str_P(PSTR("Rebooting...\r\n\r\n"));
	for(int i = 0; i < Lib_liveos -> count_tasks; i++) // Остановка всех процессов
		stop_process(i);

	Lib_liveos -> reboot();
}

