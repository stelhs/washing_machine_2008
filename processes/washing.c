#include <stdio.h>
#include <util/delay.h>
#include "../drivers.h"
#include "../lib.h"
#include "../messages.h"
#include "../os.h"
#include "../../../lib/lib_liveos.h"
#include "../config.h"
#include "config.h"
#include "../lcd_buffer.h"
#include "proc_lib.h"

extern struct machine_states Machine_states; // Состояние всей машины
extern struct time Main_current_time; // Часы реального времени
extern struct washing_settings Washing_settings; // Нстройки регулируемые с меню
extern volatile int16_t Current_temperature;
extern char Temp_buf[CLI_LENGTH]; //Универсальный буфер для всяких временных нужд

#define PREV_WASHING_COUNT_STEPS 7
#define WASHING_COUNT_STEPS 8
#define WRING_COUNT_STEPS 5

extern PGM_P const PROGMEM Select_washing_presetes[];

// Список лементов меню видимость которых необходимо инвертировать при запуске и остановке
uint8_t trigger_run_stop_washing[] = {
																7, // запуск процесса стирки
																8, // остановить стирку
																23, // Ручное управление
																2, // Замачивание
																25, // Ручное полоскание
																40 // Ручной отжим
															};

// Список лементов меню видимость которых необходимо инвертировать при входе и выходе из паузы
uint8_t trigger_pause_washing[] = {
															//7, // запуск процесса стирки
															23, // Ручное управление
														};

enum washing_steps // Этапы процесса стирки
{
	WAIT_CLOSE_DOOR, // Ожидание закрывания дверки
	BEDABBLE, // Замачивание
	PREV_WASHING, // Начало предварительной стирки
	WASHING_FILL_TANK, // Стирка в нормальном режиме
	WAIT_END_NORMAL_WASHING, // Ожидаем завершения обычной стирки
	WAIT_END_INTENSIVE_WASHING, // Ожидаем завершения интенсивной стирки
	WASHING_COOLING_WATER, // Этап охлаждения воды если та выше порога при котором запрещенно сливать воду в канализацию
	WAIT_POUR_OUT_WATER, //Ожидаем завершения сливания воды
	STEP_RINSE, //Этап многократного полоскания
	WAIT_BEFORE_LAST_RINSE, // Этап ожидания ополаскивателя перед последним полосканием, это только если включен данный режим в установках стирки
	STEP_WRINGS,  //Этап многократного отжима
	CANCEL_WASHING, // Сюда переходим в случае отмены стирки
	END_WASHING, // Конец стирки
};

enum prev_washing_steps // Этапы предварительной стирки
{
	PREV_WASHING_POUR_OLD_WATER, //Предварительный слив остатков воды
	PREV_WASHING_FILL_TANK, // Залив воды
	WAIT_END_PREV_WASHING, //Ожидаем завершение предварительной стирки
	END_PREV_WASHING //Ожидаем завершение полоскания
};

enum washing_wring_steps
{
	STEP_WRING, // Ожидание завершения отжима
	WAIT_STIR_DRESS, // Ожидание разрыхления белья после отжима
};

static uint8_t Washing_temperatures[] = {10, 30, 40, 60, 70, 95};  // Температуры стирки доступные из меню

struct
{
	uint8_t step : 4; // Текущий этап операции
	uint8_t ret_step : 4; // Этап в который необходимо будет перейти. (используется для перехода в следующий этап после проверки закрытости люка)
	uint8_t prev_washing_step : 2; // Текущий шаг операции пердварительной стирки
	uint8_t rinse_counter : 3; // Счетчик полосканий
	uint8_t wring_step; // Текущий шаг операции отжима
	uint8_t door_timer; // Таймер ожидания закрывания люка
	uint16_t washing_timer; // Таймер для ожидания окончания времени стирки
	uint16_t timer_wait_last_swill; //Таймер для ожидания заливания ополаскивателя перед последним полосканием
	uint8_t count_wait_last_swill : 4; //Текущий номер попытки дождаться ополаскивателя перед последним полосканием
	uint8_t wring_counter : 3; // Счетчик количества отжимов
	uint8_t pause : 1; // Бит выставлен в случае пользовательской паузы
	uint8_t cancel : 1; // Бит выставлен в случае начала процесса принудительного завершения замачивания
	uint16_t calc_work_time; //Расчетное время работы в минутах
	uint16_t time_bedabble; //Время потраченное на замачивание
	struct time start_time;
	struct time work_time;
	struct time calculate_time;
}Process_washing_states;


static void washing_pause(void) //Функция постановки/снятия паузы
{
	if(!Process_washing_states.cancel)
	{
		static uint8_t prev_state;
		if(!Process_washing_states.pause) // Постановка на паузу
		{
			Process_washing_states.pause = 1;
			Machine_states.pause = 1;
			draw_pause(1);
			prev_state = Machine_states.current_machine_state; // Сохраняем состояние светодиодов до паузы
			Machine_states.current_machine_state = LED_PAUSE;
			block_door(OFF); // Разблокируем дверку
		}
		else
		{
			draw_operation_name(NULL, 1);
			Machine_states.current_machine_state = prev_state; // Возвращаем то состояние светодиодов которое было до паузы
			block_door(ON); // Блокируем дверку
			Process_washing_states.ret_step = Process_washing_states.step;
			Process_washing_states.step = WAIT_CLOSE_DOOR;
		}
		triggered_menu_items(trigger_pause_washing, sizeof(trigger_pause_washing));
	}
}


CALLBACK_NAME(PROCESS_WASHING, init)(void) // Инициализация процесса стирки
{
	Process_washing_states.step = BEDABBLE;
	Process_washing_states.cancel = 0;
	Process_washing_states.time_bedabble = 0;
	Process_washing_states.rinse_counter = 0;
	Process_washing_states.wring_counter = 0;
}

CALLBACK_NAME(PROCESS_WASHING, run)(char *params) // Запуск процесса стирки
{
	send_signal(PROCESS_POWER_MANAGER, 1); //Посылаем сигнал процессу PROCESS_POWER_MANAGER чтобы тот сохранил в EEPROM все параметры стирки

	Process_washing_states.time_bedabble = 0;
	Process_washing_states.rinse_counter = 0;
	Process_washing_states.wring_counter = 0;
	Process_washing_states.door_timer = 0;
	Process_washing_states.washing_timer = 0;
	Process_washing_states.timer_wait_last_swill = 0;
	Process_washing_states.prev_washing_step = PREV_WASHING_POUR_OLD_WATER;

	// При старте вырубаем все операции
	stop_process(PROCESS_WRING);
	stop_process(PROCESS_BEDABBLE);
	stop_process(PROCESS_RINSE);

	triggered_menu_items(trigger_run_stop_washing, sizeof(trigger_run_stop_washing));

	stop_fill_tank_and_stir_powder();
	stop_rinse();
	stop_wring();
	stop_mode_rotating();

	Process_washing_states.cancel = 0;
	Process_washing_states.pause = 0;
	Machine_states.pause = 0;
	Process_washing_states.step = WAIT_CLOSE_DOOR;
	Process_washing_states.ret_step = BEDABBLE; //После проверки люка, переходим в BEDABBLE

	// Расчет времени окончания стирки
	Process_washing_states.calc_work_time = 0;
	if(Washing_settings.enable_prev_washing) // Если включенна предварительная стирка
	{
		Process_washing_states.calc_work_time +=
		STIR_POWDER_TIME / 60 // Время на размешивание порошка
		+ 3 //  Усредненное время на балансировку и заливку воды
		+ PREV_WASING_TIME / 60 // Время предварительной стирки
		+ RINSE_TIME / 60 // Время полоскания
		+ 5 // Усредненное время балансировку
		+ RINSE_WRING_TIME / 60 //Время на отжим при полоскании
		+ 1; // Время на слив воды
	}

	Process_washing_states.calc_work_time +=
	STIR_POWDER_TIME / 60 // Время на размешивание порошка
	+ 3 //  Усредненное время на балансировку и заливку воды
	+ Washing_settings.washing_time_part1; // Время стирки первой фазы
	if(Washing_settings.enable_washing_part2) //Если включенна вторая фаза стирки
		Process_washing_states.calc_work_time += Washing_settings.washing_time_part2; // Время стирки второй фазы

	Process_washing_states.calc_work_time +=
	  (RINSE_TIME / 60 // Время полоскания
	+ 5 // Усредненное время балансировку
	+ RINSE_WRING_TIME / 60  //Время на отжим при полоскании
	+ 1) // Время на слив воды
	* Washing_settings.count_rinses // Умножаем время полоскания на количество полосканий
	+ (5 // Усредненное время на балансировку перед отжимом
	+ Washing_settings.wring_time) // Время отжима
	*  WASHING_WRINGS_COUNT; // Умножаем время отжима на количество отжимов

	Process_washing_states.start_time = Main_current_time;
	Process_washing_states.calculate_time = Process_washing_states.start_time;
	time_add(&Process_washing_states.calculate_time, Process_washing_states.calc_work_time);
	draw_calculate_time(&Process_washing_states.calculate_time, 0);

	Process_washing_states.work_time.hour = 0;
	Process_washing_states.work_time.min = 0;
	Process_washing_states.work_time.sec = 0;

	power_on_system(); //Включаем основное питание системы
	block_door(ON); // Блокируем дверку
}

CALLBACK_NAME(PROCESS_WASHING, active)(struct task_info *from)
{
	clr_lcd_buf();
	draw_current_time(&Main_current_time, 1);
	draw_center_pointer();
	draw_calculate_time(NULL, 1);
	draw_work_time(NULL, 1);
	draw_prog_name((const char *)pgm_read_word(&Select_washing_presetes[Washing_settings.washing_preset]), 1); //Выводим название выбранной программы
	draw_step_name(NULL, 0, 0, 1);
	draw_operation_name(NULL, 1);
	if(Process_washing_states.pause) // Если процесс в паузе
		draw_pause(1);

}

CALLBACK_NAME(PROCESS_WASHING, main_loop)(void)
{
	if(Machine_states.change_current_time) // Отрисовываем время
	{
		draw_current_time(&Main_current_time, 1);
		draw_work_time(&Process_washing_states.work_time, 1);
		Machine_states.change_current_time = 0;
	}
}

CALLBACK_NAME(PROCESS_WASHING, background)(uint8_t is_active)
{
	static uint8_t prev_stir_powder_display_status = 0;
	static uint8_t prev_wring_display_status = 0;
	static uint8_t prev_rinse_display_status = 0;

	switch(Process_washing_states.step)
	{
		case WAIT_CLOSE_DOOR: // Ожидаем завершения закрытия дверки
		{
			static uint8_t first = 0;
			if(Machine_states.state_door == 0) // Если дверка открыта
			{
				if(!first)
				{
					Process_washing_states.door_timer = 6; // Запускаем таймер ожидания стабилизации сигнала с дверки, если по истечению этого времени state_door попрежднему равно 0 значит дверка всетаки открыта
					first = 1;
				}

				if(first == 1 && Process_washing_states.door_timer == 1 && Machine_states.state_door == 0) //Если поистечению времени дверка осталось открытой
				{
					Process_washing_states.door_timer = 0;
					draw_close_door(is_active); // Выводим на экран "Закройте люк!"
					turn_blink_speaker(ON, 100, 300, SPEAKER_SOUND_FREQ, 6); //Пищим шесть раз
					first++; // Делаем так, чтобы больше в этот блок кода незаходить, пока дверка незакроется
				}
			}
			else
			{
				first = 0;
				Process_washing_states.step = Process_washing_states.ret_step;
				turn_blink_speaker(OFF, 0, 0, 0, 0);
				Process_washing_states.pause = 0;
				Machine_states.pause = 0;
			}
		}
		break;

		case BEDABBLE: // Запуск и ожидание завершения процесса замачивания
			if(Washing_settings.bedabble_time) // Если установленно время замачивания, значит нужно запустить процесс замачивания
			{
				if(get_process_status(PROCESS_BEDABBLE) == NOT_RUN)
				{
					snprintf(Temp_buf, sizeof(Temp_buf) - 1, " -t %d", Process_washing_states.calc_work_time); //Запускаем процесс замачивания с параметром -f <время стирки>
					run_process(PROCESS_BEDABBLE, Temp_buf);
				}
			}
			else // Иначе переходим в режим предварительной стирки
			{
				Process_washing_states.step = PREV_WASHING;
			}

			if(get_process_status(PROCESS_BEDABBLE) == STOPED) // Если дождались завершения процесса замачивания
			{
				Process_washing_states.calc_work_time += Process_washing_states.time_bedabble; // Увеличиваем расчетное время стирки на время потраченное на замачивание
				switch_process(PROCESS_WASHING);  // Дождавшись завершения процесса замачивания, переключаемся в текущий процесс (процесс стирки)
				Process_washing_states.step = PREV_WASHING; // Переходим в режим предварительной стирки
				power_on_system(); //Включаем основное питание системы
			}
		break;

		case PREV_WASHING: // Предварительная стирка
			switch(Process_washing_states.prev_washing_step)
			{
				case PREV_WASHING_POUR_OLD_WATER:
					if(!Washing_settings.enable_prev_washing) // Если предварительная стирка выключенна, то переходим сразу на стирку
					{
						Machine_states.need_temperature = conv_temperature_to_adc_val(PREV_WASHING_TEMPERATURE);
						Process_washing_states.step = WASHING_FILL_TANK;
						break;
					}

					if(!Machine_states.pour_out_water && !Machine_states.pour_out_finish)  //Проверяем не запущен ли слив воды ранее
					{
						Machine_states.current_machine_state = LED_PREV_WASHING;
						if(Machine_states.level_water_in_tank >= SENSOR_WATER_LEVEL1) //Если уровень воды больше или равен первому уровню то воду несливаем, сразу переходим на замачивание.
						{
							Process_washing_states.prev_washing_step = WAIT_END_PREV_WASHING;
							break;
						}

						// Запускаем слив воды
						draw_step_name(GET_MESS_POINTER(PREV_WASHING_MESSAGE), 1, PREV_WASHING_COUNT_STEPS, is_active);
						draw_operation_name(GET_MESS_POINTER(POUR_WATER_MESSAGE), is_active); // Выводим на экран "Слив воды"
						pour_out_water();
					}

					if(Machine_states.pour_out_finish) // Если слив завершен
					{
						Machine_states.pour_out_finish = 0;
						start_fill_tank_and_stir_powder(Washing_settings.washing_water_level, Washing_settings.prev_washing_water_soure - 1, conv_temperature_to_adc_val(PREV_WASHING_TEMPERATURE)); //Включаем заполнени бака
						draw_step_name(GET_MESS_POINTER(PREV_WASHING_MESSAGE), 2, PREV_WASHING_COUNT_STEPS, is_active);
						draw_operation_name(GET_MESS_POINTER(BALANSING_MESSAGE), is_active); // Выводим на экран "Балансировка белья"
						Process_washing_states.prev_washing_step = PREV_WASHING_FILL_TANK;
					}
				break;

				case PREV_WASHING_FILL_TANK: // Заполняем бак водой
					if(Machine_states.running_stir_powder) //Если запущен процесс размешивания порошка, то выводим его тукущее состояние
					{
						if(Machine_states.stir_powder_display_status != prev_stir_powder_display_status) //Если изменилось состояние то выводим его на экран
						{
							switch(Machine_states.stir_powder_display_status)
							{
								case 1: //Если белье раскрутилось до нужеой скорости
								case 2: //Если белье несмогло раскрутиться до нужного уровня
									draw_step_name(GET_MESS_POINTER(PREV_WASHING_MESSAGE), 3, PREV_WASHING_COUNT_STEPS, is_active);
									draw_operation_name(GET_MESS_POINTER(FILL_TANK_MESSAGE), is_active); // Выводим на экран "Заполнение водой"
								break;

								case 3: //Если бак заполнился до первого уровня то устанавливаем время размешивания
									draw_step_name(GET_MESS_POINTER(PREV_WASHING_MESSAGE), 4, PREV_WASHING_COUNT_STEPS, is_active);
									draw_operation_name(GET_MESS_POINTER(STIR_POWDER_MESSAGE), is_active); // Выводим на экран "Размешивание порошка"
								break;

								case 4:  //Если размешивание завершенно
									draw_step_name(GET_MESS_POINTER(PREV_WASHING_MESSAGE), 5, PREV_WASHING_COUNT_STEPS, is_active);
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
						Process_washing_states.prev_washing_step = WAIT_END_PREV_WASHING;
					}
				break;

				case WAIT_END_PREV_WASHING: //Ожидаем завершение предварительной стирки
					if(!Machine_states.runing_rotating)
					{
						// Запускаем периодическое помешивание белья
						if(Washing_settings.washing_mode == 0) // Если выбран режим нормальной стирки, то крутим белье в стандартном режиме, иначе крутим его очень бережно
							start_mode_rotating(NORMAL_ROTATING);
						else
							start_mode_rotating(COAT_ROTATING);

						draw_step_name(GET_MESS_POINTER(PREV_WASHING_MESSAGE), 6, PREV_WASHING_COUNT_STEPS, is_active);
						draw_operation_name(GET_MESS_POINTER(PREV_WASHING_FULL_MESSAGE), is_active); // Выводим на экран "Предварит. стирка"
						Process_washing_states.washing_timer = PREV_WASING_TIME;
					}

					if(Process_washing_states.washing_timer == 1) //Если время предварительной стирки истекло
					{
						Process_washing_states.washing_timer = 0;

						// Запускаем полоскание белья
						if(Washing_settings.washing_mode == 0) // Если выбран режим нормальной стирки, то полоскаем белье в стандартном режиме, иначе полоскаем его очень бережно
							run_rinse(INTENSIVE_ROTATING, Washing_settings.prev_washing_water_soure - 1);
						else
							run_rinse(RINSE_COAT_ROTATING, Washing_settings.prev_washing_water_soure - 1);

						draw_step_name(GET_MESS_POINTER(PREV_WASHING_MESSAGE), 7, PREV_WASHING_COUNT_STEPS, is_active);
						draw_operation_name(GET_MESS_POINTER(RINSING_MESSAGE), is_active); // Выводим на экран "Полоскаем..."
						Process_washing_states.prev_washing_step = END_PREV_WASHING;
					}
				break;

				case END_PREV_WASHING: //Ожидаем завершение полоскания
					if(Machine_states.running_rinse) //Если полоскание запущенно, то выводим различные состояния
					{
						if(prev_rinse_display_status != Machine_states.rinse_display_status)
						{
							switch(Machine_states.rinse_display_status)
							{
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

					if(Machine_states.rinse_finish) //Если отжим завершен
					{
						Machine_states.rinse_finish = 0;
						Process_washing_states.prev_washing_step = PREV_WASHING_POUR_OLD_WATER;
						Process_washing_states.step = WASHING_FILL_TANK;
					}
				break;
			}
		break;

		case WASHING_FILL_TANK: // Стирка в нормальном режиме
			if(!Machine_states.running_stir_powder && !Machine_states.stir_powder_finish) // Eсли наполнение водой еще не включено, включаем наполнение водой
			{
				Machine_states.current_machine_state = LED_WASHING;

				if(Machine_states.level_water_in_tank >= SENSOR_WATER_LEVEL1) //Если уровень воды больше или равен первому уровню то воду несливаем, сразу переходим на стирку.
				{
					Machine_states.need_temperature = conv_temperature_to_adc_val(Washing_temperatures[Washing_settings.temperature_part1]);
					Process_washing_states.step = WAIT_END_NORMAL_WASHING;
					break;
				}
				start_fill_tank_and_stir_powder(Washing_settings.washing_water_level, Washing_settings.washing_water_soure - 1, conv_temperature_to_adc_val(Washing_temperatures[Washing_settings.temperature_part1]));

				draw_step_name(GET_MESS_POINTER(WASHING_MESSAGE), 1, WASHING_COUNT_STEPS, is_active);
				draw_operation_name(GET_MESS_POINTER(BALANSING_MESSAGE), is_active); // Выводим на экран "Балансировка белья"
			}

			if(Machine_states.running_stir_powder) //Если запущен процесс размешивания порошка, то выводим его тукущее состояние
			{
				if(Machine_states.stir_powder_display_status != prev_stir_powder_display_status) //Если изменилось состояние то выводим его на экран
				{
					switch(Machine_states.stir_powder_display_status)
					{
						case 1: //Если белье раскрутилось до нужеой скорости
						case 2: //Если белье несмогло раскрутиться до нужного уровня
							draw_step_name(GET_MESS_POINTER(WASHING_MESSAGE), 2, WASHING_COUNT_STEPS, is_active);
							draw_operation_name(GET_MESS_POINTER(FILL_TANK_MESSAGE), is_active); // Выводим на экран "Заполнение водой"
						break;

						case 3: //Если бак заполнился до первого уровня то устанавливаем время размешивания
							draw_step_name(GET_MESS_POINTER(WASHING_MESSAGE), 3, WASHING_COUNT_STEPS, is_active);
							draw_operation_name(GET_MESS_POINTER(STIR_POWDER_MESSAGE), is_active); // Выводим на экран "Размешивание порошка"
						break;

						case 4:  //Если размешивание завершенно
							draw_step_name(GET_MESS_POINTER(WASHING_MESSAGE), 4, WASHING_COUNT_STEPS, is_active);
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
				Process_washing_states.step = WAIT_END_NORMAL_WASHING;
				printf_P(PSTR("Go to PART1\r\n"));
			}
		break;

		case WAIT_END_NORMAL_WASHING: // Ожидаем завершения обычной стирки
			if(!Machine_states.runing_rotating && Process_washing_states.washing_timer == 0) //Если обычной запуск стирки не осуществлен. Условие Process_washing_states.washing_timer == 0 проверяется для того чтобы по истечению времени обычной стирки, запустилась интенсивная стирка (если убрать данное условие то запуститься опять обычная).
			{
				// Запускаем периодическое вращение белья
				if(Washing_settings.washing_mode == 0) // Если выбран режим нормальной стирки, то вращаем белье в стандартном режиме, иначе вращаем его очень бережно
					start_mode_rotating(NORMAL_ROTATING);
				else
					start_mode_rotating(COAT_ROTATING);

				draw_step_name(GET_MESS_POINTER(WASHING_MESSAGE), 5, WASHING_COUNT_STEPS, is_active);
				draw_operation_name(GET_MESS_POINTER(PART1_WASHING_MESSAGE), is_active); // Выводим на экран "Cтираю. Фаза 1"

				Process_washing_states.washing_timer = Washing_settings.washing_time_part1 * 60; // Устанавливаем время стирки в соответствии с фазой один
				printf_P(PSTR("Start PART1\r\n"));
			}

			if(Process_washing_states.washing_timer == 1) //Если время нормальной стирки истекло
			{
				printf_P(PSTR("END PART1\r\n"));
				stop_mode_rotating();
			}

			if(Machine_states.drum_is_stopped && Process_washing_states.washing_timer == 1) //Дожидаемся остановки барабана
			{
				Process_washing_states.washing_timer = 0;
				Machine_states.drum_is_stopped = 0;
				printf_P(PSTR("PREPARE PART2\r\n"));

				if(!Washing_settings.enable_washing_part2) //Если вторая фаза стирки выключенна
				{
					Process_washing_states.washing_timer = 1; //То переходим на следующий этап
					Process_washing_states.step = WAIT_END_INTENSIVE_WASHING;
					return;
				}
				else
					printf_P(PSTR("Go to PART2\r\n"));
				//  Переходим в фазу два
					// Запускаем интенсивное вращение белья в соответствии с фазой два
				if(Washing_settings.washing_mode == 0) // Если выбран режим нормальной стирки, то вращаем белье в интенсивное режиме, иначе вращаем его очень бережно
					start_mode_rotating(INTENSIVE_ROTATING);
				else
					start_mode_rotating(COAT_ROTATING);

				draw_step_name(GET_MESS_POINTER(WASHING_MESSAGE), 6, WASHING_COUNT_STEPS, is_active);
				draw_operation_name(GET_MESS_POINTER(PART2_WASHING_MESSAGE), is_active); // Выводим на экран "Cтираю. Фаза 2"

				Machine_states.need_temperature = conv_temperature_to_adc_val(Washing_temperatures[Washing_settings.temperature_part2]); // Устанавливаем температуру для фазы 2
				Process_washing_states.washing_timer = Washing_settings.washing_time_part2 * 60; // Устанавливаем время стирки в соответствии с фазой два
				Process_washing_states.step = WAIT_END_INTENSIVE_WASHING;
			}
		break;

		case WAIT_END_INTENSIVE_WASHING: // Ожидаем завершения второй фазы
			if(Process_washing_states.washing_timer == 1) //Если время фазы два истекло
			{
				printf_P(PSTR("END PART2\r\n"));
				Process_washing_states.washing_timer = 0;
				Machine_states.need_temperature = 0; //Выключаем нагрев воды
				turn_heater(OFF);

				if(Current_temperature < MAX_POUR_OUT_WATER_TEMPERATURE)
				{
					pour_out_water();
					Process_washing_states.step = WAIT_POUR_OUT_WATER;
					draw_step_name(GET_MESS_POINTER(WASHING_MESSAGE), 8, WASHING_COUNT_STEPS, is_active);
					draw_operation_name(GET_MESS_POINTER(POUR_WATER_MESSAGE), is_active); // Выводим на экран "Слив воды"
				}
				else
				{
					start_fill_tank(8, Washing_settings.washing_water_soure - 1, 0); //Включаем заполнение бака водой
					draw_step_name(GET_MESS_POINTER(WASHING_MESSAGE), 7, WASHING_COUNT_STEPS, is_active);
					draw_operation_name(GET_MESS_POINTER(COOLING_WATER_MESSAGE), is_active); // Выводим на экран "Охлаждение воды"
					Process_washing_states.step = WASHING_COOLING_WATER;
				}
			}
		break;

		case WASHING_COOLING_WATER: // Этап охлаждения воды
			if(Current_temperature < MAX_POUR_OUT_WATER_TEMPERATURE || Machine_states.tank_fill_finish) //Как только вода достигла температуры при которой её можно сливать в канализацию
			{
				Machine_states.tank_fill_finish = 0;
				stop_fill_tank();
				pour_out_water();
				Process_washing_states.step = WAIT_POUR_OUT_WATER;
				draw_step_name(GET_MESS_POINTER(WASHING_MESSAGE), 8, WASHING_COUNT_STEPS, is_active);
				draw_operation_name(GET_MESS_POINTER(POUR_WATER_MESSAGE), is_active); // Выводим на экран "Слив воды"
			}
		break;

		case WAIT_POUR_OUT_WATER: //Ожидаем завершения сливания воды
			if(Machine_states.pour_out_finish)
			{
				Machine_states.pour_out_finish = 0;
				stop_mode_rotating();
				Process_washing_states.step = STEP_RINSE;
			}
		break;

		case STEP_RINSE: //Полоскаем
			if(Process_washing_states.rinse_counter < Washing_settings.count_rinses) // Проверяем сколько мы отполоскали
			{
				if(!Machine_states.running_rinse && !Machine_states.rinse_finish) // Если полоскание еще незапущенно, запускаем
				{
					static uint8_t water_source = 0;
					Machine_states.current_machine_state = LED_RINSE;

					if(Process_washing_states.rinse_counter == (Washing_settings.count_rinses - 1)) //если наступило последнее полоскание
					{
						water_source = WATER_SOURCE3; // Устанавливаем забор воды из последнего отсека
						if(Washing_settings.signal_swill) //Если включен режим оповещения перед последним полосканием
						{
							draw_operation_name(GET_MESS_POINTER(WAIT_LAST_RINSE_MESSAGE), is_active); // Выводим на экран "Дайте мне крахмал!"
							Process_washing_states.step = WAIT_BEFORE_LAST_RINSE; // Переходим в режим ожидания заливки ополаскивателя
							Process_washing_states.timer_wait_last_swill = 1;
							Process_washing_states.count_wait_last_swill = 0;
							return;
						}
					}
					else
						water_source = Washing_settings.washing_water_soure - 1; // Иначе забираем воду из отсека установленного в настройках

					// Запускаем полоскание белья
					if(Washing_settings.washing_mode == 0) // Если выбран режим нормальной стирки, то полоскаем белье в стандартном режиме, иначе полоскаем его очень бережно
						run_rinse(INTENSIVE_ROTATING, water_source);
					else
						run_rinse(RINSE_COAT_ROTATING, water_source);

					draw_step_name(GET_MESS_POINTER(RINSE_MESSAGE), Process_washing_states.rinse_counter + 1, Washing_settings.count_rinses, is_active);
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

				if(Machine_states.rinse_finish) //Если полоскание завершенно
				{
					Machine_states.drum_is_stopped = 0;
					Machine_states.rinse_finish = 0;
					Process_washing_states.rinse_counter++;
				}
			}
			else // Если отполоскали нужное количество раз
			{
				Process_washing_states.wring_counter = 0;
				Process_washing_states.step = STEP_WRINGS;
				Process_washing_states.wring_step = STEP_WRING;
			}
		break;

		case WAIT_BEFORE_LAST_RINSE:  // Ожидаем заполнене 3тьего отсека ополаскивателем в течении установленного времени
			if(Process_washing_states.timer_wait_last_swill == 1) // Если истекло время ожидания перед последним полосканием
			{
				if(Process_washing_states.count_wait_last_swill >= TIMEOUT_WAIT_LAST_SWILL_COUNT) //Если количество попыток ожидать ополаскивателя, превышенно
				{
					Process_washing_states.count_wait_last_swill = 0;
					Process_washing_states.timer_wait_last_swill = 0;
					Washing_settings.signal_swill = 0; //Выключаем ожидание крахмала
					Process_washing_states.step = STEP_RINSE; // Возвращаемся к этапу полоскания, на этот раз это будет последнее полоскание
					return;
				}

				turn_blink_speaker(ON, 600, 300, SPEAKER_SOUND_FREQ, 5); //Пищим пять раз
				Process_washing_states.timer_wait_last_swill = TIMEOUT_WAIT_LAST_SWILL;
				Process_washing_states.count_wait_last_swill ++;  //Увеличиваем счетчик попыток ожидания
			}
		break;

		case STEP_WRINGS: // Этап отжима
			if(Process_washing_states.wring_counter < WASHING_WRINGS_COUNT)
			{
				switch(Process_washing_states.wring_step)
				{
					case STEP_WRING: // Отжимаем
						if(!Machine_states.running_wring && !Machine_states.wring_finish) // Если отжим еще незапущен, запускаем
						{
							Machine_states.current_machine_state = LED_WRING;
							run_wring(Washing_settings.wring_speed, Washing_settings.wring_time * 60, 0);
						}

						if(Machine_states.running_wring) // Если отжим уже запущен
						{
							if(prev_wring_display_status != Machine_states.wring_display_status)
							{
								switch(Machine_states.wring_display_status)
								{
									case 1: // Распределяем белье
										draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 1, WRING_COUNT_STEPS, is_active);
										draw_operation_name(GET_MESS_POINTER(STIR_DRESS_MESSAGE), is_active); // Выводим на экран "Распределяю белье"
									break;

									case 2: // Балансировка
										draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 2, WRING_COUNT_STEPS, is_active);
										draw_operation_name(GET_MESS_POINTER(BALANSING_MESSAGE), is_active); // Выводим на экран "Балансирую бельё"
									break;

									case 3: // Всполаскивание
										draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 3, WRING_COUNT_STEPS, is_active);
										draw_operation_name(GET_MESS_POINTER(RINSING_MESSAGE), is_active); // Выводим на экран "Полоскаю..."
									break;

									case 4: // Отжимаем на нужной скорости
										draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 4, WRING_COUNT_STEPS, is_active);
										draw_operation_name(GET_MESS_POINTER(BEST_WRINGING_MESSAGE), is_active); // Выводим на экран "Отлично отжимаю..."
									break;

									case 5: // Отжимаем на крайне низкой скорости
										draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 4, WRING_COUNT_STEPS, is_active);
										draw_operation_name(GET_MESS_POINTER(BAD_WRINGING_MESSAGE), is_active); // Выводим на экран "Плохо отжимаю..."
									break;

									case 6: // Отжимаем на низкой скорости
										draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 4, WRING_COUNT_STEPS, is_active);
										draw_operation_name(GET_MESS_POINTER(GOOD_WRINGING_MESSAGE), is_active); // Выводим на экран "Нормально отжимаю..."
									break;

									case 7: // Отжимаем на низкой скорости
										draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 4, WRING_COUNT_STEPS, is_active);
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
								Machine_states.wring_finish = 0;
								Process_washing_states.wring_step = WAIT_STIR_DRESS; // Если отжим кое-как завершон то перемешиваем белье
								start_mode_rotating(INTENSIVE_ROTATING);
								draw_step_name(GET_MESS_POINTER(WRING_MESSAGE), 5, WRING_COUNT_STEPS, is_active);
								draw_operation_name(GET_MESS_POINTER(STIR_DRESS_MESSAGE2), is_active); // Выводим на экран "Потрашу белье"
								Process_washing_states.washing_timer = STIR_DRESS_TIME;
							break;
						}

					case WAIT_STIR_DRESS: // Ожидаем когда белье разрыхлится после отжима
						if(Process_washing_states.washing_timer == 1) // Если время разрыхления белья истекло
						{
							Process_washing_states.washing_timer = 0;
							stop_mode_rotating();
							Process_washing_states.wring_counter ++;
							Process_washing_states.wring_step = STEP_WRING;
						}
					break;
				}
			}
			else // Если отжали нужное количество раз
			{
				Process_washing_states.step = END_WASHING;
			}
		break;

		case CANCEL_WASHING:
			if(!Process_washing_states.cancel)
			{
				uint8_t water_out_enable = 1; //данный флаг означает нужно ли сливать воду или нет. 1 - вода будет слита
				if(Process_washing_states.pause) //Если в данный момент установленна пауза то снимаем её
				{
					water_out_enable = 0;
					washing_pause();
				}

				send_signal(PROCESS_BEDABBLE, 7); //Посылаем сигнал быстро прервать замачивание
				switch_process(PROCESS_WASHING);
				stop_rinse();
				stop_wring();
				stop_mode_rotating();
				stop_fill_tank_and_stir_powder();

				if(water_out_enable) // если надо слить воду - то запускаем слив
					pour_out_water();
				else
					Machine_states.pour_out_finish = 1; // иначе устанавливаем флаг что вода уже слита

				Process_washing_states.cancel = 1;
				draw_operation_name(GET_MESS_POINTER(CANCELING_WASHING_MESSAGE), is_active); // Выводим на экран "Отмена стирки"

				// Перерасчет времени завершения стирки
				Process_washing_states.calculate_time = Main_current_time;
				time_add(&Process_washing_states.calculate_time, 1); // Расчитываем расчетное время завершения исходя из среднестатистических данных времени слива воды.
				draw_calculate_time(&Process_washing_states.calculate_time, is_active);
			}

			if(Machine_states.pour_out_finish && Process_washing_states.cancel) //Дождались слива воды, и завершили процесс
			{
				Machine_states.pour_out_finish = 0;
				Process_washing_states.cancel = 0;
				draw_calculate_time(&Main_current_time, 0); //Отрисовываем время завершения
				draw_operation_name(GET_MESS_POINTER(WASHING_FINISH_CANCEL_MESSAGE), is_active); // Выводим на экран "Стирка прервана"
				send_signal(PROCESS_SCREENSAVER, 3); //Посылаем в скринсейвер сигнал отрисовать результаты стирки
				stop_process(PROCESS_WASHING);
			}
		break;

		case END_WASHING:
			Machine_states.current_machine_state = LED_FINISH;
			draw_operation_name(GET_MESS_POINTER(WASHING_FINISH_MESSAGE), 0); // Выводим на экран "Стирка завершена"
			draw_calculate_time(&Main_current_time, 0); //Отрисовываем время завершения
			send_signal(PROCESS_SCREENSAVER, 3); //Посылаем в скринсейвер сигнал отрисовать результаты стирки
			stop_process(PROCESS_WASHING);
		break;
	}
}

CALLBACK_NAME(PROCESS_WASHING, click_esc)(void)
{
	switch_process(PROCESS_MENU); //Вход в меню
}

CALLBACK_NAME(PROCESS_WASHING, click_enter)(void)
{
	if(Process_washing_states.step == WAIT_BEFORE_LAST_RINSE) //Если мы сейчас ожидаем крахмала то ENTER переведет машину в состояние продолжать полоскать
	{
		Process_washing_states.timer_wait_last_swill = 1;
		Process_washing_states.count_wait_last_swill = TIMEOUT_WAIT_LAST_SWILL_COUNT;
	}
	else //Иначе ENTER - это просто пауза
		washing_pause();
}

CALLBACK_NAME(PROCESS_WASHING, console)(char input_byte)
{
	switch(input_byte)
	{
		case '7':
			switch_process(PROCESS_MENU); //Вход в меню
		break;

		case '6':
			washing_pause(); //Пауза
		break;
	}
}

CALLBACK_NAME(PROCESS_WASHING, signal)(uint8_t signal) // Обработчик входящего сигнала
{
	printf("Incoming signal = %d\r\n", signal);
	switch(signal)
	{
		case 5: //Сигнал, остановить стирку
			Machine_states.pause = 0;
			Process_washing_states.step = CANCEL_WASHING;
		break;

		case 8: //Сигнал, пауза/отмена паузы
			washing_pause();
		break;

		case 10:
			printf_P(PSTR("step = %d\r\n"), Process_washing_states.step);
			printf_P(PSTR("prev_washing_step = %d\r\n"), Process_washing_states.prev_washing_step);
			printf_P(PSTR("rinse_counter = %d\r\n"), Process_washing_states.rinse_counter);
			printf_P(PSTR("wring_step = %d\r\n"), Process_washing_states.wring_step);
			printf_P(PSTR("washing_timer = %d\r\n"), Process_washing_states.washing_timer);
			printf_P(PSTR("pause = %d\r\n"), Process_washing_states.pause);
			printf_P(PSTR("cancel = %d\r\n"), Process_washing_states.cancel);
			printf_P(PSTR("calc_work_time = %d\r\n"), Process_washing_states.calc_work_time);
			printf_P(PSTR("time_bedabble = %d\r\n"), Process_washing_states.time_bedabble);
		break;
	}
}

CALLBACK_NAME(PROCESS_WASHING, timer0)(void)
{
	if(!Process_washing_states.pause) //Увеличиваем рабочее время на одну секунду если пауза не активированна
		inc_time(&Process_washing_states.work_time);

	if(Process_washing_states.washing_timer > 1)
		Process_washing_states.washing_timer--;

	if(Process_washing_states.timer_wait_last_swill > 1)
		Process_washing_states.timer_wait_last_swill--;

	if(Process_washing_states.time_bedabble > 0) //Секундомер для засекания времени потраченного на замачивание
		Process_washing_states.time_bedabble++;

	if(Process_washing_states.door_timer > 1)
		Process_washing_states.door_timer--;
}

CALLBACK_NAME(PROCESS_WASHING, stop)(void)
{
	block_door(OFF); // Разблокируем дверку
	if(Process_washing_states.pause) //Если установленна пауза, то снимаем её
		washing_pause();
	triggered_menu_items(trigger_run_stop_washing, sizeof(trigger_run_stop_washing));

	stop_fill_tank_and_stir_powder();
	stop_rinse();
	stop_wring();
	stop_mode_rotating();
}


