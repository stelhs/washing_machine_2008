#include "epp.h"
#include <string.h>
#include <util/delay.h>
#include <stdio.h>

static struct epp_lib *Epp = NULL; // Настройки EPP (Easy Packet Protocol)
struct received_packet Received_packet_numbers[MAX_COUNT_PKG_NUMBERS];  // Очередь номеров принятых пакетов. Необходима для того чтобы помнить о нескольких последних принятых пакетах
struct received_packet Confirm_packet_data;  // Хранит информацию о пакете на котрый нужно выдать ответ
static char Received_buffer[MAX_PACKET_LEN]; // Буффер для входящих данных

volatile struct // Битовое поле всех сотояний сети и протокола
{
	unsigned int read_packet : 1; // Если флаг установлен в 1 значит распознанно начало пакета и система находится в сотоянии чтения данных пакета
	unsigned int send_state : 3; //Флаг устанавливается в 1 если начался процесс записи в сеть
	char last_received_byte : 8; //Сдесь хранится значение последнего принятого байта (по прерыванию)
	unsigned int count_retry_send : 4; // Количество попыток отправки пакета
	unsigned int timer_retry_send : 16; // Таймер отсчета времени для повторной отправки пакета
	unsigned int timer_clear_history : 16; // Таймер отсчета времени для повторной отправки пакета
	unsigned int need_confirm : 1; //Флаг устанавливается если необходимо подтвердить удачный прием пакета
	unsigned int read_byte : 1; //Флаг устанавливается когда прочитан байт с RS485 по прерыванию
	unsigned int last_packet_number : 8; // Номер последнего отправленного пакета
}State_network;


static uint8_t rand8(uint8_t seed) //Генератор случайных чисел размерностью в 1 байт
{
	static uint8_t x;
	if(seed)
		x = seed;

	uint8_t hi, lo;

	if(x == 0)
		x = 123;
	hi = x / 127773;
	lo = x % 127773;
	x = 16807 * lo - 2836 * hi;
	if (x < 0)
		x += 0x7fffffff;

	return x;
}

void init_epp(struct epp_lib *init) // Инициализация протокола
{
	Epp = init;
	memset(Epp -> incomming, 0, sizeof(struct net_packet));
	memset(&Received_buffer, 0, MAX_PACKET_LEN);
	memset(&Received_packet_numbers, 0, sizeof(Received_packet_numbers));
	State_network.read_packet = 0;
	State_network.send_state = NO_SEND;
	Epp -> received = 0;
	State_network.last_received_byte = 0;
	State_network.count_retry_send = 0;
	State_network.timer_retry_send = 0;
	State_network.timer_clear_history = 0;
	State_network.need_confirm = 0;
	rand8(Epp -> device_host * 4); //Устанавливаем начальную точку рандома для данного девайса
}

static uint8_t Crc16(char *pcBlock, unsigned short len) // Расчет контрольной суммы CRC-16 CCITT
{
    unsigned short crc = 0xFFFF;
    uint8_t i;

    while( len-- )
    {
        crc ^= *pcBlock++ << 8;

        for( i = 0; i < 8; i++ )
        crc = crc & 0x8000 ? ( crc << 1 ) ^ 0x1021 : crc << 1;
    }

    return crc;
}

static void push_buffer(char byte, char *buffer, int len) // Положить один байт в FIFO буффер. Этот буфер нужен для поиска заголовка пакета в потоке данных
{
	int i;
	for(i = 0; i < len; i++)
		if(!*(buffer + i)) //Ищем адрес нулевого байта в буфере
			break;

	if(i < len) //Если нашли адрес нулевого байт в буфере
		*(buffer + i) = byte; // То пишем в него значение
	else // Иначе сдвигаем все байты влево и в конец пишем новый
	{
		for(i = 0; i < len - 1; i++)
			*(buffer + i) = *(buffer + i + 1);

		*(buffer + len - 1) = byte;
	}
}

static void push_in_queue(struct received_packet *item, struct received_packet *buffer, int len) // Положить один элемент в FIFO буффер.
{
	int i;
	for(i = 0; i < len; i++)
		if(!(buffer[i].from)) //Ищем адрес пустой ячейки в буфере
			break;

	if(i < len) //Если нашли адрес нулевого байт в буфере
		*(buffer + i) = *item; // То пишем в него значение
	else // Иначе сдвигаем все байты влево и в конец пишем новый
	{
		for(i = 0; i < len - 1; i++)
			*(buffer + i) = *(buffer + i + 1);

		*(buffer + len - 1) = *item;
	}
}

void transmit_packet(struct net_packet *packet)
{
	int offset;
	char byte;
	// Начинаем отправку сформированного пакета
	for(offset = 0; offset < sizeof(struct net_packet); offset++)
	{
		asm("wdr");
		if(!Epp -> check_input_rs485()) //Проверяем не принимаются ли дынные с UART
		{
			Epp -> write_enable(); //Включаем передатчик
			_delay_us(100); // Небольшая задержка
			State_network.send_state = SEND; // Устанавливаем режим отправки данных
			byte = *(((char *)packet) + offset); //Извлекаем один байт
			State_network.read_byte = 0;
			Epp -> rs485out(byte); //отправляем один байт
			//print_log("send byte %d\r\n", offset);

			//Ожидаем пока не примится байт который мы отправили
			State_network.timer_retry_send = 0xFF;  // Устанавливаем таймаут 255 на ожидание входящего байта
			while(State_network.timer_retry_send > 0)
			{
				asm("wdr");
				if(State_network.read_byte)
					break;
			}

			State_network.timer_retry_send = 0;

			// Сверяем значение принятого через прерывание байта с отправленным байтом
			if(State_network.last_received_byte != byte || State_network.read_byte == 0) //Если неуспешно отправили один байт (тоесть прочитанно не то что записанно)
			{
				State_network.read_byte = 0;
				print_log("Incorrect byte\r\n", offset);
				State_network.send_state = BUSY; // Устанавливаем что сеть занята
				offset = -1; // Будем отправлять все поновой
				Epp -> write_disable(); //Выключаем передатчик

				State_network.timer_retry_send = rand8(0) * 5; //Устанавливаем рандомное время ожидания готовности сети.
				print_log("rand1 = %d\r\n", State_network.timer_retry_send);
				while(State_network.timer_retry_send > 0);
					//epp_processing();

				continue;
			}

			State_network.read_byte = 0;
		}
		else // Если в сети чтото есть то ничего неделаем просто ждем
		{
			print_log("busy\r\n");
			State_network.send_state = BUSY;
			Epp -> write_disable(); //Выключаем передатчик
			offset = -1; // Будем отправлять все поновой

			State_network.timer_retry_send = rand8(0) * 5; //Устанавливаем рандомное время ожидания готовности сети.
			print_log("rand2 = %d\r\n", State_network.timer_retry_send);
			while(State_network.timer_retry_send > 0);
				//epp_processing();

			continue;
		}
	} //Успешно отправили пакет и выключаем передатчик
	Epp -> write_disable();
}

void send_packet(uint8_t host_addr, void *buf, uint8_t len, uint8_t assignment, uint8_t pack_num, uint8_t type) // Отправить пакет
{
	struct net_packet packet;

	if(host_addr == 0) //Если посылается широковещательный пакет, то его номер назначаем автоматически
		pack_num = ++State_network.last_packet_number;

	memcpy(packet.start_key, PACKET_HEADER_KEY, KEY_LEN);
	packet.data.from = Epp -> device_host;
	packet.data.to = host_addr;
	packet.data.packet_num = pack_num;
	packet.data.type = type;
	packet.data.assignment = assignment;
	memset(packet.data.content, 0, MAX_DATA_LEN);
	memcpy(packet.data.content, buf, len);
	packet.CRC = Crc16((char *)&(packet.data), sizeof(struct net_packet_data)); // Вычисляем контрльную сумму и пишем её результат

	if(type == PACKET_WITH_CONFIRM)
		State_network.send_state = WAIT_ACCEPT; // Устанавливаем флаг ожидания подтверждения пакета

	print_log("transmit packet\r\n");
	transmit_packet(&packet);
	_delay_ms(25); // Данная задержка необходима для того чтобы при непрерывной отсылке пакетов между их отправками было хотябы 25ms
}

uint8_t add_history(uint8_t from, uint8_t pkg_num) // Добавить в ситорию информацию о полученном пакете
{
	struct received_packet item;
	uint8_t new_packet = 1;
	uint8_t i;
	item.from = from;
	item.pkg_num = pkg_num;

	State_network.timer_clear_history = CLEAR_HISTORY_TIMEOUT; // Перезаводим таймер на ожидание отчистки входящих пакетов

	for(i = 0; i < MAX_COUNT_PKG_NUMBERS; i++) // Смотрим в историю приходил ли данный пакет ранее
		if(!memcmp(&Received_packet_numbers[i], &item, sizeof(struct received_packet)))
				new_packet = 0;

	if(new_packet) //Если пришол новый пакет
	{
		push_in_queue(&item, Received_packet_numbers, MAX_COUNT_PKG_NUMBERS);
		return 1; // Возвращаем 1 если пакет является новым
	}

	return 0; // Если такой пакет уже приходил, то возвращаем 0
}


void attempt_to_receive_packet(void) //Функция пытается принять пакет. (Один байт за раз) (Функция должна встраиваться в обработчик перывания по появлению данных в UART)
{
	char byte;
	static uint8_t offset = 0;
	struct received_packet item;
	uint16_t crc;

	if(Epp == NULL) //Если EPP протокол не проинициализирован
		return;

	if(!Epp -> check_input_rs485()) //Если дынных нет
		return;

	byte = Epp -> rs485in(); // Принимаем один байт

	State_network.last_received_byte = byte;  // Сохраняем этот байт для тех кому он может понадобиться
	State_network.read_byte = 1; // Устанавливаем флаг что был прочитан байт (это нужно при отправке)

	if(State_network.need_confirm) //Если в данный момент идет отправка подтверждения то игнорируем все входящие данные
		return;

	if(!State_network.read_packet) // Если этот флаг опущен значит ищем начало пакета
	{
		push_buffer(byte, Received_buffer, KEY_LEN); //Помещаем каждый байт в  FIFO буфер
		Received_buffer[KEY_LEN] = 0; // В конец буфера пишем 0
		if(strcmp(Received_buffer, PACKET_HEADER_KEY) == 0) // Если обнаруженно начало пакета
		{
			State_network.read_packet = 1;
			offset = KEY_LEN;
		}
		return;
	}
	else //Если начало пакета было обнаруженно, то принимаем тело пакета
	{
		Received_buffer[offset] = byte; //Продолжаем считывать пакет
		offset ++;
		if(offset < MAX_PACKET_LEN) // Если пакет НЕпрочитан целиком
			return;

		// Если пакет прочитан целиком то раскрываем его
		State_network.read_packet = 0;
		offset = 0;

		struct net_packet *packet;
		packet = (struct net_packet *)Received_buffer; //Преобразуем буффер в структуру
		crc = Crc16((char *)&(packet -> data), sizeof(struct net_packet_data)); //Вычисляем контрольную сумму принятого пакета
		if(crc != packet -> CRC) // Если контрольная сумма пакета и  НЕсовпадает
		{
			print_log("BAD CRC\r\n");
			return;
		}
		print_log("Receive raw packet type = %d\r\n", packet -> data.type);

		switch(packet -> data.type)
		{
			case PACKET_WITH_CONFIRM: // если приняли пакет требующий подтвержджения
				if(Epp -> received) //Если буфер пакета занят предыдцщим пакетом то игнорируем этот пакет
					return;

				if(!Epp -> receive_all_packets) // Если ненадо принимать все подряд пакеты, то проверяем кому принадлежит пакет
				{
					if(packet -> data.to != Epp -> device_host) //если принятый пакет НЕпринадлежит данной машине
						return;
				}
				else
					if(packet -> data.from == Epp -> device_host) // Если включен режим снифера и принятый пакет принадлежит текущей машине
						return;

				item.from = packet -> data.from;
				item.pkg_num = packet -> data.packet_num;

				if(packet -> data.to == Epp -> device_host)
				{
					memcpy(&Confirm_packet_data, &item, sizeof(struct received_packet)); // Сохраняем сюда необходимые данные об этом пакете чтобы отправить подтверждение о принятии отправителю.
					State_network.need_confirm = 1;  // Устанавливаем флаг необходимости отправить подтверждение
				}

				Epp -> received = add_history(packet -> data.from, packet -> data.packet_num); // Сохраняем данные о принятом пакете, чтобы избежать принятия нескольких одинаковых пакетов
				if(Epp -> received)
					*(Epp -> incomming) = *packet;
			break;

			case PACKET_WITHOUT_CONFIRM: // если приняли пакет НЕтребующий подтвержджения
				if(Epp -> received) //Если буфер пакета занят предыдцщим пакетом то игнорируем этот пакет
					return;

				if(!Epp -> receive_all_packets) // Если ненадо принимать все подряд пакеты, то проверяем кому принадлежит пакет
				{
					if(packet -> data.to != Epp -> device_host) //если принятый пакет НЕпринадлежит данной машине
						return;
				}
				else
					if(packet -> data.from == Epp -> device_host) // Если включен режим снифера и принятый пакет принадлежит текущей машине
						return;

				Epp -> received = add_history(packet -> data.from, packet -> data.packet_num); // Сохраняем данные о принятом пакете, чтобы избежать принятия нескольких одинаковых пакетов
				if(Epp -> received)
					*(Epp -> incomming) = *packet;
			break;

			case BROADCAST_PACKET: // если приняли широковещательный пакет
				if(Epp -> received) //Если буфер пакета занят предыдцщим пакетом то игнорируем этот пакет
					return;

				Epp -> received = 1;
				*(Epp -> incomming) = *packet;
			break;

			case CONFIRM_PACKET: // если приняли пакет подтверждения
				State_network.send_state = COMPLETE;
			break;
		}
	}
}

int8_t sync_send_packet(uint8_t host_addr, uint8_t assignment, void *buf, uint8_t len)
{
	State_network.last_packet_number++; //Номер пакета (каждый раз новый)

	for(State_network.count_retry_send = 0; State_network.count_retry_send < COUNT_SEND_RETRY; State_network.count_retry_send++)
	{  // Отправляем пакет несколько раз задержкой пока непридет подтверждение получения пакета

		print_log("attempt to send\r\n");
		if(host_addr == 0)  // Если адрес указан нулевой значит пакет широковещательный
		{
			send_packet(host_addr, buf, len, assignment, 0, BROADCAST_PACKET);
			return 0;
		}
		else
			send_packet(host_addr, buf, len, assignment, State_network.last_packet_number, PACKET_WITH_CONFIRM);

		epp_processing();
		State_network.timer_retry_send = RETRY_TIMEOUT; // Устанавливаем время повторной отправки пакета если непридет подтверждения
		while(State_network.timer_retry_send > 0)
		{
			asm("wdr");
			if(State_network.send_state == COMPLETE) // Если пришло подтверждение принятого пакета
			{
				State_network.send_state = NO_SEND;
				print_log("sended %d\r\n", State_network.last_packet_number);
				_delay_ms(30); // Данная задержка необходима для ожидания готовности передатчика который осуществил подтверждение на принятие нашего пакета
				return 0;
			}
		}
	}
	print_log("not sended\r\n");
	return -1;
}

#ifdef EPP_SEND_RAW
/*void sync_send_packet_raw(struct net_packet *packet)
{
	if(packet->datas.to == 0) //Если посылается широковещательный пакет, то его номер назначаем автоматически
		packet -> data.packet_num = ++State_network.last_packet_number;

	memcpy(packet.start_key, PACKET_HEADER_KEY, KEY_LEN);
	packet.data.from = Epp -> device_host;
	packet.data.to = host_addr;
	packet.data.type = type;
	packet.data.assignment = assignment;
	memset(packet.data.content, 0, MAX_DATA_LEN);
	memcpy(packet.data.content, buf, len);
	packet.CRC = Crc16((char *)&(packet.data), sizeof(struct net_packet_data)); // Вычисляем контрльную сумму и пишем её результат

	if(type == PACKET_WITH_CONFIRM)
		State_network.send_state = WAIT_ACCEPT; // Устанавливаем флаг ожидания подтверждения пакета

	//print_log("transmit packet\r\n");
	transmit_packet(&packet);
	_delay_ms(25); // Данная задержка необходима для того чтобы при непрерывной отсылке пакетов между их отправками было хотябы 50ms
}*/

#endif

// Пропинговать устройство
#ifdef SUPPORT_EPP_PING
	uint8_t epp_ping(uint8_t host_addr)
	{
		printf("ping device %d:\r\n", host_addr);
		for(State_network.count_retry_send = 0; State_network.count_retry_send < COUNT_SEND_RETRY; State_network.count_retry_send++)
		{  // Отправляем пакет несколько раз задержкой пока непридет подтверждение получения пакета
			printf("send request -> ");
			send_packet(host_addr, NULL, 0, EPP_PING, 0, PACKET_WITH_CONFIRM);
			epp_processing();
			State_network.timer_retry_send = RETRY_TIMEOUT; // Устанавливаем время повторной отправки пакета если непридет подтверждения
			while(State_network.timer_retry_send > 0)
			{
				asm("wdr");
				if(State_network.send_state == COMPLETE) // Если пришло подтверждение принятого пакета
				{
					State_network.send_state = NO_SEND;
					printf("received OK\r\n");
					return 0;
				}
			}
			printf("not response\r\n");
		}

		return -1;
	}
#endif

 //Функция должна встраиваться в обработчик таймера с периудом запуска в  0.001 секунду
void inc_epp_timers(void)
{
	if(State_network.timer_retry_send > 0)
		State_network.timer_retry_send--;

	if(State_network.timer_clear_history > 0)
		State_network.timer_clear_history--;
}

void epp_processing(void) //Функция должна встраиваться в основной цикл программы
{
	asm("wdr");
	if(State_network.need_confirm) // Если необходимо отправить подтверждение о принятии пакета
	{
		State_network.need_confirm = 0;
		send_packet(Confirm_packet_data.from, NULL, 0, 0, Confirm_packet_data.pkg_num, CONFIRM_PACKET);
	}

	if(State_network.timer_clear_history == 1) // Если пришло время отчистить историю входящих пакетов
	{
		memset(Received_packet_numbers, 0, MAX_COUNT_PKG_NUMBERS * sizeof(struct received_packet)); // Отчищаем историю
		State_network.timer_clear_history = 0;
	}

}
