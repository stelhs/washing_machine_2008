#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <avr/pgmspace.h>

struct lib_button
{
	uint8_t contact_bounse; // 1  // Задержка дребезга
	uint16_t time_multiplier; // 300 // Задержка для увеличения множителя при долгом удерживании
	uint16_t max_delay_one_click; // 600 // Задержка 1-ного клика
	struct button *buttons; // массив структур с описанием кнопок
	uint8_t count_buttons; // Количество кнопок
};

struct button
{
	struct
	{
		uint8_t state_button : 1; // Стабильное состояние кнопки
		uint8_t one_click : 1;	// Одиночный клик
		uint8_t double_click : 1;	// Двойной клик
 		uint8_t hold : 1;	// Долгое удерживание
		uint8_t prev_hold : 1;  // предыдущее значение hold
	}state;
	uint8_t timer_bounce_down; // таймер для стабилизации нажатия кнопки
	uint8_t timer_bounce_up; // таймер для стабилизации отпускания кнопки
	uint16_t timer1;
	uint16_t timer2;
	uint16_t timer3;
	uint16_t timer_hold;
	uint8_t hold_multiplier;
	char *button_port;  // Порт кнопки
	char button_pin; // вывод порта кнопки
	uint8_t enable_dbl_click : 1; // Если данный флаг выставлен в 1 то разрешено распознавание двойного клика
};

void init_lib_button(struct lib_button *init);
void change_timer_buttons(void); // Функция опрашивает состояния всех кнопок
void clear_unused_key_code(void); // Очищает данные о нажатиях кнопок
void clear_key_data(struct button *buttons, int button); //Очистить всю информацию о кнопке button
void get_buttons(void);	//опрос состаяния кнопок
void scan_code_button(void); // Распознавание типов нажатий

#endif // BUTTON_H
