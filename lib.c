#include <stdint.h>
#include "drivers.h"
#include "config.h"
#include "lib.h"
#include "messages.h" //Файл со всеми текстовыми строками сообщений
#include "lcd_buffer.h"
#include "../../lib/button.h"
#include <avr/eeprom.h>


extern struct machine_states Machine_states; // Состояние всей машины
extern struct machine_timers Machine_timers;  // Состояния всех таймеров
extern struct machine_settings Machine_settings; // Глобальные настройки стиральной машины
extern int16_t Current_temperature; // Текущая температура воды
extern uint16_t Rotating_speed; // Текущая скорость вращения барабана
extern int16_t Motor_power; // Требуемая выходная мощность на мотор
extern int16_t Current_vibration; //Текущий уровень вибрации
extern uint16_t Speaker_frequency_timer; //Таймер для отсчета периуда колебаний диффузора динамика
extern const char *Error_message; // В случе возникновения ошибки, сюда присваивается текст ошибки
extern struct button Buttons[]; //Массив состояния всех кнопок



struct rotating_mode Rotating_modes[] =
{
	{ //Вращение барабана для режима замачивание
		.left_rotate_speed = 15,
		.left_rotate_time = 10,
		.middle_pause_time = 5,
		.right_rotate_speed = 10,
		.right_rotate_time = 15,
		.pause_time = 1200,
	},

	{ //Вращение барабана для стирки шерсти
		.left_rotate_speed = 12,
		.left_rotate_time = 8,
		.middle_pause_time = 120,
		.right_rotate_speed = 12,
		.right_rotate_time = 8,
		.pause_time = 120,
	},

	{ //Вращение барабана для интенсивной стирки шерсти
		.left_rotate_speed = 10,
		.left_rotate_time = 12,
		.middle_pause_time = 60,
		.right_rotate_speed = 10,
		.right_rotate_time = 12,
		.pause_time = 60,
	},

	{ //Вращение барабана для полоскания шерсти
		.left_rotate_speed = 10,
		.left_rotate_time = 10,
		.middle_pause_time = 30,
		.right_rotate_speed = 10,
		.right_rotate_time = 10,
		.pause_time = 30,
	},

	{ // Обычное вращение барабана
		.left_rotate_speed = 15,
		.left_rotate_time = 10,
		.middle_pause_time = 5,
		.right_rotate_speed = 21,
		.right_rotate_time = 15,
		.pause_time = 5,
	},

	{ // Интенсивное вращение барабана
		.left_rotate_speed = 17,
		.left_rotate_time = 20,
		.middle_pause_time = 1,
		.right_rotate_speed = 20,
		.right_rotate_time = 17,
		.pause_time = 1,
	},
};


//Включить наполнение бака водой до указанного уровня,
// level - Требуемый уровень воды. Если уровень воды равен 1 то это значит набрать воду до первого срабатывания датчика уровня. Все что выше 1 означает наполнение водой бака до уровня в (1 + (level - 1) / 2) .
// water_source - источник забора воды,
// temperature - требуемая температура воды, 0 - означает что нагревать воду неследует
uint8_t start_fill_tank(uint8_t level, uint8_t water_source, uint16_t temperature)
{
	printf_P(PSTR("method: start_fill_tank\r\n"));
    if(level <= Machine_states.need_water_level || Machine_states.level_water_in_tank == SENSOR_WATER_LEVEL2 || Machine_states.run_fill_tank) // Если уровень воды в баке выше или равен заданному то наливать воду нестоит
	{	// Если текущий уровень воды больше либо равен требуемому то покрайней мере включаем нагрев воды
		Machine_states.need_temperature = temperature;
		Machine_states.tank_fill_finish = 1; //и говорим что якобы залили солько требовалось
		printf_P(PSTR("Error start_fill_tank\r\n"));
        return 1;
	}

	if(Machine_states.level_water_in_tank < SENSOR_WATER_LEVEL1) // Если воды в баке нет
	{
		Machine_timers.level1_time = MAX_TIME_FILL_TUNK;
		Machine_states.complete_fill_on_level1 = 0;
		Machine_timers.fill_tunk_timer = 0;
	}
	else // Если вода уже есть в баке то нам известно время её наполнения до первого уровня и соответственно можно расчитать время на которое нужно открыть клапан до заполнения водой до указанного уровня
	{
		if(Machine_states.time_fill_on_level1 == 0) // Если отсутсвует время наполнения водой бака до первого уровня, то неизвестно на сколько времени нужно включить еще набор воды
			return 1;

		Machine_states.complete_fill_on_level1 = 1;
		Machine_timers.fill_tunk_timer = (Machine_states.time_fill_on_level1 / 2) * (level - (Machine_states.need_water_level - 1)  - 1);
	}

	Machine_states.demand_temperature = temperature;
    Machine_states.tank_fill_finish = 0;
	Machine_states.full_tank_count_error = 0;

    turn_water_source(water_source, ON);
    Machine_states.run_fill_tank = ON;
    Machine_states.need_water_level = level;
    Machine_states.need_water_source = water_source;

	return 0;
}

void stop_fill_tank() // Остановить заполнение водой
{
	Machine_states.run_fill_tank = OFF;
	turn_water_source(0, 0);
	turn_pump(OFF);
	printf_P(PSTR("method: stop_fill_tank\r\n"));
}

void pour_out_water() // Запустить слив воды
{
	turn_pump(ON);
	Machine_states.pour_out_water = ON;
	Machine_states.pour_out_finish = 0;
	Machine_states.need_temperature = 0;
	printf_P(PSTR("method: pour_out_water\r\n"));
}

void stop_pour_out_water() // Остановить слив воды
{
	turn_pump(OFF);
	Machine_states.pour_out_water = OFF;
	Machine_timers.pour_out_water_timer = 0;
	Machine_states.complete_fill_on_level1 = 0;
	printf_P(PSTR("method: stop_pour_out_water\r\n"));
}


void check_water_level() // Функция встраивается в основной цикл и проверяет текущий уровень воды
{
	static uint8_t pause_pour_out_water = 0;
	static uint8_t pause_fill_tank = 0;

	// Опрашиваем пресостат (датчик уровня воды)
	Machine_states.level_water_in_tank = TANK_IS_EMPTY; // В независимости от того есть ли PRESOSTAT_LEVEL1 или нет

/*	if(!Buttons[PRESOSTAT_LEVEL1].state.state_button) //Если датчик уровня не подключен
		sys_error(HARDWARE_ERROR_1, GET_MESS_POINTER(ERROR_MALFUNCTION_WATER_LEVEL_SENSOR)); // Неисправен датчик уровня воды*/

	if(Buttons[PRESOSTAT_LEVEL2].state.state_button && !Buttons[PRESOSTAT_LEVEL3].state.state_button)
		Machine_states.level_water_in_tank = SENSOR_WATER_LEVEL1;

	if(Buttons[PRESOSTAT_LEVEL2].state.state_button && Buttons[PRESOSTAT_LEVEL3].state.state_button)
		Machine_states.level_water_in_tank = SENSOR_WATER_LEVEL2;


	if(Machine_states.run_fill_tank) // Если в данный момент времени осуществляется набор воды
	{
		if(Machine_states.pause && !pause_fill_tank) //Если сработала пауза
		{
			pause_fill_tank = 1;
			turn_water_source(0, 0);
		}

		if(!Machine_states.pause && pause_fill_tank) //Если отменили паузу
		{
			pause_fill_tank = 0;
			turn_water_source(Machine_states.need_water_source, ON);
		}

		if(Machine_timers.level1_time == 1) // Если истек таймер ожидания набора воды
		{
			sys_error(EXTERNAL_ERROR, GET_MESS_POINTER(ERROR_NOT_WATER)); // Ошибка набора воды
			Machine_states.need_water_level = 0;
			Machine_timers.level1_time = 0;
		}

		if(Machine_states.level_water_in_tank == SENSOR_WATER_LEVEL1 && !Machine_states.complete_fill_on_level1) // Если достигли первого уровня воды в первый раз
		{
			Machine_states.time_fill_on_level1 = MAX_TIME_FILL_TUNK - Machine_timers.level1_time; // Вычисляем время за которотое был заполнен бак до первого уровня
			Machine_timers.level1_time = 0; // Выключаем таймер отсчета времени за которое набралася вода до первго уровня

			Machine_states.complete_fill_on_level1 = 1; // Объявляем об успешном заполнении водой до первого уровня
			if(Machine_states.need_water_level > Machine_states.level_water_in_tank) // Если требуется заполнить бак до более высокой отметки
				Machine_timers.fill_tunk_timer = (Machine_states.time_fill_on_level1 / 2) * (Machine_states.need_water_level - 1); // расчитываем время заполнения бака водой чтобы получить требуемый уровень
			else // Если требовалось заполнить бак до первого уровня, то готово.
			{
				Machine_states.tank_fill_finish = 1; //Устанавливаем флаг индицирующий успешное наполнение водой
				stop_fill_tank();
				Machine_states.need_temperature = Machine_states.demand_temperature; // Включаем нагрев воды если  Machine_timers.demand_temperature выше 0ля
			}
		}

		if(Machine_states.complete_fill_on_level1 && Machine_timers.fill_tunk_timer == 1) // Если нужно заполнить бак выше первого уровня, то ожидае пока истечет расчетное время заполнения
		{
			Machine_timers.fill_tunk_timer = 0;
			Machine_states.tank_fill_finish = 1; //Устанавливаем флаг индицирующий успешное наполнение водой
			stop_fill_tank();
			Machine_states.need_temperature = Machine_states.demand_temperature; // Включаем нагрев воды если  Machine_timers.demand_temperature выше 0ля
		}

		if(Machine_states.level_water_in_tank >= SENSOR_WATER_LEVEL2) // Если сработал датчик крайнего уровня воды
		{
			if(Machine_states.full_tank_count_error < MAX_COUNT_ERROR_FULL_TANK) //Считаем количество срабатываний. если оно превысило максимум, значит вероятно это не ошибка.
				Machine_states.full_tank_count_error++;
			else // останавливаем набор воды
			{
				Machine_states.tank_fill_finish = 1; //Устанавливаем флаг индицирующий успешное наполнение водой
				stop_fill_tank();
				printf_P(PSTR("FULL TANK!!!!\r\n"));
				Machine_states.need_temperature = Machine_states.demand_temperature; // Включаем нагрев воды если  Machine_timers.demand_temperature выше 0ля
			}
		}
		else // Если сигнал FULL_TANK отсутсвует то обнуляем счетчик срабаттываний FULL_TANK.
			Machine_states.full_tank_count_error = 0;
	}

	if(Machine_states.pour_out_water) // Если включен слив воды
	{
		if(Machine_states.pause && !pause_pour_out_water) //Если сработала пауза
		{
			pause_pour_out_water = 1;
			turn_pump(OFF);
		}

		if(!Machine_states.pause && pause_pour_out_water) //Если отменили паузу
		{
			pause_pour_out_water = 0;
			turn_pump(ON);
		}

		// Если уровень воды достиг 0левой отметки то запускаем таймер через который насос будет выключен
		if((Machine_states.level_water_in_tank == TANK_IS_EMPTY) && Machine_timers.pour_out_water_timer == 0)
		{
			Machine_timers.pour_out_water_timer = POUR_OUT_TIME;
			Machine_states.need_water_level = 0;
		}

		//если уровень воды достиг 0левой  и прошло время слива воды то выключаем слив
		if((Machine_states.level_water_in_tank == TANK_IS_EMPTY) && Machine_timers.pour_out_water_timer == 1)
		{
			Machine_states.pour_out_finish = 1; //Устанавливаем флаг индицирующий успешный слив воды
			stop_pour_out_water();
		}
	}

	if(Current_temperature == 0) //Если неисправен датчик температуры
		sys_error(HARDWARE_ERROR_1, GET_MESS_POINTER(ERROR_MALFUNCTION_THERMO_SENSOR)); // Неисправен термо-датчик


	if(Machine_states.level_water_in_tank == TANK_IS_EMPTY) // Если воды в баке нет, то выключаем ТЕН в независимости от того был ли он включон до этого. (Это защита от выхода тена из строя в случае исчезновения воды)
	{
		turn_heater(OFF); //Выключить ТЕН
		Machine_states.heating_water = 0;
		Machine_states.complete_fill_on_level1 = 0;
/*		if(Machine_states.need_temperature && )
		{
			sys_error(HARDWARE_ERROR_2, GET_MESS_POINTER(ERROR_LEAKAGE_WATER)); //Утечка воды
			Machine_states.need_temperature = 0;
		}*/
	}
	else //Иначе, если вода в баке всеже есть, то контролируем заданную температуру
	{
		if(!Machine_states.pause) // Если пауза еще неактивированна
		{
			if(Machine_states.need_temperature && // Если текущая температура воды ниже требуемой температуры воды и нагрев еще не включон, то включаем нагрев
				(Current_temperature < (Machine_states.need_temperature - HYSTERESIS_TEMPERATURE)) &&
				!Machine_states.heating_water)
			{
				Machine_states.heating_water = 1;
				printf_P(PSTR("Auto turn_heater(ON)\r\n"));
				turn_heater(ON);
			}

			if(Machine_states.need_temperature && // Если текущая температура воды выше требуемой температуры воды и нагрев включон, то выключаем нагрев
				(Current_temperature >= (Machine_states.need_temperature + HYSTERESIS_TEMPERATURE)) &&
				Machine_states.heating_water)
			{
				Machine_states.heating_water = 0;
				printf_P(PSTR("Auto turn_heater(OFF)\r\n"));
				turn_heater(OFF);
			}
		}
		else // Если активированна пауза, то выключаем ТЕН
		{
			Machine_states.heating_water = 0;
			turn_heater(OFF);
		}

		if(!Machine_states.need_temperature && Machine_states.heating_water) // Если требуемая температура воды установлена в 0 то значит необходимо выключить ТЕН и прекратить контроль температуры
		{
			Machine_states.heating_water = 0;
			printf_P(PSTR("NEED turn_heater(OFF)\r\n"));
			turn_heater(OFF);
		}
	}
}

void start_mode_rotating(enum rotating_mode_names mode) //Функция запускает циклическое вращение барабана с указанным типом вращения
{
    Machine_states.rotating_mode = mode;
    Machine_states.current_rotating_step = ROTATE_LEFT;
	Machine_states.runing_rotating = 1;
	Machine_states.drum_is_stopped = 0;
}

void stop_mode_rotating() // Остановить циклическое вращение барабана
{
	disable_rotate_drum();
	Machine_states.runing_rotating = 0;
	Machine_states.current_rotating_step = ROTATE_LEFT;
	Machine_timers.rotating_drum_timer = 0;
}


uint8_t enable_rotate_drum(uint8_t transmission, uint16_t speed, uint8_t direction) //Включить вращение барабана с заданной скоростью
{
    if(Rotating_speed > MIN_SWITCH_ROTATE_SPEED)
        return 1;

	calculate_PD(0, 0);
    Motor_power = 0; // Устанавливаем 0левую мощность
    turn_motor(OFF, 0, 0);  // TODO: Это временно
    turn_motor(ON, direction, transmission); // Комутируем мотор
    Motor_power = 0; // TODO: Это временно
	Machine_states.drum_is_stopping = 0;
	Machine_states.drum_is_stopped	= 0;
    Machine_states.need_speed_rotating = speed; // Устанавливаем нужную скорость
	Machine_states.drum_is_runing_transmission = transmission;
	Machine_states.drum_is_runing_direction = direction;
	Machine_states.drum_is_runing = 1;

    return 0;
}

void disable_rotate_drum() // Выключить вращение барабана
{
    Motor_power = 0; //Понижаем мощность
	Machine_states.need_speed_rotating = 0; // Выключаем стабилизатор скорости
    Machine_states.drum_is_stopping = 1; // Устанавливаем флаг остановки мотора
}

void change_transmission(uint8_t transmission) // Переключиться на указанную передачу при вращающемся барабане
{
	uint16_t need_speed;
	if(!Machine_states.drum_is_runing || !Rotating_speed || Machine_states.drum_is_runing_transmission == transmission) // Если барабан невращается в данный момент или в данный момент уже активированна передача на которую следует переключится, то ничего неделаем
		return;

	need_speed = Machine_states.need_speed_rotating;
	Machine_states.need_speed_rotating = 0; // Временное выключение ПД алгоритма
	Motor_power = 0; // Временное понижение мощности
	calculate_PD(0, 0); // Отчистка ПД регулятора
	turn_motor(OFF, 0, 0); //Выключаем питание на мотор
	turn_motor(ON, Machine_states.drum_is_runing_direction, transmission); // Включаем питание мотора на указанную передачу
	Machine_states.need_speed_rotating = need_speed; // Устанавливаем нужную скорость (включаем ПД алгоритм)
	Machine_states.drum_is_runing_transmission = transmission;
}


void rotate_drum_if_need() // Функция встраивается в основной цикл и занимается вращениями барабана если rotating_mode > 0
{
	static uint8_t pause = 0;
    if(Machine_states.drum_is_stopping && Rotating_speed < MIN_SWITCH_ROTATE_SPEED) //Если барабан необходимо остановить и его скорость вращения стала минимальной то отключаем комутацию
    {
		calculate_PD(0, 0); // Сбрасываем кэш регулятора
		Motor_power = 0;
        turn_motor(OFF, 0, 0); // Выключаем питание барабана1
        Machine_states.drum_is_stopping = 0;
        Machine_states.need_speed_rotating = 0;
        Machine_states.drum_is_stopped = 1; // Устанавливаем флаг что мотор остановился
		Machine_states.drum_is_runing = 0;
    }

	if(Machine_states.runing_rotating) //Если запущен режим вращения барабана
	{
		if(Machine_states.pause && !pause) //Если сработала пауза
		{
			pause = 1;
			disable_rotate_drum();
			Machine_states.drum_is_stopped = 0; // Снимаем флаг остановки мотора, чтобы далее система не занла что он остановлися
		}

		if(!Machine_states.pause && pause) //Если отменили паузу
		{
			pause = 0;
			Machine_states.drum_is_stopping = 1; // Устанавливаем флаг остановки мотора, чтобы далее система поняла что мотор уже остановился, и не ожидала его остановки
		}

		switch(Machine_states.current_rotating_step)
		{
			case ROTATE_LEFT:
				if(Machine_states.drum_is_runing == 0 && !Machine_states.drum_is_stopped) //Если барабан не вращается и ненаходится в состоянии остановки то запускаем вращение на три секунды
				{
					enable_rotate_drum(LOW_SPEED_ROTATION, Rotating_modes[Machine_states.rotating_mode].left_rotate_speed, ROTATION_BACKWARD);
					Machine_timers.rotating_drum_timer = Rotating_modes[Machine_states.rotating_mode].left_rotate_time + 1;
				}

				if(Machine_timers.rotating_drum_timer == 1) // Если прошло время left_rotate_time, останавливаем вращение
				{
					Machine_timers.rotating_drum_timer = 0;
					disable_rotate_drum();
				}

				if(Machine_states.drum_is_stopped) // Если барабан уже остановился, то переходим на следующий этап
				{
					Machine_states.drum_is_stopped = 0;
					Machine_states.current_rotating_step = MIDDLE_PAUSE;
					Machine_timers.rotating_drum_timer = Rotating_modes[Machine_states.rotating_mode].middle_pause_time + 1;
				}
			break;

			case MIDDLE_PAUSE:
				if(Machine_timers.rotating_drum_timer == 1) // Если прошло время middle_pause_time, запускаем вращение
				{
					Machine_timers.rotating_drum_timer = 0;
					Machine_states.current_rotating_step = ROTATE_RIGHT;
				}
			break;

			case ROTATE_RIGHT:
				if(Machine_states.drum_is_runing == 0 && !Machine_states.drum_is_stopped) //Если барабан не вращается то запускаем вращение
				{
					enable_rotate_drum(LOW_SPEED_ROTATION, Rotating_modes[Machine_states.rotating_mode].left_rotate_speed, ROTATION_FORWARD);
					Machine_timers.rotating_drum_timer = Rotating_modes[Machine_states.rotating_mode].right_rotate_time + 1;
				}

				if(Machine_timers.rotating_drum_timer == 1) // Если прошло время right_rotate_time,  останавливаем мотор
				{
					disable_rotate_drum();
					Machine_timers.rotating_drum_timer = 0;
				}

				if(Machine_states.drum_is_stopped) // Если барабан уже остановился, то переходим на следующий этап
				{
					Machine_states.drum_is_stopped = 0;
					Machine_states.current_rotating_step = ROTATE_PAUSE;
					Machine_timers.rotating_drum_timer = Rotating_modes[Machine_states.rotating_mode].pause_time + 1;
				}
			break;

			case ROTATE_PAUSE:
				if(Machine_timers.rotating_drum_timer == 1) // Если прошло время pause_time
				{
					Machine_timers.rotating_drum_timer = 0;
					Machine_states.current_rotating_step = ROTATE_LEFT;
				}
			break;
		}
	}
}

void run_wring(uint16_t max_speed, uint16_t time, uint8_t mode) // Запуск отжима, передается максимальная скорость вращения и желаемое время отжима
{									 // параметр mode если установлен в 1 то отжим запускается в упрощенном режиме
	uint32_t wring_speed;
	if(!mode) // Откачку воды включаем только при полноценном отжиме
		turn_pump(ON);
	Machine_states.wring_easy_mode = mode;
	Machine_states.drum_is_stopped = 0;
	Machine_states.wring_finish	= NO_WRINGED;
	Machine_states.wring_is_min_speed = 0;
	Machine_states.running_wring = 1;
	wring_speed = ((uint32_t)max_speed * (uint32_t)MAX_ROTATE_SPEED_ADC) / (uint32_t)MAX_SPEED_DRY;
	Machine_states.wring_speed = (uint16_t)wring_speed; // Устанавливаем желаемую скорость отжима

	Machine_states.wring_time = time; // Устанавливаем желаемое время отжима
	Machine_states.running_wring_step = SHAKE_DRESS; // Устанавливаем этап раскладки белья
	Machine_states.wring_balancing_attempt = 0; // Сбрасываем счетчик попыток расложить белье
	Machine_states.count_wring_rinse_attempt = 0; // Сбрасываем счетчик попыток сполоснуть белье для очередной попытки балансировки
}

void stop_wring() // Остановить отжим
{
	Machine_states.running_wring = 0;
	Machine_states.running_wring_step = SHAKE_DRESS;
	Machine_states.wring_finish	= NO_WRINGED;
	Machine_states.wring_display_status = 0;
	disable_rotate_drum();
	stop_fill_tank();
	turn_water_source(0, 0);
}

void process_wringing() //Функция встраивается в основной цикл и следит за процессом отжима
{
	static uint8_t pause = 0;

	if(Machine_states.running_wring)
	{
		if(Machine_states.pause && !pause) //Если сработала пауза
		{
			pause = 1;
			stop_wring(); // останавливаем отжим
			Machine_states.running_wring = 1;
		}

		if(!Machine_states.pause && pause) //Если отменили паузу
		{
			pause = 0; // запускаем отжим по новой
			run_wring(Machine_states.wring_speed, Machine_states.wring_time, Machine_states.wring_easy_mode);
		}

		switch(Machine_states.running_wring_step) // определяем этап процесса отжима белья
		{
			case SHAKE_DRESS: //Встряхнуть белье
				if(Machine_states.drum_is_runing == 0 && !Machine_states.drum_is_stopped) //Если барабан не вращается
				{
					Machine_states.wring_display_status = 1;
					enable_rotate_drum(LOW_SPEED_ROTATION, SPEED_FOR_SHAKE_DRESS, ROTATION_FORWARD); // Запускаем вращение барабана
					Machine_timers.rotating_drum_timer = TIME_FOR_SHAKE_DRESS;
				}

				if(Machine_timers.rotating_drum_timer == 1) // Если прошло время TIME_FOR_SHAKE_DRESS, останавливаем вращение
				{
					Machine_timers.rotating_drum_timer = 0;
					disable_rotate_drum();
				}

				if(Machine_states.drum_is_stopped) // Если барабан уже остановился, то переходим на следующий этап
				{
					Machine_states.drum_is_stopped = 0;
					Machine_states.running_wring_step = SHAKE_DRESS_STOPING;
					Machine_timers.rotating_drum_timer = WRING_BALANSING_DELAY_TIME;
				}
			break;

			case SHAKE_DRESS_STOPING: // Выдерживаем небольшое время для полной остановки белья и переходим в режим равномерного распределения белья
				if(Machine_timers.rotating_drum_timer == 1) // Если прошло время 2секунды
				{
					Machine_timers.rotating_drum_timer = 0;
					Machine_states.running_wring_step = TRY_TO_BALANCING;
					Machine_states.wring_vibration_criterion = TESTING_WRING_MIN_CRITERION;
				}
			break;

			case TRY_TO_BALANCING: // Попытка грамотно сбалансировать белье
				if(Machine_states.drum_is_runing == 0 && !Machine_states.drum_is_stopped) //Если барабан остановлен, запускаем вращение в противоположную сторону
				{
					Machine_states.wring_display_status = 2;
					enable_rotate_drum(LOW_SPEED_ROTATION, START_SPEED_FOR_ALLOCATE_DRESS, ROTATION_BACKWARD);
					Machine_timers.speed_change_timer = BALANCING_DELAY_TO_UP_SPEED; // Запускаем таймер по истечению которого будет увеличенна скорость вращения
				}

				if(Machine_timers.speed_change_timer == 1) // Пошагово увеличиваем скорость вращения для равномерного прилипания белья к стенкам
				{
					Machine_states.need_speed_rotating += BALANSING_CHANGE_SPEED_STEP;
					if(Machine_states.need_speed_rotating >= END_SPEED_FOR_ALLOCATE_DRESS) // Если достигли максимальной скорости для этапа раскладки белья
					{
						if(Machine_states.wring_easy_mode) // Если активирован упрощенный режим то прерываем процесс отжима
						{
							Machine_states.running_wring = 0;
							Machine_states.count_wring_rinse_attempt = 0;
							Machine_states.wring_finish = NORMAL_SPEED_SET;
							return;
						}

						change_transmission(HIGH_SPEED_ROTATION); // Переключаемся на повышенную передачу
						Machine_states.running_wring_step = INCRASE_WRING_SPEED; // Переходим на этап медленного набора скорости с контролем дисбаланса
						Machine_states.wring_speed_up_attempt = 0;
						Machine_timers.speed_change_timer = WRINGING_DELAY_CHANGE_SPEED;
					}
					else
						Machine_timers.speed_change_timer = BALANCING_DELAY_TO_UP_SPEED; // Запускаем таймер по истечению которого скорость будет увеличенна

					if(Rotating_speed > STICK_DRESS_SPEED && Current_vibration > Machine_states.wring_vibration_criterion + Machine_states.wring_easy_mode * (Machine_states.wring_vibration_criterion / 2) ) //Если уровень вибрации превышает норму то возвращаемся в режим балансировки белья. Кривохак с умножением на Machine_states.wring_easy_mode нужен для того, чтобы в случе установки данного бита значение критерия уровня дисбаланса было в два раза выше
					{
						Machine_states.running_wring_step = WAIT_FOR_REPEAT;
						disable_rotate_drum();
						Machine_timers.rotating_drum_timer = WRING_BALANSING_DELAY_TIME;
						Machine_states.wring_balancing_attempt++; // Увеличиваем счетчик попыток сбалансировать белье

						if(Machine_states.wring_vibration_criterion < TESTING_WRING_MAX_CRITERION) // Уменьшаем критерий прохождения теста дисбаланса с каждой неудачей отжать белье
							Machine_states.wring_vibration_criterion += TESTING_WRING_CRITERION_CHANGE_STEP;

						if(Machine_states.wring_balancing_attempt >= MAX_ATTEMPTS_BALANCING_DRESS) //Если превысили количество попыток сбалансировать белье то переходим на этап мини полоскания
						{
							if(Machine_states.wring_easy_mode) // Если активирован упрощенный режим то прерываем процесс отжима
							{
								Machine_states.count_wring_rinse_attempt = 0;
								Machine_states.wring_finish = WRINGED_ERROR;
								stop_wring(); // Завершаем процесс отжима
								return;
							}

							Machine_states.wring_balancing_attempt = 0;
							Machine_states.count_wring_rinse_attempt++;
							if(Machine_states.count_wring_rinse_attempt < MAX_ATTEMPTS_RINSE_BALANCING_DRESS) // Если не превысили количество попыток сполоснуть белье для очередной попытки балансировки
							{
								Machine_states.running_wring_step = RINSE_DRESS; //Переходим в режим всполаскивания белья
								turn_pump(OFF); // Выключаем слив воды
								start_fill_tank(SENSOR_WATER_LEVEL1, VALVE1, 0); //Включаем набор воды
								start_mode_rotating(NORMAL_ROTATING); // Запускаем вращение барабана в режиме полоскания
								Machine_states.wring_display_status = 3;
							}
							else
							{
								Machine_states.count_wring_rinse_attempt = 0;
								// Если неудалось сбалансировать белье
								Machine_states.wring_finish = WRINGED_ERROR;
								stop_wring(); // Завершаем процесс отжима
								sys_error(ERROR, GET_MESS_POINTER(ERROR_NOT_BALANSED)); // Немогу сбалансировать белье
							}
						}
					}
				}

			break;

			case INCRASE_WRING_SPEED: // Постепенный набор скорости до заданной
				if(Machine_timers.speed_change_timer == 1) // Пошаговый набор скорсти
				{
					if(Machine_states.need_speed_rotating < Machine_states.wring_speed) // Если недостигли заданной скорости то продолжаем набор скорости
					{
						Machine_states.need_speed_rotating += WRING_CHANGE_SPEED_STEP / 2;
					}
					else // Иначе, если достигли заданной скорости
					{
						Machine_states.running_wring_step = WRINGING_PROCESS; // Если достигли заданной скорости, то переходим в процесс отжима
						Machine_timers.wring_timer = Machine_states.wring_time; //Устанавливаем требуемое время отжима равное желаемому
						Machine_states.wring_display_status = 4;
					}

					if(Current_vibration > MAX_WRING_VIBRATION_LEVEL + 20) // Если уровень вибраций превышает норму
					{
						Machine_states.need_speed_rotating -= WRING_CHANGE_SPEED_STEP / 2; // Понижаем скорость
						if(Machine_states.need_speed_rotating < MIN_WRING_SPEED) // Если уровень вибрации превысил норму недостигнув МИНИМАЛЬНОЙ скорости отжима
						{
							if(Machine_states.wring_speed_up_attempt > COUNT_WRING_UP_ATTEMPT) // Если счетчик попыток достигнуть минимальной скорости отжима, превысил максимум, то запускаем отжим на максимально возможной скорсоти в три раза дольше
							{
								Machine_states.wring_is_min_speed = 1;
								Machine_states.running_wring_step = WRINGING_PROCESS; // Если не достигли минимальной скорости отжима
								Machine_states.wring_display_status = 5;
								Machine_timers.wring_timer = Machine_states.wring_time * 3; //Устанавливаем требуемое время отжима в четыре раза больше желаемого
							}
							else // Если есть еще попытки сбалансировать белье то запускаем балансировку белья
							{
								Machine_states.wring_speed_up_attempt++;
								Machine_states.running_wring_step = WAIT_FOR_REPEAT;
								disable_rotate_drum();
								Machine_timers.rotating_drum_timer = WRING_BALANSING_DELAY_TIME;
							}
						}
						else // Если уровень вибрации превысил норму, достигнув МИНИМАЛЬНОЙ скорости то продолжаем отжим в два раза дольше
						{
							Machine_states.running_wring_step = WRINGING_PROCESS; // Если не достигли заданной скорости но достигли минимальной скорости отжима, то переходим в процесс отжима
							Machine_timers.wring_timer = Machine_states.wring_time * 2; //Устанавливаем требуемое время отжима в два раза больше желаемого
							Machine_states.wring_display_status = 6;
						}
					}
					Machine_timers.speed_change_timer = WRINGING_DELAY_CHANGE_SPEED;
				}
			break;

			case WRINGING_PROCESS: // Процесс отжима
				if(Machine_timers.speed_change_timer == 1)
				{
					if(Current_vibration > MAX_WRING_VIBRATION_LEVEL + 20) // Если уровень вибраций превышает норму
					{
						Machine_states.need_speed_rotating -= WRING_CHANGE_SPEED_STEP / 2; // Понижаем скорость
					}
					Machine_timers.speed_change_timer = WRINGING_DELAY_CHANGE_SPEED;
				}

				if(Machine_timers.wring_timer == 1) //Если время отжима истекло, то завершаем отжим
				{
					stop_wring(); // Завершаем процесс отжима
					if(Machine_states.need_speed_rotating < Machine_states.wring_speed) //Если отжим был осуществлен не на желаемой скорости
					{
						if(Machine_states.wring_is_min_speed) // Если отжим был осуществлен на скорости ниже минимальной
							Machine_states.wring_finish = WRINGED_MIN_SPEED;
						else
							Machine_states.wring_finish = WRINGED_LOW_SPEED;
					}
					else
						Machine_states.wring_finish = WRINGED_OK;

					printf_P(PSTR("method: wring is finished by ret code %d\r\n"), Machine_states.wring_finish);
				}
			break;

			case WAIT_FOR_REPEAT: // Ожидаем остановку барабана для повтора
				if(Machine_timers.rotating_drum_timer == 1 && Machine_states.drum_is_stopped) // Если прошло время SHAKE_DRESS_PAUSE и мотор остановился
				{
					Machine_states.drum_is_stopped = 0;
					Machine_timers.rotating_drum_timer = 0;
					Machine_states.running_wring_step = SHAKE_DRESS;
				}
			break;

			case RINSE_DRESS: //Режим всполаскивания белья для очередной попытки разложить белье
				if(Machine_states.tank_fill_finish) // Ожидаем когда вода наполнится до нужного уровня
				{
					Machine_states.tank_fill_finish = 0;
					stop_mode_rotating(); // Останавливаем вращение барабана
					pour_out_water(); //запускаем слив воды
				}

				if(Machine_states.pour_out_finish) // Ожидаем завершения процесса слива воды
				{
					Machine_states.pour_out_finish = 0;
					Machine_timers.rotating_drum_timer = WRING_BALANSING_DELAY_TIME;
					Machine_states.running_wring_step = WAIT_FOR_REPEAT; // переходим в режим встряхивания белья
					turn_pump(ON);
				}
			break;
		}
	}
}

void run_rinse(uint8_t type_rotating, uint8_t water_source) //Запустить полоскание
{
	if(Machine_states.level_water_in_tank >= 1) //Если в баке на данный момент присутствует 1ый уровень воды
		pour_out_water(); //Запускаем слив воды

	Machine_states.running_rinse = 1;
	Machine_states.rinse_count_wring_error = 0;
	Machine_states.rinse_type_rotating = type_rotating;
	Machine_states.rinse_source_valve = water_source;
	Machine_states.running_rinse_step = RINSE_FILL_TANK;
	Machine_states.tank_fill_finish = 0;
}

void stop_rinse() //Остановить полоскание
{
	Machine_states.rinse_finish = 0;
	Machine_states.wring_finish = 0;
	Machine_states.running_rinse = 0;
	stop_pour_out_water();
	stop_mode_rotating();
	stop_wring();
}

void process_rinse() // 0Функция встраивается в основной цикл и следит за процессом полоскания
{
	if(Machine_states.running_rinse)
	{
		switch(Machine_states.running_rinse_step)
		{
			case RINSE_FILL_TANK:
				if(Machine_states.pour_out_water) //Если запущен слив воды то необходимо дождаться его завершения
				{
					Machine_states.rinse_display_status = 1;
					break;
				}

				if(!Machine_states.run_fill_tank && !Machine_states.tank_fill_finish)  // Если режим заполнения бака водой не активирован
				{
					printf_P(PSTR("RINSE_FILL_TANK 1\r\n"));
					start_fill_tank(RINSE_WATER_LEVEL, Machine_states.rinse_source_valve, 0); // Наполняем бак водой
					start_mode_rotating(Machine_states.rinse_type_rotating); // Запускаем вращение барабана с указанным типом вращения
					Machine_states.rinse_display_status = 2;
				}

				if(Machine_states.tank_fill_finish) //Если бак заполнился водой, переходим на режим полоскания
				{
					printf_P(PSTR("RINSE_FILL_TANK 2\r\n"));
					Machine_states.tank_fill_finish = 0;
					Machine_states.running_rinse_step = RINSE_ROTATE;
					Machine_timers.wring_timer = RINSE_TIME;
					Machine_states.rinse_display_status = 3;
				}
			break;

			case RINSE_ROTATE: // Ожидаем завершения полоскания
				if(Machine_timers.wring_timer == 1)
				{
					printf_P(PSTR("RINSE_ROTATE 1\r\n"));
					Machine_timers.wring_timer = 0;
					pour_out_water();
					Machine_states.running_rinse_step = RINSE_POUR_WATER;
					Machine_states.rinse_display_status = 4;
				}
			break;

			case RINSE_POUR_WATER:
				if(Machine_states.pour_out_finish) // Если слив воды завершон, то останавливаем вращение
				{
					printf_P(PSTR("RINSE_POUR_WATER 1\r\n"));
					Machine_states.pour_out_finish = 0;
					stop_mode_rotating();
				}

				if(Machine_states.drum_is_stopped) // Ожидаем когда барабан остановится. Если остановился
				{
					printf_P(PSTR("RINSE_POUR_WATER 2\r\n"));
					Machine_states.drum_is_stopped = 0;
					run_wring(RINSE_WRING_SPEED, RINSE_WRING_TIME, 0);
					Machine_states.running_rinse_step = RINSE_WRING;
					Machine_states.rinse_display_status = 5;
				}
			break;

			case RINSE_WRING:
				if(Machine_states.wring_finish) // Если отжим завершон
				{
					printf_P(PSTR("RINSE_WRING 1\r\n"));
					if(Machine_states.wring_finish == WRINGED_ERROR) // Если отжим небыл осуществлен успешно
					{
						Machine_states.wring_finish = 0;
						Machine_states.rinse_count_wring_error++;
						if(Machine_states.rinse_count_wring_error > 2) // Если отжим небыл осуществлен успешно два раза
						{
							Machine_states.wring_finish = 0;
							Machine_states.running_rinse = 0;
							Machine_states.rinse_finish = 1;
							printf_P(PSTR("WRING FINISH ERROR\r\n"));
						}
						else
						{
							Machine_states.wring_finish = 0;
							printf_P(PSTR("method: run wring\r\n"));
							run_wring(RINSE_WRING_SPEED, RINSE_WRING_TIME, 0); // Если есть еще попытки на осуществлдение отжима то запускаем отжим
						}
					}
					else // Если отжим осуществлен успешно
					{
						if(Machine_states.wring_finish == NORMAL_SPEED_SET) // Игнорируем данный результат работы, так как нам он сейчас неинтересен.
							Machine_states.wring_finish = 0;

						printf_P(PSTR("WRING FINISH OK\r\n"));
						Machine_states.wring_finish = 0;
						Machine_states.running_rinse = 0;
						Machine_states.rinse_finish = 1;
					}
				}
			break;
		}
	}
}

void start_fill_tank_and_stir_powder(uint8_t water_level, uint8_t water_source, uint16_t water_temperature) //Запустить заполнение бака водой и размешивание порошка
{
	// Принцип размешивая порошка заключается в следующих этапах:
	// 1. балансируем и расркучиваем белье до скорости при которой оно непадает вниз.
	// 2. включаем подачу воды и смываем порошок в бак при вращаюшемся барабане
	// 3. заливаем воду до первого уровня после чего просто крутим барабан какоето время чтобы порошок размешался
	// 4. останваливаем вращение, и доливаем воду до требуемого уровня
	Machine_states.running_stir_powder = 1;
	Machine_states.stir_powder_water_level = water_level;
	Machine_states.stir_powder_water_source = water_source;
	Machine_states.stir_powder_water_temperature = water_temperature;
	Machine_states.running_stir_powder_step = BALANCING_AND_ROTATING;
	Machine_states.stir_powder_display_status = 0;
	printf_P(PSTR("method: start_fill_tank_and_stir_powder\r\n"));
}

void stop_fill_tank_and_stir_powder() //Остановить заполнение бака водой и размешивание порошка
{
	printf_P(PSTR("method: stop_fill_tank_and_stir_powder\r\n"));
	Machine_states.running_stir_powder = 0;
	Machine_states.tank_fill_finish = 0;
	Machine_states.stir_powder_finish = 0;
	Machine_states.running_stir_powder_step = BALANCING_AND_ROTATING;
	stop_pour_out_water();
	stop_wring();
}

void process_stir_powder() // Функция встраивается в основной цикл и следит за процессом размешивания порошка
{
	static uint8_t pause = 0;

	if(Machine_states.running_stir_powder)
	{
		if(Machine_states.pause && !pause) //Если сработала пауза
		{
			pause = 1;
			switch(Machine_states.running_stir_powder_step)
			{
				case WAIT_FILL_ON_LEVEL1:
				case WAIT_END_ROTATING:
					disable_rotate_drum();
				break;
			}
		}

		if(!Machine_states.pause && pause) //Если отменили паузу
		{
			pause = 0; // запускаем отжим по новой
			switch(Machine_states.running_stir_powder_step)
			{
				case WAIT_END_ROTATING:
					Machine_timers.stir_powder_timer = STIR_POWDER_TIME;
				case WAIT_FILL_ON_LEVEL1:
					run_wring(STICK_DRESS_SPEED, 0, 1);
				break;
			}
		}

		switch(Machine_states.running_stir_powder_step)
		{
			case BALANCING_AND_ROTATING: //Балансируем и раскручиваем белье до нужной скорости
				if(!Machine_states.running_wring && !Machine_states.wring_finish) // Если отжим еще не запущен то запускаем его
				{
					printf_P(PSTR("BALANCING_AND_ROTATING 1\r\n"));
					pour_out_water(); // Запускаем слив воды
					run_wring(STICK_DRESS_SPEED, 0, 1); //Запускаем отжим ув упрощенном режиме на минимальной скорости, чисто для балансировки и раскручивания белья до скорости прилипания
				}

				if(Machine_states.wring_finish == NORMAL_SPEED_SET) //Если белье раскрутилось до нужеой скорости
				{
					Machine_states.stir_powder_display_status = 1;
					printf_P(PSTR("BALANCING_AND_ROTATING 2\r\n"));
					Machine_states.wring_finish = 0;
					stop_pour_out_water();
					start_fill_tank(1, Machine_states.stir_powder_water_source, Machine_states.stir_powder_water_temperature); // Наполняем бак водой до первого уровня
					Machine_states.running_stir_powder_step = WAIT_FILL_ON_LEVEL1; // Переходим в режим ожидания наполения водой до первого уровня
				}

				if(Machine_states.wring_finish == WRINGED_ERROR) //Если белье несмогло раскрутиться до нужного уровня
				{
					Machine_states.stir_powder_display_status = 2;
					printf_P(PSTR("BALANCING_AND_ROTATING 3\r\n"));
					stop_pour_out_water();
					start_fill_tank(Machine_states.stir_powder_water_level, Machine_states.stir_powder_water_source, Machine_states.stir_powder_water_temperature); // Наполняем бак водой до первого уровня
					Machine_states.running_stir_powder_step = WAIT_FILL_TANK; // Переходим в режим ожидания наполнения бака водой до требуемого уровня
				}
			break;

			case WAIT_FILL_ON_LEVEL1: //Дожидаемся заполения водой до первого уровня
				if(Machine_states.complete_fill_on_level1) //Если бак заполнился до первого уровня то устанавливаем время размешивания
				{
					Machine_states.need_temperature = Machine_states.demand_temperature = 0; //Принудительно выключаем нагрев на время размешивания порошка
					Machine_states.stir_powder_display_status = 3;
					printf_P(PSTR("BALANCING_AND_ROTATING 4\r\n"));
					Machine_timers.stir_powder_timer = STIR_POWDER_TIME; // Устанавливапем время размешивания порошка
					Machine_states.running_stir_powder_step = WAIT_END_ROTATING; // Переходим в режим ожидания завершения размешивания порошка
				}
			break;

			case WAIT_END_ROTATING:  // дожидаемся завершения размешивания порошка
				if(Machine_timers.stir_powder_timer == 1) //Если размешивание завершенно
				{
					Machine_states.stir_powder_display_status = 4;
					printf_P(PSTR("BALANCING_AND_ROTATING 5\r\n"));
					stop_wring();
					Machine_timers.stir_powder_timer = 0;
					start_fill_tank(Machine_states.stir_powder_water_level, Machine_states.stir_powder_water_source, Machine_states.stir_powder_water_temperature); // Дополняем бак водой до требуемого уровня
					Machine_states.running_stir_powder_step = WAIT_FILL_TANK; // Переходим в режим ожидания наполнения бака водой до требуемого уровня
				}
			break;

			case WAIT_FILL_TANK: // Ожидаем наполнение бака водой
				if(Machine_states.tank_fill_finish)
				{
					printf_P(PSTR("BALANCING_AND_ROTATING 6\r\n"));
					Machine_states.running_stir_powder = 0;
					Machine_states.tank_fill_finish = 0;
					Machine_states.stir_powder_finish = 1;
					stop_wring();
				}
			break;
		}
	}
}

int16_t calculate_PD(uint16_t need_speed, uint16_t speed) // Расчет ПД стабилизатора
{
	static int32_t dstate = 0;
	int32_t p_term, d_term, result;

	if(need_speed >= MAX_ROTATE_SPEED_ADC)
		need_speed = MAX_ROTATE_SPEED_ADC;

	if(speed >= MAX_ROTATE_SPEED_ADC)
		speed = MAX_ROTATE_SPEED_ADC;

	if(need_speed == 0)
		dstate = 0;

	p_term = ((int32_t)need_speed - (int32_t)speed) * (int32_t)MOTOR_STABILIZATOR_P_GAIN;
	d_term = ((int32_t)speed - dstate) * (int32_t)MOTOR_STABILIZATOR_D_GAIN;
	dstate = (int32_t)speed;

	result = (p_term - d_term) / 10;
	return (int16_t)result;
}

uint16_t integrator_queue(int16_t *queue, uint16_t insert_val, uint16_t queue_len) // Добавить в буфер queue значение insert_val и получить усредненное значение с буфера
{
	uint32_t round = 0;
	for(uint8_t i = 0; i < queue_len - 1; i++)
	{
		queue[i] = queue[i + 1];
		round += queue[i];
	}
	queue[queue_len - 1] = insert_val;
	round += insert_val;

	return round / queue_len;
}

void led_indicate_state(uint8_t state) //Отобразить текущее состояние стиральной машины светодиодами
{
	static uint8_t blink = 0;

	switch(state) // Состояние светодиодов
	{
		case LED_OFF: // Машина бездействует
			turn_led(LED1, OFF);
			turn_led(LED2, OFF);
			turn_led(LED3, OFF);
		break;

		case LED_PAUSE:  // во время паузы
			if(blink)
			{
				turn_led(LED1, ON);
				turn_led(LED2, ON);
				turn_led(LED3, ON);
			}
			else
			{
				turn_led(LED1, OFF);
				turn_led(LED2, OFF);
				turn_led(LED3, OFF);
			}
		break;

		case LED_BEDABBLE: // во время замачивания
			turn_led(LED1, ON);
			turn_led(LED2, OFF);
			turn_led(LED3, OFF);
		break;

		case LED_PREV_WASHING: // предварительной стирки
			if(blink)
				turn_led(LED1, ON);
			else
				turn_led(LED1, OFF);

			turn_led(LED2, OFF);
			turn_led(LED3, OFF);
		break;

		case LED_WASHING: // Основная стирка
			turn_led(LED1, OFF);
			turn_led(LED2, ON);
			turn_led(LED3, OFF);
		break;

		case LED_RINSE:  // Полоскания
			turn_led(LED1, OFF);
			if(blink)
				turn_led(LED2, ON);
			else
				turn_led(LED2, OFF);

			turn_led(LED3, OFF);
		break;

		case LED_WRING: // Отжим
			turn_led(LED1, OFF);
			turn_led(LED2, OFF);
			turn_led(LED3, ON);
		break;

		case LED_FINISH: // Стирка и/или замачивание завершено
			turn_led(LED1, OFF);
			turn_led(LED2, OFF);
			if(blink)
				turn_led(LED3, ON);
			else
				turn_led(LED3, OFF);

		break;

/*		case LED_ERROR1: // Возникла ошибка на первом или вотором шаге
			if(blink)
				turn_led(LED1, LED_COLOR2);
			else
				turn_led(LED1, OFF);

			turn_led(LED2, OFF);
			turn_led(LED3, OFF);
		break;

		case LED_ERROR2: // Возникла ошибка на третьем или четвертом
			turn_led(LED1, OFF);
			if(blink)
				turn_led(LED2, LED_COLOR2);
			else
				turn_led(LED2, OFF);

			turn_led(LED3, OFF);
		break;

		case LED_ERROR3: // Возникла ошибка на пятом или шестом шаге
			turn_led(LED1, OFF);
			turn_led(LED2, OFF);
			if(blink)
				turn_led(LED3, LED_COLOR2);
			else
				turn_led(LED3, OFF);
		break;

		case LED_FATAL_ERROR: // Возникла глобальная ошибка
			if(blink)
			{
				turn_led(LED1, LED_COLOR2);
				turn_led(LED2, LED_COLOR2);
				turn_led(LED3, LED_COLOR2);
			}
			else
			{
				turn_led(LED1, OFF);
				turn_led(LED2, OFF);
				turn_led(LED3, OFF);
			}
		break; */
	}

	blink = !blink;
}


void inc_time(struct time *t) // увеличить время 't'  на еденицу
{
	if(t -> sec < 60)
		t -> sec++;

	if(t -> sec >= 60)
	{
		t -> sec = 0;

		if(t -> min < 60)
			t -> min++;

		if(t -> min >= 60)
		{
			t -> min = 0;

			if(t -> hour < 24)
				t -> hour++;

			if(t -> hour >= 24)
				t -> hour = 0;
		}
	}
}

void time_add(struct time *t, uint16_t min) // Прибавить к времени 't', указанное количество минут
{
	uint8_t hour;
	uint8_t remainder_min;

	hour = min / 60;

	remainder_min = min - hour * 60;

	if((60 - t -> min) > remainder_min)
		t -> min += remainder_min;
	else
	{
		t -> min = remainder_min - (60 - t -> min);
		hour++;
	}

	if((24 - t -> hour) >  hour)
		t -> hour += hour;
	else
		t -> hour = hour - (24 - t -> hour);
}

void sys_error(uint8_t error_type, const char *error_message) //Выдать ошибку
{
	if(!error_type)
		return;

	Error_message = error_message;
	Machine_states.error = error_type;
}

void power_on_system(void) //Включение всей силовой системы
{
	turn_main_power(ON); //Включаем общее питание машины
	turn_display_light(ON); //Включаем подсветку дисплея
	Machine_states.main_power = 1;
}

void power_off_system(void) //Выключение всей силовой системы
{
	turn_main_power(OFF); //Выключаем общее питание машины
	turn_display_light(OFF); //Выключаем подсветку дисплея
	stop_fill_tank_and_stir_powder();
	stop_rinse();
	stop_wring();
	stop_fill_tank();
	stop_pour_out_water();
	stop_mode_rotating();
	clr_lcd_buf();
	Machine_states.main_power = 0;
}

void turn_speaker(uint8_t freq) //Включить звуковой сигнал
{
	if(freq == 0)
		speaker(OFF);

	Machine_states.speaker_freq = freq;
	Speaker_frequency_timer = Machine_states.speaker_freq;// Устанавливаем время периуда ( Это время определяет частоту звукового сигнала)
}

void turn_blink_speaker(uint8_t power, uint16_t time_up, uint16_t time_down, uint8_t freq, uint8_t count_beep) // Включить периодическое пищание пи-пи-пи...
{
	if(!power)
	{
		Machine_states.blink_beep_up = 0;
		Machine_states.blink_beep_down = 0;
		Machine_states.blink_beep_on = OFF;
		Machine_states.blink_speaker_freq = 0;
		Machine_states.blink_sound_count_beep = 0;
		turn_speaker(OFF);
		return;
	}

	Machine_states.blink_beep_up = time_up;
	Machine_states.blink_beep_down = time_down;
	Machine_states.blink_beep_on = ON;
	Machine_states.blink_speaker_freq = freq;
	Machine_timers.blink_sound_speaker = 1;
	Machine_states.blink_sound_count_beep = count_beep; //Количество пищаний
}

void do_blink_speaker(void) //Функция встраивается в основной цикл, и осуществляет периодическое пищание
{
	static uint8_t state = 1;
	static uint8_t count = 0;
	if(!Machine_states.blink_beep_on)
		return;

	if(Machine_timers.blink_sound_speaker == 1)
	{
		if(state)
		{
			if((count >= Machine_states.blink_sound_count_beep) && Machine_states.blink_sound_count_beep) //Если количество пищаний превысило установленное количество то прикращаем пищать
			{
				count = 0;
				turn_blink_speaker(OFF, 0, 0, 0, 0);
				return;
			}

			Machine_timers.blink_sound_speaker = Machine_states.blink_beep_up;
			turn_speaker(Machine_states.blink_speaker_freq);
			count ++;
		}
		else
		{
			Machine_timers.blink_sound_speaker = Machine_states.blink_beep_down;
			turn_speaker(OFF);
		}
		state = !state;
	}
}

void save_all_settings(void) //Сохранить все настройки
{
	eeprom_write_block(&Machine_settings, (void *)EEPROM_ADDRESS_MACHINE_SETTINGS, sizeof(struct machine_settings));
}

void load_all_settings(void) //Восстановить настройки
{
	eeprom_read_block(&Machine_settings, (void *)EEPROM_ADDRESS_MACHINE_SETTINGS, sizeof(struct machine_settings));
}

