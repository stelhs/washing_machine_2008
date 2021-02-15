#include "button.h"
#include <string.h>

static struct lib_button *Lib_button;

void init_lib_button(struct lib_button *init)
{
	uint8_t i;
	Lib_button = init;
	for(i = 0; i < Lib_button -> count_buttons; i++)
		clear_key_data(Lib_button -> buttons, i);
}

void change_timer_buttons(void) // Р¤СѓРЅРєС†РёСЏ РІСЃС‚СЂР°РёРІР°РµС‚СЃСЏ РІ РѕР±СЂР°Р±РѕС‚С‡РёРє С‚Р°Р№РјРµСЂР°
{
	uint8_t button;
	
	for(button = 0; button < Lib_button -> count_buttons; button++)
	{
		if(Lib_button -> buttons[button].timer_bounce_down > 1)
			Lib_button -> buttons[button].timer_bounce_down--;
	
		if(Lib_button -> buttons[button].timer_bounce_up > 1)
			Lib_button -> buttons[button].timer_bounce_up--;
			
#ifdef BUTTONS_SCAN_CODE
		if(Lib_button -> buttons[button].timer1 > 1 && !Lib_button -> buttons[button].timer2 && !Lib_button -> buttons[button].timer3)
			Lib_button -> buttons[button].timer1--;
			
		if(Lib_button -> buttons[button].timer2 > 1 && !Lib_button -> buttons[button].timer3)
			Lib_button -> buttons[button].timer2--;
			
		if(Lib_button -> buttons[button].timer3 > 1)
			Lib_button -> buttons[button].timer3--;
			
		if(Lib_button -> buttons[button].timer_hold > 1)
			Lib_button -> buttons[button].timer_hold--; 
#endif
	}
}

void clear_unused_key_code(void) // РћС‡РёС‰Р°РµС‚ РґР°РЅРЅС‹Рµ Рѕ РЅР°Р¶Р°С‚РёСЏС… РєРЅРѕРїРѕРє
{
	uint8_t button;
	
	for(button = 0; button < Lib_button -> count_buttons; button++)
	{
		Lib_button -> buttons[button].state.one_click = 0;
		Lib_button -> buttons[button].state.double_click = 0;
		Lib_button -> buttons[button].state.prev_hold = Lib_button -> buttons[button].state.hold;
	}
}

void clear_key_data(struct button *buttons, int button) //РћС‡РёСЃС‚РёС‚СЊ РІСЃСЋ РёРЅС„РѕСЂРјР°С†РёСЋ Рѕ РєРЅРѕРїРєРµ button
{
	buttons[button].timer_bounce_down = 0;
	buttons[button].timer_bounce_up = 0;
	buttons[button].timer1 = 0;
	buttons[button].timer2 = 0;
	buttons[button].timer3 = 0;
	buttons[button].timer_hold = 0;
	buttons[button].hold_multiplier = 1;
	buttons[button].state.state_button = 0;
	buttons[button].state.one_click = 0;
	buttons[button].state.double_click = 0;
	buttons[button].state.hold  = 0;
}

void get_buttons(void)	//РѕРїСЂРѕСЃ СЃРѕСЃС‚Р°СЏРЅРёСЏ РєРЅРѕРїРѕРє
{
	for(uint8_t i = 0; i < Lib_button -> count_buttons; i++)
	{
		if((!(*(Lib_button -> buttons[i].button_port) & (1 << Lib_button -> buttons[i].button_pin))) && !Lib_button -> buttons[i].state.state_button && !Lib_button -> buttons[i].timer_bounce_down) // Р•СЃР»Рё РїРѕСЏРІРёР»СЃСЏ РёРјРїСѓР»СЊСЃ РЅР°Р¶Р°С‚РёСЏ РєРЅРѕРїРєРё
			Lib_button -> buttons[i].timer_bounce_down = Lib_button -> contact_bounse;
			
		if((*(Lib_button -> buttons[i].button_port) & (1 << Lib_button -> buttons[i].button_pin)) && !Lib_button -> buttons[i].state.state_button && Lib_button -> buttons[i].timer_bounce_down > 1) // Р•СЃР»Рё РїРѕСЏРІРёР»СЃСЏ РёРјРїСѓР»СЊСЃ РѕС‚РїСѓСЃРєР°РЅРёСЏ РєРЅРѕРїРєРё
			Lib_button -> buttons[i].timer_bounce_down = 0;
			
		if((!(*(Lib_button -> buttons[i].button_port) & (1 << Lib_button -> buttons[i].button_pin))) && !Lib_button -> buttons[i].state.state_button && Lib_button -> buttons[i].timer_bounce_down == 1) // Р•СЃР»Рё РёРјРїСѓР»СЊСЃ РЅР°Р¶Р°С‚РёСЏ РґР»РёР»СЃСЏ Lib_button -> contact_bounse С†РёРєР»РѕРІ Р·РЅР°С‡РёС‚ РЅР°Р¶Р°Р»Рё
			Lib_button -> buttons[i].timer_bounce_down = 0, Lib_button -> buttons[i].state.state_button = 1;
			
		if((*(Lib_button -> buttons[i].button_port) & (1 << Lib_button -> buttons[i].button_pin)) && Lib_button -> buttons[i].state.state_button && !Lib_button -> buttons[i].timer_bounce_up) // Р•СЃР»Рё РѕС‚РїСѓСЃС‚РёР»Рё РєРЅРѕРїРєСѓ (РѕС‚СЂР°Р±Р°С‚С‹РІР°РµРј РґСЂРµР±РµР·Рі)
			Lib_button -> buttons[i].timer_bounce_up = Lib_button -> contact_bounse;
			
		if((!(*(Lib_button -> buttons[i].button_port) & (1 << Lib_button -> buttons[i].button_pin))) && Lib_button -> buttons[i].state.state_button && Lib_button -> buttons[i].timer_bounce_up > 1)
			Lib_button -> buttons[i].timer_bounce_up = 0;
			
		if((*(Lib_button -> buttons[i].button_port) & (1<< Lib_button -> buttons[i].button_pin)) && Lib_button -> buttons[i].state.state_button && Lib_button -> buttons[i].timer_bounce_up == 1) // Р•СЃР»Рё РѕС‚РїСѓСЃС‚РёР»Рё Р»РµРІСѓСЋ РІРµСЂС…РЅСЋСЋ РєРЅРѕРїРєСѓ Рё РїСЂРѕС€Р»Рѕ РІСЂРµРјСЏ
			Lib_button -> buttons[i].timer_bounce_up = 0, Lib_button -> buttons[i].state.state_button = 0; 
	}
}

#ifdef BUTTONS_SCAN_CODE
void scan_code_button(void) // Р Р°СЃРїРѕР·РЅР°РІР°РЅРёРµ С‚РёРїРѕРІ РЅР°Р¶Р°С‚РёР№ РєРЅРѕРїРѕРє
{
	uint8_t i = 0;
	for(i = 0; i < Lib_button -> count_buttons; i++)
	{
		if(Lib_button -> buttons[i].state.state_button && !Lib_button -> buttons[i].timer1)Lib_button -> buttons[i].timer1 = Lib_button -> max_delay_one_click; //Р•СЃР»Рё РЅР°Р¶Р°Р»Рё
		if(!Lib_button -> buttons[i].state.state_button && Lib_button -> buttons[i].timer1 > 1 && !Lib_button -> buttons[i].timer2)Lib_button -> buttons[i].timer2 = Lib_button -> max_delay_one_click; //Р•СЃР»Рё РѕС‚РїСѓСЃС‚РёР»Рё РІ С‚РµС‡РµРЅРёРё tamer1
		if(!Lib_button -> buttons[i].state.state_button &&  Lib_button -> buttons[i].timer1 == 1)Lib_button -> buttons[i].timer1 = 0; //Р•СЃР»Рё РѕС‚РїСѓСЃС‚РёР»Рё РїРѕСЃР»Рµ tamer1
		if(Lib_button -> buttons[i].timer1 > 1 && Lib_button -> buttons[i].timer2 >= 1) //Р•СЃР»Рё РѕС‚РїСѓСЃС‚РёР»Рё РІ С‚РµС‡РµРЅРёРё tamer1 РЅРѕ РЅРµ РЅР°Р¶Р°Р»Рё РїРѕРІС‚РѕСЂРЅРѕ РІ С‚РµС‡РµРЅРёРё timer2
		{			// 1-РЅ РєР»РёРє
			if(Lib_button -> buttons[i].enable_dbl_click)
				{
					if(Lib_button -> buttons[i].timer2 == 1)
					{
						Lib_button -> buttons[i].state.one_click = 1;
						Lib_button -> buttons[i].timer1 = 0, Lib_button -> buttons[i].timer2 = 0;	
					}
				}
			else
				{
					Lib_button -> buttons[i].state.one_click = 1;
					Lib_button -> buttons[i].timer1 = 0, Lib_button -> buttons[i].timer2 = 0;	
				}
		}
			
		if(Lib_button -> buttons[i].state.state_button && Lib_button -> buttons[i].timer1 == 1 && !Lib_button -> buttons[i].state.hold) // Р”РѕР»РіРѕРµ СѓРґРµСЂР¶РёРІР°РЅРёРµ РЅР°Р¶Р°С‚РёСЏ
			Lib_button -> buttons[i].state.hold = 1, Lib_button -> buttons[i].timer_hold = Lib_button -> time_multiplier, Lib_button -> buttons[i].hold_multiplier = 1; 
			
		if(Lib_button -> buttons[i].timer_hold == 1) // Р•СЃР»Рё СѓРґРµСЂР¶РёРІР°РµРј РІ С‚РµС‡РµРЅРёРё Lib_button -> time_multiplier С‚Рѕ СѓРІРµР»РёС‡РёРІР°СЋ РјРЅРѕР¶РёС‚РµР»СЊ
			Lib_button -> buttons[i].hold_multiplier++, Lib_button -> buttons[i].timer_hold = Lib_button -> time_multiplier;

		if(!Lib_button -> buttons[i].state.state_button && Lib_button -> buttons[i].state.hold)Lib_button -> buttons[i].state.hold = 0, Lib_button -> buttons[i].hold_multiplier = 1, Lib_button -> buttons[i].timer_hold = 0; // Р•СЃР»Рё РѕС‚РїСѓСЃС‚РёР»Рё РїРѕСЃР»Рµ РґРѕР»РіРѕРіРѕ СѓРґРµСЂР¶РёРІР°РЅРёСЏ
		
		if(Lib_button -> buttons[i].state.state_button && Lib_button -> buttons[i].timer1 > 1 && Lib_button -> buttons[i].timer2 > 1 && !Lib_button -> buttons[i].timer3)Lib_button -> buttons[i].timer3 = Lib_button -> max_delay_one_click; //Р•СЃР»Рё РїРѕСЏРІРёР»РѕСЃСЊ РїРѕРІС‚РѕСЂРЅРѕРµ РЅР°Р¶Р°С‚РёРµ РІ С‚РµС‡РµРЅРёРё timer2
		if(Lib_button -> buttons[i].timer1 > 1 && Lib_button -> buttons[i].timer2 > 1 && Lib_button -> buttons[i].timer3 == 1) //Р•СЃР»Рё РїРѕРІС‚РѕСЂРЅРѕРµ РЅР°Р¶Р°С‚РёРµ Р·Р°С‚СЏРЅСѓР»РѕСЃСЊ Рё РїСЂРµРІС‹СЃРёР»Рѕ timer3
			Lib_button -> buttons[i].timer1 = 0, Lib_button -> buttons[i].timer2 = 0, Lib_button -> buttons[i].timer3 = 0;
		
		if(!Lib_button -> buttons[i].state.state_button && Lib_button -> buttons[i].timer1 > 1 && Lib_button -> buttons[i].timer2 > 1 && Lib_button -> buttons[i].timer3 > 1) // Р•СЃР»Рё РїСЂРѕРёР·РѕС€Р»Рѕ РїРѕРІС‚РѕСЂРЅРѕРµ РѕС‚РїСѓСЃРєР°РЅРёРµ РІ С‚РµС‡РµРЅРёРё timer3
		{	// 2-РЅРѕР№ РєР»РёРє
			Lib_button -> buttons[i].timer1 = 0, Lib_button -> buttons[i].timer2 = 0, Lib_button -> buttons[i].timer3 = 0;
			Lib_button -> buttons[i].state.one_click = 0;
			Lib_button -> buttons[i].state.double_click = 1;
		}
	}
}

#endif