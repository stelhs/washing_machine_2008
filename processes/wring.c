#include <stdio.h>
#include <avr/eeprom.h>
#include "../lib.h"
#include "../messages.h"
#include "../os.h"
#include "lib_liveos.h"
#include "../config.h"
#include "config.h"
#include "../lcd_buffer.h"
#include "proc_lib.h"

#define COUNT_STEPS 4

extern struct machine_states Machine_states; // Состояние всей машины
extern struct time Main_current_time; // Часы реального времени
extern struct washing_settings Washing_settings; // Нстройки регулируемые с меню

// Список лементов меню видимость которых необходимо инвертировать при запуске и остановке
uint8_t trigger_run_stop_wring[] = {
															43,  // Отжать
															41, // Максимальная скорость
															42, // Время отжима
															25, // Полоскание
															24, // Управление водой
															44, // Остановить отжим
															2, // Замачивание
															7, // Запуск стирки
														};

// Список лементов меню видимость которых необходимо инвертировать при входе и выходе из паузы
uint8_t trigger_pause_wring[] = {
														41, // Максимальная скорость
														42, // Время отжима
														24, // Управление водой
													};

enum steps_wring
{
	NORMAL_WRING, // Режим нормального отжиима
	CANCEL_WRING // Режим отмены отжиима
};

struct 
{
	uint8_t step : 2; // Текуший шаг операции отжиима
	uint8_t pause : 1; // Бит выставлен в случае пользовательской паузы
	uint8_t cancel : 1; // Бит выставлен в случае начала процесса принудительного завершения отжиима
	uint16_t calc_work_time; //Расчетное время работы в минутах
	struct time start_time;
	struct time work_time;
	struct time calculate_time;
	uint8_t finished : 1; // Если процесс успешно доделал все доконца то выставляется данный флаг, для того чтобы обработчик события stop_process мог понять вызван он аварийно или планово
}Wring_states;


static void wring_pause(void) //Функция постановки/снятия паузы
{
	if(!Wring_states.cancel)
	{
		static uint8_t prev_state;
		if(!Wring_states.pause) // Постановка на паузу
		{
			Wring_states.pause = 1;
			Machine_states.pause = 1;
			draw_pause(1);
			prev_state = Machine_states.current_machine_state; // Сохраняем состояние светодиодов до паузы
			Machine_states.current_machine_state = LED_PAUSE;
			block_door(OFF); // Разблокируем дверку
		}
		else
		{
			Machine_states.wring_speed = Washing_settings.wring_wring_speed; //Устанавливаем требуемую изначально скорость отжима
			Wring_states.pause = 0;
			draw_operation_name(NULL, 1);
			Machine_states.current_machine_state = prev_state; // Возвращаем то состояние светодиодов которое было до паузы
			Machine_states.pause = 0;
			block_door(ON); // Блокируем дверку
		}
		triggered_menu_items(trigger_pause_wring, sizeof(trigger_pause_wring));
	}
}

static void finish_wring(void) //Процедура завершения полоскания
{
	if(Wring_states.pause) //Если установленна пауза, то снимаем её
		wring_pause();
	triggered_menu_items(trigger_run_stop_wring, sizeof(trigger_run_stop_wring));
	stop_fill_tank_and_stir_powder();
	stop_rinse();
	stop_wring();
	stop_mode_rotating();
	Wring_states.finished = 1;
	stop_process(PROCESS_WRING);
}


CALLBACK_NAME(PROCESS_WRING, init)(void) // Инициализация процесса полоскания
{
	uint8_t i = 0;
	Wring_states.step = NORMAL_WRING;
	//i = eeprom_read_byte((uint8_t *)EEPROM_ADDRESS_WRING);
	if(i == 'R') // Проверяем установлен ли флаг означающий аварийное восстановление после сбоя питания
	{
		run_process(PROCESS_WRING, NULL); //Запускаем сами себя.
		//eeprom_write_byte((uint8_t *)EEPROM_ADDRESS_WRING, 0); // Стираем инфу об аварийном завершении
	}
}

CALLBACK_NAME(PROCESS_WRING, run)(char *params)
{
	send_signal(PROCESS_POWER_MANAGER, 1); //Посылаем сигнал процессу PROCESS_POWER_MANAGER чтобы тот сохранил в EEPROM все параметры стирки

	// При старте вырубаем все операции
	stop_process(PROCESS_RINSE);
	stop_process(PROCESS_BEDABBLE);
	stop_process(PROCESS_WASHING);

	triggered_menu_items(trigger_run_stop_wring, sizeof(trigger_run_stop_wring));

	stop_fill_tank_and_stir_powder();
	stop_rinse();
	stop_wring();
	stop_mode_rotating();

	Wring_states.step = NORMAL_WRING;
	Wring_states.calc_work_time = 0;

	// Расчет времени окончания полоскания
	Wring_states.calc_work_time += 
	+ 5 // Усредненное время на балансировку перед отжимом
	+ Washing_settings.wring_wring_time; // Время отжима
	
	Wring_states.cancel = 0;
	Machine_states.pause = 0;
	
	Wring_states.start_time = Main_current_time;
	Wring_states.calculate_time = Wring_states.start_time;
	time_add(&Wring_states.calculate_time, Wring_states.calc_work_time);
	draw_calculate_time(&Wring_states.calculate_time, 0);
	Wring_states.finished = 0;
	power_on_system(); //Включаем основное питание системы
	block_door(ON); // Блокируем дверку
}

CALLBACK_NAME(PROCESS_WRING, active)(struct task_info *from)
{
	clr_lcd_buf();
	draw_current_time(&Main_current_time, 1);
	draw_center_pointer();
	draw_calculate_time(NULL, 1);
	draw_work_time(NULL, 1);
	draw_prog_name(GET_MESS_POINTER(MENU_ITEM_WRING), 1);  //Выводим название "Отжим"
	draw_step_name(NULL, 0, 0, 1);
	draw_operation_name(NULL, 1);
	if(Wring_states.pause) // Если процесс в паузе
		draw_pause(1);
}

CALLBACK_NAME(PROCESS_WRING, main_loop)(void)
{
	if(Machine_states.change_current_time) // Отрисовываем время
	{
		draw_current_time(&Main_current_time, 1);
		draw_work_time(&Wring_states.work_time, 1);
		Machine_states.change_current_time = 0;
	}
}

CALLBACK_NAME(PROCESS_WRING, background)(uint8_t is_active)
{
	static uint8_t prev_wring_display_status = 0;
	
	switch(Wring_states.step)
	{
		case NORMAL_WRING:
			if(!Machine_states.running_wring && !Machine_states.wring_finish) //Если полоскание незапущенно
				run_wring(Washing_settings.wring_wring_speed, Washing_settings.wring_wring_time * 60, 0);
			
				if(Machine_states.running_wring) // Если отжим уже запущен
				{
					if(prev_wring_display_status != Machine_states.wring_display_status)
					{
						switch(Machine_states.wring_display_status)
						{
							case 1: // Распределяем белье
								draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 1, COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(STIR_DRESS_MESSAGE), is_active); // Выводим на экран "Распределяю белье"
							break;
								
							case 2: // Балансировка
								draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 2, COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(BALANSING_MESSAGE), is_active); // Выводим на экран "Балансирую бельё"
							break;
							
							case 3: // Всполаскивание
								draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 3, COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(RINSING_MESSAGE), is_active); // Выводим на экран "Полоскаю..."
							break;
							
							case 4: // Отжимаем на нужной скорости
								draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 4, COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(BEST_WRINGING_MESSAGE), is_active); // Выводим на экран "Отлично отжимаю..."
							break;

							case 5: // Отжимаем на крайне низкой скорости
								draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 4, COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(BAD_WRINGING_MESSAGE), is_active); // Выводим на экран "Плохо отжимаю..."
							break;

							case 6: // Отжимаем на низкой скорости
								draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 4, COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(GOOD_WRINGING_MESSAGE), is_active); // Выводим на экран "Нормально отжимаю..."
							break;

							case 7: // Отжимаем на низкой скорости
								draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 4, COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(GOOD_WRINGING_MESSAGE), is_active); // Выводим на экран "Нормально отжимаю..."
							break;
						}
						prev_wring_display_status = Machine_states.wring_display_status;
					}
				}
			
				switch(Machine_states.wring_finish) // Анализируем результат работы "отжима"
				{
					case WRINGED_ERROR:
					case WRINGED_OK:
					case WRINGED_LOW_SPEED:
					case WRINGED_MIN_SPEED:
						{
							draw_operation_name(GET_MESS_POINTER(WRING_FINISH_MESSAGE), 0); // Выводим на экран "Отжим завершен"
							draw_calculate_time(&Main_current_time, 0); //Отрисовываем время завершения
							send_signal(PROCESS_SCREENSAVER, 3); //Посылаем в скринсейвер сигнал отрисовать результаты полоскания
							finish_wring();
						}
					break;
				}
		break;
		
		case CANCEL_WRING:
			if(!Wring_states.cancel)
			{
				stop_wring();
				Wring_states.cancel = 1;
				draw_operation_name(GET_MESS_POINTER(WRING_CANCELING_MESSAGE), is_active); // Выводим на экран "Отмена отжима"
				if(!Machine_states.drum_is_stopping) //Если барабан не находится в режиме остановки то сразу переходим на этап завершения
					Machine_states.drum_is_stopped = 1;
			}
			
			if(Machine_states.drum_is_stopped) //Дождались остановки барабана
			{
				Machine_states.drum_is_stopped = 0;
					// TODO: Разблокировали дверку
				Wring_states.step = NORMAL_WRING;
				Wring_states.cancel = 0;
				draw_operation_name(GET_MESS_POINTER(WRING_CANCEL_MESSAGE), 0); // Выводим на экран "Отжим прерван"
				draw_calculate_time(&Main_current_time, 0); //Отрисовываем время завершения
				send_signal(PROCESS_SCREENSAVER, 3); //Посылаем в скринсейвер сигнал отрисовать результаты замачивания
				finish_wring();
			}
		break;
	}
	
	if(Machine_states.main_power && Machine_states.ext_power == 0 && !Wring_states.pause) // если пропало питание 220v
	{
		wring_pause(); // ставим паузу
		sys_error(ERROR, GET_MESS_POINTER(ERROR_NOT_POWER_220)); // Нет питания 220v
	}
	
	if(Machine_states.error && !Wring_states.pause) // Если случилась глобальная ошибка, то ставим все на паузу
		wring_pause(); // ставим паузу
}

CALLBACK_NAME(PROCESS_WRING, click_esc)(void)
{
	switch_process(PROCESS_MENU); //Вход в меню
}

CALLBACK_NAME(PROCESS_WRING, click_enter)(void)
{
	wring_pause();
}

CALLBACK_NAME(PROCESS_WRING, timer0)(void)
{
	if(!Wring_states.pause) //Увеличиваем рабочее время на одну секунду если пауза не активированна
		inc_time(&Wring_states.work_time);
}

CALLBACK_NAME(PROCESS_WRING, signal)(uint8_t signal)
{
	switch(signal)
	{
		case 5: //Сигнал, остановить полоскание
			if(Wring_states.pause) //Если в данный момент установленна пауза то снимаем её
				wring_pause();
				
			Wring_states.step = CANCEL_WRING;
		break;
	}
}

CALLBACK_NAME(PROCESS_WRING, console)(char input_byte)
{
	switch(input_byte)
	{
		case '7':
			switch_process(PROCESS_MENU); //Вход в меню
		break;

		case '6':
			wring_pause(); //Пауза
		break;
	}
}

CALLBACK_NAME(PROCESS_WRING, stop)(void)
{
	block_door(OFF); // Разблокируем дверку
	if(Wring_states.finished == 1) //Если процесс завершается планово
		return;
	
//	eeprom_write_byte((uint8_t *)EEPROM_ADDRESS_WRING , 'R'); //Записываем ключ 'R' (вообще он от болды, нужен чтобы при чтении хоть немного быть уверенным что данные адекватные)
	stop_wring();
}


