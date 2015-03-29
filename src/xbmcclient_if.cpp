#include <semaphore.h>

#include "xbmcclient.h"


static int sockXbmc;
static CAddress my_addr; // Address => localhost on 9777
static sem_t xi_sem;


void XBMC_ClientInit(const char *pMessage, const char *pImg)
{
    /* Initialize semaphore */
    sem_init(&xi_sem, 0, 1);

	/* Create Xbmc UDP connection */
	sockXbmc = socket(AF_INET, SOCK_DGRAM, 0);
	my_addr.Bind(sockXbmc);

	CPacketHELO HeloPackage(pMessage, ICON_PNG, pImg);
	HeloPackage.Send(sockXbmc, my_addr);
}

void XBMC_ClientUnInit()
{
    sem_wait(&xi_sem);

    // BYE is not required since XBMC would have shut down
    CPacketBYE bye;
    bye.Send(sockXbmc, my_addr);

    shutdown(sockXbmc, SHUT_RDWR);
    sem_post(&xi_sem);

    sem_destroy(&xi_sem);
}

void XBMC_ClientButton(const char *pComm, const char *pType)
{
    sem_wait(&xi_sem);
	CPacketBUTTON btn(pComm, pType, BTN_DOWN | BTN_NO_REPEAT | BTN_QUEUE);
	btn.Send(sockXbmc, my_addr);
	sem_post(&xi_sem);
}

void XBMC_ClientAction(const char *pComm)
{
    sem_wait(&xi_sem);
	CPacketACTION packAction(pComm, ACTION_EXECBUILTIN);
	packAction.Send(sockXbmc, my_addr);
	sem_post(&xi_sem);
}
