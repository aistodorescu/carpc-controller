#include <pthread.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "common.h"
#include "config.h"
#include "si4703.h"
#include "xbmcclient_if.h"
#include "event-server.h"
#include "radio.h"

/*
 * Defines
 */


/*
 * Typedefs
 */


/*
 * Private Functions Declarations
 */
void *Radio_Loop(void *pParam);


/*
 * Local Variables
 */
static uint8_t      mRssiLimit = 35;
static rdsData_t    mRdsData;

static uint8_t      mTmpPSName[3][9];


/*
 * Global Variables
 */
extern uint8_t  gSystemMode; /* systemModes_t */

/*
 * Public functions definitions
 */
void Radio_Init(uint8_t volume, int frequency, uint8_t rssiLimit)
{
    //pthread_mutex_init(&lockX, NULL);
    print("Starting Radio Module\n");

    mRssiLimit = rssiLimit;
    memset(&mRdsData.programServiceName, ' ', sizeof(mRdsData.programServiceName));
    memset(&mRdsData.radioText, ' ', sizeof(mRdsData.radioText));

    memset(mTmpPSName, ' ', 3*9);
    mTmpPSName[0][8] = 0;
    mTmpPSName[1][8] = 0;
    mTmpPSName[2][8] = 0;

    {
        /* SI4703 Setup */
        si_power(PWR_ENABLE);
        si_set_volume(volume);
        si_tune(frequency);
    }

    Radio_FrequencyUpdate(FALSE);
}

void Radio_UnInit()
{
}

void Radio_FrequencyUpdate(bool_t resetRds)
{
    char *pPos;
    char replyMsg[20];
    char aXbmcAction[150];

    si_read_regs();
    sprintf(replyMsg, "%d", si_get_freq());

    pPos = replyMsg + strlen(replyMsg) - 2;
    //*(pPos + 2) = *(pPos + 1); /* no need for the last 0 in frequency */
    *(pPos + 1) = *pPos;
    *pPos = '.';

    sprintf(aXbmcAction, "SetProperty(Radio.Frequency,%s,10000)", replyMsg);
    XBMC_ClientAction(aXbmcAction);

    if(resetRds)
    {
        memset(&mRdsData.programServiceName, ' ', sizeof(mRdsData.programServiceName));
        memset(&mRdsData.radioText, ' ', sizeof(mRdsData.radioText));
        mRdsData.programServiceName[sizeof(mRdsData.programServiceName) - 1] = 0;
        mRdsData.radioText[sizeof(mRdsData.radioText) - 1] = 0;

        /* RDS Radiotext */
        sprintf(aXbmcAction, "SetProperty(Radio.RadioText,\"%s\",10000)", mRdsData.radioText);
        XBMC_ClientAction(aXbmcAction);

        /* RDS Program name */
        sprintf(aXbmcAction, "SetProperty(Radio.StationName,\"%s\",10000)",
            mRdsData.programServiceName);
        XBMC_ClientAction(aXbmcAction);
    }

}

void Radio_RdsUpdate()
{
    if(gSystemMode == gModeRadio_c)
    {
        rdsStatus_t rdsStatus;
        char        aXbmcAction[150];

        si_rds_update(&mRdsData, &rdsStatus);
        if(rdsStatus.radiotext)
        {
            /* RDS Radiotext */
            sprintf(aXbmcAction, "SetProperty(Radio.RadioText,\"%s\",10000)", mRdsData.radioText);
            XBMC_ClientAction(aXbmcAction);
            print("RT: [%s]\n", aXbmcAction);
        }

        if(rdsStatus.stationName)
        {
            /* RDS Program name */
            sprintf(aXbmcAction, "SetProperty(Radio.StationName,\"%s\",10000)",
                mRdsData.programServiceName);
            XBMC_ClientAction(aXbmcAction);
            print("SN: [%s]\n", aXbmcAction);
        }
    }
}

