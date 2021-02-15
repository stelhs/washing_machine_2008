// Функции для работы с буфером LCD
#include <string.h>
#include "drivers.h" 
#include "lcd.h" 

static struct //Структура буфера LCD
{
	char buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
	uint8_t x;
	uint8_t y;
}Display_buffer;


void gotoxy_lcd_buf(uint8_t x, uint8_t y) // Установить курсор
{
	Display_buffer.x = x;
	Display_buffer.y = y;
}

void clr_lcd_buf(void) // Отчистить дисплей
{
	memset(Display_buffer.buffer, ' ', sizeof(Display_buffer.buffer));
}

void putchar_lcd_buf(char chr) // Вывести один символ
{
	if(Display_buffer.x >= DISPLAY_WIDTH || Display_buffer.y >= DISPLAY_HEIGHT)
		return;

	Display_buffer.buffer[Display_buffer.y * DISPLAY_WIDTH + Display_buffer.x] = chr;
	Display_buffer.x ++;
	
}

void print_lcd_buf(char *str) // Вывод строки из оперативной памяти
{
	if(Display_buffer.x >= DISPLAY_WIDTH || Display_buffer.y >= DISPLAY_HEIGHT)
		return;
		
	while(*str != '\0')
	{
		Display_buffer.buffer[Display_buffer.y * DISPLAY_WIDTH + Display_buffer.x] = *str++;
		Display_buffer.x ++;
		if(Display_buffer.x >= DISPLAY_WIDTH)
			break;
	}
}

void print_lcd_buf_P(const char *str)  // Вывод строки из программной памяти
{
	if(Display_buffer.x >= DISPLAY_WIDTH || Display_buffer.y >= DISPLAY_HEIGHT)
		return;
		
	while(pgm_read_byte(str) != 0x00)
	{
		Display_buffer.buffer[Display_buffer.y * DISPLAY_WIDTH + Display_buffer.x] = pgm_read_byte(str++);
		Display_buffer.x ++;
		if(Display_buffer.x >= DISPLAY_WIDTH)
			break;
	}
}

void clear_line_buf(uint8_t line_num)    //Отчистить строку
{
	Display_buffer.x = 0;
	Display_buffer.y = line_num;
	
	memset(Display_buffer.buffer + Display_buffer.y * DISPLAY_WIDTH, ' ', DISPLAY_WIDTH);
}

void fill_line_buf(uint8_t line_num, char ch)    //Залить строку указанным символом строку
{
	Display_buffer.x = 0;
	Display_buffer.y = line_num;
	
	memset(Display_buffer.buffer + Display_buffer.y * DISPLAY_WIDTH, ch, DISPLAY_WIDTH);
}


void copy_buf_to_lcd(void) // Отправить буфер в LCD
{
	lcd_home();
	for(uint8_t y = 0; y < DISPLAY_HEIGHT; y++)
	{
		lcd_gotoxy(0, y);
		for(uint8_t x = 0; x < DISPLAY_WIDTH; x++)
			lcd_rus(Display_buffer.buffer[y * DISPLAY_WIDTH + x], 0);
	}
}

