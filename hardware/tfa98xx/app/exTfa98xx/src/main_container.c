#include <assert.h>
#include <string.h>
#if defined(WIN32) || defined(_X64)
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <NXP_I2C.h>

#include <pthread.h>

#define LOG_TAG "exTfa98xx"
#include <cutils/log.h>

#ifndef LOGD
#define LOGD(...) ALOGD(__VA_ARGS__)
#endif

#ifndef WIN32
#include <inttypes.h>
#endif
#include "tfa.h" /* top tfa interface */
#include "tfaContUtil.h"
#include "tfa_container.h"
#include "dbgprint.h"
#include "tfaOsal.h" /* tfaReadFile */
#include "tfa98xxLiveData.h"
#include "Tfa98API.h"
#include "tfa_service.h"
#include "tfa98xxCalibration.h"
#include "tfa98xx_cust.h"
#include "exTfa98xx.h"

#ifndef WIN32
#define Sleep(ms) usleep((ms)*1000)
#define _GNU_SOURCE   /* to avoid link issues with sscanf on NxDI? */
#endif

#define MAX_DEVICES 4

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations of helper functions */
static int get_impedance(int dev_idx, float *imp);
static int find_live_data_item_index(Tfa98xx_handle_t handle, const char *item_name);

typedef struct {
    // offsets known from original code
    uint8_t  enabled;               // [0]
    int      unknown1;               // [4]
    uint64_t unknown2[2];             // [8]
    int      unknown3[3];             // [24]
    uint64_t unknown4[2];             // [36]
    uint8_t  pad_to_96[96 - 36 - 16]; // fill up to offset 96 (adjust as needed)
    uint8_t  byte_96;                 // [96]
    float    impedance;               // [100]
    int      mode;                    // [108]
    int      volume;                  // [112]
    int      device_index;            // [116]
    int      flag_120;                // [120]
    uint8_t  byte_124;                // [124]
    // function pointers (offsets 128..151)
    int     (*enable)(unsigned char *ctx, int on);
    int     (*get_impedance)(unsigned char *ctx, float *imp);
    int     (*calibrate_impedance)(unsigned char *ctx, int flag);
    int     (*set_volume_step)(unsigned char *ctx, int left, int right);
    int     (*set_volume_attenuation)(unsigned char *ctx, int left, int right);
    int     (*mute)(unsigned char *ctx, int mute);
    // our internal state – stored after the function pointers (offsets 152+)
    int      calibrated;     // true after cold boot done
    int      prev_mode;      // last mode used
    int      enable_cnt;     // number of times enabled (for logging)
    int      coldboot_cnt;   // number of cold boots performed
    int      coldboot_flag;  // flag indicating cold boot was done
} tfa_context_t;

int load_container_file(char *fname,  nxpTfaContainer_t **buffer) {
	int size;

	size = tfaReadFile(fname, (void**) buffer); //mallocs file buffer

	if ( size==0 )
		printf("error reading %s\n", fname);

	return size;
}

/*------------------------------------------------------------------------------
 * Helper: build volume steps array with per‑device clamping.
 *------------------------------------------------------------------------------*/
static void build_vsteps_with_clamping(int mode, int requested_step, int vsteps[MAX_DEVICES]) {
    int maxdev = tfa98xx_cnt_max_device();
    for (int i = 0; i < maxdev && i < MAX_DEVICES; i++) {
        int max = tfacont_get_max_vstep(i, mode);
        if (max <= 0) {
            vsteps[i] = 0;
        } else if (requested_step >= max) {
            vsteps[i] = max - 1;          // clamp to max-1
        } else {
            vsteps[i] = requested_step;
        }
    }
}

/*------------------------------------------------------------------------------
 * Improved recordLiveData: obtains the index of the requested live data item
 * per device, then reads and prints the value for each device.
 *------------------------------------------------------------------------------*/
enum tfa_error recordLiveData(char* item_name) {
	Tfa98xx_Error_t err = Tfa98xx_Error_Ok;
	Tfa98xx_handle_t handles[MAX_DEVICES];
	unsigned char slaveAddress;
	int i, maxdev = tfa98xx_cnt_max_device();
	int item_index[MAX_DEVICES];          /* index of the item for each device */
	float live_data[MEMTRACK_MAX_WORDS] = {0};
	int nr_of_items;

	if (maxdev > MAX_DEVICES)
		maxdev = MAX_DEVICES;

	/* Open all devices */
	for (i = 0; i < maxdev; i++) {
		tfaContGetSlave(i, &slaveAddress);
		err = Tfa98xx_Open(slaveAddress * 2, &handles[i]);
		if (err != Tfa98xx_Error_Ok) {
			ALOGE("Failed to open device %d (addr 0x%02x)", i, slaveAddress);
			goto error_close;
		}
	}

	/* For each device, find the index of the requested live data item */
	for (i = 0; i < maxdev; i++) {
		item_index[i] = find_live_data_item_index(handles[i], item_name);
		if (item_index[i] < 0) {
			ALOGE("Item '%s' not found for device %d", item_name, i);
			err = Tfa98xx_Error_Bad_Parameter;
			goto error_close;
		}
	}

	/* Enable live data streaming */
	err = tfa98xx_set_live_data(handles);
	if (err != Tfa98xx_Error_Ok) {
		ALOGE("tfa98xx_set_live_data failed: %d", err);
		goto error_close;
	}

	/* Read and print the live data value for each device */
	for (i = 0; i < maxdev; i++) {
		tfaContGetSlave(i, &slaveAddress);
		err = tfa98xx_get_live_data(handles, i, live_data, &nr_of_items);
		if (err != Tfa98xx_Error_Ok) {
			ALOGE("tfa98xx_get_live_data failed for device %d: %d", i, err);
			goto error_close;
		}
		if (item_index[i] < nr_of_items) {
			printf("%s [0x%02x]: %f\n", item_name, slaveAddress, live_data[item_index[i]]);
		} else {
			ALOGE("Item index %d out of range (max %d) for device %d",
			      item_index[i], nr_of_items-1, i);
		}
	}

error_close:
	for (i = 0; i < maxdev; i++) {
		if (handles[i] != -1)
			Tfa98xx_Close(handles[i]);
	}
	return (enum tfa_error)err;
}

/*------------------------------------------------------------------------------
 * Helper: find the index of a named live data item for a given device handle.
 * Returns the index (>=0) on success, -1 on failure.
 *------------------------------------------------------------------------------*/
static int find_live_data_item_index(Tfa98xx_handle_t handle, const char *item_name) {
	char buffer[256];
	char *item;
	int idx = 0;

	buffer[0] = '\0';
	/* Get the comma-separated list of live data items for this device.
	 * Note: tfa98xx_get_live_data_items() expects an array of handles,
	 * but we only care about the first device. We'll pass a temporary array.
	 */
	Tfa98xx_handle_t handles[2] = {handle, -1};
	Tfa98xx_Error_t err = tfa98xx_get_live_data_items(handles, 0, buffer);
	if (err != Tfa98xx_Error_Ok) {
		ALOGE("tfa98xx_get_live_data_items failed: %d", err);
		return -1;
	}

	/* Tokenize the string (it is comma-separated) */
	item = strtok(buffer, ",");
	while (item) {
		if (strstr(item, item_name) != NULL) {
			return idx;
		}
		idx++;
		item = strtok(NULL, ",");
	}
	return -1;
}

/*------------------------------------------------------------------------------
 * Replacement for the original tfa_enable function.
 * Uses the exTfa98xx API to control NXP TFA98xx devices.
 *
 * Parameters:
 *   ctx - pointer to a context structure (must contain at least):
 *         byte 0:    enabled flag (1 = on, 0 = off)
 *         float at offset 100: impedance value (initialized to -1.0)
 *         int at offset 108:   audio mode (profile)
 *         int at offset 112:   volume step
 *         int at offset 116:   device index (default 13, overwritten later)
 *         int at offset 120:   flag (0 = attenuation only, non-zero = volume step)
 *   on  - non-zero to turn on, zero to turn off
 *
 * Returns 0 on success, non-zero on error.
 *------------------------------------------------------------------------------*/
int tfa_enable(unsigned char *ctx, int on) {
    tfa_context_t *tctx = (tfa_context_t*)ctx;
    int ret = 0;
    float impedance_val;
    int i;

    ALOGD("[NXP] %s: turning tfa %s (dev_idx=%d)\n", "tfa_enable",
          on ? "on" : "off", tctx->device_index);

    if (on) {
        // ----- TURN ON -----
        if (tctx->enabled && tctx->mode == tctx->prev_mode) {
            ALOGD("[NXP] %s: Tfa is already turned on", "tfa_enable");
            return 0;
        }

        tctx->enable_cnt++;
        ALOGD("[NXP] %s: tfa_log_enable_cnt = %d\n", "tfa_enable", tctx->enable_cnt);

        if (!tctx->calibrated) {
            // ----- COLD BOOT -----
            tctx->coldboot_cnt++;
            ALOGD("[NXP] %s: cold boot\n", "tfa_enable");
            ALOGD("[NXP] %s: tfa_log_coldboot_cnt = %d\n", "tfa_enable", tctx->coldboot_cnt);
            ALOGD("[NXP] %s: ic stable time 80 ms\n", "tfa_enable");
            usleep(80 * 1000);

            ret = exTfa98xx_init();
            if (ret != 0) {
                ALOGE("[NXP] %s: calibration failed, retrying...", "tfa_enable");
                usleep(1000);
                usleep(1000);
                ret = exTfa98xx_init();
                if (ret != 0) {
                    ALOGE("[NXP] %s: calibration failed for second time", "tfa_enable");
                    return ret;
                }
            }
            tctx->calibrated = 1;

            ret = exTfa98xx_speakeron(tctx->mode);
            if (ret != 0) {
                ALOGE("[NXP] %s: speakeron after calibration failed", "tfa_enable");
                return ret;
            }

            if (tctx->impedance == -1.0f) {
                if (get_impedance(tctx->device_index, &impedance_val) == 0) {
                    tctx->impedance = impedance_val;
                    ALOGD("[NXP] %s: impedance = %f", "tfa_enable", impedance_val);
                } else {
                    tctx->impedance = 0.0f;
                    ALOGE("[NXP] %s: get impedance error ! impedance = %f", "tfa_enable", 0.0f);
                }
            }

            tctx->enabled = 1;
            tctx->prev_mode = tctx->mode;
            tctx->coldboot_flag = 1;
        } else {
            // ----- WARM BOOT -----
            ALOGD("[NXP] %s: warm boot\n", "tfa_enable");
            ALOGD("[NXP] %s: ic stable time 80 ms\n", "tfa_enable");
            usleep(80 * 1000);

            ALOGD("[NXP] %s: Tfa mode would be switched. prev = %d, current = %d",
                  "tfa_enable", tctx->prev_mode, tctx->mode);

            if (!tctx->coldboot_flag) {
                tctx->coldboot_flag = 1;
                tctx->coldboot_cnt++;
                ALOGD("[NXP] %s: check cold startup condition\n", "tfa_enable");
            }

            ret = exTfa98xx_speakeron(tctx->mode);
            if (ret != 0) {
                ALOGE("[NXP] %s: speakeron failed, retrying...", "tfa_enable");
                usleep(100 * 1000);
                ret = exTfa98xx_speakeron(tctx->mode);
                if (ret != 0) {
                    ALOGE("[NXP] %s: speakeron failed for second time", "tfa_enable");
                    return ret;
                }
            }

            if (tctx->impedance == -1.0f) {
                if (get_impedance(tctx->device_index, &impedance_val) == 0) {
                    tctx->impedance = impedance_val;
                    ALOGD("[NXP] %s: impedance = %f", "tfa_enable", impedance_val);
                } else {
                    tctx->impedance = 0.0f;
                    ALOGE("[NXP] %s: get impedance error ! impedance = %f", "tfa_enable", 0.0f);
                }
            }

            tctx->enabled = 1;
            tctx->prev_mode = tctx->mode;
        }

        // ----- SET VOLUME (with clamping) -----
        int vsteps[MAX_DEVICES];
        build_vsteps_with_clamping(tctx->mode, tctx->volume, vsteps);

		for (int i = 0; i < tfa98xx_cnt_max_device(); i++) {
    		ALOGD("[NXP] %s: device %d clamped step = %d", "tfa_enable", i, vsteps[i]);
		}
        if (tctx->flag_120) {
            ALOGD("[NXP] %s: tfaVolume : %d", "tfa_enable", tctx->volume);
        } else {
            ALOGD("[NXP] %s: tfaVolume : %d only attenuation", "tfa_enable", tctx->volume);
        }
        if (tfa_start(tctx->mode, vsteps) != tfa_error_ok) {
            ALOGE("[NXP] %s: failed to set volume step", "tfa_enable");
            // non‑fatal
        }

		// Log the maximum possible volume step for this device and mode
		//int max_vstep = tfacont_get_max_vstep(tctx->device_index, tctx->mode);
		//ALOGD("[NXP] %s: max volume step for device %d mode %d = %d", "tfa_enable", 
      	//		tctx->device_index, tctx->mode, max_vstep);
    } else {
        // ----- TURN OFF -----
        ALOGD("[NXP] %s: turning tfa off", "tfa_enable");
        if (!tctx->enabled) {
            ALOGD("[NXP] %s: Tfa is already turned off", "tfa_enable");
            return 0;
        }
        tctx->enabled = 0;
        exTfa98xx_speakeroff();
    }

    ALOGD("[NXP] %s: end\n", "tfa_enable");
    return ret;
}

/*------------------------------------------------------------------------------
 * Helper: get impedance for a given device index using live data.
 * Returns 0 on success, -1 on error.
 *------------------------------------------------------------------------------*/
static int get_impedance(int dev_idx, float *imp) {
	Tfa98xx_Error_t err;
	Tfa98xx_handle_t handle = -1;
	unsigned char slaveAddress;
	float live_data[MEMTRACK_MAX_WORDS] = {0};
	int nr_of_items = 0;
	int item_idx;

	if (dev_idx < 0 || dev_idx >= tfa98xx_cnt_max_device())
		return -1;

	tfaContGetSlave(dev_idx, &slaveAddress);
	err = Tfa98xx_Open(slaveAddress * 2, &handle);
	if (err != Tfa98xx_Error_Ok) {
		ALOGE("get_impedance: cannot open device %d (addr 0x%02x)", dev_idx, slaveAddress);
		return -1;
	}

	/* Find the index of the "Impedance" live data item */
	item_idx = find_live_data_item_index(handle, "Impedance");
	if (item_idx < 0) {
		ALOGE("get_impedance: 'Impedance' not found for device %d", dev_idx);
		Tfa98xx_Close(handle);
		return -1;
	}

	/* Enable live data and read current values */
	Tfa98xx_handle_t handles[2] = {handle, -1};
	err = tfa98xx_set_live_data(handles);
	if (err != Tfa98xx_Error_Ok) {
		ALOGE("get_impedance: tfa98xx_set_live_data failed: %d", err);
		Tfa98xx_Close(handle);
		return -1;
	}

	err = tfa98xx_get_live_data(handles, 0, live_data, &nr_of_items);
	if (err != Tfa98xx_Error_Ok || item_idx >= nr_of_items) {
		ALOGE("get_impedance: tfa98xx_get_live_data failed or index out of range");
		Tfa98xx_Close(handle);
		return -1;
	}

	*imp = live_data[item_idx];
	Tfa98xx_Close(handle);
	return 0;
}


/*------------------------------------------------------------------------------
 * Implementations of the function pointers stored in the context.
 * All take the context pointer as first argument.
 *------------------------------------------------------------------------------*/

static int tfa_getImpedance_impl(unsigned char *ctx, float *imp) {
    tfa_context_t *tctx = (tfa_context_t*)ctx;
    ALOGD("tfa_getImpedance: dev_idx=%d", tctx->device_index);
    return get_impedance(tctx->device_index, imp);
}

static int tfa_calibrateImpedance_impl(unsigned char *ctx, int flag) {
    // tfa_context_t *tctx = (tfa_context_t*)ctx;  // not needed here
    ALOGD("tfa_calibrateImpedance: flag=%d", flag);
    return exTfa98xx_init();
}

static int tfa_setvolumestep_impl(unsigned char *ctx, int left, int right) {
    tfa_context_t *tctx = (tfa_context_t*)ctx;
    ALOGD("tfa_setvolumestep: mode=%d, left=%d, right=%d", tctx->mode, left, right);

    int vsteps[MAX_DEVICES];
    build_vsteps_with_clamping(tctx->mode, left, vsteps);
    tctx->volume = left;   // store requested value

    return (tfa_start(tctx->mode, vsteps) == tfa_error_ok) ? 0 : -1;
}

static int tfa_setvolumeattenuation_impl(unsigned char *ctx, int left, int right) {
    tfa_context_t *tctx = (tfa_context_t*)ctx;
    ALOGD("tfa_setvolumeattenuation: mode=%d, left=%d, right=%d", tctx->mode, left, right);

    int vsteps[MAX_DEVICES];
    build_vsteps_with_clamping(tctx->mode, left, vsteps);
    tctx->volume = left;

    return (tfa_start(tctx->mode, vsteps) == tfa_error_ok) ? 0 : -1;
}

static int tfa_mute_impl(unsigned char *ctx, int mute) {
    tfa_context_t *tctx = (tfa_context_t*)ctx;
    ALOGD("tfa_mute: mute=%d", mute);
    if (mute) {
        tfa_stop();
    } else {
        int vsteps[MAX_DEVICES];
        build_vsteps_with_clamping(tctx->mode, tctx->volume, vsteps);
        tfa_start(tctx->mode, vsteps);
    }
    return 0;
}

/*------------------------------------------------------------------------------
 * Replacement for tfa_device_open – initializes the context structure.
 *------------------------------------------------------------------------------*/
int tfa_device_open(unsigned char *ctx) {
    tfa_context_t *tctx = (tfa_context_t*)ctx;

    ALOGD("[NXP] %s %s: begin", "[MAR 28, 2017]", "tfa_device_open");

    // Clear the whole context
    memset(tctx, 0, sizeof(tfa_context_t));

    // Set default values (matching original disassembly)
    tctx->unknown1 = 0;
    tctx->byte_96 = 0;
    tctx->mode = 0;                     // Audio_Mode_Music_Normal
    tctx->impedance = -1.0f;
    tctx->byte_124 = 0;
    tctx->device_index = 13;             // default (may be overwritten later)

    // Enable full volume step mode by default
    tctx->flag_120 = 1;

    // Function pointers (standard C calling convention)
    tctx->enable                   = tfa_enable;
    tctx->get_impedance            = tfa_getImpedance_impl;
    tctx->calibrate_impedance      = tfa_calibrateImpedance_impl;
    tctx->set_volume_step          = tfa_setvolumestep_impl;
    tctx->set_volume_attenuation   = tfa_setvolumeattenuation_impl;
    tctx->mute                     = tfa_mute_impl;

    // Our internal state starts zeroed (calibrated = 0, prev_mode = 0, etc.)

    ALOGD("[NXP] %s: end", "tfa_device_open");
    return 0;
}

/*------------------------------------------------------------------------------
 * Main function (kept as originally provided, but with fixed recordLiveData)
 *------------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
	enum tfa_error err = tfa_error_ok;
	nxpTfaContainer_t *cntbuf;
	int length;
	int i = 0, j = 0, vsteps[MAX_DEVICES]={0,0,0,0};
	int profile;
	unsigned char  tfa98xxI2cSlave; // for i2c address

	char target[FILENAME_MAX];

	printf("Starting application.\n");

	/* Get the container file from command line */
	if ( argc < 2 ) {
		printf("Please supply a container file as command line argument.\n");
		return 0;
	} else if ( argc < 3 ) {
		strcpy(target, "dummy"); /* no target specified, use default*/
	} else {
		strncpy(target, argv[2], sizeof(target)); /* target specified */
	}
	//	tfa_cnt_verbose( argc>3);

	/* load the container file into the memory */
	length = load_container_file(argv[1], &cntbuf);
	if (length ==0)  { // read params
		fprintf(stderr, "Load container failed\n");
		return 1;
	} else {
		printf("Loaded container file %s.\n", argv[1]);
	}

	/* pass the container file to the tfa */
	tfa_load_cnt( (void*) cntbuf, length);

	/* open the target device */
	if (NXP_I2C_Open(target) == -1 ) { /* use input device */
		fprintf(stderr, "Could not open target device:%s.\n The second argument can be used to specify a device\n.", target);
		return 1;
	}
	NXP_I2C_Trace(0);

	/* Make sure device family type is set */
	if (tfa98xx_get_cnt() != NULL) {
		//tfa_soft_probe_all(tfa98xx_get_cnt());
		if( tfa_probe_all(tfa98xx_get_cnt()) != Tfa98xx_Error_Ok ) {
			goto error_exit;
		}
	}

	/* print container files details */
	// for debugging use: tfaContShowContainer();
	for (i=0; i < tfa98xx_cnt_max_device(); i++ ) {
		tfaContGetSlave(i, &tfa98xxI2cSlave); /* get device I2C address */
		printf("\tFound Device[%d]: %s at 0x%02x.\n", i, tfaContDeviceName(i), tfa98xxI2cSlave);
		for (j=0; j < tfaContMaxProfile(i); j++ )
			printf("\t\tProfile [%d]: %s, %d vsteps.\n", j, tfaContProfileName(i,j), tfacont_get_max_vstep(i,j));
	}

	printf ("Select a profile [0-%d]: ", (tfaContMaxProfile(0)-1));
	j = scanf ("%d", &profile);

	/* always load vstep[0] */
	err = tfa_start(profile, vsteps);

	/* loop up through all vsteps if > 0 */
	for(i=0;i<tfacont_get_max_vstep(0,profile);i++){
		for(j=0;j<tfa98xx_cnt_max_device();j++) /* put all to same vstep */
			vsteps[j]=i;
		err = tfa_start(profile, vsteps);
		if ( err!=tfa_error_ok)
			goto error_exit;
	}

	/* loop down through all vsteps if > 0 */
	for(i=tfacont_get_max_vstep(0,profile)-1;i>0;i--){
		for(j=0;j<tfa98xx_cnt_max_device();j++) /* put all to same vstep */
			vsteps[j]=i;
		err = tfa_start(profile, vsteps);
		if ( err!=tfa_error_ok)
			goto error_exit;
	}

	if (argc >= 4) {
		err = recordLiveData(argv[3]); /* third argument is the livedata item to be tracked */
		if (err != tfa_error_ok)
			goto error_exit;
	}

	printf ("Play music at 48kHz. Do you want to quit? [type number/char to quit] ");
	j = scanf ("%d", &i);
	if(!j) {
		/**** Erroneous input, get rid of it and retry! */
		scanf ("%*[^\n]");
	}

error_exit:
	if ( err!=tfa_error_ok) {
		printf("an error occured, code:%d\n",err );
	}
	printf ("\nPowering down.\n");

	tfa_stop();
	tfa_deinit();

	NXP_I2C_Close();

	return 0;
}

/*------------------------------------------------------------------------------
 * Calibration routine – runs calibration on all devices.
 *------------------------------------------------------------------------------*/
static int calibrationOnce(void)
{
	Tfa98xx_Error_t error = Tfa98xx_Error_Ok;
	unsigned char tfa98xxI2cSlave = 0;
	int maxdev = tfa98xx_cnt_max_device(), i = 0;
	Tfa98xx_handle_t handle;
	int ret = 0;

	if ( maxdev <= 0 ) {
		ALOGE("Please provide/load container file for calibration.");
		ret = -1;
		goto EXIT;
	}

	for (i=0; i < maxdev; i++ ) {
		tfaContGetSlave(i, &tfa98xxI2cSlave); /* get device I2C address */
		ALOGD("%s: Found Device[%d]: %s at 0x%02x.", __func__, i, tfaContDeviceName(i), tfa98xxI2cSlave);

		error = Tfa98xx_Open(tfa98xxI2cSlave*2, &handle);
		if (error != Tfa98xx_Error_Ok) {
			ALOGE("last status: %d (%s)", error, Tfa98xx_GetErrorString(error));
			ret = error;
			continue;
		}

		//run calibration (with profile 0)
		error = tfa98xxCalibration(&handle, i, 1, 0);
		if ( error!=Tfa98xx_Error_Ok ) {
			ALOGE("Calibration failed: error %d (%s)", error, Tfa98xx_GetErrorString(error));
			ret = error;
		}

		Tfa98xx_Close(handle);
	}
EXIT:
	return ret;
}

/*------------------------------------------------------------------------------
 * Public API functions (with mutex protection)
 *------------------------------------------------------------------------------*/
int exTfa98xx_check_tfaopen(void)
{
	int ret = 0;
	ALOGD("%s into\n", __func__);
	pthread_mutex_lock(&mutex);

	nxpTfaContainer_t *cntbuf =  tfa98xx_get_cnt();
	ret = (cntbuf != NULL) ? 1 : 0;

	ALOGD("%s out\n", __func__);
	pthread_mutex_unlock(&mutex);
	return ret;
}

int exTfa98xx_init()
{
	Tfa98xx_Error_t err = Tfa98xx_Error_Ok;
	nxpTfaContainer_t *cntbuf;
	int length;
	int ret = 0;

	ALOGD("%s into\n", __func__);
	pthread_mutex_lock(&mutex);

	/* open the target device */
	if (NXP_I2C_Open(TFA_I2CDEVICE) == -1 ) {
		ALOGE("Could not open target device:%s.", TFA_I2CDEVICE);
		ret = -1;
		goto EXIT;
	}

	NXP_I2C_Trace(0);

	/* load the container file into the memory */
	length = load_container_file(LOCATION_FILES CNT_FILENAME, &cntbuf);
	if (length == 0)  {
		ALOGE("Load container file %s failed.", LOCATION_FILES CNT_FILENAME);
		ret = -1;
		goto EXIT;
	}

	/* pass the container file to the tfa */
	tfa_load_cnt( (void*) cntbuf, length);

	ALOGE("will call calibration function\n");
	err = calibrationOnce();
	if (err) {
		NXP_I2C_Close();
		tfa_deinit();
		free(cntbuf);
		ret = -1;
		goto EXIT;
	}

	ALOGE("will stop tfa\n");
	tfa_stop();

EXIT:
	ALOGD("%s out\n", __func__);
	pthread_mutex_unlock(&mutex);
	return ret;
}

int exTfa98xx_deinit()
{
	int ret = 0;
	nxpTfaContainer_t *cntbuf =  tfa98xx_get_cnt();

	ALOGD("%s into\n", __func__);
	pthread_mutex_lock(&mutex);

	NXP_I2C_Close();
	tfa_deinit();

	if (cntbuf != NULL) {
		free(cntbuf);
	}

	ALOGD("%s out\n", __func__);
	pthread_mutex_unlock(&mutex);
	return ret;
}

/*------------------------------------------------------------------------------
 * Set samplerate – currently a placeholder.  If needed, store the value in a
 * global variable and use it in exTfa98xx_speakeron to select the profile.
 *------------------------------------------------------------------------------*/
void exTfa98xx_setSamplerate(int samplerate)
{
	ALOGV("%s into samplerate = %d", __func__, samplerate);
	pthread_mutex_lock(&mutex);
	// TODO: store samplerate in a global if needed for profile selection
	// (exTfa98xx_speakeron already receives a mode argument, so this may be unused)
	pthread_mutex_unlock(&mutex);
}

int exTfa98xx_speakeron(exTfa98xx_audio_mode_t mode)
{
	enum tfa_error err;
	int vsteps[MAX_DEVICES]={0,0,0,0};
	int ret = 0;

	ALOGD("%s into\n", __func__);
	pthread_mutex_lock(&mutex);

	/* always load vstep[0] */
	err = tfa_start((int)mode, vsteps);
	if (tfa_error_ok != err) {
		ALOGE("tfa_start failed, code:%d", err);
		tfa_stop();
		ret = -1;
	}

	ALOGD("%s out\n", __func__);
	pthread_mutex_unlock(&mutex);
	return ret;
}

void exTfa98xx_speakeroff()
{
	ALOGD("%s into\n", __func__);
	pthread_mutex_lock(&mutex);

	tfa_stop();

	ALOGD("%s out\n", __func__);
	pthread_mutex_unlock(&mutex);
}
