#ifndef _XBMCCLIENT_IF_H_
#define _XBMCCLIENT_IF_H_

void XBMC_ClientInit(const char *pMessage, const char *pImg);
void XBMC_ClientUnInit();
void XBMC_ClientButton(const char *pComm, const char *pType);
void XBMC_ClientAction(const char *pComm);

#endif /* XBMCCLIENT_IF_H_ */
