#include "encoders.h"

static struct lib_encoder *Lib_encoder;

void init_encoders(struct lib_encoder *init) // РџСЂРѕРёРЅРёС†РёР°Р»РёР·РёСЂРѕРІР°С‚СЊ РІСЃРµ СЌРЅРєРѕРґРµСЂС‹
{
	Lib_encoder = init;
	uint8_t i;

	for(i = 0; i < Lib_encoder -> count_encoders; i++)
	{
		Lib_encoder -> encoders[i].state_strob = 0;
		Lib_encoder -> encoders[i].state = 0;
	}
}

void scan_encoders(void) // Р¤СѓРЅРєС†РёСЏ РІСЃС‚СЂР°РёРІР°РµС‚СЃСЏ РІ РѕР±СЂР°Р±РѕС‚С‡РёРє С‚Р°Р№РјРµСЂР°
{
	uint8_t i = 0, current_state_B = 0;
	for(i = 0; i < Lib_encoder -> count_encoders; i++)
	{
		current_state_B = (!(*(Lib_encoder -> encoders[i].port_line_B) & (1 << Lib_encoder -> encoders[i].pin_line_B)));
		if(current_state_B && Lib_encoder -> encoders[i].state_strob == 0) // Р•СЃР»Рё РїРѕСЏРІРёР»СЃСЏ РїРµСЂРµС…РѕРґ С„СЂРѕРЅС‚Р° СЃ 0Р»СЏ РЅР° 1С†Сѓ
		{
			if(!(*(Lib_encoder -> encoders[i].port_line_A) & (1 << Lib_encoder -> encoders[i].pin_line_A)))
				Lib_encoder -> encoders[i].state = RIGHT_ROTATING;
			else
				Lib_encoder -> encoders[i].state = LEFT_ROTATING;
		}
		Lib_encoder -> encoders[i].state_strob = current_state_B;
	}
}

