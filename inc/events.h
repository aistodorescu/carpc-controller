#ifndef EVENTS_H_
#define EVENTS_H_

#include "settings.h"

typedef void(*evIsrFp_t)(void);

void Events_Init(settings_t *pSettings);
void Events_UnInit();
evIsrFp_t *Events_GetHandlers();

#endif /* EVENTS_H_ */
