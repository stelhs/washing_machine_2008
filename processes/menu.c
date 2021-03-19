#include <stdio.h>
#include "../lib.h"
#include "../messages.h"
#include "../os.h"
#include "../../../lib/lib_liveos.h"
#include "../../../lib/lib_menu.h"
#include "config.h"
#include "../lcd_buffer.h"
#include "proc_lib.h"

extern struct lib_menu Lib_menu; //Настройки библиотеки меню
extern struct machine_states Machine_states; // Состояние всей машины

struct
{
	uint8_t return_task_id; //Номер процесса куда необходимо реализовать возврат с меню
}States_menu;


CALLBACK_NAME(PROCESS_MENU, init)(void) // Инициализация процесса стирки
{
	States_menu.return_task_id = DEFAULT_PROCESS;
}

CALLBACK_NAME(PROCESS_MENU, run)(char *params)
{
	init_menu(&Lib_menu);  // Инициализируем меню
}

CALLBACK_NAME(PROCESS_MENU, active)(struct task_info *from)
{
    printf("menu: active from = %d\r\n", from -> from_task_id);
    States_menu.return_task_id = from -> from_task_id; // Сохраняем номер процесса для возврата в него по выходу из меню
	full_draw_menu();
}

CALLBACK_NAME(PROCESS_MENU, main_loop)(void)
{
}

CALLBACK_NAME(PROCESS_MENU, background)(uint8_t is_active)
{
}

CALLBACK_NAME(PROCESS_MENU, click_up)(void)
{
    menu_move_up();
}

CALLBACK_NAME(PROCESS_MENU, click_down)(void)
{
    menu_move_down();
}

CALLBACK_NAME(PROCESS_MENU, click_esc)(void)
{
	int rc = menu_press_esc();
	if (rc) {
        printf("menu: exit to %d\r\n", States_menu.return_task_id);
        switch_process(States_menu.return_task_id);
	}

}

CALLBACK_NAME(PROCESS_MENU, click_enter)(void)
{
	menu_press_enter();
}

CALLBACK_NAME(PROCESS_MENU, click_ext1)(void)
{
}

CALLBACK_NAME(PROCESS_MENU, timer2)(void)
{

}

CALLBACK_NAME(PROCESS_MENU, console)(char ch)
{
	switch(ch)
	{
		case '8':
			menu_move_up();
		break;

		case '5':
			menu_move_down();
		break;

		case '6':
			menu_press_enter();
		break;

		case '4':
			menu_press_esc();
		break;

		case '0':
            switch_process(States_menu.return_task_id);
		break;
	}
}
