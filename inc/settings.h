#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include "common.h"

typedef struct settings_tag
{
    uint8_t         systemMode; /* systemModes_t */
    uint8_t         systemVolume;
    uint8_t         buttonsNb;
    uint8_t         encodersNb;
    uint32_t        clickHold;
    uint32_t        encoderHold;
    button_t        buttons[BUTTONS_NB];
    encoder_t       encoders[ENCODERS_NB];
    uint16_t        radioFrequency;
    uint8_t         clickSkip;
    uint8_t band;
    uint8_t radioRst;
    uint8_t radioGpio2;
    uint8_t radioSpacing;
} settings_t;

settings_t* Settings_ReadConfig(const char *pFileName);
void Settings_UnInit();

#endif /* _SETTINGS_H_ */
