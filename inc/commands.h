#ifndef _COMMANDS_H_
#define _COMMANDS_H_

#include "settings.h"

void Commands_Init(settings_t *pSettings);
void Commands_UnInit();
void Commands_Signal(char *pStr);


#endif /* _COMMANDS_H_ */
