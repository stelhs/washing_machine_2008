/*
	Мини-операционная система LiveOS

	Для установки системы необходимо проинициализировать структуру struct lib_liveos а также
	проинициализировать список всех процессов в системе struct task *all_tasks.
	После инициализации всех драйверов запустить init_liveos(struct lib_liveos).
	Автор: Михаил Курочкин
	e-mail: stelhs@ya.ru
*/

#ifndef LIB_LIVEOS_H
#define LIB_LIVEOS_H

#include <stdint.h>
#include <avr/pgmspace.h>

#define SET_CALLBACK(process, action)	.on_##action = process##_##action // макрос генерации имени обработчика для использования в структуре инициализации процесса
#define CALLBACK_NAME(process, action) void process##_##action // макрос генерирующий имя функции обработчика события

#define CLI_LENGTH 80 //Длина буфера консоли

#define DEFAULT_PROCESS 1

// Создать строку в программной памяти
#define CREATE_PGM_STR(num, str)    \
    const char __str_pgm_##num[] PROGMEM = {str} \

// Получить созданную ранее строку
#define GET_MESS_POINTER(num)   \
    (const char *)(__str_pgm_##num)

struct cmd_command
{
	const char *cmd_name; //Имя команды
	void (*cmd_callback)(char *); // Обработчик команды
	const char *cmd_help; //Указатель на описание команды
};

enum task_status //Состояние процесса, если он остановлен то on_background незапускается
{
	NOT_RUN, // Процесс незапущен
	STOPED,  // Процесс только что остановлен и незапущен в данный момент
	RUNNING, // Процесс запущен
};

struct task_info // Структура с информацией о процессе вызвавшем switch_process
{
	uint8_t from_task_id; // Номер процесса с которого произошол переход
};

struct task
{
	const char *name; // Название процесса
	uint8_t status; // Состояние процесса
	void (*on_init)(void); //Инициализация процесса
	void (*on_run)(char *); //Запуск процесса
	void (*on_stop)(void); //Остановка процесса
	void (*on_active)(struct task_info *from); // Функция запускается когда процесс активируется
	void (*on_main_loop)(void); // Встраивание в основной цикл на время активности
	void (*on_background)(uint8_t); // Встраивание в основной цикл пока процесс запущен
	void (*on_signal)(uint8_t); // Если пришол сигнал то запускаем обработчик сигнала
	void (*on_console)(char);  // Обработчик событие по прибытию данных в консоль
};

struct lib_liveos
{
	struct task *all_tasks; //Список всех процессов
	uint8_t count_tasks; // Количество процессов
	void (*generate_actions)(void); // Генератор событий
	char (*console_in)(void); //Драйвер консоли (вход)
	void (*console_out_ch)(char); //Драйвер консоли вывод одного символа
	void (*console_out_str)(char *); //Драйвер консоли вывод строки
	void (*console_out_str_P)(const char *); //Драйвер консоли для вывода строк из программной памяти
	struct cmd_command *external_cmd; //Указатель на внешние команды
	uint8_t external_cmd_count; //Количество внешних команд
	void (*autoexec)(void); // Автозагрузка
	void (*reboot)(void);   //Вызов перезагрузки
};

void init_liveos(struct lib_liveos *init);

void sys_input_console(void); //Данная функция встраивается в обработчик перывания входящих данных в UART

void sys_main_loop(void);

void run_process(uint8_t task_id, char *params);

int8_t run_bg_process(uint8_t task_id, char *params);

void stop_process(uint8_t task_id);

int8_t switch_process(uint8_t task_id);

int send_signal(uint8_t task_id, uint8_t signal);

uint8_t get_active_proc_num(void); // Получить номер активной задачи

int8_t get_process_status(uint8_t task_id); //Получить состояние процесса

uint8_t get_count_processes(void); //Получить общее количество процессов в системе

char * copy_cli_parameter(char *params, char *parameter); //Функция копирует первый параметр из строки с параметрами params в буффер parameter. Функция возвращает указатель на следующий параметр.

uint8_t get_end_word(char *str); // Функция возвращает указатель на конец строки, или 0 если конца строки ненайденно

uint8_t get_start_word(char *str); // Функция возвращает указатель на начало строки

#endif //LIB_LIVEOS_H

