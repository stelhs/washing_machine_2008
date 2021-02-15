#include <stdio.h>
#include <stdlib.h>
#include "../lib.h"
#include "../messages.h"
#include "../os.h"
#include "lib_liveos.h"
#include "lib_menu.h"
#include "../config.h"
#include "config.h"
#include "../lcd_buffer.h"
#include "proc_lib.h"

#define BEDDABLE_COUNT_STEPS 6 // Количество этапов замачивания

extern struct machine_states Machine_states; // Состояние всей машины
extern struct time Main_current_time; // Часы реального времени
extern struct washing_settings Washing_settings; // Нстройки регулируемые с меню
extern char Temp_buf[CLI_LENGTH]; //Универсальный буфер для всяких временных нужд

// Список элементов меню видимость которых необходимо инвертировать при запуске и остановке
uint8_t trigger_run_stop_bedabble[] = {
																3, // Выбор времени замачивания
																4, // Источник порошка (для замачивания)
																5, // Замочить сейчас
																23, // Ручное управление
																6,  // Остановить замачивание
																7, // Запуск стирки
																25, // Ручное полоскание
																40 // Ручной отжим
															};

// Список элементов меню видимость которых необходимо инвертировать при входе и выходе из паузы
uint8_t trigger_pause_bedabble[] =  {
															  23, // Ручное управление
														   };

enum steps_bedabble
{
	WAIT_CLOSE_DOOR, // Ожидание закрывания дверки
	STIR_POWDER, // Набираем воду и размешиваем порошок
	STEP_BEDABBLE, // Замачивание
	POUR_WATER, // Слив воды и конец процесса
	CANCEL_BEDABBLE, // Отмена замачивания
};

struct 
{
	uint8_t step : 3; // Текуший шаг операции замачивание
	uint8_t ret_step : 3; // Этап в который необходимо будет перейти. (используется для перехода в следующий этап после проверки закрытости люка)
	uint8_t door_timer; // Таймер ожидания закрывания люка
	uint16_t timer; // Таймер ожидания окончания замачивания
	uint8_t pause : 1; // Бит выставлен в случае пользовательской паузы
	uint8_t cancel : 1; // Бит выставлен в случае начала процесса принудительного завершения замачивания
	uint16_t calc_work_time; //Расчетное время работы в минутах
	struct time start_time;
	struct time work_time;
	struct time calculate_time;
}States_bedabble;

uint16_t Bedabble_times[] = {1, 60 * 60, 120 * 60, 180 * 60, 300 * 60, 480 * 60}; // Возможные варианты времени замачивания (соответствуют пунктам меню) (в минутах)

static void bedabble_pause(void) //Функция постановки/снятия паузы
{
	if(!States_bedabble.cancel)
	{
		static uint8_t prev_state;
		if(!States_bedabble.pause)
		{
			States_bedabble.pause = 1;
			Machine_states.pause = 1;
			draw_pause(1);
			prev_state = Machine_states.current_machine_state; // Сохраняем состояние светодиодов до паузы
			Machine_states.current_machine_state = LED_PAUSE;
			block_door(OFF); // Разблокируем дверку
		}
		else
		{
			States_bedabble.pause = 0;
			draw_operation_name(NULL, 1);
			Machine_states.current_machine_state = prev_state; // Возвращаем то состояние светодиодов которое было до паузы
			block_door(ON); // Блокируем дверку
			States_bedabble.ret_step = States_bedabble.step;
			States_bedabble.step = WAIT_CLOSE_DOOR;
		}
		triggered_menu_items(trigger_pause_bedabble, sizeof(trigger_pause_bedabble));
	}
}

CALLBACK_NAME(PROCESS_BEDABBLE, init)(void) // Инициализация процесса замачивания
{
	States_bedabble.step = STIR_POWDER;
	States_bedabble.timer = 0;
	States_bedabble.pause = 0;
}

CALLBACK_NAME(PROCESS_BEDABBLE, run)(char *params) // Запуск процесса замачивания
{
	States_bedabble.step = WAIT_CLOSE_DOOR;
	States_bedabble.ret_step = STIR_POWDER;
	
	draw_prog_name(GET_MESS_POINTER(MENU_ITEM_WASHING_BEDABBLE), 1);
	States_bedabble.calc_work_time = 0;

	Machine_states.pause = 0;
	params = copy_cli_parameter(params, Temp_buf);
	if(Temp_buf[0] != 0) // Если пришли параметры 
	{
		if(!strcasecmp_P(Temp_buf, PSTR("-t"))) //Если пришол параметр -t <время стирки>
		{
			copy_cli_parameter(params, Temp_buf); // Получаем время стирки
			draw_prog_name(GET_MESS_POINTER(WASHING_BEDABBLE_MESSAGE), 1); // Выводим "замач&стир"
			States_bedabble.calc_work_time += atoi(Temp_buf); // Добавляем расчетное время стирки
		}
		else //Если параметры не адекватны
		{
			stop_process(PROCESS_BEDABBLE); 
			return;
		}
	}
	else // Если замачивание запущенно без параметров тоесть самостоятельно из меню
	{
		send_signal(PROCESS_POWER_MANAGER, 1); //Посылаем сигнал процессу PROCESS_POWER_MANAGER чтобы тот сохранил в EEPROM все параметры стирки

		stop_process(PROCESS_WRING);
		stop_process(PROCESS_WASHING);
		stop_process(PROCESS_RINSE);
		
		triggered_menu_items(trigger_run_stop_bedabble, sizeof(trigger_run_stop_bedabble));
		stop_fill_tank_and_stir_powder();
		stop_rinse();
		stop_wring();
		stop_mode_rotating();
	}
	
	draw_step_name(GET_MESS_POINTER(STEP_MESSAGE), 1, BEDDABLE_COUNT_STEPS, 0);
	draw_operation_name(GET_MESS_POINTER(POUR_WATER_MESSAGE), 0); // Выводим на экран "Слив воды"

	States_bedabble.start_time = Main_current_time;
	States_bedabble.calculate_time = States_bedabble.start_time;
	
	States_bedabble.calc_work_time +=  // Расчет времени окончания полоскания
	STIR_POWDER_TIME / 60 + // Время на размешивание порошка
	3 + //  Усредненное время на балансировку и заливку воды
	Bedabble_times[Washing_settings.bedabble_time] / 60 + // Время замачивания
	1; // Время на слив
	
	time_add(&States_bedabble.calculate_time, States_bedabble.calc_work_time);
	draw_calculate_time(&States_bedabble.calculate_time, 0);
	
	States_bedabble.cancel = 0;
	Machine_states.current_machine_state = LED_BEDABBLE;
	
	States_bedabble.work_time.hour = 0;
	States_bedabble.work_time.min = 0;
	States_bedabble.work_time.sec = 0;

	power_on_system(); // Выключаем основное питание системы
	block_door(ON); // Блокируем дверку
}

CALLBACK_NAME(PROCESS_BEDABBLE, active)(struct task_info *from)
{
	clr_lcd_buf();
	draw_current_time(&Main_current_time, 1);
	draw_center_pointer();
	draw_calculate_time(NULL, 1);
	draw_work_time(NULL, 1);
	draw_prog_name(NULL, 1);
	draw_step_name(NULL, 0, 0, 1);
	draw_operation_name(NULL, 1);
	if(States_bedabble.pause) // Если процесс в паузе
		draw_pause(1);
}

CALLBACK_NAME(PROCESS_BEDABBLE, main_loop)(void)
{
	if(Machine_states.change_current_time) // Отрисовываем время
	{
		draw_current_time(&Main_current_time, 1);
		draw_work_time(&States_bedabble.work_time, 1);
		Machine_states.change_current_time = 0;
	}
}

CALLBACK_NAME(PROCESS_BEDABBLE, background)(uint8_t is_active)
{
	static uint8_t prev_stir_powder_display_status = 0; // Переменная для хранения предыдущего состояния метода размешивания порошка
	
	if(!States_bedabble.pause)
		switch(States_bedabble.step)
		{
			case WAIT_CLOSE_DOOR: // Ожидаем завершения закрытия дверки
			{
				static uint8_t first = 0;
				if(Machine_states.state_door == 0) // Если дверка открыта
				{
					if(!first)
					{
						States_bedabble.door_timer = 6; // Запускаем таймер ожидания стабилизации сигнала с дверки, если по истечению этого времени state_door попрежднему равно 0 значит дверка всетаки открыта
						first = 1;
					}
					
					if(first == 1 && States_bedabble.door_timer == 1 && Machine_states.state_door == 0) //Если поистечению времени дверка осталось открытой
					{
						States_bedabble.door_timer = 0;
						draw_close_door(is_active); // Выводим на экран "Закройте люк!"
						turn_blink_speaker(ON, 100, 300, SPEAKER_SOUND_FREQ, 6); //Пищим шесть раз
						first++; // Делаем так чтобы больше в этот блок кода незаходить, пока дверка незакроется
					}
				}
				else
				{
					first = 0;
					States_bedabble.step = States_bedabble.ret_step;
					turn_blink_speaker(OFF, 0, 0, 0, 0);
					Machine_states.pause = 0;
				}
			}
			break;
		
			case STIR_POWDER: // Набор воды и размешивание порошка
				if(!Machine_states.running_stir_powder && !Machine_states.stir_powder_finish) // Если залив воды с размешиванием порошка ранее небыл запущен, то запускаем его
				{
					if(Machine_states.level_water_in_tank >= SENSOR_WATER_LEVEL1) //Если уровень воды больше или равен первому уровню то воду несливаем, сразу переходим на замачивание.
					{
						States_bedabble.step = STEP_BEDABBLE;
						break;
					}
					
					draw_step_name(GET_MESS_POINTER(STEP_MESSAGE), 1, BEDDABLE_COUNT_STEPS, is_active);
					draw_operation_name(GET_MESS_POINTER(BALANSING_MESSAGE), is_active); // Выводим на экран "Балансировка белья"
					start_fill_tank_and_stir_powder(BEDABBLE_WATER_LEVEL, Washing_settings.bedabble_water_soure - 1, conv_temperature_to_adc_val(BEDABBLE_TEMPERATURE));
				}
				
				if(Machine_states.running_stir_powder) //Если запущен процесс размешивания порошка, то выводим его тукущее состояние
				{
					if(Machine_states.stir_powder_display_status != prev_stir_powder_display_status) //Если изменилось состояние то выводим его на экран
					{
						switch(Machine_states.stir_powder_display_status)
						{
							case 1: //Если белье раскрутилось до нужеой скорости
							case 2: //Если белье несмогло раскрутиться до нужного уровня
								draw_step_name(GET_MESS_POINTER(STEP_MESSAGE), 2, BEDDABLE_COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(FILL_TANK_MESSAGE), is_active); // Выводим на экран "Заполнение водой"
							break;

							case 3: //Если бак заполнился до первого уровня то устанавливаем время размешивания
								draw_step_name(GET_MESS_POINTER(STEP_MESSAGE), 3, BEDDABLE_COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(STIR_POWDER_MESSAGE), is_active); // Выводим на экран "Размешивание порошка"
							break;

							case 4:  //Если размешивание завершенно
								draw_step_name(GET_MESS_POINTER(STEP_MESSAGE), 4, BEDDABLE_COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(FILL_TANK_MESSAGE), is_active); // Выводим на экран "Заполнение водой"
							break;
						}
						prev_stir_powder_display_status = Machine_states.stir_powder_display_status;
					}
				}
				
				if(Machine_states.stir_powder_finish && Machine_states.drum_is_stopped) // Если режим размешивания завершен и барабан остановился 
				{
					Machine_states.stir_powder_finish = 0;
					Machine_states.drum_is_stopped = 0;
					States_bedabble.step = STEP_BEDABBLE;
				}
			break;
			
			case STEP_BEDABBLE: // Ожидаем окончание замачивания белья
				if(!Machine_states.runing_rotating)
				{
					start_mode_rotating(BEDABBLE_ROTATING); // Запускаем редкое помешивание белья для этапа замачивания
					draw_step_name(GET_MESS_POINTER(STEP_MESSAGE), 5, BEDDABLE_COUNT_STEPS, is_active);
					draw_operation_name(GET_MESS_POINTER(BEDDABLE_STEP_WATER_MESSAGE), is_active); // Выводим на экран "Замачиваем..."
					States_bedabble.timer = Bedabble_times[Washing_settings.bedabble_time]; //Запускаем таймер с обратным отсчетом
				}
			
				if(States_bedabble.timer == 1) //Если режим замачивания завершен
				{
					States_bedabble.timer = 0;
					stop_mode_rotating();
					pour_out_water(); // Запускаем слив воды
					draw_step_name(GET_MESS_POINTER(STEP_MESSAGE), 6, BEDDABLE_COUNT_STEPS, is_active);
					draw_operation_name(GET_MESS_POINTER(POUR_WATER_MESSAGE), is_active); // Выводим на экран "Слив воды"
					States_bedabble.step = POUR_WATER;
				}
			break;
			
			case POUR_WATER: // Ожидаем слив воды
				if(Machine_states.pour_out_finish) // Если слив завершен
				{
					Machine_states.pour_out_finish = 0;
					States_bedabble.step = STIR_POWDER;
					Washing_settings.bedabble_time = 0; // Сбрасываем настройку времени замачивания
					draw_operation_name(GET_MESS_POINTER(BEDDABLE_FINISH_MESSAGE), 0); // Выводим на экран "Замач-ние завершено"
					draw_calculate_time(&Main_current_time, 0); //Отрисовываем время завершения
					Machine_states.current_machine_state = LED_FINISH;
					send_signal(PROCESS_SCREENSAVER, 3); //Посылаем в скринсейвер сигнал отрисовать результаты замачивания
					stop_process(PROCESS_BEDABBLE);
				}
			break;
			
			case CANCEL_BEDABBLE: // Принудительне завершение замачивания
				if(!States_bedabble.cancel)
				{
					uint8_t water_out_enable = 1; //данный флаг означает нужно ли сливать воду или нет. 1 - вода будет слита
					if(States_bedabble.pause) //Если в данный момент установленна пауза то снимаем её
					{
						water_out_enable = 0;
						bedabble_pause();
					}
					
					stop_fill_tank_and_stir_powder();
					stop_mode_rotating();
					
					if(water_out_enable) // если надо слить воду - то запускаем слив
						pour_out_water();
					else
						Machine_states.pour_out_finish = 1; // иначе устанавливаем флаг что вода уже слита
					
					States_bedabble.cancel = 1;
					draw_operation_name(GET_MESS_POINTER(CANCEL_BEDABBLE_MESSAGE), is_active); // Выводим на экран "Отмена замачивания"
					// Расчет времени завершения
					States_bedabble.calculate_time = Main_current_time;  
					time_add(&States_bedabble.calculate_time, 1); // Расчитываем расчетное время завершения исходя из среднестатистических данных времени слива воды.
					draw_calculate_time(&States_bedabble.calculate_time, is_active);
				}
				
				if(Machine_states.pour_out_finish) //Дождались слива воды, и завершили процесс
				{
					Machine_states.pour_out_finish = 0;
						// TODO: Разблокировали дверку
					States_bedabble.step = STIR_POWDER;
					States_bedabble.cancel = 0;
					draw_operation_name(GET_MESS_POINTER(BEDDABLE_FINISH_CANCEL_MESSAGE), 0); // Выводим на экран "Замач-ние прерванно"
					draw_calculate_time(&Main_current_time, 0); //Отрисовываем время завершения
					triggered_menu_items(trigger_run_stop_bedabble, sizeof(trigger_run_stop_bedabble));

					Machine_states.current_machine_state = LED_OFF;
					send_signal(PROCESS_SCREENSAVER, 3); //Посылаем в скринсейвер сигнал отрисовать результаты замачивания
					stop_process(PROCESS_BEDABBLE);
				}
			break;
		}
		
	if(Machine_states.main_power && Machine_states.ext_power == 0 && !States_bedabble.pause) // если пропало питание 220v
	{
		bedabble_pause(); // ставим паузу
		sys_error(ERROR, GET_MESS_POINTER(ERROR_NOT_POWER_220)); // Нет питания 220v
	}
	
	if(Machine_states.error && !States_bedabble.pause) // Если случилась глобальная ошибка, то ставим все на паузу
		bedabble_pause(); // ставим паузу
}

CALLBACK_NAME(PROCESS_BEDABBLE, click_enter)(void)
{
	bedabble_pause();
}

CALLBACK_NAME(PROCESS_BEDABBLE, click_esc)(void)
{
	switch_process(PROCESS_MENU); //Вход в меню
}

CALLBACK_NAME(PROCESS_BEDABBLE, console)(char input_byte)
{
	switch(input_byte)
	{
		case '7':
			switch_process(PROCESS_MENU); //Вход в меню
		break;

		case '6':
			bedabble_pause(); //Пауза
		break;
	}
}

CALLBACK_NAME(PROCESS_BEDABBLE, timer0)(void) // Обработчик таймера с частотой 1на секунда
{
	if(!States_bedabble.pause) //Увеличиваем рабочее время на одну секунду если пауза не активированна
		inc_time(&States_bedabble.work_time);
		
	if(States_bedabble.timer > 1)
		States_bedabble.timer--;
		
	if(States_bedabble.door_timer > 1)
		States_bedabble.door_timer--;
}

CALLBACK_NAME(PROCESS_BEDABBLE, signal)(uint8_t signal)
{
	switch(signal)
	{
		case 5: //Сигнал, завершить замачивание
			Machine_states.pause = 0;
			States_bedabble.step = CANCEL_BEDABBLE;
		break;

		case 7: //Сигнал, быстро прервать замачивание
			stop_fill_tank_and_stir_powder();
			stop_mode_rotating();
			Washing_settings.bedabble_time = 0; // Сбрасываем настройку времени замачивания
			States_bedabble.pause = 0;
			stop_process(PROCESS_BEDABBLE);
		break;

		case 10:
			printf_P(PSTR("step = %d\r\n"), States_bedabble.step);
			printf_P(PSTR("timer = %d\r\n"), States_bedabble.timer);
			printf_P(PSTR("pause = %d\r\n"), States_bedabble.pause);
			printf_P(PSTR("cancel = %d\r\n"), States_bedabble.cancel);
			printf_P(PSTR("calc_work_time = %d\r\n"), States_bedabble.calc_work_time);
		break;
	}
}

CALLBACK_NAME(PROCESS_BEDABBLE, stop)(void)
{
	Washing_settings.bedabble_time = 0; // Сбрасываем настройку времени замачивания
	refresh_menu(0); //Это запускаем для того чтобы обновить ветку меню Washing_settings.bedabble_time
	block_door(OFF); // Разблокируем дверку

	if(States_bedabble.pause) //Если установленна пауза, то снимаем её
		triggered_menu_items(trigger_pause_bedabble, sizeof(trigger_pause_bedabble));
	
	triggered_menu_items(trigger_run_stop_bedabble, sizeof(trigger_run_stop_bedabble));
	
	stop_fill_tank_and_stir_powder();
	stop_rinse();
	stop_wring();
	stop_mode_rotating();
}


