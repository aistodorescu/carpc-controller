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

#include "settings.h"
#include "events.h"
#include "event-server.h"
#include "commands.h"
#include "radio.h"
#include "si4703.h"

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
static settings_t mSettings = {0};

/*
 * Global Variables
 */


/*
 * Public functions definitions
 */
int main(int argc, char *argv[])
{
    settings_t *pSettings = &mSettings;

    if(argc > 1 && !strcmp(argv[1], "--help"))
    {
        printf("CarPC Remote controller\n");
        printf("usage: %s\n", argv[0]);
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

    /* Initialize Events manager */
    Events_Init(pSettings);
    printf("events ok\n");

    /* XBMC Event Client Module */
    XBMC_ClientInit("CarPC GPIO Controller", (const char*)NULL);
    printf("client ok\n");

    /* Commands Module Interface */
    Commands_Init(pSettings);
    printf("commands ok\n");

    /* Radio setup */
    si_init();

    /* CarPC Event Server Module */
    EventServer_Init(pSettings);

    /* Main loop */
    while(!mForceStop)
    {
        /* Reduce CPU usage: sleep 0.5ms */
        //usleep(500);
        usleep(10000000);
    }

    return 0;
}


/*
 * Private functions definitions
 */
static void intHandler(int dmmy)
{
    printf("Interrupt signal caught\n");

    Settings_UnInit();
    Events_UnInit();
    XBMC_ClientUnInit();
    Commands_UnInit();
    EventServer_UnInit();
    mForceStop = TRUE;
}


