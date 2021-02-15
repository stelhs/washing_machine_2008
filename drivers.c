#include <avr/interrupt.h>
#include <avr/wdt.h>
#include "drivers.h"
#include "lcd.h"
#include "button.h"
#include "encoders.h"
#include "i2c.h"

uint8_t Need_redraw = 0; // Флаг который выставляется в 1 если нужно обновить изображение на дисплее

FILE *Std_in_out;

void reboot(void)
{
	wdt_enable(WDTO_1S);
	for(;;);
}

void out0(char var) // Вывод в RS232 порт 0
{
	while ( !( UCSR0A & (1<<UDRE0)) );
	UDR0 = var;
}

char in0(void) // Чтение из RS232 порт 0
{
	while ( !(UCSR0A & (1<<RXC0)) );
	return UDR0;
}

void USART_Init0(unsigned int ubrr)  //Инициалтизация RS232 порта 0
{
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1 << RXCIE0);

	UCSR0C = (3<<UCSZ00)|(3<<UCSZ01);
}

void out1(char var) // Вывод в RS232 порт 1
{
	asm("wdr");
	while ( !( UCSR1A & (1<<UDRE1)) );
	UDR1 = var;
}

char in1(void) // Чтение из RS232 порт 1
{
	while ( !(UCSR1A & (1<<RXC1)) );
	return UDR1;
}

void USART_Init1(unsigned int ubrr)  //Инициалтизация RS232 порта 1
{
	UBRR1H = (unsigned char)(ubrr>>8);
	UBRR1L = (unsigned char)ubrr;
	UCSR1B = (1<<RXEN1)|(1<<TXEN1) | (1 << RXCIE1);

	UCSR1C = (3<<UCSZ10)|(3<<UCSZ11);
}

void set_dev_in_out(int com)
{
	fclose(Std_in_out);

	switch(com)
	{
		case COM_PORT1:
			Std_in_out = fdevopen((void *)out0, (void *)in0);
		break;

		case COM_PORT2:
			Std_in_out = fdevopen((void *)out1, (void *)in1);
		break;
	}
}

void console_put_ch(char ch) //Вывод одного символа в UART
{
	putchar((int)ch);
}

void console_out_str(char *str) //Вывод строки в UART
{
	while(*str != 0)
	{
		putchar(*str);
		str++;
	}
}

void console_out_str_P(const char *str) //Вывод строки из программной памяти в UART
{
	while(pgm_read_byte(str) != 0x00)
		putchar(pgm_read_byte(str++));
}


uint8_t check_input_rs485() // Проверяет есть ли в UARTе данные
{
	switch(COMPORT_RS485)
	{
		case COM_PORT1:
			return (UCSR0A & (1 << RXC0));

		case COM_PORT2:
			return (UCSR1A & (1 << RXC1));
	}
}

void rs485out(char byte)
{
	switch(COMPORT_RS485)
	{
		case COM_PORT1:
			out0(byte);
		break;

		case COM_PORT2:
			out1(byte);
		break;
	}
}

char rs485in()
{
	switch(COMPORT_RS485)
	{
		case COM_PORT1:
			return in0();

		case COM_PORT2:
			return in1();
	}
}

struct button Buttons[] = // Инициализируем все кнопки
{
	{ // S1
		.button_port = (void *)&PINC,
		.button_pin = PINC6,
		.enable_dbl_click = 0,
	},
	{ // S2
		.button_port = (void *)&PINC,
		.button_pin = PINC5,
		.enable_dbl_click = 0,
	},
	{ // S3
		.button_port = (void *)&PINC,
		.button_pin = PINC4,
		.enable_dbl_click = 0,
	},
	{ // S4
		.button_port = (void *)&PINB,
		.button_pin = PINB4,
		.enable_dbl_click = 0,
	},
	{ // S5
		.button_port = (void *)&PINB,
		.button_pin = PINB3,
		.enable_dbl_click = 0,
	},
	{ // Enter
		.button_port = (void *)&PINE,
		.button_pin = PINE2,
		.enable_dbl_click = 0,
	},
	{ // Left
		.button_port = (void *)&PINE,
		.button_pin = PINE3,
		.enable_dbl_click = 0,
	},
	{ // Right
		.button_port = (void *)&PINE,
		.button_pin = PINE4,
		.enable_dbl_click = 0,
	},

	{ // Level1
		.button_port = (void *)&PING,
		.button_pin = PING0,
		.enable_dbl_click = 0,
	},
	{ // Level2
		.button_port = (void *)&PIND,
		.button_pin = PIND7,
		.enable_dbl_click = 0,
	},
	{ // Level3
		.button_port = (void *)&PIND,
		.button_pin = PIND6,
		.enable_dbl_click = 0,
	},
};


struct lib_button Buttons_info = // Инициализируем структуру для библиотеки buttons
{
	.contact_bounse = 1,
	.time_multiplier = 300,
	.max_delay_one_click = 600,
	.buttons = Buttons,
	.count_buttons = (sizeof(Buttons) / sizeof(struct button)),
};

struct encoder Encoders[] =  // Описываем енкодер
{
	{ //Основной энкодер для управления меню
		.port_line_A = (void *)&PINE,
		.pin_line_A = PINE3,
		.port_line_B = (void *)&PINE,
		.pin_line_B = PINE4,
	},
};

struct lib_encoder Lib_encoders = // Инициализируем структуру для библиотеки encoders
{
	.encoders = Encoders,
	.count_encoders = (sizeof(Encoders) / sizeof(struct encoder)),
};

void init_input_controls(void) // Инициализация всех органов управления
{
	DDRC &= ~_BV(PC6);
	DDRC &= ~_BV(PC5);
	DDRC &= ~_BV(PC4);
	DDRB &= ~_BV(PB4);
	DDRB &= ~_BV(PB3);

	DDRE &= ~_BV(PE2);
	DDRE &= ~_BV(PE3);
	DDRE &= ~_BV(PE4);

	DDRG &= ~_BV(PG0);
	DDRD &= ~_BV(PD7);
	DDRD &= ~_BV(PD6);


	PORTC |= _BV(PC6);
	PORTC |= _BV(PC5);
	PORTC |= _BV(PC4);
	PORTB |= _BV(PB4);
	PORTB |= _BV(PB3);

	PORTE |= _BV(PE2);
	PORTE |= _BV(PE3);
	PORTE |= _BV(PE4);

	PORTG |= _BV(PG0);
	PORTD |= _BV(PD7);
	PORTD |= _BV(PD6);

	init_lib_button(&Buttons_info);
	init_encoders(&Lib_encoders);
}


void block_door(uint8_t mode) // Заблокировать или разблокировать дверку
{
	static uint8_t state = 0;

	if(mode != state)
		Need_redraw = 1;

	if(mode)
		PORTF |= _BV(PF7);
	else
		PORTF &= ~_BV(PF7);

	state = mode;
}

void turn_heater(uint8_t mode) // Включить или выключить ТЕН
{
	static uint8_t state = 0;

	if(mode != state)
		Need_redraw = 1;

	if(mode)
		PORTF |= _BV(PF6);
	else
		PORTF &= ~_BV(PF6);

	state = mode;
}

void turn_pump(uint8_t mode) // Включить или выключить насос слива воды
{
	static uint8_t state = 0;

	if(mode != state)
		Need_redraw = 1;

	if(mode)
		PORTF |= _BV(PF5);
	else
		PORTF &= ~_BV(PF5);

	state = mode;
}

void turn_motor_power(uint8_t mode) //Подать/снять питание на мотор
{
	if(mode)
		PORTE &= ~_BV(PE6);
	else
		PORTE |= _BV(PE6);
}

// Включить или выключить впуск воды на заданном клапане (num)
void turn_valve(uint8_t num, uint8_t mode)
{
	static uint8_t state[2] = {0, 0};

	if(mode != state[num])
		Need_redraw = 1;

	switch(num)
	{
		case VALVE1:
			if(mode)
				PORTF |= _BV(PF3);
			else
				PORTF &= ~_BV(PF3);
		break;

		case VALVE2:
			if(mode)
				PORTF |= _BV(PF4);
			else
				PORTF &= ~_BV(PF4);
		break;
	}

	state[num] = mode;
}

// Включить забор воды из указанной ячейки
void turn_water_source(uint8_t num, uint8_t mode)
{
	turn_valve(VALVE1, OFF);
	turn_valve(VALVE2, OFF);
	if(mode)
	{
		switch(num)
		{
			case WATER_SOURCE1:
				turn_valve(VALVE1, ON);
			break;

			case WATER_SOURCE2:
				turn_valve(VALVE2, ON);
			break;

			case WATER_SOURCE3:
				turn_valve(VALVE1, ON);
				turn_valve(VALVE2, ON);
			break;
		}
	}
}

//Включить или выключить питание мотора, reverse - направление вращения, speed_mode - номер скоростного режима
void turn_motor(uint8_t mode, uint8_t reverse, uint8_t speed_mode)
{
	// Отключаем питание мотора
	PORTA &= ~_BV(PA2);
	PORTA &= ~_BV(PA0);
	PORTF &= ~_BV(PF2);

	if(mode) // Если нужно подать питание на мотор
	{
		switch(speed_mode)
		{
			case LOW_SPEED_ROTATION:
				PORTA &= ~_BV(PA2);
			break;

			case HIGH_SPEED_ROTATION:
				PORTA |= _BV(PA2);
			break;
		}

		switch(reverse)
		{
			case ROTATION_BACKWARD:
				PORTF |= _BV(PF2);
			break;

			case ROTATION_FORWARD:
				PORTA |= _BV(PA0);
			break;
		}
	}

	Need_redraw = 1;
}


void turn_led(int num, int mode) // Включить или выключить светодиод num
{
	switch(num)
	{
		case LED4:
			switch(mode)
			{
				case ON:
					PORTB |= _BV(PB2);
				break;

				case OFF:
					PORTB &= ~_BV(PB2);
				break;
			}
		break;

		case LED3:
			switch(mode)
			{
				case ON:
					PORTB |= _BV(PB0);
				break;

				case OFF:
					PORTB &= ~_BV(PB0);
				break;
			}
		break;

		case LED2:
			switch(mode)
			{
				case ON:
					PORTE |= _BV(PE7);
				break;

				case OFF:
					PORTE &= ~_BV(PE7);
				break;
			}
		break;

		case LED1:
			switch(mode)
			{
				case ON:
					PORTE |= _BV(PE5);
				break;

				case OFF:
					PORTE &= ~_BV(PE5);
				break;
			}
		break;
	}
}

void turn_main_power(uint8_t state) //Включить или выключить общее питание стиральной машины
{
	if(state)
		PORTC |= _BV(PC3);
	else
		PORTC &= ~_BV(PC3);
}

void turn_display_light(uint8_t state) //Включить или выключить подсветку дисплея
{
	if(state)
		PORTC |= _BV(PC2);
	else
		PORTC &= ~_BV(PC2);
}

void lcd_rus(char c, FILE *unused)
{
	const unsigned char TransTable[] = {
	0x41,0xA0,0x42,0xA1,0xE0,0x45,0xAB,0xA4,0xA5,0xA6,0x4B,0xA7,0x4D,0x48,0x4F,0xA8,
	0x50,0x43,0x54,0xA9,0xAA,0x58,0xE1,0xAB,0xAC,0xE2,0xAD,0xAE,0x62,0xAF,0xB0,0xB1,
	0x61,0xB2,0xB3,0xB4,0xE3,0x65,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0x6F,0xBE,
	0x70,0x63,0xBF,0x79,0xE4,0x78,0xE5,0xC0,0xC1,0xE6,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7 };

	if (c > 0xBF)
		lcd_putc(TransTable[(c - 0xC0)]);
	else
		lcd_putc(c);

}

void write_enable_to_rs485(void) // Включить запись в сеть
{
	PORTB |= _BV(PB6);
}

void write_disable_to_rs485(void) // Выключить запись в сеть
{
	PORTB &= ~_BV(PB6);
}

// Функции для i2c
void set_sda(uint8_t mode) // Установить линию SDA в 1 или 0
{
	if(!mode)
	{
		DDRD |= _BV(PD4);
		PORTD &= ~_BV(PD4);
	}
	else
		DDRD &= ~_BV(PD4);
}

void set_scl(uint8_t mode) // Установить линию SCL в 1 или 0
{
	if(!mode)
	{
		DDRD |= _BV(PD5);
		PORTD &= ~_BV(PD5);
	}
	else
		DDRD &= ~_BV(PD5);
}

uint8_t get_sda(void) // Получить состояние линии SDA
{
	DDRD &= ~_BV(PD4);
	return (PIND & (1 << PIND4));
}

uint8_t get_scl(void) // Получить состояние линии SCL
{
	DDRD &= ~_BV(PD5);
	return (PIND & (1 << PIND5));
}

struct lib_i2c Lib_i2c =  // Инициализируем структуру для библиотеки i2c
{
	.set_sda = set_sda,
	.set_scl = set_scl,
	.get_sda = get_sda,
	.get_scl = get_scl,
	.step_delay = 10, // Шаг задержки 6ть наносекунд
};

void init_external_clock(void) //Инициализация внешних часов
{
	uint8_t sec;
	struct time default_time =
	{
		.sec = 0,
		.min = 0,
		.hour = 0,
		.day_of_week = 1,
		.day_of_month = 1,
		.month = 1,
		.year = 10,
	};

	DDRD &= ~_BV(PD4);
	DDRD &= ~_BV(PD5);
	PORTD &= ~_BV(PD4);
	PORTD &= ~_BV(PD5);

	init_i2c(&Lib_i2c);
	i2c_recv_data(I2C_ADDR_CLOCK, 0, &sec, 1); // Считываем с часов нулевой байт
	if(sec & (1 << 7)) // Если часы остановленны то запускаем часы
	{
		sec = 0;
		write_datetime(&default_time);
	}
}

static uint8_t bcd_to_byte(uint8_t bcd_val) // Сконвертировать BCD формат в бинарный
{
    return (bcd_val&0xf) + ((bcd_val&0xf0) >> 4) * 10;
}

static uint8_t byte_to_bcd(uint8_t byte_val) // Сконвертировать бинарный формат в BCD
{
    return (byte_val%10) + ((byte_val/10) << 4);
}

uint8_t read_current_datetime(struct time *time) // Прочитать текущую дату и время
{
	struct time bcd_datetime;
	uint8_t *src;
	uint8_t *dst;
	uint8_t i;

	src = (uint8_t *)&bcd_datetime; //Получаем указатель на bcd_datetime
	i = i2c_recv_data(I2C_ADDR_CLOCK, 0, src, sizeof(struct time)); // Считываем с часов данные длинною в sizeof(struct time)
	if(i < sizeof(struct time)) //Если получили меньше информации чем было запрошенно
		return 1;

	// Записываем полученное время и дату по переданному адресу time
	dst = (uint8_t *)time;
	for(i = 0; i < sizeof(struct time); i++)
		dst[i] = bcd_to_byte(src[i]);

	return 0;
}

uint8_t write_datetime(struct time *time) // Установить текущую дату и время
{
	struct time bcd_datetime;
	uint8_t *src;
	uint8_t *dst;
	uint8_t i;

	src = (uint8_t *)time;
	dst = (uint8_t *)&bcd_datetime;
	for(i = 0; i < sizeof(struct time); i++)
		dst[i] = byte_to_bcd(src[i]);

	i = i2c_send_data(I2C_ADDR_CLOCK, 0, dst, sizeof(struct time));

	if(i < sizeof(struct time)) //Если записали меньше информации чем требовалось
		return 1;

	return 0;
}

