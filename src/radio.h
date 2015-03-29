#ifndef _RADIO_H_
#define _RADIO_H_

#include "common.h"


void Radio_Init(uint8_t volume, int frequency, uint8_t rssiLimit);
void Radio_UnInit();

void Radio_FrequencyUpdate(bool_t resetRds);
void Radio_RdsUpdate();

int Radio_spectrum(const char *arg);

#endif /* _COMMANDS_H_ */
