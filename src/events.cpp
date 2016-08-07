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

#include "events.h"
#include "config.h"
#include "common.h"
#include "commands.h"
#include "settings.h"

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
        printf("----------------%d\n", pin);\
        res = mq_send(mEventsMessageQueueFd, (char*)&event, sizeof(event), 1);\
        if(res < 0)\
        {\
            printf("error fd %d\n", mEventsMessageQueueFd);\
        }\
    }\
}

/*
 * Typedefs
 */


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

ISR(0); ISR(1); ISR(2); ISR(3); ISR(4); ISR(5); ISR(6); ISR(7); ISR(8); ISR(9);
ISR(10); ISR(11); ISR(12); ISR(13); ISR(14); ISR(15); ISR(16); ISR(17); ISR(18);
ISR(19);ISR(20);ISR(21);ISR(22);ISR(23);ISR(24); ISR(25);ISR(26);ISR(27);
static evIsrFp_t aSupportedIsrHandlers[] =
{
    ISR0_Handler, ISR1_Handler, ISR2_Handler, ISR3_Handler, ISR4_Handler,
    ISR5_Handler, ISR6_Handler, ISR7_Handler, ISR8_Handler, ISR9_Handler,
    ISR10_Handler, ISR11_Handler, ISR12_Handler, ISR13_Handler, ISR14_Handler,
    ISR15_Handler, ISR16_Handler, ISR17_Handler, ISR18_Handler, ISR19_Handler,
    ISR20_Handler, ISR21_Handler, ISR22_Handler, ISR23_Handler,
    ISR24_Handler, ISR25_Handler, ISR26_Handler, ISR27_Handler
};
static settings_t *mpSettings = NULL;

/*
 * Global Variables
 */


/*
 * Public Functions Definitions
 */
static void ISR_RDS_Handler(void)
{
    printf("----------------RDS----------------\n");
    Commands_Signal((char*)"interrupt_rds");
}

void Events_Init(settings_t *pSettings)
{
    uint8_t idx = 0;

    printf("Starting Events Module\n");

    /* Update buttons handlers */
    for(idx=0;idx<pSettings->buttonsNb;idx++)
    {
        printf("button: %d\n", pSettings->buttons[idx].gpio);
        pinMode(pSettings->buttons[idx].gpio, INPUT);
        wiringPiISR(pSettings->buttons[idx].gpio, INT_EDGE_FALLING,
            aSupportedIsrHandlers[pSettings->buttons[idx].gpio]);
    }

    for(idx=0;idx<pSettings->encodersNb;idx++)
    {
        printf("clk: %d\n", pSettings->encoders[idx].gpioClk);
        pinMode(pSettings->encoders[idx].gpioClk, INPUT);
        wiringPiISR(pSettings->encoders[idx].gpioClk, INT_EDGE_BOTH,
        aSupportedIsrHandlers[pSettings->encoders[idx].gpioClk]);

        printf("dt: %d\n", pSettings->encoders[idx].gpioDt);
        pinMode(pSettings->encoders[idx].gpioDt, INPUT);
        wiringPiISR(pSettings->encoders[idx].gpioDt, INT_EDGE_BOTH,
        aSupportedIsrHandlers[pSettings->encoders[idx].gpioDt]);
    }

    /* Create message queue */
    mEventsMessageQueueBuf.mq_msgsize = 512;
    mEventsMessageQueueBuf.mq_maxmsg = 32;
    mEventsMessageQueueFd = mq_open("/QCarPcEvents", O_CREAT | O_RDWR, 600,
        &mEventsMessageQueueBuf);

    if(mEventsMessageQueueFd < 0)
    {
        perror("Error");
    }
    else
    {
        printf("init ok %d\n", mEventsMessageQueueFd);

        mpSettings = pSettings;

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

evIsrFp_t *Events_GetHandlers()
{
	return aSupportedIsrHandlers;
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

            //printf("%d received\n", event);
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
            printf("received: %d\n", ret);
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

    for(i=0;i<mpSettings->buttonsNb;i++)
    {
        if(mpSettings->buttons[i].gpio == gpio)
        {
            mpSettings->buttons[i].clickTimes++;

            /* Treat just one click from 7(CLICK_DEBOUNCE) */
            if(mpSettings->buttons[i].clickTimes >= mpSettings->clickSkip)
            {
                char *pComm;

                /* Get the next group of commands(in case there are more groups of commands) */
                pComm = Events_ButGetNextCommand((button_t*)(mpSettings->buttons + i));
                printf("%s\n", pComm);
                /* Execute commands in a group of commands */
                Commands_Signal(pComm);
                res = TRUE;
                mpSettings->buttons[i].clickTimes = 0;
                mpSettings->buttons[i].clickHold++;
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

    for(i=0;i<mpSettings->encodersNb;i++)
    {
        /* Rotary Encoder */
            mpSettings->encoders[i].dtNew = digitalRead(mpSettings->encoders[i].gpioDt);
            mpSettings->encoders[i].clkNew = digitalRead(mpSettings->encoders[i].gpioClk);
            mpSettings->encoders[i].valNew = grayToBinary((mpSettings->encoders[i].clkNew << 1) | mpSettings->encoders[i].dtNew);

            /* New values arrived */
            if(mpSettings->encoders[i].valNew != mpSettings->encoders[i].valOld)
            {
                /* Update movement once in 4 changes(and not for every value change), to avoid unneeded
                 * changes */
                /* LEFT */
                if(mpSettings->encoders[i].valOld == 0 && mpSettings->encoders[i].valNew == 1)
                {
                    mpSettings->encoders[i].direction = RIGHT;
                    mpSettings->encoders[i].changed = 1;
                }
                else if(mpSettings->encoders[i].valOld == 1 && mpSettings->encoders[i].valNew == 2)
                {
                    mpSettings->encoders[i].direction = RIGHT;
                    mpSettings->encoders[i].changed = 1;
                }
                else if(mpSettings->encoders[i].valOld == 2 && mpSettings->encoders[i].valNew == 3)
                {
                    mpSettings->encoders[i].direction = RIGHT;
                    mpSettings->encoders[i].changed = 1;
                }
                else if(mpSettings->encoders[i].valOld == 3 && mpSettings->encoders[i].valNew == 0)
                {
                    mpSettings->encoders[i].direction = RIGHT;
                    mpSettings->encoders[i].changed = 1;
                }
                /* RIGHT */
                else if(mpSettings->encoders[i].valOld == 0 && mpSettings->encoders[i].valNew == 3)
                {
                    mpSettings->encoders[i].direction = LEFT;
                    mpSettings->encoders[i].changed = 1;
                }
                else if(mpSettings->encoders[i].valOld == 3 && mpSettings->encoders[i].valNew == 2)
                {
                    mpSettings->encoders[i].direction = LEFT;
                    mpSettings->encoders[i].changed = 1;
                }
                else if(mpSettings->encoders[i].valOld == 2 && mpSettings->encoders[i].valNew == 1)
                {
                    mpSettings->encoders[i].direction = LEFT;
                    mpSettings->encoders[i].changed = 1;
                }
                else if(mpSettings->encoders[i].valOld == 1 && mpSettings->encoders[i].valNew == 0)
                {
                    mpSettings->encoders[i].direction = LEFT;
                    mpSettings->encoders[i].changed = 1;
                }

                //printf("direction:%d, changed:%d, old:%d, new:%d\n", mpSettings->encoders[i].direction,mpSettings->encoders[i].changed,
                //    mpSettings->encoders[i].valOld, mpSettings->encoders[i].valNew);

                /* Check if we have a new movement */
                if(mpSettings->encoders[i].changed)
                {
                    /* Treat the LEFT movement */
                    if(mpSettings->encoders[i].direction == LEFT)
                    {
                        /* If we haven't reached the number of steps to be ignored just increase the
                         * current step value */
                        if(mpSettings->encoders[i].leftSteps < mpSettings->encoders[i].skipTimesLeft)
                        {
                            //printf("increase left step: %d\n", mpSettings->encoders[i].leftSteps);
                            mpSettings->encoders[i].leftSteps++;
                        }
                        /* If we have reached the number of steps to be skipped then reset the current
                         * step number and execute the assigned commands */
                        else
                        {
                            mpSettings->encoders[i].leftSteps = 0; /* Reset value */
                            Commands_Signal((char*)mpSettings->encoders[i].actionLeft);
                            res = TRUE;
                            //usleep(mEncoderHold);
                        }
                    }
                    /* Treat the RIGHT movement */
                    else
                    {
                        /* If we haven't reached the number of steps to be ignored just increase the
                         * current step value */
                        if(mpSettings->encoders[i].rightSteps < mpSettings->encoders[i].skipTimesRight)
                        {
                            //printf("increase right step: %d\n", mpSettings->encoders[i].rightSteps);
                            mpSettings->encoders[i].rightSteps++;
                        }
                        /* If we have reached the number of steps to be skipped then reset the current
                         * step number and execute the assigned commands */
                        else
                        {
                            mpSettings->encoders[i].rightSteps = 0; /* Reset value */
                            Commands_Signal((char*)mpSettings->encoders[i].actionRight);
                            res = TRUE;
                            //usleep(mEncoderHold);
                        }
                    }

                    mpSettings->encoders[i].changed = 0;
                }

                /* Update old value */
                mpSettings->encoders[i].valOld = mpSettings->encoders[i].valNew;

                break;
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

