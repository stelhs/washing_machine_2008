#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include "config.h" //Файл конфигурации параметров
#include "drivers.h" // Драйвера
#include "lcd.h"    //Функции для работы с LCD
#include "lcd_buffer.h"  //Функции для работы буфером LCD
#include "button.h" // Обработчик кнопок
#include "encoders.h" // Обработчик енкодеров
#include "lib_menu.h" //Описание библиотеки для работы с меню
#include "os.h" // Настройка ОС под текущий проект
#include "menu.h" //Описание дерева меню и всех функций для его отрисовки (проектно - зависимая часть)
#include "lib.h" // Подключаем все структуры
#include "messages.h" //Файл со всеми текстовыми строками сообщений

#include <avr/wdt.h>

extern struct lib_liveos Lib_liveos;
extern uint8_t Need_redraw; //Данный флаг выставляет драйвер, в случае комутирования реле, для полной перерисовки экрана.

struct lib_menu Lib_menu;

struct machine_timers Machine_timers; // Счетчики всех основных таймеров
struct machine_states Machine_states;
struct washing_settings Washing_settings; // Настройки регулируемые с меню
struct machine_settings Machine_settings;

const char *Error_message; // В случе возникновения ошибки, сюда присваивается указатель на текст ошибки

struct time Main_current_time; //Часы реального времени

volatile uint16_t  Rotating_speed = 0; // Текущая скорость вращения барабана
volatile int16_t Motor_power = 0; // Требуемая выходная мощность на мотор
volatile int16_t Current_temperature = 0; //Текущая температура воды
volatile int16_t Current_vibration = 0; //Текущий уровень вибрации
int16_t Temperature_buffer[LEN_QUEUE_TEMPERATURE]; // Создаем буфер для стабилизации температуры
int16_t Vibration_buffer[LEN_QUEUE_VIBRATION]; // Создаем буфер для стабилизации сигнала с датчика вибрации

volatile uint16_t Delay_start_semistor = 0; // Переменная нужна для управления ШИМом синхронизированным на 100HZ из сети 220V. Она осуществляет отсчет времени с момента появления стробирующего импульса от сети 220V до момента включения семистора. Чем выше значение этой переменной тем посже включается семистор (ближе к концу полупериода) и тем ниже напряжение питания на мотор
volatile uint16_t Tahogenerator_pulse_counter = 0; // Счетчик импульсов от тахогенератора
volatile uint16_t Tahogenerator_timer_counter = 0; // Обратный отсчет времени с момента появления импульса с тахогенератора до 1цы. В течении этого отсчета все последующие импульсы от тахогенератора будут игнорированны, это нужно для фильтрации высокочастотных помех наводящихся на провода тахогенератора.
volatile uint16_t  Main_counter = 0; // Счетчик интервала в 0.001сек
volatile int16_t Alt_motor_power = 0; // Расчетная мощность мотора, переменная нужна только для расчета значения Motor_power
volatile uint16_t Adc_value = 0; // Временный буфер для оцифрованного ADC значения
volatile uint16_t Start_ADC_timer_counter = 0; //Счетчик времени ожидания для запуска ADC преобразования
volatile uint16_t Led_blink_counter = 0; // Счетчик для отсчета периода мигания светодиодов
volatile uint16_t Speaker_frequency_timer = 0; // Переменная нужна чтобы отмерять периуд колебаний диффузора динамика
volatile uint16_t Timer_refresh_datetime = 0; // Переменная счетчик временного интервала для обновления информации с часов

ISR(TIMER2_OVF_vect) // Прерывание таймера
{
	if(Machine_timers.delay_next_impulse > 1) // Счетчик времени между импульсами с тахо генератора, нужен для ограничения максимального количества импульсов в еденицу времени
		Machine_timers.delay_next_impulse--;

	if(!Machine_states.enable_full_motor_power) // Если данный бит опущен то можно контролировать мощность с помощь PWM иначе мотор включается на 100%
	{
		if(Delay_start_semistor > 1) //Инкриминтируем счетчик задержки включения питания на семистор
			Delay_start_semistor--;
		if(Delay_start_semistor == 10) // Если он стал равен значению 10 то пора включить семистор
			turn_motor_power(ON);
		if(Delay_start_semistor == 1) // А при значении 1 пора выключить семистор
		{
			turn_motor_power(OFF);
			Delay_start_semistor = 0;
		}
	}

	if(Speaker_frequency_timer > 1) //Таймер для отсчета периуда колебаний диффузора пищалки, как только таймер истек, изменяем напряжение на диффузор и зпускаем таймер по новой
		Speaker_frequency_timer--;

	if(Speaker_frequency_timer == 1)
	{
		Speaker_frequency_timer = Machine_states.speaker_freq;// Устанавливаем время периода ( Это время определяет частоту звукового сигнала)
		static uint8_t i = 0;
		speaker(i);
		i = !i;
	}

	if(Main_counter > SEC_LIMITER) // Если прошло приблизительно 0.001 секунды
	{
		sys_call_timer2();
		scan_encoders();

		if(Machine_timers.speed_change_timer > 1)
			Machine_timers.speed_change_timer --;

		if(Machine_timers.blink_sound_speaker > 1)
			Machine_timers.blink_sound_speaker --;

		if(Machine_states.state_door > 0) //Данный счетчик является индикатором состояния дверки. Поскольку на ногу двегки поступает переменное напряжение то необходимо фильтровать пульсацию. Если данная переменная выше 0ля значит дверка закрыта
			Machine_states.state_door --;

		if(Machine_states.ext_power > 0) //Данный счетчик является индикатором наличия 220V. Поскольку на ногу INT0 поступает переменное напряжение то необходимо фильтровать пульсацию. Если данная переменная выше 0ля значит питание присутсвует
			Machine_states.ext_power --;

		change_timer_buttons(); //  Обновляем значения таймеров для работы логики опроса состояния кнопок

		if(Tahogenerator_timer_counter >= PERIODIC_GET_ROTATING_SPEED_TIME) // Если подошло время для снятия скорости вращения барабана
		{
			Rotating_speed = Tahogenerator_pulse_counter; // Rotating_speed - Скорость вращения барабана
			Tahogenerator_pulse_counter = 0;
			Tahogenerator_timer_counter = 0;
			Machine_states.refresh_rotate_speed = 1; // Выставляем флаг что полученна текущая скорость вращения барабана
		}
		else
			Tahogenerator_timer_counter ++;

		if(Start_ADC_timer_counter >= PERIODIC_GET_ADC_VALUE) // Если подошло время для оцифровки ADC
		{
			if(!Machine_states.adc_success) // Если предыдущий результат обработки ADC сохранен
				start_adc(); //Запуск ADC преобразования
			Start_ADC_timer_counter = 0;
		}
		else
			Start_ADC_timer_counter ++;

		if(Led_blink_counter >= PERIODIC_LED_BLINK_TIME) // Мигалка
		{
			led_indicate_state(Machine_states.current_machine_state);
			Led_blink_counter = 0;
		}
		else
			Led_blink_counter++;

		Main_counter = 0;
	}

	Main_counter ++;
}

ISR(TIMER0_OVF_vect) // Прерывание RTC таймера с частотой 1на секунда
{
//	inc_time(&Main_current_time); // Расчитываем время
//	Machine_states.change_current_time = 1;

	if(Timer_refresh_datetime > 1) //отсчитываем временной интервал опроса часов
		Timer_refresh_datetime--;

	if(!Machine_states.pause)
	{
		if(Machine_timers.rotating_drum_timer > 1)
			Machine_timers.rotating_drum_timer--;

		if(Machine_timers.pour_out_water_timer > 1)
			Machine_timers.pour_out_water_timer--;

		if(Machine_timers.wring_timer > 1)
			Machine_timers.wring_timer--;

		if(Machine_timers.stir_powder_timer > 1)
			Machine_timers.stir_powder_timer--;

		if(Machine_timers.level1_time > 1)
			Machine_timers.level1_time--;

		if(Machine_timers.fill_tunk_timer > 1)
			Machine_timers.fill_tunk_timer--;
	}

	sys_call_timer0(); // генератор события срабатывания таймера для всех процессов
}

ISR(USART0_RX_vect)
{
	sys_input_console();
}



ISR(INT0_vect) // Сигнал 100Hz для синхронизации управления мотором
{
	int16_t pwm;

	Machine_states.ext_power = 20; // 20 - Количество тиков по 0.001 секунде до следующего прерывания. Если следующего прерывания не случится по причине отсутствия 220V то данная переменная быстро опуститься до 0ля

	if(Motor_power) //Если установленна мощность на мотор
	{
		pwm = MAX_PWM_VALUE - (uint16_t)Motor_power; // Расчитываем задержку для достижения требуемой мощности
/*		if(pwm < MIN_PWM_VALUE)
		{
			pwm = MIN_PWM_VALUE - 1;
			printf("enable full power");
			Machine_states.enable_full_motor_power = 1; //Включаем полную мощность
			turn_motor_power(ON);
		}
		else
			if(Machine_states.enable_full_motor_power)
			{
				turn_motor_power(OFF);
				Machine_states.enable_full_motor_power = 0;
			}*/

		if(pwm > MAX_PWM_VALUE)
			pwm = MAX_PWM_VALUE;

		turn_motor_power(OFF);
		Delay_start_semistor = pwm;
	}
	else //Если установленна нулевая мощность на мотор
	{
		turn_motor_power(OFF);
		Machine_states.enable_full_motor_power = 0;
		Delay_start_semistor = 0;
	}
}

ISR(INT1_vect) //Сигнал с тахогенератора
{
	if(Machine_timers.delay_next_impulse == 1) //Механизм ограничения максималдьной частоты следования следования импульсов (защита от помех)
	{ // Каждый раз при получении импульса с тахо генератора, в течении времени равном Machine_timers.delay_next_impulse, все последующие импульсы будут игнорированны
		Tahogenerator_pulse_counter++;
		Machine_timers.delay_next_impulse = DELAY_GET_NEXT_IMPULSE;
	}
	if(Tahogenerator_pulse_counter > MAX_ROTATE_SPEED_ADC)
		Tahogenerator_pulse_counter = MAX_ROTATE_SPEED_ADC;
}

ISR(ADC_vect)
{
	save_adc_data(Adc_value); // Получаем значение ADC
	Machine_states.adc_success = 1;
}

void init(void)
{
	cli();
	init_external_clock(); // Инициализация внешних часов
	init_relays();
	init_int0(); //Инициализация внешнего прерывания для снятия сигнала 100Hz
	init_int1(); // Инициализация внешнего прерывания для сигнала с тахогенератора
	init_leds();

	init_input_controls();	// Инициализируем все устройства ввода
	init_speaker(); // Инициализация динамика
	init_timer0(); // Инициализация RTC таймера
	speaker(OFF); //Выключаем питание на динамик
	init_timer2(); // Инициализация таймера 2

	USART_Init0(UBRR0); // Инициализация COM1
	set_dev_in_out(COMPORT_USER);

printf("STEP 1\r\n");

    change_channel_adc(TEMPERATURE_SENSOR_CHANNEL_NUM);
printf("STEP 2\r\n");
    init_adc();
printf("STEP 3\r\n");

	init_display();
printf("STEP 4\r\n");

	clr_lcd_buf();

printf("STEP 5\r\n");

	Main_current_time.hour = 0;
	Main_current_time.min = 0;
	Main_current_time.sec = 0;

	Machine_states.current_machine_state = 0; //
	Machine_states.main_power = 0;
	Machine_states.enable_full_motor_power = 0; // !!!
	Machine_states.current_adc_channel = 0;
	Machine_states.level_water_in_tank = TANK_IS_EMPTY;
	Machine_states.need_water_level = TANK_IS_EMPTY;
	Machine_states.run_fill_tank = 0;
	Machine_states.need_temperature = 0;
	Machine_states.demand_temperature = 0;
	Machine_states.heating_water = 0;
	Machine_states.drum_is_runing = 0;
	Machine_states.drum_is_stopping = 0;
	Machine_states.drum_is_stopped = 0;
	Machine_states.need_speed_rotating = 0;
	Machine_states.rotating_mode = 0;
	Machine_states.current_rotating_step = 0;
	Machine_states.running_wring = 0;
	Machine_states.running_wring_step = 0;
	Machine_states.time_fill_on_level1 = 0;
	Machine_states.stir_powder_display_status = 0;
	Machine_states.speaker_freq = 0;
	Machine_states.blink_beep_on = 0;
	Machine_states.state_door = 0;
	Machine_states.ext_power = 100;  //Любое число больше 1

	// Обнуляем все таймеры
	Machine_timers.delay_next_impulse = 1;
	Machine_timers.pour_out_water_timer = 0;

	power_off_system(); // Поумолчанию приводим все в выключенное состояние

	init_liveos(&Lib_liveos); // Инициализируем операционную систему

	wdt_enable(WDTO_2S); // Включаем вэтчдог
 	sei();	// Разрешаем все прерывания
}

int main(void)
{
	init(); // полная инициализация

//	sync_send_packet(0, EPP_WELCOM, (char *)&Device_info, sizeof(struct device_info));
	for(;;)
	{
		asm("wdr");

		if(Machine_states.adc_success) //Если законченно новое ADС преобразование
		{
			switch(Machine_states.current_adc_channel)	// Смотрим какой канал оцифровываеться в данный момент
			{
				case 0:	// Оцифровываем температуру
					Current_temperature = 1023 - integrator_queue((int16_t *)Temperature_buffer, Adc_value, LEN_QUEUE_TEMPERATURE);
					Machine_states.current_adc_channel = 1; // Выбор канала для последующей оцифровки
					change_channel_adc(VIBRATION_SENSOR_CHANNEL_NUM);
				break;

				case 1:	// Оцифровываем уровень вибраций
					Current_vibration = integrator_queue((int16_t *)Vibration_buffer, Adc_value, LEN_QUEUE_VIBRATION);

					Machine_states.current_adc_channel = 0; // Выбор канала для последующей оцифровки
					change_channel_adc(TEMPERATURE_SENSOR_CHANNEL_NUM);
				break;
			}
			Machine_states.adc_success = 0;
		}

		if(check_door()) //Если поступает импульс от дверки то заряжаем не нулевым числом переменную Machine_states.state_door, если по истечению 1/50 секунды следующий такой же импульс непояится значит дверка уже открыта и Machine_states.state_door упадет до нуля
			Machine_states.state_door = FILTER_50HZ_TIME;

		if(Machine_states.need_speed_rotating && Machine_states.refresh_rotate_speed) // Если нужно вращать с указанной скоростью, и посутпила новое значение скорости, то расчитываем ПД (для мощности на мотор)
		{
			int16_t pd= calculate_PD(Machine_states.need_speed_rotating, Rotating_speed); //вызываем расчет ПД и изменяем текущее состояние мощности current_power

			Alt_motor_power += pd;

			if(Alt_motor_power >= (MAX_PWM_VALUE * 10)) // Проверяем чтобы мощность не вышла за рамки регулировок
				Alt_motor_power = MAX_PWM_VALUE * 10;
			if(Alt_motor_power < 0)
				Alt_motor_power = 0;

			Motor_power = Alt_motor_power / 10; // пересчитываем мощность в формат для PWM
			if(Motor_power > MAX_PWM_VALUE)
				Motor_power = MAX_PWM_VALUE;

			Machine_states.refresh_rotate_speed = 0;
		}

		if(!Machine_states.need_speed_rotating)
			Alt_motor_power = START_ROTATE_SPEED * 10;


		if(Timer_refresh_datetime <= 1) //Если пришло время получить текущую дату и время
		{
			static uint8_t old_sec; // будет хранить предыдущее количество секунд, нужно чтобы сравнивать с текущим  и вслучае разницы выставлять флаг
			read_current_datetime(&Main_current_time);

			if(old_sec != Main_current_time.sec) //Если время изменилось
			{
				Machine_states.change_current_time = 1;
				old_sec = Main_current_time.sec;
			}

			Timer_refresh_datetime = 2; // Заводим таймер на 0.1 секунды для последующего обновления текущего времени
		}

		sys_main_loop();
		check_water_level();

		rotate_drum_if_need();
		process_wringing();
		process_rinse();
		process_stir_powder();
		do_blink_speaker();

		if(Current_temperature == 0 && Machine_states.main_power) //Если неисправен датчик температуры во время включонной машины
			sys_error(HARDWARE_ERROR_1, GET_MESS_POINTER(ERROR_MALFUNCTION_THERMO_SENSOR)); // Неисправен термо-датчик

		if(Need_redraw) //Данный флаг выставляют драйверы устройств при любых камутациях, для полной переинициализации дисплея
		{
			Need_redraw = 0;
			init_display();
		}
		copy_buf_to_lcd(); //Копирование буфера LCD в сам LCD

	}
	return 0;
}
