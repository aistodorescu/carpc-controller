#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "wiringPi.h"

#include "settings.h"
#include "event-server.h"
#include "commands.h"
#include "radio.h"

#include "config.h"
#include "common.h"

/*
 * Defines
 */


/*
 * Typedefs
 */


/*
 * Private Functions Declaration
 */


/*
 * Local Variables
 */
static settings_t mSettings;


/*
 * Global Variables
 */


/*
 * Public functions definitions
 */
settings_t* Settings_ReadConfig(const char *pFileName)
{
    FILE    *fp;
    char    *pBuffer;
    uint8_t i;
    char    *pData;
    uint8_t ipAddress[INET_ADDRSTRLEN];
    settings_t *pResult = NULL;

    memset(mSettings.encoders, 0, sizeof(encoder_t) * ENCODERS_NB);
    memset(mSettings.buttons, 0, sizeof(button_t) * BUTTONS_NB);
    memset((void*)ipAddress, 0, sizeof(ipAddress));
    memset(mSettings.buttons, 0, sizeof(button_t) * BUTTONS_NB);

    pBuffer = (char*)malloc(LINE_MAX_SIZE);
    fp = fopen(pFileName, "r");

    if((fp < 0) && (pBuffer != NULL))
    {
        printf("file read went wrong\n");
    }
    else
    {
        while(fgets(pBuffer, LINE_MAX_SIZE, fp))
        {
            if(pBuffer[0] == '#' || pBuffer[0] == '\n' || pBuffer[0] == '\r')
                continue;

            /* Replace ':' with '\0'(end of string) character */
            for(i = 0; i < LINE_MAX_SIZE; i++)
            {
                if(!pBuffer[i]) break;
                if(pBuffer[i] == ':' || pBuffer[i] == '\n' || pBuffer[i] == '\r' || pBuffer[i] == '#')
                    pBuffer[i] = 0;
            }

            /* Get ip address */
            pData = strstr(pBuffer, "[ip]");
            if(pData)
            {
                pData += strlen(pData) + 1;
                strcpy((char*)ipAddress, pData);
                pData += strlen(pData);
                continue;
            }
            
            /* Get radio freq */
            pData = strstr(pBuffer, "radio_freq");
            if(pData)
            {
                pData += strlen(pData) + 1;
                mSettings.radioFrequency = atoi(pData);
                pData += strlen(pData);
                continue;
            }
            
            /* Get volume */
            pData = strstr(pBuffer, "volume");
            if(pData)
            {
                pData += strlen(pData) + 1;
                mSettings.systemVolume = atoi(pData);
                pData += strlen(pData);
                continue;
            }

            /* Get system mode */
            pData = strstr(pBuffer, "system_mode:");
            if(pData)
            {
                pData += strlen(pData) + 1;
                pData = strstr(pBuffer, "radio");
                if(pData)
                {
                    mSettings.systemMode = gModeRadio_c;
                }
                else
                {
                    mSettings.systemMode = gModeXBMC_c;
                }
                continue;
            }

            /* Get click debounce value */
            pData = strstr(pBuffer, "click_skip");
            if(pData)
            {
                pData += strlen(pData) + 1;
                mSettings.clickSkip = atoi(pData);
                continue;
            }

            /* Get click hold value */
            pData = strstr(pBuffer, "click_hold");
            if(pData)
            {
                pData += strlen(pData) + 1;
                mSettings.clickHold = atoi(pData);
                continue;
            }

            /* Get encoder hold value */
            pData = strstr(pBuffer, "encoder_hold");
            if(pData)
            {
                pData += strlen(pData) + 1;
                mSettings.encoderHold = atoi(pData);
                continue;
            }

            /* Parse button entry */
            pData = strstr(pBuffer, "button");
            if(pData)
            {
                pData += strlen(pData) + 1; /* skip 'button:' */
                mSettings.buttons[mSettings.buttonsNb].gpio = atoi(pData); /* get pin number */
                pData += strlen(pData) + 1; /* skip 'pin number:' */
                strcpy(mSettings.buttons[mSettings.buttonsNb].action, pData); /* get command(s) */

                {
                    char *pStr;

                    pStr = mSettings.buttons[mSettings.buttonsNb].action;
                    mSettings.buttons[mSettings.buttonsNb].pCommands[mSettings.buttons[mSettings.buttonsNb].iCmd] = pStr;
                    while(*pStr)
                    {
                        if(*pStr == '>')
                        {
                            *pStr = 0;
                            mSettings.buttons[mSettings.buttonsNb].iCmd++;
                            mSettings.buttons[mSettings.buttonsNb].pCommands[mSettings.buttons[mSettings.buttonsNb].iCmd] = pStr + 1;
                        }

                        /* Don't exceed available array */
                        if(mSettings.buttons[mSettings.buttonsNb].iCmd >= MAX_COMMANDS) break;

                        pStr++;
                    }

                    /* Set the number of commands */
                    mSettings.buttons[mSettings.buttonsNb].nbCmd = mSettings.buttons[mSettings.buttonsNb].iCmd + 1;
                }

                mSettings.buttonsNb++;
                continue;
            }

            /* Parse rotary encoder entry */
            pData = strstr(pBuffer, "encoder");
            if(pData)
            {
                pData += strlen(pData) + 1; /* skip 'encoder:' */
                pData += strlen(pData) + 1; /* skip 'sl:' */
                mSettings.encoders[mSettings.encodersNb].skipTimesLeft = atoi(pData);
                pData += strlen(pData) + 1; /* skip value: */
                pData += strlen(pData) + 1; /* skip 'sr:' */
                mSettings.encoders[mSettings.encodersNb].skipTimesRight = atoi(pData);
                pData += strlen(pData) + 1; /* skip value: */
                pData += strlen(pData) + 1; /* skip 'CLK:' */
                mSettings.encoders[mSettings.encodersNb].gpioClk = atoi(pData); /* get CLK pin(11) */
                pData += strlen(pData) + 1; /* skip value: */
                pData += strlen(pData) + 1; /* skip DT: */
                mSettings.encoders[mSettings.encodersNb].gpioDt = atoi(pData); /* get DT pin(9) */
                pData += strlen(pData) + 1; /* skip '9' */
                strcpy(mSettings.encoders[mSettings.encodersNb].actionLeft, pData); /* get left command(s) */
                pData += strlen(pData) + 1; /* skip left command(s) */
                strcpy(mSettings.encoders[mSettings.encodersNb].actionRight, pData); /* get right command(s) */

                mSettings.encodersNb++;
                continue;

            }
        }

        pResult = &mSettings;
    }

    free(pBuffer);
    fclose(fp);

    return pResult;
}

void Settings_UnInit()
{
    /* Save settings */
}

/*
 * Private functions definitions
 */
