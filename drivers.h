#ifndef DRIVERS_H
#define DRIVERS_H

#include <stdint.h>
#include <stdio.h>
#include <avr/pgmspace.h>


#define ON	 1 // Включить
#define OFF  0 // Выключить

// Конфигурация порта 0 связь с ПК
#define COM_PORT1											0
#define BAUD0 9600
#define UBRR0 F_CPU/16/BAUD0-1

// Конфигурация порта 1 (RS-485)
#define COM_PORT2											1
#define BAUD1 38400
#define UBRR1 F_CPU/16/BAUD1-1

// Установка портов для юзера и для RS-485
#define COMPORT_USER							COM_PORT1
#define COMPORT_RS485							COM_PORT2

void reboot(void);

void out0(char var); // Вывод в RS232 порт 0

char in0(void); // Чтение из RS232 порт 0

void USART_Init0(unsigned int ubrr);  //Инициалтизация RS232 порта 0

void out1(char var); // Вывод в RS232 порт 1

char in1(void); // Чтение из RS232 порт 1

void USART_Init1(unsigned int ubrr);  //Инициалтизация RS232 порта 1

void set_dev_in_out(int com); // Установить COM порт для вывода информации

void console_put_ch(char ch); //Вывод одного символа в UART

void console_out_str(char *str); //Вывод строки в UART

void console_out_str_P(const char *str); //Вывод строки из программной памяти в UART

uint8_t check_input_rs485(void); // Проверяет есть ли в UARTе данные

void rs485out(char byte); //Запись байта в RS485

char rs485in(void); //Чтение байта из RS485

//Инициализация таймера0 RTC с дополнительным кварцем периуд 1на секунда
#define init_timer0() 	\
{	\
	ASSR |= _BV(AS0);	\
	TCCR0=0b101; \
	TIFR=0;	\
	TIMSK |= _BV(TOIE0);	\
}

#define init_timer1() 	\
{	\
	TCCR1A= _BV(COM1A1) | _BV(WGM11) | _BV(WGM10); \
	TCCR1B= 0b001;	\
}


#define TIMER_DIVISOR 1
#define SEC_LIMITER ((F_CPU / 256 / TIMER_DIVISOR) / 1000) //Высчитываем количкество тиков таймера для получения задержки в 0.001сек
#define init_timer2() 	\
{	\
	TCCR2 = 0b01101001; \
	TIFR=0;	\
	TIMSK |= _BV(TOIE2);	\
}


// Инициализация кнопок
#define	BUTT_EXT1		0
#define	BUTT_EXT2		1
#define	BUTT_EXT3		2
#define	BUTT_EXT4		3
#define	BUTT_ESC			4
#define	BUTT_ENTER		5
#define	BUTT_LEFT			6
#define	BUTT_RIGHT		7

#define	PRESOSTAT_LEVEL1		8
#define	PRESOSTAT_LEVEL2		9
#define	PRESOSTAT_LEVEL3		10



void init_input_controls(void);

#define init_rs485()	\
{	\
	DDRB |= _BV(PB6);	\
	PORTB &= ~_BV(PB6);	\
}

void write_enable_to_rs485(void); // Включить запись в сеть

void write_disable_to_rs485(void); // Выключить запись в сеть


// разрешить внешнее прерывание Int0
#define init_int0()	\
{	\
	DDRD &= ~_BV(PD0); \
	PORTD |= _BV(PD0); \
	EICRA |= _BV(ISC01); \
	EICRA |= _BV(ISC11); \
	EIMSK |= _BV(INT0); \
}

// разрешить внешнее прерывание Int1
#define init_int1()	\
{	\
	DDRD &= ~_BV(PD1); \
	PORTD |= _BV(PD1); \
	EICRA |= _BV(ISC01); \
	EICRA |= _BV(ISC11); \
	EIMSK |= _BV(INT1); \
}

// Инициализация всех реле
#define init_relays()	\
{	\
	DDRF |= _BV(PF7);	\
	DDRF |= _BV(PF6);	\
	DDRF |= _BV(PF5);	\
	DDRF |= _BV(PF4);	\
	DDRF |= _BV(PF3);	\
	DDRF |= _BV(PF2);	\
	DDRA |= _BV(PA2);	\
	DDRA |= _BV(PA0);	\
	DDRE |= _BV(PE6);	\
	DDRC |= _BV(PC3);	\
	DDRB |= _BV(PB5);	\
	DDRC &= ~_BV(PC0);	\
	\
	PORTF &= ~_BV(PF7);	\
	PORTF &= ~_BV(PF6);	\
	PORTF &= ~_BV(PF5);	\
	PORTF &= ~_BV(PF4);	\
	PORTF &= ~_BV(PF3);	\
	PORTF &= ~_BV(PF2);	\
	PORTA &= ~_BV(PA2);	\
	PORTA &= ~_BV(PA0);	\
	PORTE &= ~_BV(PE6);	\
	PORTC &= ~_BV(PC3);	\
	PORTB &= ~_BV(PB5); \
	PORTC |= _BV(PC0);	\
}

// Управление диффузором динамика
#define speaker(i)	\
{	\
	if(i)	\
		PORTB |= _BV(PB5); \
	else	\
		PORTB &= ~_BV(PB5); \
}

// Проверить мгновенное состояние дверки
#define check_door()	(!(PINC & (1 << PINC0)))


void block_door(uint8_t mode); // Заблокировать или разблокировать дверку

void turn_heater(uint8_t mode); // Включить или выключить ТЕН

void turn_pump(uint8_t mode); // Включить или выключить насос слива воды

void turn_motor_power(uint8_t mode); //Подать/снять питание на мотор

// Включить или выключить впуск воды на заданном клапане (num)
#define VALVE1 0 //Клапан 1. Используется как основной забор порошка
#define VALVE2 1 //Клапан 2. Используется для забора ополаскивателя
void turn_valve(uint8_t num, uint8_t mode);

// Включить забор воды из указанной ячейки
#define WATER_SOURCE1 0
#define WATER_SOURCE2 1
#define WATER_SOURCE3 2
void turn_water_source(uint8_t num, uint8_t mode);

//Включить или выключить питание мотора, reverse - направление вращения, speed_mode - номер скоростного режима
#define ROTATION_BACKWARD 0 //Направление вращения барабана против часовой стрелке
#define ROTATION_FORWARD 1 //Направление вращения барабана по часовой стрелке
#define LOW_SPEED_ROTATION 0 	// Две скорости вращения барабана
#define HIGH_SPEED_ROTATION 1
void turn_motor(uint8_t mode, uint8_t reverse, uint8_t speed_mode);

//Управление светодиодами
#define LED1 0
#define LED2 1
#define LED3 2
#define LED4 3
#define COUNT_LEDS 4 //Общее количество светодиодов

//Инициализация всех светодиодов
#define init_leds()	\
{	\
	DDRE |= _BV(PE5);	\
	DDRE |= _BV(PE7);	\
	DDRB |= _BV(PB0);	\
	DDRB |= _BV(PB2);	\
	\
	PORTE &= ~_BV(PE5);	\
	PORTE &= ~_BV(PE7);	\
	PORTB &= ~_BV(PB0);	\
	PORTB &= ~_BV(PB2);	\
}

void turn_led(int num, int mode); // Включить или выключить светодиод num

void turn_main_power(uint8_t state); //Включить или выключить общее питание стиральной машины

void turn_display_light(uint8_t state); //Включить или выключить подсветку дисплея

void lcd_rus(char c, FILE *unused); // Вывод символа в LCD

// Инициализация дисплея
#define DISPLAY_WIDTH 20 // 20 символов
#define DISPLAY_HEIGHT 4 // 4ре строки
#define init_display()	\
{	\
	DDRC |= _BV(PC2);	\
	lcd_init(LCD_DISP_ON);	\
}

#define init_speaker()	\
{	\
	DDRB |= _BV(PB5);	\
	\
}


// Инициализация ADC
#define init_adc()	\
{	\
	DDRF &= ~_BV(PF0);	\
	DDRF &= ~_BV(PF1);	\
	ADCSRA = _BV(ADEN) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);	\
	ADMUX = 0b10000000;	\
	SREG |= _BV(7);	\
	ADCSRA |= _BV(ADSC);	\
	PORTF &= ~_BV(PF0);	\
	PORTF &= ~_BV(PF1);	\
}

#define TEMPERATURE_SENSOR_CHANNEL_NUM 0 //Номер канала на который подключен термо-датчик
#define VIBRATION_SENSOR_CHANNEL_NUM 1 //Номер канала на который подключен датчик вибрации

// сменить канал мультиплексора ADC
#define change_channel_adc(channel)	\
{	\
	ADMUX	= ((ADMUX & (((1 << 8) - 1) ^ ((1 << 3) - 1))) | channel);	\
}

// запись значения ADC в adc_val
#define save_adc_data(adc_val)	\
{	\
	char *adc; 	\
	adc = (char *)&adc_val;	\
	*adc = ADCL;		\
	*(adc + 1) = ADCH;		\
	adc_val;	\
}

// Запуск одного преобразования
#define start_adc()		\
{	\
	ADCSRA |= _BV(ADSC);	\
}

void init_external_clock(void); //Инициализация внешних часов
#define I2C_ADDR_CLOCK 0b1101000 // Адрес микросхемы часов в I2C сети
struct time
{
	uint8_t sec;            //< Seconds:       0  to 59
	uint8_t min;            //< Minutes:       0  to 59
	uint8_t hour;           //< Hours:         0  to 23
	uint8_t day_of_week;    //< Day of Week:   1  to 7
	uint8_t day_of_month;   //< Days of Month: 1  to 31 (depending on month)
	uint8_t month;          //< Months:        1  to 12
	uint8_t year;           //< Years:         00 to 99
};
uint8_t  read_current_datetime(struct time *time); //Получить текущее время и дату
uint8_t write_datetime(struct time *time); // Установить текущую дату и время

#endif //DRIVERS_H
