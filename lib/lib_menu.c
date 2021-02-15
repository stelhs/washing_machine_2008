#include "lib_menu.h"
#include <stdio.h>

static struct lib_menu *Menu;

static struct
{
    uint8_t selected_menu_item; // Текущий выбранный элемент
    uint8_t state : 1; // состояние меню VIEW_LIST or ITEM_EDIT
	uint16_t value; // Временное  хранилище редактируемого значения, нужное для того чтобы можно было выйти без сохранения параметров
} Menu_state;

void refresh_menu(uint8_t ignore_id) //Обновить меню
{
	uint8_t i;

    for(i = 0; i < Menu -> count_items; i++) // Запускаем все триггеры для CHANGE_NUMBER и SELECT_PARAM
		switch(Menu -> tree[i].type)
		{
			case SELECT_PARAM:
			case CHANGE_NUMBER:
				if(Menu -> tree[i].id == ignore_id) //Если наткнулись на элемент который рефрешить ненужно
					continue;

				if(Menu -> tree[i].on_click)
					Menu -> tree[i].on_click();
			break;

			case GROUP:
			case RUN_PROCESS:
			case RUN_FUNCTION:
			break;
		}
}

uint8_t get_center(uint8_t len) //Получить смещение для вывода строки в центр экрана
{
    return Menu -> display_width / 2 - len / 2;
}

static uint8_t get_index_by_id(uint8_t item_id) //Получить индекс указанного пункта меню по id
{
    uint8_t i;
    for(i = 0; i < Menu -> count_items; i++)
        if(Menu -> tree[i].id == item_id)
            return i;

    return 0;
}

static uint8_t get_first_item_from_group(uint8_t parent_id) // Получить id первого элемента меню в указанной группе parent_id
{
    uint8_t i;
    for(i = 1; i < Menu -> count_items; i++)
        if((Menu -> tree[i].parent_id == parent_id) && Menu -> tree[i].visible)
            return Menu -> tree[i].id;

	return 0;
}

static uint8_t seach_prev_item(uint8_t item_id) //Функция возвращает предшествующий видимый элемент меню
{
	uint8_t item_id_index = get_index_by_id(item_id);
	uint8_t parent_id, i;

	if(item_id_index <= 0)
		return 0;

	parent_id = Menu -> tree[item_id_index].parent_id;
    for(i = item_id_index - 1; i > 0; i--)
		if(Menu -> tree[i].parent_id == parent_id && Menu -> tree[i].visible)
			return Menu -> tree[i].id;

    return 0;
}

static uint8_t seach_next_item(uint8_t item_id) //Функция возвращает последующий видимый элемент меню
{
	uint8_t item_id_index = get_index_by_id(item_id);
	uint8_t parent_id, i;

	if(item_id_index >= Menu -> count_items - 1)
		return 0;

	parent_id = Menu -> tree[item_id_index].parent_id;
    for(i = item_id_index + 1; i < Menu -> count_items; i++)
		if(Menu -> tree[i].parent_id == parent_id && Menu -> tree[i].visible)
			return Menu -> tree[i].id;

    return 0;
}

static uint8_t draw_list_menu(uint8_t active_item) // Функция отрисовывает элементы меню
{
    uint8_t rc;

	correct_menu_values();

    if(active_item <= 0) // Вывести можно только элементы меню начинающиеся с 1-цы. 0 - Это root элемент меню
        return 1;

    uint8_t view_item;

    Menu -> print_item(Menu -> middle_row_num, PROGMEM_POINTER, (void *)Menu -> tree[get_index_by_id(active_item)].name, 1); // Выводим активный элемент меню

    view_item = active_item;
    for(uint8_t row = Menu -> middle_row_num; row > 0; row--) //Перебираем все элементы которые выше активного
    {
        rc = seach_prev_item(view_item); //Функция возвращает id предыдущего элемента меню или -1 если предыдущего нет.
        if(rc == 0) // Если наткнулись на крайний элемент меню
        {
            Menu -> print_item(row - 1, PROGMEM_POINTER, 0, 0);
            break;
        }

        view_item = rc;
        Menu -> print_item(row - 1, PROGMEM_POINTER, (void *)Menu -> tree[get_index_by_id(view_item)].name, 0); //Выводим название эелемента меню в указанную строку. 0 - это не начало экрана а начало меню
    }

    view_item = active_item;
    for(uint8_t row = Menu -> middle_row_num; row < Menu -> count_rows - 1; row++) //Перебираем все элементы которые ниже активного
    {
        rc = seach_next_item(view_item); //Функция возвращает id предыдущего элемента меню или -1 если предыдущего нет.
        if(rc == 0) // Если наткнулись на крайний элемент меню
        {
            Menu -> print_item(row + 1, PROGMEM_POINTER, 0, 0);
            break;
        }

        view_item = rc;
        Menu -> print_item(row + 1, PROGMEM_POINTER, (void *)Menu -> tree[get_index_by_id(view_item)].name, 0); //Выводим название эелемента меню в указанную строку. 0 - это не начало экрана а начало меню
    }
    return 0;
}

static void full_draw_list_menu(uint8_t active_item) // Функция полностью отрисовывает меню на указанной ветке
{
    Menu_state.state = VIEW_LIST;
    Menu -> clear_screen();
    uint8_t parent_id = Menu -> tree[get_index_by_id(active_item)].parent_id; //Определяем родительский объект
    Menu -> print_header(Menu -> tree[get_index_by_id(parent_id)].name); //Выводим заголовок с названием родительского пункта меню
    draw_list_menu(active_item);
}

void full_draw_menu(void) // Интрерфейсная функция отрисовки меню
{
	full_draw_list_menu(Menu_state.selected_menu_item);
}

static void draw_menu_item(uint8_t active_item) // Функция отрисовывает значение элемента меню на экран
{
    uint16_t view_val, active_val;
	uint8_t active_item_index = get_index_by_id(active_item);
	char str[Menu -> max_width_str];

    switch(Menu -> tree[active_item_index].type) //Предпологается что могут быть еще и другие типы значение параметра меню, кроме SELECT_PARAM
    {
        case SELECT_PARAM: //Если тип элемента меню Select то отрисовываем его значение
            active_val = Menu_state.value;

            Menu -> print_item(Menu -> middle_row_num, PROGMEM_POINTER, (void *)pgm_read_word(&(Menu -> tree[active_item_index].select_variants[active_val])), 1); // Выводим активный элемент меню

            view_val = active_val;
            for(uint8_t row = Menu -> middle_row_num; row > 0; row--) //Перебираем все элементы которые выше активного
            {
                if(view_val >0)
                    view_val--;
                else
				{
					Menu -> print_item(row - 1, PROGMEM_POINTER, 0, 0); //Стираем строку row - 1
                    break;
                }
                Menu -> print_item(row - 1, PROGMEM_POINTER, (void *)pgm_read_word(&(Menu -> tree[active_item_index].select_variants[view_val])), 0); //Выводим название эелемента меню в указанную строку. 0 - это не начало экрана а начало меню
            }

            view_val = active_val;
            for(uint8_t row = Menu -> middle_row_num; row < Menu -> count_rows - 1; row++) //Перебираем все элементы которые выше активного
            {
                if(view_val < (Menu -> tree[active_item_index].count - 1))
                    view_val++;
                else
				{
					Menu -> print_item(row + 1, PROGMEM_POINTER, 0, 0); //Стираем строку row + 1
                    break;
                }
                Menu -> print_item(row + 1, PROGMEM_POINTER, (void *)pgm_read_word(&(Menu -> tree[active_item_index].select_variants[view_val])), 0); //Выводим название элемента меню в указанную строку. 0 - это не начало экрана а начало меню
            }
        break;

        case CHANGE_NUMBER: //Если тип элемента меню целое значение то отрисовываем его значение
            active_val = Menu_state.value;

			snprintf(str, sizeof(str) - 1, "%d %s", Menu_state.value, Menu -> tree[active_item_index].postfix);
            Menu -> print_item(Menu -> middle_row_num, MEMORY_POINTER, str, 1); // Выводим активный элемент меню

            view_val = active_val;
            for(uint8_t row = Menu -> middle_row_num; row > 0; row--) //Перебираем все элементы которые выше активного
            {
                if(view_val > Menu -> tree[active_item_index].min)
                    view_val -= Menu -> tree[active_item_index].step;
                else
				{
					Menu -> print_item(row - 1, PROGMEM_POINTER, 0, 0); //Стираем строку row + 1
                    break;
                }
				snprintf(str, sizeof(str) - 1, "%d %s", view_val, Menu -> tree[active_item_index].postfix);
                Menu -> print_item(row - 1, MEMORY_POINTER, str, 0); //Выводим название эелемента меню в указанную строку. 0 - это не начало экрана а начало меню
            }

            view_val = active_val;
            for(uint8_t row = Menu -> middle_row_num; row < Menu -> count_rows - 1; row++) //Перебираем все элементы которые выше активного
            {
                if(view_val < (Menu -> tree[active_item_index].count * Menu -> tree[active_item_index].step + Menu -> tree[active_item_index].min))
                    view_val += Menu -> tree[active_item_index].step;
                else
				{
					Menu -> print_item(row + 1, PROGMEM_POINTER, 0, 0); //Стираем строку row + 1
                    break;
                }
				snprintf(str, sizeof(str) - 1, "%d %s", view_val, Menu -> tree[active_item_index].postfix);
                Menu -> print_item(row + 1, MEMORY_POINTER, str, 0); //Выводим название элемента меню в указанную строку. 0 - это не начало экрана а начало меню
            }
        break;

		case GROUP:
		case RUN_PROCESS:
		case RUN_FUNCTION:
		break;
    }
}

static void full_draw_menu_item(uint8_t active_item)   //Функция полностью отрисовывает на весь экран элемент меню для дальнейшего изменения его значения
{
    Menu -> clear_screen();
    Menu -> print_header(Menu -> tree[get_index_by_id(active_item)].name); //Выводим название пункта меню
    draw_menu_item(active_item);
}

void disable_menu_item(uint8_t item_id) // Заблокировать элемент меню
{
	uint8_t index = get_index_by_id(item_id);
	Menu -> tree[index].visible = 0;
}

void enable_menu_item(uint8_t item_id) // Разблокировать элемент меню
{
	uint8_t index = get_index_by_id(item_id);
	Menu -> tree[index].visible = 1;
}

void invert_menu_item(uint8_t item_id) // Инвертировать блокировку элемента меню
{
	uint8_t index = get_index_by_id(item_id);
	Menu -> tree[index].visible = !Menu -> tree[index].visible;
}

void correct_menu_values(void) //Функция проверяет корректность установленных занченией параметров и если те некорректные то обнуляет выставляет их в минимальное состояние
{
    uint8_t i;
	for(i = 0; i < Menu -> count_items; i++) // Устанавливаем дефолтные установки всех элементов меню
	{
		switch(Menu -> tree[i].type)
		{
			case CHANGE_NUMBER:
				if((*(Menu -> tree[i].value) < Menu -> tree[i].min) ||  (*(Menu -> tree[i].value) > (Menu -> tree[i].min + Menu -> tree[i].step * Menu -> tree[i].count)))
					*(Menu -> tree[i].value) = Menu -> tree[i].min;
			break;

			case SELECT_PARAM:
				if((*(Menu -> tree[i].value) < 0) ||  (*(Menu -> tree[i].value) >= (Menu -> tree[i].count)))
					*(Menu -> tree[i].value) = 0;
			break;

			case GROUP:
			case RUN_PROCESS:
			case RUN_FUNCTION:
			break;
		}
	}
}

void init_menu(struct lib_menu *init)
{
	Menu = init;

    Menu_state.selected_menu_item = get_first_item_from_group(0);
    Menu_state.state = VIEW_LIST;

	correct_menu_values();
 	refresh_menu(0);
}


void menu_move_up(void) //Вверх
{
    uint8_t rc;
	uint8_t curr_menu_item_index = get_index_by_id(Menu_state.selected_menu_item);

    switch(Menu_state.state)
    {
        case VIEW_LIST: // Если текущий режим просмотр списка элементов меню
            rc = seach_prev_item(Menu_state.selected_menu_item);    // Находим предыдущий элемент меню
            if(rc == 0) // Если ненашли то ничего неделаем
                break;

            Menu_state.selected_menu_item = rc;
            draw_list_menu(Menu_state.selected_menu_item);
        break;

        case ITEM_EDIT: // Если текущий режим - режим редактирования параметра
            switch(Menu -> tree[curr_menu_item_index].type)  // В зависимости от типа параметра, различные действия
            {
				case SELECT_PARAM:
					if(Menu_state.value > 0) //Уменьшаем значение параметра если тот больше минимального
						Menu_state.value -= 1;
				break;

				case CHANGE_NUMBER:
					if(Menu_state.value > Menu -> tree[curr_menu_item_index].min) //Уменьшаем значение параметра если тот больше минимального
						Menu_state.value -= Menu -> tree[curr_menu_item_index].step;
				break;

				case GROUP:
				case RUN_PROCESS:
				case RUN_FUNCTION:
				break;
			}
			draw_menu_item(Menu_state.selected_menu_item);
        break;
    }
}


void menu_move_down(void) //Вниз
{
    uint8_t rc;
	uint8_t curr_menu_item_index = get_index_by_id(Menu_state.selected_menu_item);

    switch(Menu_state.state)
    {
        case VIEW_LIST: // Если текущий режим просмотр списка элементов меню
            rc = seach_next_item(Menu_state.selected_menu_item);    // Находим следующий элемент меню
            if(rc == 0)    // Если ненашли то ничего неделаем
                break;

            Menu_state.selected_menu_item = rc;
            draw_list_menu(Menu_state.selected_menu_item);
        break;

        case ITEM_EDIT: // Если текущий режим - режим редактирования параметра
            switch(Menu -> tree[curr_menu_item_index].type)  // В зависимости от типа параметра, различные действия
            {
				case SELECT_PARAM:
					if(Menu_state.value < (Menu -> tree[curr_menu_item_index].count - 1)) //Увеличиваем значение параметра если тот меньше максимального
						Menu_state.value += 1;
				break;

				case CHANGE_NUMBER:
					if(Menu_state.value < (Menu -> tree[curr_menu_item_index].count * Menu -> tree[curr_menu_item_index].step + Menu -> tree[curr_menu_item_index].min)) //Увеличиваем значение параметра если тот меньше максимального
						Menu_state.value += Menu -> tree[curr_menu_item_index].step;
				break;

				case GROUP:
				case RUN_PROCESS:
				case RUN_FUNCTION:
				break;
			}
			draw_menu_item(Menu_state.selected_menu_item);
        break;
    }
}

uint8_t menu_press_enter(void) // Ввод
{
	uint8_t curr_menu_item_index = get_index_by_id(Menu_state.selected_menu_item);
    switch(Menu_state.state)
    {
        case VIEW_LIST: // Если находимся в режиме отображения параметров
            switch(Menu -> tree[curr_menu_item_index].type)  // В зависимости от типа параметра, различные действия
            {
                case GROUP:
					if(Menu -> tree[curr_menu_item_index].on_click) // Если установлен обработчик события перед входом в группу, то запускаем его
						Menu -> tree[curr_menu_item_index].on_click();

                    Menu_state.selected_menu_item = get_first_item_from_group(Menu_state.selected_menu_item); // Открыть группу элементов меню (получаем id первого элемента меню в группе)
                    full_draw_list_menu(Menu_state.selected_menu_item);
                break;

                case RUN_PROCESS:
					if(Menu -> tree[curr_menu_item_index].on_click) // Если установлен обработчик события перед запуском процесса, то запускаем его
						Menu -> tree[curr_menu_item_index].on_click();

                    Menu -> run_process(Menu -> tree[curr_menu_item_index].process_id); // Переключиться в другой процесс
					Menu_state.selected_menu_item = get_first_item_from_group(0);
					Menu_state.state = VIEW_LIST;
                break;

                case RUN_FUNCTION:
					if(Menu -> tree[curr_menu_item_index].on_click) // Если установлен обработчик
						Menu -> tree[curr_menu_item_index].on_click();

					return 1; // Возвращаем флаг выхода из меню
                break;

                case SELECT_PARAM:
                case CHANGE_NUMBER:
					Menu_state.value = *(Menu -> tree[curr_menu_item_index].value);
                    Menu_state.state = ITEM_EDIT; // Переходим в режим редактирования значения
                    full_draw_menu_item(Menu_state.selected_menu_item);
                break;
            }
        break;

        case ITEM_EDIT: //Выходим в меню из режима редактирования с сохранением параметров
			*(Menu -> tree[curr_menu_item_index].value) = Menu_state.value;
			if(Menu -> tree[curr_menu_item_index].on_click) // Если установлен обработчик события после редактированя параметра, то запускаем его
				Menu -> tree[curr_menu_item_index].on_click();

            Menu_state.state = VIEW_LIST;
            full_draw_list_menu(Menu_state.selected_menu_item);
        break;
    }

	return 0;
}

uint8_t menu_press_esc(void) // Выход
{
    switch(Menu_state.state)
    {
        case VIEW_LIST:
            if(Menu -> tree[get_index_by_id(Menu_state.selected_menu_item)].parent_id > 0) //Если не вышли в корень то поднимаемся на уровень выше
            {
                Menu_state.selected_menu_item = Menu -> tree[get_index_by_id(Menu_state.selected_menu_item)].parent_id;
                full_draw_list_menu(Menu_state.selected_menu_item);
            }
			else //если нажали на ESC из корня то возвращаемся в предыдущий процесс
				return 1;  // Возвращаем флаг выхода из меню
        break;

        case ITEM_EDIT: //Выходим в меню из режима редактирования
            Menu_state.state = VIEW_LIST;
            full_draw_list_menu(Menu_state.selected_menu_item);
        break;
    }

	return 0;
}
