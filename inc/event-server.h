#ifndef _EVENT_SERVER_H_
#define _EVENT_SERVER_H_


extern void EventServer_Init();
extern void EventServer_UnInit();

/* Not the best module for the following functions */
extern void System_VolumeUpdate(char *pComm);
extern void System_RSSIUpdate(uint16_t *regs);

#endif /* _EVENT_SERVER_H_ */
