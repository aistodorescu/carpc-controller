/*
 * Includes
 */
#include <stdint.h>
#include <string.h>
#include <rpc/types.h>
#include <mqueue.h>
#include <pthread.h>
#include <unistd.h>

#include "wiringPi.h"

#include "config.h"
#include "common.h"
#include "commands.h"

/*
 * Defines
 */
#define EVENT_BUTTON        1
#define EVENT_ENCODER       2
#define ISR(pin) \
static void ISR##pin##_Handler(void) \
{\
    if(mAllowEvent)\
    {\
        int res;\
        char event;\
        event = pin;\
        print("----------------%d\n", pin);\
        res = mq_send(mEventsMessageQueueFd, (char*)&event, sizeof(event), 1);\
        if(res < 0)\
        {\
            print("error fd %d\n", mEventsMessageQueueFd);\
        }\
    }\
}

/*
 * Typedefs
 */
typedef void(*isrFp_t)(void);

/*
 * Private Functions Prototypes
 */
static void *Events_ProcessLoop(void *pParam);
static void Events_ReadConfig(const char *pFileName);
static uint8_t grayToBinary(uint8_t num);
static void Events_ReadConfig(const char *fileName);
static bool_t Events_ProcessButton(char gpio);
static bool_t Events_ProcessEncoders();
static char *Events_ButGetNextCommand(button_t *pBut);


/*
 * Local Variables
 */
static pthread_t        mEventsProcessThread;
static mqd_t            mEventsMessageQueueFd;
static struct mq_attr   mEventsMessageQueueBuf;
static bool_t           mForceStop = FALSE;
static bool_t           mAllowEvent = TRUE;

ISR(0);ISR(1);ISR(2);ISR(3);ISR(4);ISR(5);ISR(6);ISR(7);ISR(8);ISR(9);ISR(10);ISR(11);ISR(12);
ISR(13);ISR(14);ISR(15);ISR(16);ISR(17);ISR(18);ISR(19);ISR(20);ISR(21);ISR(22);ISR(23);ISR(24);
ISR(25);ISR(26);ISR(27);
static isrFp_t aSupportedIsrHandlers[] =
{
    ISR0_Handler, ISR1_Handler, ISR2_Handler, ISR3_Handler, ISR4_Handler, ISR5_Handler,
    ISR6_Handler, ISR7_Handler, ISR8_Handler, ISR9_Handler, ISR10_Handler, ISR11_Handler,
    ISR12_Handler, ISR13_Handler, ISR14_Handler, ISR15_Handler, ISR16_Handler, ISR17_Handler,
    ISR18_Handler, ISR19_Handler, ISR20_Handler, ISR21_Handler, ISR22_Handler, ISR23_Handler,
    ISR24_Handler, ISR25_Handler, ISR26_Handler, ISR27_Handler
};

static uint8_t          mClickSkip = 7;
static uint32_t         mClickHold = 10000;
static uint32_t         mEncoderHold = 10000;
static button_t         mButtons[BUTTONS_NB];
static uint8_t          mButtonsNb = 0;
static encoder_t        mEncoders[ENCODERS_NB];
static uint8_t          mEncodersNb = 0;

/*
 * Global Variables
 */


/*
 * Public Functions Definitions
 */
static void ISR_RDS_Handler(void)
{
    print("----------------RDS----------------\n");
    Commands_Signal((char*)"interrupt_rds");
}

void Events_Init(const char *pFileName)
{
    print("Starting Events Module\n");

    /* Init configuration for events */
    memset(mEncoders, 0, sizeof(encoder_t) * ENCODERS_NB);
    memset(mButtons, 0, sizeof(button_t) * BUTTONS_NB);
    Events_ReadConfig(pFileName);

    /* Create message queue */
    mEventsMessageQueueBuf.mq_msgsize = 512;
    mEventsMessageQueueBuf.mq_maxmsg = 32;
    mEventsMessageQueueFd = mq_open("/QCarPcEvents", O_CREAT | O_RDWR, 600, &mEventsMessageQueueBuf);

    if(mEventsMessageQueueFd < 0)
    {
        perror("Error");
    }
    else
    {
        print("init ok %d\n", mEventsMessageQueueFd);

        pinMode(25, INPUT);
        wiringPiISR(25, INT_EDGE_BOTH, ISR_RDS_Handler);

        /* Create radio periodically update thread */
        pthread_create(&mEventsProcessThread, NULL, Events_ProcessLoop, NULL);
    }
}

void Events_UnInit()
{
    mForceStop = TRUE;
    mq_close(mEventsMessageQueueFd);
}


/*
 * Private Functions Definitions
 */
static void *Events_ProcessLoop(void *pParam)
{
    unsigned int msgPrio;
    char event;

    while(!mForceStop)
    {
        int ret;

        ret = mq_receive(mEventsMessageQueueFd, (char*)&event, mEventsMessageQueueBuf.mq_msgsize,
            &msgPrio);
        if (ret >= 0) /* got a message */
        {
            bool_t executed;

            //print("%d received\n", event);
            executed = Events_ProcessButton(event);
            if(!executed)
            {
                executed = Events_ProcessEncoders();
                if(executed)
                {
                    mAllowEvent = FALSE;
                    delayMicroseconds(70000);
                    mAllowEvent = TRUE;
                }
            }
            else
            {
                mq_attr attr;
                /* Remove other messages from the Q */
                do
                {
                    mq_getattr(mEventsMessageQueueFd, &attr);
                    if(attr.mq_curmsgs)
                    {
                        ret = mq_receive(mEventsMessageQueueFd, (char*)&event,
                            mEventsMessageQueueBuf.mq_msgsize, &msgPrio);
                    }
                } while(attr.mq_curmsgs);
            }
        }
        else if(ret < 0)
        {
            print("received: %d\n", ret);
        }
    }
    pthread_exit(NULL);

    return NULL;
}

static uint8_t grayToBinary(uint8_t num)
{
    uint8_t mask;
    for(mask = num >> 1; mask != 0; mask = mask >> 1)
    {
        num = num ^ mask;
    }
    return num;
}




bool_t Events_ProcessButton(char gpio)
{
    uint8_t i;
    bool_t res = FALSE;

    for(i=0;i<mButtonsNb;i++)
    {
        if(mButtons[i].gpio == gpio)
        {
            mButtons[i].clickTimes++;

            /* Treat just one click from 7(CLICK_DEBOUNCE) */
            if(mButtons[i].clickTimes >= mClickSkip)
            {
                char *pComm;

                /* Get the next group of commands(in case there are more groups of commands) */
                pComm = Events_ButGetNextCommand((button_t*)(mButtons + i));
                print("%s\n", pComm);
                /* Execute commands in a group of commands */
                Commands_Signal(pComm);
                res = TRUE;
                mButtons[i].clickTimes = 0;
                mButtons[i].clickHold++;
            }
            break;
        }
    }

    return res;
}

bool_t Events_ProcessEncoders()
{
    uint8_t i;
    bool_t res = FALSE;

    for(i=0;i<mEncodersNb;i++)
    {
        /* Rotary Encoder */
        {
            mEncoders[i].dtNew = digitalRead(mEncoders[i].gpioDt);
            mEncoders[i].clkNew = digitalRead(mEncoders[i].gpioClk);
            mEncoders[i].valNew = grayToBinary((mEncoders[i].clkNew << 1) | mEncoders[i].dtNew);

            /* New values arrived */
            if(mEncoders[i].valNew != mEncoders[i].valOld)
            {
                /* Update movement once in 4 changes(and not for every value change), to avoid unneeded
                 * changes */
                /* LEFT */
                if(mEncoders[i].valOld == 0 && mEncoders[i].valNew == 1)
                {
                    mEncoders[i].direction = RIGHT;
                    mEncoders[i].changed = 1;
                }
                else if(mEncoders[i].valOld == 1 && mEncoders[i].valNew == 2)
                {
                    mEncoders[i].direction = RIGHT;
                    mEncoders[i].changed = 1;
                }
                else if(mEncoders[i].valOld == 2 && mEncoders[i].valNew == 3)
                {
                    mEncoders[i].direction = RIGHT;
                    mEncoders[i].changed = 1;
                }
                else if(mEncoders[i].valOld == 3 && mEncoders[i].valNew == 0)
                {
                    mEncoders[i].direction = RIGHT;
                    mEncoders[i].changed = 1;
                }
                /* RIGHT */
                else if(mEncoders[i].valOld == 0 && mEncoders[i].valNew == 3)
                {
                    mEncoders[i].direction = LEFT;
                    mEncoders[i].changed = 1;
                }
                else if(mEncoders[i].valOld == 3 && mEncoders[i].valNew == 2)
                {
                    mEncoders[i].direction = LEFT;
                    mEncoders[i].changed = 1;
                }
                else if(mEncoders[i].valOld == 2 && mEncoders[i].valNew == 1)
                {
                    mEncoders[i].direction = LEFT;
                    mEncoders[i].changed = 1;
                }
                else if(mEncoders[i].valOld == 1 && mEncoders[i].valNew == 0)
                {
                    mEncoders[i].direction = LEFT;
                    mEncoders[i].changed = 1;
                }

                //print("direction:%d, changed:%d, old:%d, new:%d\n", mEncoders[i].direction,mEncoders[i].changed,
                //    mEncoders[i].valOld, mEncoders[i].valNew);

                /* Check if we have a new movement */
                if(mEncoders[i].changed)
                {
                    /* Treat the LEFT movement */
                    if(mEncoders[i].direction == LEFT)
                    {
                        /* If we haven't reached the number of steps to be ignored just increase the
                         * current step value */
                        if(mEncoders[i].leftSteps < mEncoders[i].skipTimesLeft)
                        {
                            //print("increase left step: %d\n", mEncoders[i].leftSteps);
                            mEncoders[i].leftSteps++;
                        }
                        /* If we have reached the number of steps to be skipped then reset the current
                         * step number and execute the assigned commands */
                        else
                        {
                            mEncoders[i].leftSteps = 0; /* Reset value */
                            Commands_Signal((char*)mEncoders[i].actionLeft);
                            res = TRUE;
                            //usleep(mEncoderHold);
                        }
                    }
                    /* Treat the RIGHT movement */
                    else
                    {
                        /* If we haven't reached the number of steps to be ignored just increase the
                         * current step value */
                        if(mEncoders[i].rightSteps < mEncoders[i].skipTimesRight)
                        {
                            //print("increase right step: %d\n", mEncoders[i].rightSteps);
                            mEncoders[i].rightSteps++;
                        }
                        /* If we have reached the number of steps to be skipped then reset the current
                         * step number and execute the assigned commands */
                        else
                        {
                            mEncoders[i].rightSteps = 0; /* Reset value */
                            Commands_Signal((char*)mEncoders[i].actionRight);
                            res = TRUE;
                            //usleep(mEncoderHold);
                        }
                    }

                    mEncoders[i].changed = 0;
                }

                /* Update old value */
                mEncoders[i].valOld = mEncoders[i].valNew;

                break;
            }
        }
    }

    return res;
}


/*! \brief Get the next command to be executed, from a list of commands.
 *
 *  \param[IN] pBut pointer to the button
 *  \return char * pointer to the command to be executed
 */
static char *Events_ButGetNextCommand(button_t *pBut)
{
    char *pRes = NULL;

    pRes = pBut->pCommands[pBut->iCmd];

    if(pBut->iCmd >= pBut->nbCmd - 1)
    {
        pBut->iCmd = 0;
    }
    else
    {
        pBut->iCmd++;
    }

    return pRes;
}

static void Events_ReadConfig(const char *pFileName)
{
    FILE    *fp;
    char    *pBuffer;
    uint8_t i;
    char    *pData;
    uint8_t ipAddress[INET_ADDRSTRLEN];

    memset((void*)ipAddress, 0, sizeof(ipAddress));
    memset(mButtons, 0, sizeof(button_t) * BUTTONS_NB);

    pBuffer = (char*)malloc(LINE_MAX_SIZE);
    fp = fopen(pFileName, "r");

    if(fp<0)
    {
        perror("file read went wrong\n");
        print("file read went wrong\n");
    }

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

        /* Get click debounce value */
        pData = strstr(pBuffer, "click_skip");
        if(pData)
        {
            pData += strlen(pData) + 1;
            mClickSkip = atoi(pData);
            continue;
        }

        /* Get click hold value */
        pData = strstr(pBuffer, "click_hold");
        if(pData)
        {
            pData += strlen(pData) + 1;
            mClickHold = atoi(pData);
            continue;
        }

        /* Get encoder hold value */
        pData = strstr(pBuffer, "encoder_hold");
        if(pData)
        {
            pData += strlen(pData) + 1;
            mEncoderHold = atoi(pData);
            continue;
        }

        /* Parse button entry */
        pData = strstr(pBuffer, "button");
        if(pData)
        {
            pData += strlen(pData) + 1; /* skip 'button:' */
            mButtons[mButtonsNb].gpio = atoi(pData); /* get pin number */
            pinMode(mButtons[mButtonsNb].gpio, INPUT) ;
            wiringPiISR(mButtons[mButtonsNb].gpio, INT_EDGE_FALLING,
                aSupportedIsrHandlers[mButtons[mButtonsNb].gpio]);
            pData += strlen(pData) + 1; /* skip 'pin number:' */
            strcpy(mButtons[mButtonsNb].action, pData); /* get command(s) */

            {
                char *pStr;

                pStr = mButtons[mButtonsNb].action;
                mButtons[mButtonsNb].pCommands[mButtons[mButtonsNb].iCmd] = pStr;
                while(*pStr)
                {
                    if(*pStr == '>')
                    {
                        *pStr = 0;
                        mButtons[mButtonsNb].iCmd++;
                        mButtons[mButtonsNb].pCommands[mButtons[mButtonsNb].iCmd] = pStr + 1;
                    }

                    /* Don't exceed available array */
                    if(mButtons[mButtonsNb].iCmd >= MAX_COMMANDS) break;

                    pStr++;
                }

                /* Set the number of commands */
                mButtons[mButtonsNb].nbCmd = mButtons[mButtonsNb].iCmd + 1;
            }

            mButtonsNb++;
            continue;
        }

        /* Parse rotary encoder entry */
        pData = strstr(pBuffer, "encoder");
        if(pData)
        {
            pData += strlen(pData) + 1; /* skip 'encoder:' */
            pData += strlen(pData) + 1; /* skip 'sl:' */
            mEncoders[mEncodersNb].skipTimesLeft = atoi(pData);
            pData += strlen(pData) + 1; /* skip value: */
            pData += strlen(pData) + 1; /* skip 'sr:' */
            mEncoders[mEncodersNb].skipTimesRight = atoi(pData);
            pData += strlen(pData) + 1; /* skip value: */
            pData += strlen(pData) + 1; /* skip 'CLK:' */
            mEncoders[mEncodersNb].gpioClk = atoi(pData); /* get CLK pin(11) */
            pinMode(mEncoders[mEncodersNb].gpioClk, INPUT) ;
            wiringPiISR(mEncoders[mEncodersNb].gpioClk, INT_EDGE_BOTH,
                aSupportedIsrHandlers[mEncoders[mEncodersNb].gpioClk]);
            pData += strlen(pData) + 1; /* skip value: */
            pData += strlen(pData) + 1; /* skip DT: */
            mEncoders[mEncodersNb].gpioDt = atoi(pData); /* get DT pin(9) */
            pinMode(mEncoders[mEncodersNb].gpioDt, INPUT) ;
            wiringPiISR(mEncoders[mEncodersNb].gpioDt, INT_EDGE_BOTH,
                aSupportedIsrHandlers[mEncoders[mEncodersNb].gpioDt]);
            pData += strlen(pData) + 1; /* skip '9' */
            strcpy(mEncoders[mEncodersNb].actionLeft, pData); /* get left command(s) */
            pData += strlen(pData) + 1; /* skip left command(s) */
            strcpy(mEncoders[mEncodersNb].actionRight, pData); /* get right command(s) */

            mEncodersNb++;
            continue;

        }
    }

    free(pBuffer);
    fclose(fp);
}
