#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rpc/types.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

#include "wiringPi.h"
#include "xbmcclient_if.h"

#include "events.h"
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
static void intHandler(int dmmy);



/*
 * Local Variables
 */
static bool_t   mForceStop = FALSE;


/*
 * Global Variables
 */
uint8_t         gSystemMode; /* systemModes_t */

/*
 * Public functions definitions
 */
int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("Please provide the path to the configuration file\n");
        printf("usage: %s path/to/configuration_file\n", argv[0]);
    }

    if(!strcmp(argv[1], "--help"))
    {
        printf("CarPC Remote controller\n");
        printf("usage: %s path/to/configuration_file\n\n", argv[0]);
        printf("Author: Andrei Istodorescu\n");
        printf("e-mail: andrei.istodorescu@gmail.com\n");
        printf("web: www.engineering-diy.blogspot.com\n");
        printf("forum: http://engineeringdiy.freeforums.org/\n\n");
    }


    /* Install signal handler */
    signal(SIGINT, intHandler);

    /* Initialize GPIO module */
    //wiringPiSetup();
    wiringPiSetupGpio();

    /* Get configuration from the input file */
    Events_Init(argv[1]);
    //Events_Init("/home/pi/config/gpio_description");

    /* Restore default mode */
    gSystemMode = gModeRadio_c;

    /* XBMC Event Client Module */
    XBMC_ClientInit("CarPC GPIO Controller", (const char*)NULL);

    /* CarPC Event Server Module */
    EventServer_Init();

    /* Commands Module Interface */
    Commands_Init();

    /* Radio Module Interface */
    Radio_Init(RADIO_START_VOL, RADIO_START_FREQ, 35);

    /* Set XBMC mode */
    (gSystemMode == gModeRadio_c)?XBMC_ClientAction("SetProperty(Radio.Active,true,10000)"):
        XBMC_ClientAction("SetProperty(Radio.Active,false,10000)");

    /* Main loop */
    while(!mForceStop)
    {
        /* Reduce CPU usage: sleep 0.5ms */
        //usleep(500);
    	usleep(1000000);
    }

    return 0;
}


/*
 * Private functions definitions
 */
static void intHandler(int dmmy)
{
    print("Interrupt signal caught\n");

    Events_UnInit();
    XBMC_ClientUnInit();
    Commands_UnInit();
    EventServer_UnInit();
    mForceStop = TRUE;
}


