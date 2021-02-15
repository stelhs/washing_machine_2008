// Функции для работы с буфером LCD

void gotoxy_lcd_buf(uint8_t x, uint8_t y); // Установить курсор
void clr_lcd_buf(void); // Отчистить дисплей
void putchar_lcd_buf(char chr);
void print_lcd_buf(char *str);
void print_lcd_buf_P(const char *str);
void clear_line_buf(uint8_t line_num);    //Отчистить строку
void fill_line_buf(uint8_t line_num, char ch);    //Залить строку указанным символом строку
void copy_buf_to_lcd(void); // Отправить буфер в LCD

