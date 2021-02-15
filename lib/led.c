#include <stdint.h>
#include "led.h"

void dec_timers(void *timers, int len) // Р”Р°РЅРЅР°СЏ С„СѓРЅРєС†РёСЏ РІСЃС‚СЂР°РёРІР°РµС‚СЃСЏ РІ РѕР±СЂР°Р±РѕС‚С‡РёРє С‚Р°Р№РјРµСЂР° Рё РѕР±СЃР»СѓР¶РёРІР°РµС‚ РІСЃРµ С‚Р°Р№РјРµСЂС‹
{
	uint8_t count = len / sizeof(t_counter);
	
	for(; count > 0; --count)
		if(((t_counter *)timers)[count - 1] > 1)
			((t_counter *)timers)[count - 1]--;
}

struct // РЎС‚СЂСѓРєС‚СѓСЂР° РґР»СЏ СЂР°Р±РѕС‚С‹ СЃРѕ СЃРІРµС‚РѕРґРёРѕРґР°РјРё
{
	struct led *leds; // РЈРєР°Р·Р°С‚РµР»СЊ РЅР° РјР°СЃСЃРёРІ СЃРІРµС‚РѕРґРёРѕРґРѕРІ
	uint8_t count; // РљРѕР»РёС‡РµСЃС‚РІРѕ СЃРІРµС‚РѕРґРёРѕРґРѕРІ РІ РјР°СЃСЃРёРІРµ
}Lib_leds;

static void turn_led(uint8_t num, uint8_t mode) // Р’РєР»СЋС‡РёС‚СЊ РёР»Рё РІС‹РєР»СЋС‡РёС‚СЊ СЃРІРµС‚РѕРґРёРѕРґ
{
	if(mode) // Р•СЃР»Рё СЃРІРµС‚РѕРґРёРѕРґ РЅСѓР¶РЅРѕ Р·Р°Р¶РµС‡СЊ
		*((uint8_t *)Lib_leds.leds[num].led_port) |= (1 << Lib_leds.leds[num].led_pin);
	else // РёР»Рё РїРѕС‚СѓС€РёС‚СЊ
		*((uint8_t *)Lib_leds.leds[num].led_port) &= ~(1 << Lib_leds.leds[num].led_pin);
		
	Lib_leds.leds[num].led_state = mode;
}



void do_leds(void)
{
	uint8_t i;
    for(i = 0; i < Lib_leds.count; i++)
    {
        if(Lib_leds.leds[i].blink_timer > 1)
			Lib_leds.leds[i].blink_timer--;
			
        if(Lib_leds.leds[i].interval1 && Lib_leds.leds[i].blink_timer == 1) // Р•СЃР»Рё С‚Р°Р№РјРµСЂ РёСЃС‚РµРє
		{
			if(Lib_leds.leds[i].led_state) // Р