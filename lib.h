#ifndef LIB_H
#define LIB_H

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "drivers.h"
#include "button.h"
#include "lib_liveos.h"// Описание Микро ОС


#define INT_TO_STR_LEN 6 //Длинна строки для преобразования uint16_t -> char*

// Вывести значение переменной
#define print_var(var) { console_out_str_P(PSTR(#var " = ")); itoa(var, itoa_buffer, 10); console_out_str(itoa_buffer); console_out_str_P(PSTR("\r\n")); }

enum sensor_levels_water_in_tank // Состояния датчика уровня воды в баке
{
    TANK_IS_EMPTY, // Бак пустой
    SENSOR_WATER_LEVEL1, // Бак наполнен водой до первого уровня
    SENSOR_WATER_LEVEL2, // Бак наполнен водой до второго уровня
};

enum rotating_steps // Этапы вращения барабана
{
    ROTATE_LEFT,    // Вращаем против часовой
    MIDDLE_PAUSE,   // Пауза
    ROTATE_RIGHT, // Вращаем по часовой
    ROTATE_PAUSE // Пауза после одной итерации
};

enum wringing_steps // Этапы процесса отжима
{
    SHAKE_DRESS,    // Встряхнуть белье
    SHAKE_DRESS_STOPING,   // Выдерживаем небольшое время для полной остановки белья
	TRY_TO_BALANCING,	// Попытка грамотно сбалансировать белье
	INCRASE_WRING_SPEED, // Постепенный набор скорости
    WRINGING_PROCESS,   // Процесс отжима
	WAIT_FOR_REPEAT, // Ожидаем остановку барабана для повтора процесса разложить белье
	RINSE_DRESS, // Всполоснуть белье если то нехочет балансироваться
};

enum wring_return_codes // Результаты отжима
{
	NO_WRINGED, // Отжим не осуществлен
	WRINGED_ERROR, //Отжим небыл осуществлен по причине высокого дисбаланса
	WRINGED_OK, //Осуществлен отжим на запланированной скорости
	WRINGED_LOW_SPEED, // Осуществлен отжим на меньшей скорости но выше минимальной
	WRINGED_MIN_SPEED, // Осуществлен отжим на минимальной скорости
	NORMAL_SPEED_SET, // Осуществленно раскручивание барабана на заданную скорость, барабан продолжает крутиться
};

enum rinse_steps // Этапы процесса полоскания
{
	RINSE_FILL_TANK, // Этап заполнения бака водой
	RINSE_ROTATE, // Этап полоскания белья
	RINSE_POUR_WATER, // Этап слива воды
	RINSE_WRING, // Этап отжима
};

enum stir_powder_steps // Этапы размешивания порошка
{
	BALANCING_AND_ROTATING, //Балансируем и раскручиваем белье до нужной скорости
	WAIT_END_ROTATING, // Дожидаемся завершения размешивания порошка
	WAIT_FILL_ON_LEVEL1, //Дожидаемся заполения водой до первого уровня
	WAIT_FILL_TANK, // Ожидаем наполнение бака водой
};

enum led_states // Вывод сотояния на светодиоды
{
	LED_OFF, // Все светодиоды выключенны
	LED_PAUSE, // Состояние светодиодов во время паузы

	// Состояния светодиодов на этапах работы машины
	LED_BEDABBLE,  // Зеленый 1 горит
	LED_PREV_WASHING, // Зеленый 1 мигает
	LED_WASHING, // Зеленый 2 горит
	LED_RINSE, // Зеленый 2 мигает
	LED_WRING, // Зеленый 3 горит
	LED_FINISH, // Зеленый 3 мигает

	LED_ERROR1, // Состояние светодиодa1 во время ошибки на этапах LED_BEDABBLE или LED_PREV_WASHING
	LED_ERROR2, // Состояние светодиодa2 во время ошибки на этапах LED_WASHING или LED_RINSE
	LED_ERROR3, // Состояние светодиодa3 во время ошибки на этапе LED_WRING
	LED_FATAL_ERROR, // При возникновении серьездной ошибки все светодиоды мигают крастным
};

enum sys_error_types // Типы ошибок
{
	HARDWARE_ERROR_1 = 1, // Ошибка при возникновении которой система блокируется и сливает воду
	HARDWARE_ERROR_2, // Ошибка при возникновении которой система блокируется и не сливает воду
	EXTERNAL_ERROR, // Внешняя ошибка приводящая к оставновке всех процессов
	ERROR, // Ошибка которая не приводит к остановке стирки
};

struct rotating_mode // Структура описывающая один метод вращения барабана
{
	uint16_t left_rotate_speed; // Скорость вращения барабана влево
	uint16_t left_rotate_time; // Время вращения барабана влево
	uint16_t middle_pause_time; // Время пауза между вращениями
	uint16_t right_rotate_speed; // Скорость вращения барабана вправо
	uint16_t right_rotate_time; // Время вращения барабана вправо
	uint16_t pause_time;  // Время паузы по завершению метода
};

enum rotating_mode_names // Методы вращения барабана
{
	BEDABBLE_ROTATING,	// Вращение барабана для режима замачивание
	COAT_ROTATING, // Вращение барабана для стирки шерсти
	INTENSIVE_COAT_ROTATING, // Вращение барабана для интенсивной стирки шерсти
	RINSE_COAT_ROTATING, // Вращение барабана для полоскания шерсти
	NORMAL_ROTATING, // Обычное вращение барабана
	INTENSIVE_ROTATING, // Интенсивное вращение барабана
};

enum control_EPP_water_valves // Варианты контроля удаленных вентилей подачи воды
{
	CONTROL_EPP_WATER_COLD, // Управление только холодной водой
	CONTROL_EPP_WATER_HOT, // Управление только горячей водой
	CONTROL_EPP_WATER_COLD_OR_HOT, // Управление холодно и горячей водой автоматически
	CONTROL_EPP_WATER_DISABLE, // контроль удаленных вентилей отключен
};

struct machine_timers// Всевозможные счетчики для таймеров
{
	uint8_t delay_next_impulse; // Счетчик времени между импульсами с тахо генератора, нужен для ограничения максимального количества импульсов в еденицу времени
    uint16_t level1_time; //Таймер для отсчета времени за которое вода наберется до первого уровня
	uint8_t fill_tunk_timer; //Таймер для отсчета времени для заполнения бака водой до расчетного уровня
    uint8_t rotating_drum_timer; // Таймер отсчитывающий время вращения барабана и промежутки между вращениями
	uint8_t pour_out_water_timer; // Таймер отсчитывающий время слива воды после достижения нулевого уровня воды в баке
	uint16_t speed_change_timer; // Таймер для отсчета интервалов для изменения скорости вращения барабана
	uint16_t wring_timer; //Таймер отсчитывающий время поолскания
	uint16_t stir_powder_timer; //Таймер отсчитывающий время размешивания порошка
	uint16_t blink_sound_speaker; //Таймер отсчитывающий время пищпния/молчания динамика
};

struct machine_states
{
	uint8_t main_power : 1; // Бит выставляется в 1 если машина находится во включенном состоянии
	uint8_t pause : 1; // //Бит выставляется в 1 в случае паузы
	uint8_t error : 3; // Данный флаг отображает факт любой ошибки
	uint8_t lock : 1; // Заблокировать все основные операции
	uint8_t change_current_time : 1; //Бит выставляется в 1 когда изменяется время
	uint8_t speaker_freq; // Частота звукового сигнала пищалки
	uint8_t blink_speaker_freq; // Частота звукового сигнала пищалки для режима прерывистого пищания
	uint16_t blink_beep_up; // Это для пищалки. Время пищания
	uint16_t blink_beep_down; // Это для пищалки. Время тишины
	uint8_t blink_sound_count_beep; //Установленное количество пищаний
	uint8_t blink_beep_on : 1; //Включить или выключить периодическое пищание

	uint8_t state_door; //Состояние дверки, если число больше нуля значит дверка закрыта
	uint8_t ext_power; //Наличие 220V, если число больше нуля значит 220V присутсвует

	uint8_t full_tank_count_error : 5; // Максимальное количество срабатываний сигнала FULL_TANK для того чтобы принять решение что бак заполнен до максимума
	uint8_t current_adc_channel : 2; //Текущий канал ADC для захвата
	uint8_t adc_success : 1; //Если преобразование завершено выставляется данный флаг
	uint8_t refresh_rotate_speed : 1; // Если информация о скорости обновилась то выставляем этот флаг
	uint8_t enable_full_motor_power : 1; // Если данный бит устанавливается в 1то мотор включается в сеть 220V без ШИМ

    uint8_t level_water_in_tank : 2; // Состояние воды в баке по датчику уровня
    uint8_t need_water_level; //Требуемый уровень воды в баке, 1 - до первой метки, все что выше 1ой метки это число * 0.5 первой метки
	uint8_t need_water_source; //Источник забора воды
    uint8_t run_fill_tank : 1; //Если происходит набор воды то этот флаг выставляется в 1
	uint8_t time_fill_on_level1; //Время за которое бак наполнился до первого уровня
	uint8_t complete_fill_on_level1 : 1; // Бит устанавливается в 1 если уровень воды достиг 1ого уровня
	uint8_t tank_fill_finish : 1; //Флаг устанавливается в 1 если процесс заполнения бака водой завершон
	uint8_t pour_out_water : 1; // Если запущен слив воды то данный флаг выставляется в 1
	uint8_t pour_out_finish : 1; // Флаг выставляется в 1 если процесс слива воды завершон

    uint16_t need_temperature; // Система самостоятельно занимается доведением воды до нужной температоуры
	uint16_t demand_temperature; // Температура воды которую необходимо достигнуть после набора воды
    uint8_t heating_water : 1; // Флаг устанавливается в 1 если вода в настоящий момент нагревается

    uint8_t drum_is_runing : 1; //Бит устанавливается в 1 если барабан находится в состоянии вращения
	uint8_t drum_is_runing_transmission : 1; // Передача на которой запущенно вращение в настоящий момент
	uint8_t drum_is_runing_direction : 1; // Установленное направление вращения
    uint8_t drum_is_stopping : 1; //Бит устанавливается в 1 если барабан находится в состоянии остановки
    uint8_t drum_is_stopped : 1; //Бит устанавливается в 1 если барабан уже остановился
    uint16_t need_speed_rotating; // Требуемая скорость вращения

    uint8_t runing_rotating : 1; // Если данный бит установлен в 1 то значит запущен режим врашения барабана
    uint8_t rotating_mode : 4; // Тип вращения барабана.
    uint8_t current_rotating_step : 4; // Этап вращения барабана одной итерации вращения (Одна итерация вращения барабана включает в себя 4ре шага описанных в перечислении rotating_steps)

	uint16_t wring_speed; //Максимальная скорость отжима
	uint8_t running_wring : 1; // Если запущен режим отжима
	uint8_t running_wring_step : 4; //Текущий этап процесса отжима
	uint8_t wring_easy_mode : 1; // Упрощенный вариант запуска отжима (при котором не используются полоскания и ниже кретерии к уровню дисбаланса)
	uint8_t wring_speed_up_attempt; //Количество попыток раскрутить белье при отжиме
	uint8_t wring_balancing_attempt; //Количество попыток сбалансировать белье
	uint8_t count_wring_rinse_attempt; //Количество попыток сполоснуть белье для очередной попытки балансировки
	uint16_t wring_time; // Желаемое время отжима
	uint8_t wring_is_min_speed : 1; // Бит устанавливается в 1 если скорость отжима была ниже минимальной
	uint8_t wring_finish : 3; // Бит неравен 0 если отжим завершен. Бит сигнализирует о том как был завершон отжим (см. enum wring_return_codes)
	uint8_t wring_display_status : 3; //Номер текущего состояния отжима для отображения на экране
	uint8_t wring_vibration_criterion; //Текущий критерий прохождения теста дисбаланса

	uint8_t running_rinse : 1;  //Запущенно полоскание
	uint8_t running_rinse_step : 4; // Текущий этап процесса полоскания
	uint8_t rinse_count_wring_error : 2; // Количество ошибок отжима при полоскании
	uint8_t rinse_source_valve : 2; // Источник забора воды для полоскания
	uint8_t rinse_type_rotating : 4; // Тип вращения барабана при полоскании
	uint8_t rinse_finish : 1;  // Бит устанавливается если полоскание было завершенно
	uint8_t rinse_display_status : 3; //Номер текущего состояния полоскания для отображения на экране

	uint8_t running_stir_powder : 1; //Запущенно наполнение бака водой и размешивание порошка
	uint8_t running_stir_powder_step : 3; // Текущий этап процесса размешивания порошка
	uint8_t stir_powder_water_level; //Требуемый уровень воды с размешанным порошком
	uint8_t stir_powder_water_source: 2; //Источник забора порошка с водой
	uint16_t stir_powder_water_temperature; //Требуемая температура воды с порошком
	uint8_t stir_powder_finish : 1; // Бит устанавливается если размешивание порошка было завершенно
	uint8_t stir_powder_display_status : 3; //Номер текущего состояния размешивания порошка для отображения на дисплее.

	uint8_t current_machine_state :  4; //Текущее состояние стиральной машины, для отображения светодиодами
	uint8_t current_net_valve_state : 2; // Текущее состояние удаленных вентилей на подачу воды к стиральной машине. Возможные состояния воды: Горячая/Холодная/Закрыто
	uint8_t current_net_valve_state_received : 1; // Бит выставляется в 1 в случае успешного обновления значения current_net_valve_state
};

struct washing_settings //Настройки стирки которые изменяются из меню
{
	uint16_t bedabble_time; // Время замачивания (select)

	uint16_t enable_prev_washing; // Предварительная стирка включенна или выключенна
	uint16_t signal_swill; // Сигнал оповещения последнего ополаскивания включен или нет
	uint16_t washing_preset; // Выбранный пресет стирки  (select)
	uint16_t washing_water_level; // Количество воды
	uint16_t washing_time_part1; // Выбранное время стирки в минутах для первой фазы
	uint16_t temperature_part1; // Выбранная температура (select) для первой фазы
	uint16_t enable_washing_part2; // Разрешить использование второй фазы или нет
	uint16_t washing_time_part2; // Выбранное время стирки в минутах для второй фазы
	uint16_t temperature_part2; // Выбранная температура (select) для второй фазы
	uint16_t count_rinses; // Количество полосканий
	uint16_t wring_speed; // Скорость отжима в об./мин.
	uint16_t wring_time; // Время отжима в минутах
	uint16_t washing_mode; // Режим стирки (select)
	uint16_t bedabble_water_soure; // Источник порошка для замачивания
	uint16_t prev_washing_water_soure; // Источник порошка для предварительной стирки
	uint16_t washing_water_soure; // Источник порошка для стирки

	uint16_t rinse_mode; // Тип полосканий (нормальный или бережный) для программы полоскание
	uint16_t rinse_water_soure; //Источник воды для программы полоскание
	uint16_t rinse_count; // Количество полосканий для программы полоскание

	uint16_t wring_wring_speed; //Скорость отжима для программы отжим
	uint16_t wring_wring_time; // Время отжима для программы отжим
};


struct machine_settings  //Глобальные настройки стиральной машины которые изменяются из меню
{
	uint16_t sound; // Звук включен или выключен
	uint16_t input_water; // режим подачи воды (select)
	uint16_t epp_voice_message; // Разрешить или нет оглошать голосом о событиях используя сеть EPP и сервер
};

//Включить наполнение бака водой до указанного уровня,
// level - Требуемый уровень воды. Если уровень воды равен 1 то это значит набрать воду до первого срабатывания датчика уровня. Все что выше 1 означает наполнение водой бака до уровня в (1 + (level - 1) / 2) .
// water_source - источник забора воды,
// temperature - требуемая температура воды, 0 - означает что нагревать воду неследует
uint8_t start_fill_tank(uint8_t level, uint8_t water_source, uint16_t temperature);

void stop_fill_tank(void); // Остановить заполнение водой

void pour_out_water(void); // Запустить слив воды

void stop_pour_out_water(void); // Остановить слив воды

void check_water_level(void); // Функция встраивается в основной цикл и проверяет текущий уровень воды

void start_mode_rotating(enum rotating_mode_names mode); //Функция запускает циклическое вращение барабана с указанным типом вращения

void stop_mode_rotating(void); // Остановить циклическое вращение барабана

uint8_t enable_rotate_drum(uint8_t transmission, uint16_t speed, uint8_t direction); //Включить вращение барабана с заданной скоростью

void disable_rotate_drum(void); // Выключить вращение барабана

void change_transmission(uint8_t transmission); // Переключиться на указанную передачу при вращающемся барабане

void rotate_drum_if_need(void); // Функция встраивается в основной цикл и занимается вращениями барабана если rotating_mode > 0

void run_wring(uint16_t max_speed, uint16_t time, uint8_t mode); // Запуск отжима, передается максимальная скорость вращения и желаемое время отжима

void stop_wring(void); // Остановить отжим

void process_wringing(void); //Функция встраивается в основной цикл и следит за процессом отжима

void run_rinse(uint8_t type_rotating, uint8_t water_source); //Запустить полоскание

void stop_rinse(void); //Остановить полоскание

void process_rinse(void); // Функция встраивается в основной цикл и следит за процессом полоскания

void start_fill_tank_and_stir_powder(uint8_t water_level, uint8_t water_source, uint16_t water_temperature); //Запустить заполнение бака водой и размешивание порошка

void stop_fill_tank_and_stir_powder(void); //Остановить заполнение бака водой и размешивание порошка

void process_stir_powder(void); // Функция встраивается в основной цикл и следит за процессом размешивания порошка

int16_t calculate_PD(uint16_t need_speed, uint16_t speed); // Расчет ПД стабилизатора

uint16_t integrator_queue(int16_t *queue, uint16_t insert_val, uint16_t queue_len); // Добавить в буфер queue значение insert_val и получить усредненное значение с буфера

void led_indicate_state(uint8_t state); //Отобразить текущее состояние стиральной машины светодиодами

void inc_time(struct time *t); // увеличить время 't'  на еденицу

void time_add(struct time *t, uint16_t min); // Прибавить к времени 't', указанное количество минут

void sys_error(uint8_t error_type, const char *error_message); //Выдать ошибку

void power_on_system(void); //Включение всей силовой системы

void power_off_system(void); //Выключение всей силовой системы

void turn_speaker(uint8_t freq); //Включить звуковой сигнал

void turn_blink_speaker(uint8_t power, uint16_t time_up, uint16_t time_down, uint8_t freq, uint8_t count_beep); // Включить периодическое пищание пи-пи-пи...

void do_blink_speaker(void); //Функция встраивается в основной цикл, и осуществляет периодическое пищание

void save_all_settings(void); //Сохранить все настройки

void load_all_settings(void); //Восстановить настройки

int8_t get_net_water_state(void); // Получить состояние удаленных вентилей через протокол EPP. Результат получения информации о вентилях, автоматом сохранится в Machine_states.current_net_valve_state о чем будет свидетельствовать флаг Machine_states.current_net_valve_state_received

int8_t turn_net_water_valve(uint8_t water_type); // Включить или выключить воду на стиралку. Флаг Machine_states.current_net_valve_state_received известит о удачном выполнении функции

void request_water(uint8_t water_type); // Попытаться запросить воду

int8_t request_server_time(void); // Запросить текущее время и дату

extern struct button Buttons[]; //Массив состояния всех кнопок
extern struct machine_states Machine_states; // Состояние всей машины
extern struct machine_settings Machine_settings; // Глобальные настройки стиральной машины
extern struct washing_settings Washing_settings; // Нстройки регулируемые с меню
extern const char *Error_message; // В случе возникновения ошибки, сюда присваивается текст ошибки
extern struct time Main_current_time; // Часы реального времени
extern struct machine_timers Machine_timers; // Счетчики всех основных таймеров

#endif //LIB_H
