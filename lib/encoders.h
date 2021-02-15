#ifndef ENCODERS_H
#define ENCODERS_H

#include <stdint.h>

enum encoder_states
{
	NO_ROTATING,
	LEFT_ROTATING,
	RIGHT_ROTATING
};

struct encoder
{
	char *port_line_A;  // Указатель на порт линии A
	uint8_t pin_line_A : 3; // Номер вывода линии A
	
	char *port_line_B;  // Указатель на порт линии B
	uint8_t pin_line_B : 3; // Номер вывода линии B
	
	uint8_t state_strob : 1; // текущее состояние линии A
	
	uint8_t state : 2; // Итоговое состояние энкодера
};

struct lib_encoder
{
	struct encoder *encoders; // массив структур с описанием всех энкодеров
	uint8_t count_encoders; // Количество энкодеров
};

void init_encoders(struct lib_encoder *init); // Проинициализировать все энкодеры

void scan_encoders(void); // Функция встраивается в основной цикл и опрашивает состояния всех проинициализированных энкодеров

#endif // ENCODERS_H
