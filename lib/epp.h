#ifndef EPP_H
#define EPP_H 1

#include <stdint.h>
#include <avr/pgmspace.h>

#define PACKET_HEADER_KEY "STP" // Ключ начала пакета
#define KEY_LEN sizeof(PACKET_HEADER_KEY) - 1 //Длинна ключа пакета
#define MAX_DATA_LEN   32 //Максимальная длинна данных в пакете
#define MAX_DEVICE_NAME_LEN 10 //Максимальная длинна имени устройства
#define PACKET_LEN sizeof(struct net_packet) // Полная длинна пакета

#define COUNT_SEND_RETRY 7 //Количество попыток отправки пакета (максимум 7)
#define MAX_COUNT_PKG_NUMBERS 5 //Количество входящих пакетов о которых будет помнить программа, нужно чтобы не принимать один и тотже пакет несколько раз
#define RETRY_TIMEOUT 3000  //Время через которое пакет будет отправлен повторно если небыл принят
#define CLEAR_HISTORY_TIMEOUT 30000  //Время через которое история фходящих пакетов будет удалена

//Возможные коды пакетов:
#define EPP_PING 0 // пропинговать устройство
#define GET_DEVICE_INFO 1 // Выдать инфу о себе
#define DEVICE_INFO 2 // Получить инфу об устройстве
#define GET_CURRENT_STATE 3 // Выдать текущее состояние
#define CURRENT_STATE 4 // Получить состояние устройства
#define EPP_WELCOM 5 // При включении каждое устройство посылает в сеть приветствие, и структуру device_info
#define PLEASE_START_NOW 6 // Можно приступать к работе
	// Номера кодов с 1 по 10 зарезервированны для всех устройств, все специфичные коды пакетов должны быть в  пределах 11-255

//#define DEBUG_LOG 1 //Включить дебаг

#ifdef DEBUG_LOG
    #define print_log(frm, args...)\
    {\
		printf(frm, ##args);\
    }
#else
    #define print_log(frm, args...) 
#endif 



struct epp_lib
{
	struct net_packet *incomming; // Указатель на входящий пакет
	uint8_t received : 1; // Флаг устанавливается в случае успешного приема пакета, его необходимо сбрасывать после обработки принятого пакета
	
	uint8_t device_host; // Сетевой номер данного устройства
	uint8_t (*check_input_rs485)(void); //Указатель на функцию проверяющий наличие данных в UART буфере
	char (*rs485in)(void); //  Указатель на функцию для приема байта с UART
	void (*rs485out)(char byte); // Указатель на функцию для отправки данных в UART
	void (*write_enable)(void); // Включить запись в сеть
	void (*write_disable)(void); // Выключить запись в сеть
	uint8_t receive_all_packets : 1; // Определяет нужно ли принимать все пакеты или только адресованные получателю
};

struct device_info // Формат пакета с информацией об устройстве (ответ на запрос GET_DEVICE_INFO)
{
	uint8_t host; // Номер устройства в сети
	uint8_t type; // Тип устройства
	char device_name[16]; // Имя устройства
};

enum packet_types //Типы пакетов
{
	PACKET_WITH_CONFIRM = 1,  // Обычный пакет c подтвержджением
	PACKET_WITHOUT_CONFIRM,  // Пакет без подтвержджения
	BROADCAST_PACKET, // Широковещательный пакет
	CONFIRM_PACKET, // Пакет подтверждения принятого пакета
};

enum send_packet_states //Состояния отправки пакета
{
	NO_SEND,
	SEND,
	BUSY,
	WAIT_ACCEPT,
	COMPLETE,
};

struct net_packet_data //Структура данных пакета (от нее берется CRC)
{
	uint8_t type; // Тип пакета
	uint8_t to; // Адрес хоста которому необходимо отправить пакет
	uint8_t from; // От кого отправляется пакет
	uint8_t packet_num; // Уникальный номер пакета
	uint8_t assignment; // Назначение пакета (Код пакета). Данное поле определяет логику обработки данного пакета (действие которое будет применено к пакету)
	char content[MAX_DATA_LEN]; // Данные пакета
};

struct net_packet //Структура пакета
{
	char start_key[KEY_LEN];  //Заголовок пакета
	uint16_t CRC; // Контрольная сумма пакета несчитая ключа пакета
	struct net_packet_data data; // Данные пакета (выделенны отдельно чтобы было удобно считать CRC)
};

struct received_packet // Структура элемента очереди с краткой информацией о принятых пакетах достаточной чтобы не принимать один и тотже пакет несколько раз
{
	uint8_t from;
	uint8_t pkg_num;
};

#define MAX_PACKET_LEN sizeof(struct net_packet) // Максимальная длинна пакета

void init_epp(struct epp_lib *init); // Инициализация протокола

// Отправить пакет
// host_addr - куда
// buf - данные
// len - длинна данных
// assignment - Назначение пакета (Код пакета). Данное поле определяет логику обработки данного пакета (действие которое будет применено к пакету)
// pack_num - уникальный номер пакета
// type - Тип пакета (enum packet_types)
void send_packet(uint8_t host_addr, void *buf, uint8_t len, uint8_t assignment, uint8_t pack_num, uint8_t type); 

void transmit_packet(struct net_packet *packet); // Отправить полностью сформированный пакет

void attempt_to_receive_packet(void); //Функция пытается принять пакет. (Один байт за раз) (Функция должна встраиваться в обработчик перывания по появлению данных в UART)

// Отправить пакет в автомат режме
// host_addr - куда
// assignment - Назначение пакета (Код пакета). Данное поле определяет логику обработки данного пакета (действие которое будет применено к пакету)
// buf - данные
// len - длинна данных
int8_t sync_send_packet(uint8_t host_addr, uint8_t assignment, void *buf, uint8_t len);

// Пропинговать устройство
int8_t ping(uint8_t host_addr);

 //Функция должна встраиваться в обработчик таймера с периудом запуска в  0.001 секунду
void inc_epp_timers(void);

void epp_processing(void); //Функция должна встраиваться в основной цикл программы

uint8_t epp_ping(uint8_t host_addr); // Пропинговать устройство

#endif // EPP_H
