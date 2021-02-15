#define AUTO_POWER_OFF_TIMEOUT 60*5 //Время автоматического выключения питания машины если та неактивна


// Настройки режима замачивания
#define BEDABBLE_WATER_LEVEL  4 //Уровень воды 
#define BEDABBLE_TEMPERATURE 30 //Температура воды для замачивания (в градусах)
#define BLINK_TIME 300 //Время мигания

//Настройки режима стирка
#define PREV_WASHING_TEMPERATURE 40 //Температура воды для предварительной стирки (в градусах)
#define PREV_WASING_TIME 60*20 //Время предварительной стирки в секундах
#define WASHING_WRINGS_COUNT 1 //Количество отжимов
#define STIR_DRESS_TIME 60 // Время разрыхления белья после отжима
#define TIMEOUT_WAIT_LAST_SWILL (60 * 5) // Время ожидания между сигналами о последнем полоскании
#define TIMEOUT_WAIT_LAST_SWILL_COUNT 6 //Количество интервалов TIMEOUT_WAIT_LAST_SWILL перед последним полосканием


// Настройки "управления водой"
#define BLINK_PRESSED_BUTTON_SPEED 300 //Периодичность мигания активной строчки 
#define KEY_UP_DELAY 100 //Время через которое сработает отпускание кнопки

// Настройки вывода ошибки
#define SCROLL_TEXT_SPEED 250 // Пошаговая задержка смещения текста ошибки влево
#define SCROLL_TEXT_START_SPEED 1500 // // Первая пошаговая задержка смещения текста ошибки влево

// Адреса в EEPROM для каждого процесса
#define EEPROM_ADDRESS_POWER_MGR 0 //Адрес куда писать параметры процесса PROCESS_POWER_MANAGER
#define EEPROM_ADDRESS_MENU_SETTINGS 20 //Адрес куда писать настройки меню

