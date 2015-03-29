#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "wiringPi.h"
#include "wiringPiI2C.h"

#include "common.h"
#include "si4703.h"
#include "radio.h"

#define PI2C_BUS0   0 /*< P5 header I2C bus */
#define PI2C_BUS1   1 /*< P1 header I2C bus */
#define PI2C_BUS    PI2C_BUS1 // default bus


static int      si_band[3][2] = { {8750, 10800}, {7600, 10800}, {7600, 9000}};
static int      si_space[3] = { 20, 10, 5 };
static sem_t    si_sem;
static uint16_t si_regs[16];
bool_t          gFrequencyChanged = TRUE;
static int      i2c_bus[2] = { -1, -1 };



int pi2c_open(uint8_t bus)
{
    int res = -1;
    char bus_name[64];

    if (bus <= PI2C_BUS1)
    {
        // already opened?
        if (i2c_bus[bus] < 0)
        {
            // open i2c bus and store file descriptor
            sprintf(bus_name, "/dev/i2c-%u", bus);
            if ((i2c_bus[bus] = open(bus_name, O_RDWR)) < 0)
            {
                res = 0;
            }
            else
            {
                res = -1;
            }
        }
        else
        {
            res = 0;
        }
    }

    return res;
}

/* close I2C bus file descriptor */
int pi2c_close(uint8_t bus)
{
    if (bus > PI2C_BUS1)
        return -1;
    if (i2c_bus[bus] >= 0)
        close(i2c_bus[bus]);
    i2c_bus[bus] = -1;
    return 0;
}

/* select I2C device for pi2c_write() calls */
int pi2c_select(uint8_t bus, uint8_t slave)
{
    if ((bus > PI2C_BUS1) || (i2c_bus[bus] < 0))
        return -1;
    return ioctl(i2c_bus[bus], I2C_SLAVE, slave);
}

/* write to I2C device selected by pi2c_select() */
int pi2c_write(uint8_t bus, const uint8_t *data, uint32_t len)
{
    if ((bus > PI2C_BUS1) || (i2c_bus[bus] < 0))
        return -1;
    if (write(i2c_bus[bus], data, len) != (ssize_t)len)
        return -1;
    return 0;
}

/* read I2C device selected by pi2c_select() */
int pi2c_read(uint8_t bus, uint8_t *data, uint32_t len)
{
    if ((bus > PI2C_BUS1) || (i2c_bus[bus] < 0))
        return -1;
    if (read(i2c_bus[bus], data, len) != (ssize_t)len)
        return -1;
    return 0;
}


int si_read_regs()
{
    uint8_t buf[32];
    int i, x;
    sem_wait(&si_sem);
    if (pi2c_read(PI2C_BUS, buf, 32) < 0)
    {
        sem_post(&si_sem);
        return -1;
    }
    sem_post(&si_sem);

    // Si4703 sends back registers as 10, 11, 12, 13, 14, 15, 0, ...
    // so we need to shuffle our buffer a bit
    for(i = 0, x = 10; x != 9; x++, i += 2) {
        x &= 0x0F;
        si_regs[x] = buf[i] << 8;
        si_regs[x] |= buf[i+1];
    }
    return 0;
}

int si_update()
{
    int i = 0, ret = 0, reg;
    uint8_t buf[32];
    // write automatically begins at register 0x02
    for(reg = 2 ; reg < 8 ; reg++) {
        buf[i++] = si_regs[reg] >> 8;
        buf[i++] = si_regs[reg] & 0x00FF;
    }
    sem_wait(&si_sem);
    ret = pi2c_write(PI2C_BUS, buf, i);
    sem_post(&si_sem);

    return ret;
}

void si_power(uint16_t mode)
{
    if(mode == PWR_ENABLE)
    {
        sem_init(&si_sem, 0, 1);

        /* GPIO initialization using wiringPi */
        {
            pinMode(SI4703_RESET, OUTPUT);
            pinMode(SI4703_SDA, OUTPUT);

            digitalWrite(SI4703_SDA, LOW);
            digitalWrite(SI4703_RESET, LOW);

            delay(1200);

            digitalWrite(SI4703_RESET, HIGH);

            delay(1200);

            pinModeAlt(SI4703_SCL, FSEL_ALT0);
            pinModeAlt(SI4703_SDA, FSEL_ALT0);
        }

        pi2c_open(PI2C_BUS);
        pi2c_select(PI2C_BUS, SI4703_ADDR);
        if (si_read_regs() != 0) {
            printf("Unable to read Si4703!\n");
            return;
        }

        si_regs[TEST1] = 0xC100;
        si_update();
        delay(1500);

        si_read_regs();
        // set only ENABLE bit, leaving device muted
        si_regs[POWERCFG] = PWR_ENABLE;
        // by default we allow wrap by not setting SKMODE
        // regs[POWERCFG] |= SKMODE;
        si_regs[SYSCONF1] |= RDS | RDSIEN | (1<<2); // enable RDS
        // set mono/stereo blend adjust to default 0 or 31-49 RSSI
        si_regs[SYSCONF1] &= ~BLNDADJ;
        // set different BLDADJ if needed
        // x00=31-49, x40=37-55, x80=19-37,xC0=25-43 RSSI
        // regs[SYSCONF1] |= 0x0080;
        // enable RDS High-Performance Mode
        si_regs[SYSCONF3] |= RDSPRF;
        si_regs[SYSCONF1] |= DE; // set 50us De-Emphasis for Europe, skip for USA
        // select general Europe/USA 87.5 - 108 MHz band
        si_regs[SYSCONF2] = BAND0 | SPACE100; // 100kHz channel spacing for Europe
        // apply recommended seek settings for "most stations"
        si_regs[SYSCONF2] &= ~SEEKTH;
        si_regs[SYSCONF2] |= 0x0C00; // SEEKTH 12
        si_regs[SYSCONF3] &= 0xFF00;
        si_regs[SYSCONF3] |= 0x004F; // SKSNR 4, SKCNT 15
    }
    else {
        // power down condition
        si_regs[POWERCFG] = PWR_DISABLE | PWR_ENABLE;
    }

    si_update();

    // recommended powerup time
    delay(1500);

    si_read_regs();
}


void si_set_volume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 15) volume = 15;
    if (volume)
        si_regs[POWERCFG] |= DSMUTE | DMUTE; // unmute
    else
        si_regs[POWERCFG] &= ~(DSMUTE | DMUTE); // mute
    si_regs[SYSCONF2] &= ~VOLUME; // clear volume bits
    si_regs[SYSCONF2] |= volume; // set new volume
    si_update();
}

void si_set_channel(int chan)
{
    int band = (si_regs[SYSCONF2] >> 6) & 0x03;
    int space = (si_regs[SYSCONF2] >> 4) & 0x03;
    int nchan = (si_band[band][1] - si_band[band][0])/si_space[space];
    if (chan > nchan) chan = nchan;
    si_regs[CHANNEL] &= 0xFC00;
    si_regs[CHANNEL] |= chan;
    si_regs[CHANNEL] |= TUNE;
    si_update();
    delay(10);

    // poll to see if STC is set
    int i = 0;
    while(i++ < 100) {
        si_read_regs();
        if (si_regs[STATUSRSSI] & STC) break;
        delay(10);
    }
    si_regs[CHANNEL] &= ~TUNE;
    si_update();
    // wait for the si4703 to clear the STC as well
    i = 0;
    while(i++ < 100) {
        si_read_regs();
        if (!(si_regs[STATUSRSSI] & STC)) break;
        delay(10);
    }

    gFrequencyChanged = TRUE;
}

// freq: 9500 for 95.00 MHz
void si_tune(int freq)
{
    si_read_regs();
    int band = (si_regs[SYSCONF2] >> 6) & 0x03;
    int space = (si_regs[SYSCONF2] >> 4) & 0x03;
    if (freq < si_band[band][0]) freq = si_band[band][0];
    if (freq > si_band[band][1]) freq = si_band[band][1];
    int nchan = (freq - si_band[band][0])/si_space[space];
    si_set_channel(nchan);
    gFrequencyChanged = TRUE;
}

int si_get_freq()
{
    int band = (si_regs[SYSCONF2] >> 6) & 0x03;
    int space = (si_regs[SYSCONF2] >> 4) & 0x03;
    int nchan = si_regs[READCHAN] & 0x03FF;
    int freq = nchan*si_space[space] + si_band[band][0];
    return freq;
}

void si_rds_update(rdsData_t *pRdsData, rdsStatus_t *pStatus)
{
    char *pText;
    bool newPS, newRT;

    pStatus->radiotext = FALSE;
    pStatus->stationName = FALSE;

#if 1
    pText = pRdsData->radioText;
    //memset(text, 0, 65);
    if(gFrequencyChanged)
    {
        memset(&pRdsData->programServiceName, ' ', sizeof(pRdsData->programServiceName));
        memset(&pRdsData->radioText, ' ', sizeof(pRdsData->radioText));
        pRdsData->programServiceName[sizeof(pRdsData->programServiceName) - 1] = 0;
        pRdsData->radioText[sizeof(pRdsData->radioText) - 1] = 0;
        gFrequencyChanged = FALSE;
        pStatus->radiotext = TRUE;
        pStatus->stationName = TRUE;
    }

    si_read_regs();

    /* Check RDS */
    if((si_regs[STATUSRSSI] & RDSR) &&
        (((si_regs[STATUSRSSI] & BLERA) >> 9) < 3) &&
        (((si_regs[READCHAN] & BLERB) >> 14) < 3) &&
        (((si_regs[READCHAN] & BLERC) >> 12) < 3) &&
        ((si_regs[READCHAN] & BLERA) >> 10) < 3
    )
    {
        uint16_t gt  = (si_regs[RDSB] >> 12) & 0x0F; // group type
        uint16_t ver = (si_regs[RDSB] >> 11) & 0x01; // version
        uint16_t idx;
        uint8_t ch;

        newPS = newRT = false;

        //print("%d group ready\n", gt);
        switch(gt)
        {
            /* Basic tuning and switching information */
            case 0:
            {
                idx = (si_regs[RDSB] & 0x03); // PS name

                /* group 0A */
                if(ver == 0)
                {
                    ch = si_regs[RDSB] >> 8;
                    pRdsData->alternativeFreq[0] = ch;
                    ch = si_regs[RDSB] & 0xFF;
                    pRdsData->alternativeFreq[1] = ch;
                }
                /* group 0B */
                else
                {
                    /* New text come */
                    /*if(idx == 0)
                            {
                                memset(mRdsData.programServiceName, 0, 9);
                            }*/


                }
                /*uint8_t countryCode = si_regs[RDSA] >> 12;
                uint8_t programType = (si_regs[RDSA] >> 8) & 0x0F;
                uint8_t programRefNumber = si_regs[RDSB] & 0xFF;*/
                /* Common 0A and 0B */
                ch = si_regs[RDSD] >> 8;
                if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '.' || ch == ' ')
                    pRdsData->programServiceName[idx * 2] = ch;
                //mTmpPSName[miTmpPSName][idx * 2] = ch;
                ch = si_regs[RDSD] & 0xFF;
                if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '.' || ch == ' ')
                    pRdsData->programServiceName[idx * 2 + 1] = ch;
                //mTmpPSName[miTmpPSName][idx * 2 + 1] = ch;

                /* print("%d <%c%c><0x%02x,0x%02x> country code: 0x%02x, program type: %d, ref: %d, %d\n",
                            idx, si_regs[RDSD] >> 8, si_regs[RDSD] & 0xFF, si_regs[RDSD] >> 8,
                            si_regs[RDSD] & 0xFF, countryCode, programType, programRefNumber,
                            (si_regs[RDSB] >> 2) & 0x01);*/
                newPS = true;

                //print("Program: [% 8s] [%s]\n", mRdsData.programServiceName, mRdsData.radioText);
            }
            break;
            case 1:
                break;
            case 2:
                idx = si_regs[RDSB] & 0x0F;

                /* group 2A */
                if(ver == 0)
                {
                    ch = si_regs[RDSC]>>8;
                    if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '.')
                        pText[idx * 4] = ch;
                    ch = si_regs[RDSC] & 0xFF;
                    if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '.')
                        pText[idx * 4 + 1] = ch;
                    ch = si_regs[RDSD]>>8;
                    if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '.')
                        pText[idx * 4 + 2] = ch;
                    ch = si_regs[RDSD] & 0xFF;
                    if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '.')
                        pText[idx * 4 + 3] = ch;
                    newRT = TRUE;
                }
                /* group 2B */
                else
                {

                }
                break;
            case 14:
                break;
            case 15:
                break;
        }

        {
            /*print("%d %d %d %d\n",
                        (si_regs[STATUSRSSI] & BLERA) >> 9,
                        (si_regs[READCHAN] & BLERB) >> 14,
                        (si_regs[READCHAN] & BLERC) >> 12,
                        (si_regs[READCHAN] & BLERA) >> 10);*/


            /* RadioText is ended with 0x0D('\r') -> remove it */
            if(pRdsData->radioText[strlen(pRdsData->radioText) - 1] == '\r')
            {
                pRdsData->radioText[strlen(pRdsData->radioText) - 1] = 0;
            }

            if(newRT)
            {
                pStatus->radiotext = TRUE;
            }

            if(newPS)
            {
                pStatus->stationName = TRUE;
            }
        }
    }

    //System_RSSIUpdate(si_regs);
#endif
}


void si_mute(int mode)
{
    if (!mode)
        si_regs[POWERCFG] |= DSMUTE | DMUTE; // unmute
    else
        si_regs[POWERCFG] &= ~(DSMUTE | DMUTE); // mute

    si_update();
}

int si_get_volume()
{
    return si_regs[SYSCONF2]&VOLUME;
}


int si_get_rssi()
{
    return si_regs[STATUSRSSI] & RSSI;
}

int si_seek(int dir)
{
	si_read_regs();

	int channel = si_regs[READCHAN] & RCHAN; // current channel
	if(dir == SEEK_DOWN)
	    si_regs[POWERCFG] &= ~SEEKUP; //Seek down is the default upon reset
	else
	    si_regs[POWERCFG] |= SEEKUP; //Set the bit to seek up

	si_regs[POWERCFG] |= SEEK; //Start seek
	si_update(); //Seeking will now start

	//Poll to see if STC is set
	int i = 0;
	while(i++ < 500) {
		si_read_regs();

		if(i%4 == 1)
		{
		    /* Send frequency updates */
		    Radio_FrequencyUpdate(FALSE);
		}

		if((si_regs[STATUSRSSI] & STC) != 0) break; //Tuning complete!
		delay(10);
	}

	si_read_regs();
	int valueSFBL = si_regs[STATUSRSSI] & SFBL; //Store the value of SFBL
	si_regs[POWERCFG] &= ~SEEK; //Clear the seek bit after seek has completed
	si_update();

	//Wait for the si4703 to clear the STC as well
	i = 0;
	while(i++ < 500) {
		si_read_regs();
		if( (si_regs[STATUSRSSI] & STC) == 0) break; //Tuning complete!
		delay(10);
	}

	if (channel == (si_regs[READCHAN] & RCHAN))
		return 0;
	if (valueSFBL) //The bit was set indicating we hit a band limit or failed to find a station
		return 0;

	gFrequencyChanged = TRUE;
	return si_get_freq();
}
