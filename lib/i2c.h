#ifndef I2C_H
#define I2C_H

#include <stdint.h>

struct lib_i2c
{
	void (*set_sda)(uint8_t); // Обработчик установики сигнала SDA
	void (*set_scl)(uint8_t); // Обработчик установики сигнала SCL
	uint8_t (*get_sda)(void); // Обработчик возвращает состояние линии SDA
	uint8_t (*get_scl)(void); // Обработчик возвращает состояние линии SCL
	uint16_t step_delay; // Задержка регулирующая скорость обмена
};

void init_i2c(struct lib_i2c *init); // Инициализация библиотеки

uint8_t i2c_send_data(uint8_t device, uint8_t addr, uint8_t *data, uint8_t len); //Записать данные в устройство. Возвращается количество успешно записанных байт

uint8_t i2c_recv_data(uint8_t device, uint8_t addr, uint8_t *data, uint8_t len); //Прочитать данные с устройства

#endif // I2C_H
