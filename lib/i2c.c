#define __DELAY_BACKWARD_COMPATIBLE__
#include <util/delay.h>
#include "i2c.h"

static struct lib_i2c *Lib_i2c;

void init_i2c(struct lib_i2c *init) // Инициализация библиотеки
{
	Lib_i2c = init;
	Lib_i2c -> set_sda(1);
	Lib_i2c -> set_scl(1);
}

static uint8_t i2c_write(char byte) //Запись байта в линию. Функция возвращает 0 если байт был принят или 1 если непринят
{
	int8_t i, ret;

//	while(!Lib_i2c -> get_scl()); //Ожидаем пока SCL освободится
	for(i = 7; i >= 0; i--)
	{
		if(byte & (1 << i))
			Lib_i2c -> set_sda(1);
		else
			Lib_i2c -> set_sda(0);

		_delay_us(Lib_i2c -> step_delay);
		Lib_i2c -> set_scl(1);
		_delay_us(Lib_i2c -> step_delay);
		Lib_i2c -> set_scl(0);
		_delay_us(Lib_i2c -> step_delay);
	}

	Lib_i2c -> set_sda(1);

	_delay_us(Lib_i2c -> step_delay);
	Lib_i2c -> set_scl(1);
	_delay_us(Lib_i2c -> step_delay / 2);
	ret = Lib_i2c -> get_sda();
	_delay_us(Lib_i2c -> step_delay / 2);
	Lib_i2c -> set_scl(0);
	Lib_i2c -> set_sda(0);
	_delay_us(Lib_i2c -> step_delay);

	return ret;
}

static uint8_t i2c_read(uint8_t ack) // Чтение байта с линии
{
	uint8_t input = 0, i;

//	while(!Lib_i2c -> get_scl()); //Ожидаем пока SCL освободится
	Lib_i2c -> set_sda(1);
	Lib_i2c -> set_scl(0);
	_delay_us(Lib_i2c -> step_delay);

	for(i = 0; i < 8; i++)
	{
		Lib_i2c -> set_scl(1);
		_delay_us(Lib_i2c -> step_delay / 2);

		input <<= 1;
		if(Lib_i2c -> get_sda())
			input +=1;

		_delay_us(Lib_i2c -> step_delay / 2);
		Lib_i2c -> set_scl(0);

		_delay_us(Lib_i2c -> step_delay);
	}

	if(ack)
		Lib_i2c -> set_sda(0);
	else
		Lib_i2c -> set_sda(1);

	_delay_us(Lib_i2c -> step_delay);
	Lib_i2c -> set_scl(1);
	_delay_us(Lib_i2c -> step_delay);
	Lib_i2c -> set_scl(0);
	_delay_us(Lib_i2c -> step_delay);
	Lib_i2c -> set_sda(1);

	return input;
}

static uint8_t i2c_query(uint8_t device, char rw) // Отправить служебный пакет к slave. Функция возвращает 0 если пакет был принят или 1 если непринят
{
	uint8_t packet;

	packet = device << 1;
	if(rw == 'r')
		packet += 1;

	return i2c_write(packet);
}

static void i2c_start(void) //Послать сигнал СТАРТ
{
	Lib_i2c -> set_sda(1);
	Lib_i2c -> set_scl(1);
	_delay_us(Lib_i2c -> step_delay * 2);

	Lib_i2c -> set_sda(0);
	_delay_us(Lib_i2c -> step_delay);
	Lib_i2c -> set_scl(0);
	_delay_us(Lib_i2c -> step_delay);
}

static void i2c_stop(void) //Послать сигнал СТОП
{
	Lib_i2c -> set_sda(0);
	_delay_us(Lib_i2c -> step_delay * 2);
	Lib_i2c -> set_scl(1);
	_delay_us(Lib_i2c -> step_delay);
	Lib_i2c -> set_sda(1);
	_delay_us(Lib_i2c -> step_delay);
}

uint8_t i2c_send_data(uint8_t device, uint8_t addr, uint8_t *data, uint8_t len) //Записать данные в устройство. Возвращается количество успешно записанных байт
{
	uint8_t rc, i;

	i2c_start();
	rc = i2c_query(device, 'w');
	if(rc)
		return 0;

	rc = i2c_write(addr);
	if(rc)
		return 0;

	for(i = 0; i < len; i++) //Записываем в цикле все байты один за другим
	{
		rc = i2c_write(data[i]);
		if(rc)
			return i;
	}
	i2c_stop();

	return len;
}

uint8_t i2c_recv_data(uint8_t device, uint8_t addr, uint8_t *data, uint8_t len) //Прочитать данные с устройства
{
	uint8_t rc, i, ack;

	i2c_start();
	rc = i2c_query(device, 'w');
	if(rc)
		return 0;

	rc = i2c_write(addr);
	if(rc)
		return 0;

	i2c_start();
	rc = i2c_query(device, 'r');
	if(rc)
		return 0;

	for(i = 0; i < len; i++)
	{
		ack = 1;

		if(i == len - 1)
			ack = 0;

		data[i] = i2c_read(ack);
	}

	i2c_stop();

	return len;
}


