#include <stdio.h>
#include "../lib.h"
#include "../messages.h"
#include "../os.h"
#include "lib_liveos.h"
#include "../config.h"
#include "config.h"
#include "../lcd_buffer.h"
#include "proc_lib.h"

extern struct machine_states Machine_states; // Состояние всей машины
extern struct time Main_current_time; // Часы реального времени
extern struct washing_settings Washing_settings; // Нстройки регулируемые с меню

// Список лементов меню видимость которых необходимо инвертировать при запуске и остановке
uint8_t trigger_run_stop_rinse[] = {
															36, // Кол-во полосканий
															37, // Режим полоскания
															45, // Источник воды
															38, // Полоскать
															40, // Отжим
															24, // Управление водой
															39, //  остановить полоскание
															2, // Замачивание
															7, // Запуск стирки
														};

// Список лементов меню видимость которых необходимо инвертировать при входе и выходе из паузы
uint8_t trigger_pause_rinse[] = {
														36, // Кол-во полосканий
														37, // Режим полоскания
														45, // Источник воды
														24, // Управление водой
													};

enum steps_rinse
{
	WAIT_CLOSE_DOOR, // Ожидание закрывания дверки
	NORMAL_RINSE, // Режим нормального полоскания
	CANCEL_RINSE // Режим отмены полоскания
};

struct 
{
	uint8_t step : 2; // Текуший шаг операции полоскания
	uint8_t ret_step : 2; // Этап в который необходимо будет перейти. (используется для перехода в следующий этап после проверки закрытости люка)
	uint8_t pause : 1; // Бит выставлен в случае пользовательской паузы
	uint8_t cancel : 1; // Бит выставлен в случае начала процесса принудительного завершения полоскания
	uint16_t calc_work_time; //Расчетное время работы в минутах
	uint8_t door_timer; // Таймер ожидания закрывания люка
	uint8_t count; // Номер текущего полоскания
	struct time start_time;
	struct time work_time;
	struct time calculate_time;
}Rinse_states;

static void rinse_pause(void) //Функция постановки/снятия паузы
{
	if(!Rinse_states.cancel)
	{
		static uint8_t prev_state;
		if(!Rinse_states.pause) // Постановка на паузу
		{
			Rinse_states.pause = 1;
			Machine_states.pause = 1;
			draw_pause(1);
			prev_state = Machine_states.current_machine_state; // Сохраняем состояние светодиодов до паузы
			Machine_states.current_machine_state = LED_PAUSE;
			block_door(OFF); // Разблокируем дверку
		}
		else
		{
			Rinse_states.pause = 0;
			draw_operation_name(NULL, 1);
			Machine_states.current_machine_state = prev_state; // Возвращаем то состояние светодиодов которое было до паузы
			block_door(ON); // Блокируем дверку
			Rinse_states.ret_step = Rinse_states.step;
			Rinse_states.step = WAIT_CLOSE_DOOR;
		}
		triggered_menu_items(trigger_pause_rinse, sizeof(trigger_pause_rinse));
	}
}


CALLBACK_NAME(PROCESS_RINSE, init)(void) // Инициализация процесса полоскания
{
	Rinse_states.step = NORMAL_RINSE;
	Rinse_states.count = 0;
}

CALLBACK_NAME(PROCESS_RINSE, run)(char *params)
{
	send_signal(PROCESS_POWER_MANAGER, 1); //Посылаем сигнал процессу PROCESS_POWER_MANAGER чтобы тот сохранил в EEPROM все параметры стирки

	// При старте вырубаем все операции
	stop_process(PROCESS_WRING);
	stop_process(PROCESS_BEDABBLE);
	stop_process(PROCESS_WASHING);

	triggered_menu_items(trigger_run_stop_rinse, sizeof(trigger_run_stop_rinse));

	stop_fill_tank_and_stir_powder();
	stop_rinse();
	stop_wring();
	stop_mode_rotating();

	Rinse_states.step = WAIT_CLOSE_DOOR;
	Rinse_states.ret_step = NORMAL_RINSE; //После проверки люка, переходим в NORMAL_RINSE
	
	Rinse_states.count = 0;
	draw_prog_name(GET_MESS_POINTER(RINSE_MESSAGE), 1);
	Rinse_states.calc_work_time = 0;

	// Расчет времени окончания полоскания
	Rinse_states.calc_work_time += 
	RINSE_TIME / 60 // Время полоскания
	+ 5 // Усредненное время балансировку
	+ RINSE_WRING_TIME / 60 //Время на отжим при полоскании
	+ 1; // Время на слив воды
	
	Rinse_states.cancel = 0;
	Machine_states.pause = 0;
	
	Rinse_states.start_time = Main_current_time;
	Rinse_states.calculate_time = Rinse_states.start_time;
	time_add(&Rinse_states.calculate_time, Rinse_states.calc_work_time);
	draw_calculate_time(&Rinse_states.calculate_time, 0);
	Machine_states.rinse_finish = 0;
	power_on_system(); //Включаем основное питание системы
	block_door(ON); // Блокируем дверку
}

CALLBACK_NAME(PROCESS_RINSE, active)(struct task_info *from)
{
	clr_lcd_buf();
	draw_current_time(&Main_current_time, 1);
	draw_center_pointer();
	draw_calculate_time(NULL, 1);
	draw_work_time(NULL, 1);
	draw_prog_name(GET_MESS_POINTER(RINSE_MESSAGE), 1);  //Выводим название "Полоскание"
	draw_step_name(NULL, 0, 0, 1);
	draw_operation_name(NULL, 1);
	if(Rinse_states.pause) // Если процесс в паузе
		draw_pause(1);
}

CALLBACK_NAME(PROCESS_RINSE, main_loop)(void)
{
	if(Machine_states.change_current_time) // Отрисовываем время
	{
		draw_current_time(&Main_current_time, 1);
		draw_work_time(&Rinse_states.work_time, 1);
		Machine_states.change_current_time = 0;
	}
}

CALLBACK_NAME(PROCESS_RINSE, background)(uint8_t is_active)
{
	static uint8_t prev_rinse_display_status;
	
	switch(Rinse_states.step)
	{
		case WAIT_CLOSE_DOOR: // Ожидаем завершения закрытия дверки
		{
			static uint8_t first = 0;
			if(Machine_states.state_door == 0) // Если дверка открыта
			{
				if(!first)
				{
					Rinse_states.door_timer = 6; // Запускаем таймер ожидания стабилизации сигнала с дверки, если по истечению этого времени state_door попрежднему равно 0 значит дверка всетаки открыта
					first = 1;
				}
				
				if(first == 1 && Rinse_states.door_timer == 1 && Machine_states.state_door == 0) //Если поистечению времени дверка осталось открытой
				{
					Rinse_states.door_timer = 0;
					draw_close_door(is_active); // Выводим на экран "Закройте люк!"
					turn_blink_speaker(ON, 100, 300, SPEAKER_SOUND_FREQ, 6); //Пищим шесть раз
					first++; // Делаем так, чтобы больше в этот блок кода незаходить, пока дверка незакроется
				}
			}
			else
			{
				first = 0;
				Rinse_states.step = Rinse_states.ret_step;
				turn_blink_speaker(OFF, 0, 0, 0, 0);
				Rinse_states.pause = 0;
				Machine_states.pause = 0;
			}
		}
		break;
	
		case NORMAL_RINSE:
			if(!Machine_states.running_rinse && !Machine_states.rinse_finish) //Если полоскание незапущенно
			{
				if(Washing_settings.rinse_mode == 0) // Если выбран режим обычного полоскания
					run_rinse(INTENSIVE_ROTATING, Washing_settings.rinse_water_soure - 1);
				else // Если выбран режим бережного полоскания
					run_rinse(RINSE_COAT_ROTATING, Washing_settings.rinse_water_soure - 1);
					
				draw_step_name(GET_MESS_POINTER(RINSE_MESSAGE), Rinse_states.count + 1, Washing_settings.rinse_count, is_active);
			}
			
			if(Machine_states.running_rinse) // Если полоскание запущенно
			{
				if(prev_rinse_display_status != Machine_states.rinse_display_status)
				{
					switch(Machine_states.rinse_display_status)
					{
						case 2: // Наполнение водой
							draw_operation_name(GET_MESS_POINTER(FILL_TANK_MESSAGE), is_active); // Выводим на экран "Заполнение водой"
						break;
						
						case 3: // Полоскаем
							draw_operation_name(GET_MESS_POINTER(RINSING_MESSAGE), is_active); // Выводим на экран "Полоскаем..."
						break;
						
						case 4: // Сливаем воду
							draw_operation_name(GET_MESS_POINTER(POUR_WATER_MESSAGE), is_active); // Выводим на экран "Слив воды"
						break;
						
						case 5: // Отжимаем
							draw_operation_name(GET_MESS_POINTER(WRINGING_MESSAGE), is_active); // Выводим на экран "Отжимаем..."
						break;
					}
					prev_rinse_display_status = Machine_states.rinse_display_status;
				}
			}
			
			if(Machine_states.rinse_finish) // Если полоскание завершенно
			{
				printf_P(PSTR("rinse_finish, count = %d/%d\r\n"), Rinse_states.count, Washing_settings.rinse_count);
				if(Rinse_states.count < (Washing_settings.rinse_count - 1)) //Если еще неотполоскали нужное количество раз
				{
					Machine_states.rinse_finish = 0;
					Rinse_states.count ++;
					printf_P(PSTR("Rinse_states.count++\r\n"));
				}
				else //Иначе завершаем полоскание
				{
					printf_P(PSTR("ending rinse\r\n"));
					draw_operation_name(GET_MESS_POINTER(RINSE_FINISH_MESSAGE), 0); // Выводим на экран "Полоскание завершено"
					draw_calculate_time(&Main_current_time, 0); //Отрисовываем время завершения
					send_signal(PROCESS_SCREENSAVER, 3); //Посылаем в скринсейвер сигнал отрисовать результаты полоскания
					stop_process(PROCESS_RINSE);
				}
			}
		break;
		
		case CANCEL_RINSE:
			if(!Rinse_states.cancel)
			{
				uint8_t water_out_enable = 1; //данный флаг означает нужно ли сливать воду или нет. 1 - вода будет слита
				if(Rinse_states.pause) //Если в данный момент установленна пауза то снимаем её
				{
					water_out_enable = 0;
					rinse_pause();
				}
				
				stop_rinse();
				
				if(water_out_enable) // если надо слить воду - то запускаем слив
					pour_out_water();
				else
					Machine_states.pour_out_finish = 1; // иначе устанавливаем флаг что вода уже слита
					
				Rinse_states.cancel = 1;
				draw_operation_name(GET_MESS_POINTER(RINSE_CANCELING_MESSAGE), is_active); // Выводим на экран "Отмена полоскания"
				// Расчет времени завершения
				Rinse_states.calculate_time = Main_current_time;  
				time_add(&Rinse_states.calculate_time, 1); // Расчитываем расчетное время завершения исходя из среднестатистических данных времени слива воды.
				draw_calculate_time(&Rinse_states.calculate_time, is_active);
			}
			
			if(Machine_states.pour_out_finish) //Дождались слива воды, и завершили процесс
			{
				Machine_states.pour_out_finish = 0;
					// TODO: Разблокировали дверку
				Rinse_states.step = NORMAL_RINSE;
				Rinse_states.cancel = 0;
				draw_operation_name(GET_MESS_POINTER(RINSE_CANCEL_MESSAGE), 0); // Выводим на экран "Полоскание прервано"
				draw_calculate_time(&Main_current_time, 0); //Отрисовываем время завершения
				send_signal(PROCESS_SCREENSAVER, 3); //Посылаем в скринсейвер сигнал отрисовать результаты замачивания
				stop_process(PROCESS_RINSE);
			}
		break;
	}
	
	if(Machine_states.main_power && Machine_states.ext_power == 0 && !Rinse_states.pause) // если пропало питание 220v
	{
		rinse_pause(); // ставим паузу
		sys_error(ERROR, GET_MESS_POINTER(ERROR_NOT_POWER_220)); // Нет питания 220v
	}
	
	if(Machine_states.error && !Rinse_states.pause) // Если случилась глобальная ошибка, то ставим все на паузу
		rinse_pause(); // ставим паузу
	
}

CALLBACK_NAME(PROCESS_RINSE, click_esc)(void)
{
	switch_process(PROCESS_MENU); //Вход в меню
}

CALLBACK_NAME(PROCESS_RINSE, click_enter)(void)
{
	rinse_pause();
}

CALLBACK_NAME(PROCESS_RINSE, timer0)(void)
{
	if(!Rinse_states.pause) //Увеличиваем рабочее время на одну секунду если пауза не активированна
		inc_time(&Rinse_states.work_time);
}

CALLBACK_NAME(PROCESS_RINSE, signal)(uint8_t signal)
{
	switch(signal)
	{
		case 5: //Сигнал, остановить полоскание
			Rinse_states.step = CANCEL_RINSE;
		break;
	}
}

CALLBACK_NAME(PROCESS_RINSE, console)(char input_byte)
{
	switch(input_byte)
	{
		case '7':
			switch_process(PROCESS_MENU); //Вход в меню
		break;

		case '6':
			rinse_pause(); //Пауза
		break;
	}
}

CALLBACK_NAME(PROCESS_RINSE, stop)(void)
{
	block_door(OFF); // Разблокируем дверку
	if(Rinse_states.pause) //Если установленна пауза, то снимаем её
		rinse_pause();
		
	triggered_menu_items(trigger_run_stop_rinse, sizeof(trigger_run_stop_rinse));
	stop_fill_tank_and_stir_powder();
	stop_rinse();
	stop_wring();
	stop_mode_rotating();
}

