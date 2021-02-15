/*
	Данный модуль недописан и не используется

*/


#include <avr/interrupt.h>
#include <stdio.h>
#include <avr/eeprom.h>
#include "config.h"
#include "../drivers.h"
#include "../lib.h"
#include "lib_liveos.h"// Описание Микро ОС

extern struct machine_states Machine_states; // Состояние всей машины
extern struct washing_settings Washing_settings;

struct
{
	uint8_t prev_ext_power; // Хранит предыдущее состояние питания 220V, нужно чтобы иметь возможность отловить перепад в ноль
}Restore_states;

static void save_settings(void) //Сохранить настройки меню
{
	eeprom_write_block(&Washing_settings, (void *)EEPROM_ADDRESS_MENU_SETTINGS, sizeof(struct washing_settings));
}

static void save_all(void) //Сохранить  все состояние
{
	uint8_t i;
	uint8_t count_runned_processes = 0;
	
	eeprom_write_byte((uint8_t *)EEPROM_ADDRESS_POWER_MGR,  'R'); //Сохраняем флаг аварийного завершения
	for(i = 0; i < get_count_processes(); i++)
	{
		if(get_process_status(i + 1) == RUNNING) //Если данный процесс запущен
		{
			eeprom_write_byte((uint8_t *)EEPROM_ADDRESS_POWER_MGR + 1 + count_runned_processes,  i + 1); //То сохраняем его номер
			count_runned_processes++;
		}
	}
	eeprom_write_byte((uint8_t *)EEPROM_ADDRESS_POWER_MGR + 1 + count_runned_processes,  0); //0ль означает что конец секции с номерами за\пущенных процессов
	eeprom_write_byte((uint8_t *)EEPROM_ADDRESS_POWER_MGR + 1 + count_runned_processes + 1,  get_active_proc_num()); // Сохраняем номер активного процесса
	
}

static void restore_machine(void) //Восстановить состояние машины до момента перебоя питания
{
	uint8_t i, proc_num = 0;
	
	i = eeprom_read_byte((uint8_t *)EEPROM_ADDRESS_POWER_MGR);
	
	if(i != 'R') // Проверяем установлен ли флаг означающий необходимость аварийного восстановление после сбоя питания
		return; //Если флага нет то и восстанавливать нечего
		
	for(i = 0; i < get_count_processes(); i++)
	{
		proc_num = eeprom_read_byte((uint8_t *)EEPROM_ADDRESS_POWER_MGR + 1 + i);
		if(proc_num == 0)
			break;
			
		run_bg_process(proc_num, " -restore"); //Запускаем процесс с ключом -restore
	}
	
	proc_num = eeprom_read_byte((uint8_t *)EEPROM_ADDRESS_POWER_MGR + 1 + i + 1); //Считываем номер активного процесса
	eeprom_write_byte((uint8_t *)EEPROM_ADDRESS_POWER_MGR, 0); // Стираем метку аварийного завершения
	
	printf("switch_process %d\r\n", proc_num);
	switch_process(proc_num); // Активируем процесс который был активным до сбоя
}

CALLBACK_NAME(PROCESS_POWER_MANAGER, init)(void) // Инициализация процесса
{
	Restore_states.prev_ext_power = 1;
}

CALLBACK_NAME(PROCESS_POWER_MANAGER, run)(char *params)
{
	printf("eeprom_read_block\r\n");
	eeprom_read_block(&Washing_settings, (void *)EEPROM_ADDRESS_MENU_SETTINGS, sizeof(struct washing_settings)); // Считываем все настройки с EEPROM
	printf("finished\r\n");
}

CALLBACK_NAME(PROCESS_POWER_MANAGER, background)(uint8_t is_active)
{
	if((Machine_states.ext_power == 0 && Restore_states.prev_ext_power > 0) && Machine_states.main_power) //Если неожиданно пропало питание системы
	{
	/*	PORTA = 0;
		PORTB = 0;
		PORTC = 0;
		PORTD = 0;
		PORTE = 0;
		PORTF = 0;
		PORTG = 0;
		save_all();
		reboot();*/
	}
	
	Restore_states.prev_ext_power = Machine_states.ext_power;
}

CALLBACK_NAME(PROCESS_POWER_MANAGER, signal)(uint8_t signal) // Обработчик входящего сигнала
{
	switch(signal)
	{
		case 1:  // Сигнал означающий что необходимо сохранить настройки меню
			save_settings();
		break;
		
		case 2: //Данный сигнал заставляет проверить необходимость аварийного восстановления после сбоя
			printf("receive signal 2\r\n");
			restore_machine();
		break;
		
		case 4:
			save_all();
		break;
	}
}


