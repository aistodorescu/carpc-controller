#ifndef _COMMON_H_
#define _COMMON_H_

#include "config.h"
//#include <rpc/types.h>

#ifdef __cplusplus
extern "C" {
#endif



/*
 * Defines
 */
#define LEFT                (0)
#define RIGHT               (1)

#ifndef __int8_t_defined
# define __int8_t_defined
typedef signed char     int8_t;
typedef short int       int16_t;
typedef int         int32_t;
# if __WORDSIZE == 64
typedef long int        int64_t;
# else
__extension__
typedef long long int       int64_t;
# endif
#endif

/* Unsigned.  */
typedef unsigned char       uint8_t;
typedef unsigned short int  uint16_t;
#ifndef __uint32_t_defined
typedef unsigned int        uint32_t;
# define __uint32_t_defined
#endif
#if __WORDSIZE == 64
typedef unsigned long int   uint64_t;
#else
__extension__
typedef unsigned long long int  uint64_t;
#endif

#ifndef bool_t
#   define bool_t uint8_t
#endif

#ifndef TRUE
#   define TRUE     1
#endif

#ifndef FALSE
#   define FALSE    0
#endif

/*
 * Typedefs
 */
typedef enum systemModes_tag {
    gModeXBMC_c = 0x00,
    gModeRadio_c = 0x01
} systemModes_t;

typedef struct encoder_tag {
    char actionLeft[ACTION_MAX_SIZE];
    char actionRight[ACTION_MAX_SIZE];
    uint8_t gpioClk;
    uint8_t gpioDt;
    uint8_t dtNew, dtOld, clkNew, clkOld, valOld, valNew;
    uint8_t changed, direction;
    uint8_t skipTimesLeft;
    uint8_t leftSteps;
    uint8_t skipTimesRight;
    uint8_t rightSteps;
} encoder_t;

typedef struct button_tag {
    char action[ACTION_MAX_SIZE];
    char *pCommands[MAX_COMMANDS]; /* Pointers to commands groups */
    uint8_t iCmd; /* Current index of command group */
    uint8_t nbCmd; /* Number of command groups */
    uint8_t gpio;
    uint8_t clickTimes;
    uint8_t clickHold;
    uint8_t click;
} button_t;



#ifdef __cplusplus
}
#endif

#endif /* _COMMON_H_ */
