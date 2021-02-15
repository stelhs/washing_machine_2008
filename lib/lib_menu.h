#ifndef MENU_LIB_H
#define MENU_LIB_H

#include <stdint.h>
#include <avr/pgmspace.h>

enum pointer_types // типы указателей
{
	PROGMEM_POINTER,
	MEMORY_POINTER
};

enum item_types // Возможные типы элемента меню
{
	GROUP, // Группа элементов меню
	RUN_PROCESS, // Переключиться в указанный процесс
	RUN_FUNCTION, // Запустить функцию
	SELECT_PARAM, // Выбора элемента из списка элементов
	CHANGE_NUMBER, // Изменять значение целого числа
};

enum menu_states    //Состояния пользовательского меню
{
    VIEW_LIST,  //Режим просмотра меню
    ITEM_EDIT   // Режим редактирования параметра меню
};

struct menu_item // Описание одного элемента меню
{
	uint8_t id; // Номер элемента меню
	uint8_t parent_id; // Номер родительского элемента меню
	const char *name; // Имя элемента меню
	enum item_types type; // Тип элемента
    uint8_t process_id; // Этот параметр только для типа SWITCH_PROCESS, номер процесса в который нужно переключится
	uint16_t *value; // Указатель на значение которое будет подвергаться изменению
    uint8_t min; // Этот параметр только для типа CHANGE_NUMBER, указывает минимальное значение *value, максимальное равно min + count
	uint8_t count; //Количество значений (это касается типа SELECT_PARAM и CHANGE_NUMBER)
    uint8_t step; // Шаг на который надо менять параметр
	char *postfix; // Используется только для типа CHANGE_NUMBER, это постфикс который будет поставляться после каждого значения
	PGM_P const *select_variants; //Указатель на строки селекта, только для SELECT_PARAM
	uint8_t visible; //Показывать элемент или нет. 1 - элемент отображается, 0 - элемент не отображается
	void (*on_click)(void); // Обработчик события входа в группу элементов или изменения параметра, если ненужен то равен NULL
};

struct lib_menu
{
	struct menu_item *tree; // Дерево меню
	uint8_t count_items; // Количество элементов в дереве меню
	uint8_t display_width; //Ширина экрана в символах
	uint8_t max_width_str; // Максимальная длинна строки в меню
	uint8_t middle_row_num; // Номер строки для активного элемента
	uint8_t count_rows; // Количество строк отводимых под меню

	void (*clear_screen)(void); // Функция для отчистки экрана
	void (*print_header)(const char *); // Функция для отрисовки заголовока меню
	void (*print_item)(uint8_t, uint8_t, void *, uint8_t); // Отрисовать элемент меню
																					// 1 - Номер строки
																					// 2 - тип указателя на строку
																					// 3 - строка с текстом
																					// 4 - Элемент активный или нет
	void (*run_process)(uint8_t); //Функция запускающая процесс
};

void init_menu(struct lib_menu *init); // Проинициализировать меню
void correct_menu_values(void); //Функция проверяет корректность установленных занченией параметров и если те некорректные то обнуляет выставляет их в минимальное состояние
void refresh_menu(uint8_t ignore_id); // Обновить меню (запустить все триггеры)
uint8_t get_center(uint8_t len); // Получить координату в которую необходимо выводить текст, чтобы он оказался в центре
void full_draw_menu(void); // Перерисовать все меню
void disable_menu_item(uint8_t item_id); // Заблокировать узел меню
void enable_menu_item(uint8_t item_id); // Разблокировать узел меню
void invert_menu_item(uint8_t item_id); // Инвертировать доступ к  узлу меню
void set_return_from_menu(uint8_t task_id); // Установить номер процесса в который будетвыход из меню

// Навигация по меню
void menu_move_up(void);
void menu_move_down(void);
uint8_t menu_press_enter(void);
uint8_t menu_press_esc(void);
uint8_t exit_menu(void);

#endif //MENU_LIB_H
