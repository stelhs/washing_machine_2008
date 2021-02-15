#ifndef LED_LIB_H
#define LED_LIB_H

typedef uint16_t t_counter; // В структуре таймеров обязательно чтобы все поля структуры были единого размера типа t_counter

void dec_timers(void *timers, int len); // Данная функция встраивается в обработчик таймера и обслуживает все таймеры

struct led
{
    void *led_ddr;
    void *led_port;
    uint8_t led_pin : 3; // Номер пина на котором установлен светодиод
    uint8_t led_state : 1; // Текущее состояние светодиода
    t_counter blink_timer; // Таймер для мигания
    t_counter interval1; // Установленный интервал мигания, 0 - немигать
    t_counter interval2; // Установленный интервал мигания, 0 - мигать с равными интервалами interval1
};

void do_leds(void);
void init_leds(struct led *leds, uint8_t count);

// Установить состояние светодиода
// interval1 и interval2 это время горения и время негорения светодиода
void set_led_state(uint8_t num, uint8_t mode, t_counter interval1, t_counter interval2); 

#endif