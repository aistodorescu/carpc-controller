#include <pthread.h>
#include <mqueue.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "radio.h"
#include "common.h"
#include "config.h"
#include "si4703.h"
#include "xbmcclient_if.h"
#include "event-server.h"
#include "settings.h"

/*
 * Defines
 */


/*
 * Typedefs
 */

/*
 * Private Functions Declarations
 */
static void Commands_Execute(char *pStr);
static void *Commands_ProcessLoop(void *pParam);

/*
 * Local Variables
 */
static settings_t       *mpSettings = NULL;
static pthread_t        mCommandsProcessThread;
static mqd_t            mCommandsMessageQueueFd;
static struct mq_attr   mCommandsMessageQueueBuf;
static bool_t           mForceStop = FALSE;


/*
 * Global Variables
 */
extern bool_t           gRdsEnabled;
extern bool_t           gFrequencyChanged;

/*
 * Public functions definitions
 */
void Commands_Init(settings_t *pSettings)
{
    //pthread_mutex_init(&lockX, NULL);
    printf("Starting Commands Module\n");

    mpSettings = pSettings;

    /* Create message queue */
    mCommandsMessageQueueBuf.mq_msgsize = 512;
    mCommandsMessageQueueBuf.mq_maxmsg = 16;
    mCommandsMessageQueueFd = mq_open("/QCarPC", O_CREAT | O_RDWR, 600, &mCommandsMessageQueueBuf);

    if(mCommandsMessageQueueFd < 0)
    {
        perror("Error");
    }
    else
    {
        printf("init ok %d\n", mCommandsMessageQueueFd);

        /* Create radio periodically update thread */
        pthread_create(&mCommandsProcessThread, NULL, Commands_ProcessLoop, NULL);
    }
}

void Commands_UnInit()
{
    mq_close(mCommandsMessageQueueFd);
}

void Commands_Signal(char *pStr)
{
    int res;
    //printf("sending: %s\n", pStr);

    res = mq_send(mCommandsMessageQueueFd, pStr, strlen(pStr) + 1, 1);
    if(res < 0)
	{
    	printf("error fd %d\n", mCommandsMessageQueueFd);
	}
}


/*
 * Private functions definitions
 */
static void *Commands_ProcessLoop(void *pParam)
{
    unsigned int msgPrio;
    void *pBuff = calloc(512, 1);

    while(!mForceStop)
    {
        int ret;

        ret = mq_receive(mCommandsMessageQueueFd, (char*)pBuff,
            mCommandsMessageQueueBuf.mq_msgsize, &msgPrio);
        if (ret >= 0) /* got a message */
        {
            Commands_Execute((char*)pBuff);
        }
        else if(ret < 0)
        {
            printf("received: %d\n", ret);
        }
    }

    pthread_exit(NULL);
    free(pBuff);

    return NULL;
}

void Commands_ProcessRadioSetup
(
    char *aComm[],
    uint32_t cComm
)
{
    uint32_t iComm = 0;
    uint8_t gpio2 = 0;
    uint8_t rst = 0;
    uint8_t band = 0;
    uint8_t spacing = 0;
    uint16_t frequency = 0;

    for(iComm=0;iComm<cComm;iComm++)
    {
        char *pComm = aComm[iComm];

        if(strstr(pComm, "GPIO2_"))
        {
            gpio2 = atoi(pComm + strlen("GPIO2_"));
        }
        else if(strstr(pComm, "RST_"))
        {
            rst = atoi(pComm + strlen("RST_"));
        }
        else if(strstr(pComm, "BAND_"))
        {
            band = atoi(pComm + strlen("BAND_"));
        }
        else if(strstr(pComm, "SPACE_"))
        {
            spacing = atoi(pComm + strlen("SPACE_"));
        }
        else if(strstr(pComm, "FREQ_"))
        {
            frequency = (uint16_t)(atof(pComm + strlen("FREQ_")) * 100.0f);
        }
    }

    printf("*RADIO: band:%d freq:%d gpio2:%d rst:%d spacing:%d\n", band, frequency, gpio2, rst, spacing);

    /*  */
    if((gpio2 != 0) /*&& (gpio2 != mpSettings->radioGpio2)*/ &&
        (rst != 0) /*&& (rst != mpSettings->radioRst)*/ &&
        //(band != 0) /*&& (band != mpSettings->band)*/ &&
        (spacing != 0) /*&& (spacing != mpSettings->radioSpacing)*/)
    {
        mpSettings->band = band;
        mpSettings->radioFrequency = frequency;
        mpSettings->radioGpio2 = gpio2;
        mpSettings->radioRst = rst;
        mpSettings->radioSpacing = spacing;

        si_init();
        si_power(PWR_ENABLE, mpSettings->radioRst, mpSettings->band,
            mpSettings->radioSpacing);
        si_mute(1);
        si_set_volume(Radio_Volume(mpSettings->systemVolume));
        si_tune(mpSettings->radioFrequency);
    }

    if(frequency != 0)
    {
        si_tune(mpSettings->radioFrequency);
    }
}

void Commands_ProcessEncoderSetup
(
    char *aComm[],
    uint32_t cComm
)
{
    uint32_t iComm = 0;
    uint8_t id = 0xFF;

    for(iComm=0;iComm<cComm;iComm++)
    {
        char *pComm = aComm[iComm];

        if(id == 0xFF)
        {
            if(strstr(pComm, "ID_"))
            {
                id = atoi(pComm + strlen("ID_")) - 1;
            }
            else
            {
                break;
            }
        }
        else
        {
            if(strstr(pComm, "A_"))
            {
                mpSettings->encoders[id].gpioClk = atoi(pComm + strlen("A_"));
            }
            else if(strstr(pComm, "B_"))
            {
                mpSettings->encoders[id].gpioDt = atoi(pComm + strlen("B_"));
            }
            else if(strstr(pComm, "MAPL_"))
            {
                memcpy(mpSettings->encoders[id].actionLeft,
                    pComm + strlen("MAPL_"),
                    strlen(pComm) - strlen("MAPL_") + 1);
            }
            else if(strstr(pComm, "MAPR_"))
            {
                memcpy(mpSettings->encoders[id].actionRight,
                    pComm + strlen("MAPR_"),
                    strlen(pComm) - strlen("MAPR_"));
            }
        }
    }

    if(id != 0xFF)
    {
        printf("*ENC: %d A:%d, B: %d, L:%s, R:%s\n",
            id + 1, mpSettings->encoders[id].gpioClk,
            mpSettings->encoders[id].gpioDt,
            mpSettings->encoders[id].actionLeft,
            mpSettings->encoders[id].actionRight);
    }
}

void Commands_ProcessButtonSetup
(
    char *aComm[],
    uint32_t cComm
)
{
    uint32_t iComm = 0;
    uint8_t id = 0xFF;

    for(iComm=0;iComm<cComm;iComm++)
    {
        char *pComm = aComm[iComm];

        if(id == 0xFF)
        {
            if(strstr(pComm, "ID_"))
            {
                id = atoi(pComm + strlen("ID_")) - 1;
            }
            else
            {
                break;
            }
        }
        else
        {
            if(strstr(pComm, "GPIO_"))
            {
                mpSettings->buttons[id].gpio = atoi(pComm + strlen("GPIO_"));
            }
            else if(strstr(pComm, "MAPBTN_"))
            {
                memcpy(mpSettings->buttons[id].action,
                    pComm + strlen("MAPBUTTON_"),
                    strlen(pComm) - strlen("MAPBUTTON_") + 1);
            }
        }
    }

    if(id != 0xFF)
    {
        printf("*BTN: %d GPIO:%d, C:%s\n",
            id + 1, mpSettings->buttons[id].gpio,
            mpSettings->buttons[id].action);
    }
}

void Commands_ProcessSystemSetup
(
    char *aComm[],
    uint32_t cComm
)
{
    uint32_t iComm = 0;

    for(iComm=0;iComm<cComm;iComm++)
    {
        char *pComm = aComm[iComm];

        if(strstr(pComm, "VOL_"))
        {
            mpSettings->systemVolume = atoi(pComm + strlen("VOL_"));
        }
        else if(strstr(pComm, "SMODE_"))
        {
            mpSettings->systemMode = atoi(pComm + strlen("SMODE_"));
        }
    }

    printf("*system mode: %d, system volume: %d\n", mpSettings->systemMode,
        mpSettings->systemVolume);
    si_set_volume(Radio_Volume(mpSettings->systemVolume));

    if(mpSettings->systemMode == gModeXBMC_c)
    {
        si_mute(1);
    }
    else
    {
        si_mute(0);
    }
}

/*! \brief Parse a string and send the commands found via sockets.
 *
 *  \param[IN/OUT] pStr pointer to the command that should be executed
 */
static void Commands_Execute(char *pStr)
{
    uint32_t commLength = 0;

    if(pStr)
    {
        uint32_t idx;
        uint8_t iComm = 0;
        char *aCommands[20] = {0};

        //printf("%s\n", pStr);

        /* Setup input command in multiple strings */
        commLength = strlen(pStr);
        iComm = 1;
        for(idx=0;idx<commLength;idx++)
        {
            if(pStr[idx] == ';')
            {
                pStr[idx] = 0;
            }
            if(pStr[idx] == ',')
            {
                pStr[idx] = 0;
                aCommands[iComm] = &pStr[idx + 1];
                iComm++;
            }
        }

        /* RADIO:GPIO2_25,RST_4,BAND_0,SPACING_100,FREQUENCY_101.1 */
        if(strstr(pStr, "RADIO:"))
        {
            aCommands[0] = pStr + strlen("RADIO:");
            Commands_ProcessRadioSetup(aCommands, iComm);
        }
        /*  */
        else if(strstr(pStr, "SYSTEM:"))
        {
            aCommands[0] = pStr + strlen("SYSTEM:");
            Commands_ProcessSystemSetup(aCommands, iComm);
        }
        /* ENC:ID_1,A_11,B_9,MAPL_<left>,MAPR_<right> */
        else if(strstr(pStr, "ENC:"))
        {
            aCommands[0] = pStr + strlen("ENC:");
            Commands_ProcessEncoderSetup(aCommands, iComm);
        }
        /* BTN:ID_1,GPIO_1,MAPBTN_<btn> */
        else if(strstr(pStr, "BTN:"))
        {
            aCommands[0] = pStr + strlen("BTN:");
            Commands_ProcessButtonSetup(aCommands, iComm);
        }
        else if(strstr(pStr, "radio_seek_up"))
        {
            si_seek(SEEK_UP);
        }
        else if(strstr(pStr, "radio_seek_down"))
        {
            si_seek(SEEK_DOWN);
        }
        else if(strstr((const char*)pStr, RADIO_CMD_TUNE))
        {
            int frequency;
            char *pPos;
            char *pPos2;

            pPos = (char*)pStr + strlen(RADIO_CMD_TUNE);

            pPos2 = strchr(pPos, '.');
            *pPos2 = *(pPos2 + 1);
            *(pPos2 + 1) = *(pPos2 + 2);
            *(pPos2 + 2) = 0;

            frequency = atoi((const char*)(pStr + strlen(RADIO_CMD_TUNE)));
//            printf("new freq: %d\n", frequency);
//            printf("frequency to tune: %s\n", (char*)pStr + strlen(RADIO_CMD_TUNE));

            si_tune(frequency * 10);
        }
    }

#if 0

            if(!strcmp(pComm, "interrupt_rds") && (mpSettings->systemMode == gModeRadio_c))
            {
                Radio_RdsUpdate();
            }
            /* XBMC Keyboard action */
            else if((strstr(pComm, XBMCBUTTON_CMD)) && (mpSettings->systemMode == gModeXBMC_c))
            {
                XBMC_ClientButton((const char*)pComm + strlen(XBMCBUTTON_CMD), "KB");
                //printf("KB_%s\n", pComm + strlen(XBMCBUTTON_CMD));
            }
            /* XBMC built-in function */
            else if((strstr(pComm, XBMCBUILTIN_CMD)) && (mpSettings->systemMode == gModeXBMC_c))
            {
                XBMC_ClientAction(pComm + strlen(XBMCBUILTIN_CMD));
                //printf("XBMC builtin_%s\n", pComm + strle(XBMCBUILTIN_CMD));
            }
            /* Toggle Radio/Media Center */
            else if(strstr((const char*)pComm, SYSTEM_MODE_TOGGLE))
            {
                mpSettings->systemMode ^= 0x01;

                if(mpSettings->systemMode == gModeRadio_c)
                {
                    XBMC_ClientAction("PlayerControl(Stop)");
                    XBMC_ClientAction("SetProperty(Radio.Active,true,10000)");
                    si_mute(0);
                }
                else
                {
                    XBMC_ClientAction("SetProperty(Radio.Active,false,10000)");
                    si_mute(1);
                }
                printf("Mode: %s\n", (mpSettings->systemMode==gModeRadio_c)?"radio":"media center");
            }
            /* Volume should be updated for both Radio and XBMC */
            else if(strstr(pComm, SYS_CMD_VOLUME))
            {
                System_VolumeUpdate(pComm);
            }
            /* RADIO action */
            else if((strstr(pComm, RADIO_CMD)) && (mpSettings->systemMode == gModeRadio_c))
            {
                if(!memcmp(pComm, RADIO_CMD_SEEK_UP, strlen(RADIO_CMD_SEEK_UP)))
                {
                    si_seek(SEEK_UP);
                    gFrequencyChanged = TRUE;
                }
                else if(!memcmp(pComm, RADIO_CMD_SEEK_DOWN, strlen(RADIO_CMD_SEEK_DOWN)))
                {
                    si_seek(SEEK_DOWN);
                    gFrequencyChanged = TRUE;
                }
                else if(!memcmp(pComm, RADIO_CMD_TUNE_UP, strlen(RADIO_CMD_TUNE_UP)))
                {
                    int freq = si_get_freq();

                    if(freq == 10800)
                    {
                        freq = 8800;
                    }
                    else
                    {
                        freq += 10;
                    }

                    si_tune(freq);
                    gFrequencyChanged = TRUE;
                }
                else if(!memcmp(pComm, RADIO_CMD_TUNE_DOWN, strlen(RADIO_CMD_TUNE_DOWN)))
                {
                    int freq = si_get_freq();

                    if(freq == 8800)
                    {
                        freq = 10800;
                    }
                    else
                    {
                        freq -= 10;
                    }

                    si_tune(freq);
                    gFrequencyChanged = TRUE;
                }
                else if(strstr((const char*)pComm, RADIO_CMD_TUNE))
                {
                    int frequency;
                    char *pPos;
                    char *pPos2;

                    pPos = (char*)pComm + strlen(RADIO_CMD_TUNE);

                    pPos2 = strchr(pPos, '.');
                    *pPos2 = *(pPos2 + 1);
                    *(pPos2 + 1) = *(pPos2 + 2);
                    *(pPos2 + 2) = 0;

                    frequency = atoi((const char*)(pComm + strlen(RADIO_CMD_TUNE)));
                    printf("new freq: %d\n", frequency);
                    printf("frequency to tune: %s\n", (char*)pComm + strlen(RADIO_CMD_TUNE));

                    si_tune(frequency * 10);
                    gFrequencyChanged = TRUE;
                }
            }
            /* NAVIGATION commands */
            /*else if(!strcmp(pComm, NAV_CMD_ZOOM_IN))
              {
               system("xdotool keyup '+'");
              }
              else if(!strcmp(pComm, NAV_CMD_ZOOM_OUT))
              {
               system("xdotool keyup '-'");
              }
              else if(!strcmp(pComm, NAV_CMD_UP))
              {
               system("xdotool keyup 'up'");
              }
              else if(!strcmp(pComm, NAV_CMD_DOWN))
              {
               system("xdotool keyup 'down'");
              }
              else if(!strcmp(pComm, NAV_CMD_LEFT))
              {
               system("xdotool keyup 'left'");
              }
              else if(!strcmp(pComm, NAV_CMD_RIGHT))
              {
               system("xdotool keyup 'right'");
              }*/
            iComm--;

        }

        /*if(gFrequencyChanged)
        {
            Radio_FrequencyUpdate(TRUE);
            gFrequencyChanged = FALSE;
        }*/

        free(pOrigStr);
    }
    //pthread_mutex_unlock(&lockX);
#endif

}
