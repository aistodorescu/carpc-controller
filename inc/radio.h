#ifndef _RADIO_H_
#define _RADIO_H_

#include "common.h"
#include "settings.h"


void Radio_Init(settings_t *pSettings);
void Radio_UnInit();

void Radio_FrequencyUpdate(bool_t resetRds);
void Radio_RdsUpdate();

int Radio_spectrum(const char *arg);

uint8_t Radio_Volume(uint8_t systemVolume);

#endif /* _COMMANDS_H_ */
