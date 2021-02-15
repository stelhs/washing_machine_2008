#ifndef PROCESS_PROC_LIB_H
#define PROCESS_PROC_LIB_H

#include <stdio.h>
#include "../lib.h"

uint16_t conv_temperature_to_adc_val(uint8_t temperature); //Функция конвертации температуры в значение с ADC преобразователя

void draw_current_time(struct time *t, uint8_t need_redraw); //Отрисовать реальное время

void draw_current_date(struct time *t, uint8_t need_redraw); //Отрисовать текущую дату

void draw_work_time(struct time *t, uint8_t need_redraw); //Отрисовать время работы

void draw_center_pointer(void); // Отрисовать стрелочку в центре

void draw_calculate_time(struct time *t, uint8_t need_redraw); //Отрисовать расчетное время

void draw_prog_name(const char *name, uint8_t need_redraw); //Отрисовать название программы

void draw_close_door(uint8_t need_redraw); //Отрисовать текст ожидания закрытия люка

void draw_step_name(const char *name, uint8_t step_num, uint8_t count_step, uint8_t need_redraw); //Отрисовать название и номер текущего шага выполнения

void draw_operation_name(const char *name, uint8_t need_redraw); //Отрисовать название текущей операции

void draw_pause(uint8_t need_redraw); //Отрисовать название программы

void triggered_menu_items(uint8_t *list, uint8_t count);

#endif //PROCESS_PROC_LIB_H
