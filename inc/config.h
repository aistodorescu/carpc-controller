#ifndef _CONFIG_H_
#define _CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/*
 * Defines
 */
#define DEBUG                   (TRUE)
#define MAX_COMMANDS            (5)
#define BUTTONS_NB              (10)
#define ENCODERS_NB             (5)
#define ACTION_MAX_SIZE         (500)
#define LINE_MAX_SIZE           (500)

#define RADIO_START_VOL         (1)
#define RADIO_START_FREQ        (10670)
#define XBMC_START_VOL          (RADIO_START_VOL * 6 + 5)
#define XBMC_VOL_INC            (6)


#define SYS_CMD_VOLUME          "volume"
#define XBMCBUILTIN_CMD         "xbmcbuiltin_"
#define XBMCBUTTON_CMD          "KB_"
#define RADIO_CMD               "radio_"

#define RADIO_CMD_SEEK_DOWN     "radio_seek_down"
#define RADIO_CMD_SEEK_UP       "radio_seek_up"
#define RADIO_CMD_TUNE_DOWN     "radio_tune_down"
#define RADIO_CMD_TUNE_UP       "radio_tune_up"
#define RADIO_CMD_TUNE          "radio_tune_"
#define RADIO_UPDATE_RDS        "radio_update_rds"
#define RADIO_UPDATE_FREQUENCY  "radio_update_frequency"

#define NAV_CMD_UP              "nav_up"
#define NAV_CMD_DOWN            "nav_down"
#define NAV_CMD_LEFT            "nav_left"
#define NAV_CMD_RIGHT           "nav_right"
#define NAV_CMD_ZOOM_IN         "nav_zoom_in"
#define NAV_CMD_ZOOM_OUT        "nav_zoom_out"


#define SYSTEM_MODE_TOGGLE      "system_mode_toggle"

#if DEBUG
#define print                   printf
#else
#define print(...)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _CONFIG_H_ */
