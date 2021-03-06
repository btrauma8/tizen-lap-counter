#include <tizen.h>
#include <service_app.h>
#include "z9service.h"
#include <device/haptic.h>
#include <dlog.h>
#include <message_port.h>
#include <locations.h>
#include <Eina.h>
#include <string.h>
#include <Ecore.h>
#include <unistd.h>
#include <math.h>

static time_t getTime(void) {
	struct timeval te;
	gettimeofday(&te, NULL); // get current time
	return te.tv_sec;
}

static void doBuzz(int ms) {
	haptic_device_h haptic_handle;
	haptic_effect_h effect_handle;
	if (device_haptic_open(0, &haptic_handle) == DEVICE_ERROR_NONE)
	{
		device_haptic_vibrate(haptic_handle, ms, 100, &effect_handle);
	}
}

static int buzzRemainCount = 0;
static int buzzDurationMs = 0;
static double buzzOffInterval = 0.3;

static Eina_Bool buzzWithTimer(void *data EINA_UNUSED)
{
	if (buzzRemainCount < 1) return ECORE_CALLBACK_CANCEL;
	doBuzz(buzzDurationMs);
	buzzRemainCount = buzzRemainCount - 1;
	if (buzzRemainCount > 0)
	{
		double delay = ((buzzDurationMs + 500) / 1000) + buzzOffInterval;
		ecore_timer_add(delay, buzzWithTimer, NULL);
	}
	return ECORE_CALLBACK_CANCEL;
}

static void buzz(int duration, int count, double pauseIntervalSec) {
	if (buzzRemainCount > 0) return; // buzz gets ignored--finishing last one
	buzzDurationMs = duration;
	buzzRemainCount = count;
	buzzOffInterval = pauseIntervalSec;
	buzzWithTimer(NULL);
}

static double PRECISION = 0.00000000000001; // for turnign lat longs into readable strings
char* dtoa(char *s, double n) {
    // handle special cases
    if (isnan(n)) {
        strcpy(s, "nan");
    } else if (isinf(n)) {
        strcpy(s, "inf");
    } else if (n == 0.0) {
        strcpy(s, "0");
    } else {
        int digit, m, m1;
        char *c = s;
        int neg = (n < 0);
        if (neg)
            n = -n;
        // calculate magnitude
        m = log10(n);
        int useExp = (m >= 14 || (neg && m >= 9) || m <= -9);
        if (neg)
            *(c++) = '-';
        // set up for scientific notation
        if (useExp) {
            if (m < 0)
               m -= 1.0;
            n = n / pow(10.0, m);
            m1 = m;
            m = 0;
        }
        if (m < 1.0) {
            m = 0;
        }
        // convert the number
        while (n > PRECISION || m >= 0) {
            double weight = pow(10.0, m);
            if (weight > 0 && !isinf(weight)) {
                digit = floor(n / weight);
                n -= (digit * weight);
                *(c++) = '0' + digit;
            }
            if (m == 0 && n > 0)
                *(c++) = '.';
            m--;
        }
        if (useExp) {
            // convert the exponent
            int i, j;
            *(c++) = 'e';
            if (m1 > 0) {
                *(c++) = '+';
            } else {
                *(c++) = '-';
                m1 = -m1;
            }
            m = 0;
            while (m1 > 0) {
                *(c++) = '0' + m1 % 10;
                m1 /= 10;
                m++;
            }
            c -= m;
            for (i = 0, j = m-1; i<j; i++, j--) {
                // swap without temporary
                c[i] ^= c[j];
                c[j] ^= c[i];
                c[i] ^= c[j];
            }
            c += m;
        }
        *(c) = '\0';
    }
    return s;
}

static double box[4][2] = {
		{
			-89.6085852,
			34.3516497
		},
		{
			-89.6085289,
			34.3511957
		},
		{
			-89.6081936,
			34.3512356
		},
		{
			-89.6082795,
			34.3516962
		}
};

bool isInside(double lat, double lng)
{
	// i always say "lat/long", but the standard is "long/lat" for things like this.
	double x = lng;
	double y = lat;
	bool inside = false;
	for (int i=0, j = 3; i < 4; j = i++) {
		double xi = box[i][0];
		double yi = box[i][1];
		double xj = box[j][0];
		double yj = box[j][1];
		bool intersect = ((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi);
		if (intersect) inside = !inside;
	}
	return inside;
}

// TODO: RESET MANAGER IF x CONSECUTIVE [0,0] READINGS
// Actually, that just means bluetooth paired phone--UI should WARN.

static location_manager_h manager;

static double lastLat = 0;
static double lastLng = 0;
static double lastAlt = 0;
static bool lastInBox = false;
static bool active = false;

// times since 1970:
static time_t runStartEpoch = 0;
static time_t lastPausedEpoch = 0;
static time_t doNotRegisterLapUntilEpoch = 0;

// total sec times:
static time_t totalPausedSec = 0;
static time_t minTimeBetweenLaps = 35;
static int lapCount = 0;

static int hitsOutsideBox = 0;
static int minRequiredHitsOutsideBox = 3;

static time_t lastLapEpoch = 0;
static time_t forcePauseAfterSecWithoutLap = 400; // 6.6 min

void sendKeyVal(char* key, char* val)
{
    bundle *b = bundle_create();
    bundle_add_str(b, key, val);
    message_port_send_message("DaCMXXuEWw.Z9Web", "PIG", b);
    bundle_free(b);
}
void sendError(char* s)
{
    bundle *b = bundle_create();
    bundle_add_str(b, "err", s);
    message_port_send_message("DaCMXXuEWw.Z9Web", "PIG", b);
    bundle_free(b);
}
void sendKeyInt(char* k, int n)
{
	char nStr[128];
	eina_convert_itoa(n, nStr);
    bundle *b = bundle_create();
    bundle_add_str(b, k, nStr);
    message_port_send_message("DaCMXXuEWw.Z9Web", "PIG", b);
    bundle_free(b);
}

void addTimeToBundle(bundle *b, char *key, time_t t)
{
	char strBuf[128];
	sprintf(strBuf, "%u", (unsigned) t);
	bundle_add_str(b, key, strBuf);
}

void addDoubleToBundle(bundle *b, char *key, double d)
{
	char strBuf[256];
	dtoa(strBuf, d);
	bundle_add_str(b, key, strBuf);
}

void addIntToBundle(bundle *b, char *key, int n) {
	char strBuf[128];
	eina_convert_itoa(n, strBuf);
	bundle_add_str(b, key, strBuf);
}

void sendState(void)
{
	// Send everything about current state
    bundle *b = bundle_create();
    bundle_add_str(b, "isState", "true");
    addDoubleToBundle(b, "lat", lastLat);
    addDoubleToBundle(b, "lng", lastLng);
    addDoubleToBundle(b, "alt", lastAlt);
    addTimeToBundle(b, "runStartEpoch", runStartEpoch);
    addTimeToBundle(b, "lastPausedEpoch", lastPausedEpoch);
    addTimeToBundle(b, "lastLapEpoch", lastLapEpoch);
    addTimeToBundle(b, "doNotRegisterLapUntilEpoch", doNotRegisterLapUntilEpoch);
    addTimeToBundle(b, "totalPausedSec", totalPausedSec);
    addTimeToBundle(b, "minTimeBetweenLaps", minTimeBetweenLaps);

    addIntToBundle(b, "lapCount", lapCount);
    if (active) {
    	bundle_add_str(b, "active", "true");
    } else {
    	bundle_add_str(b, "active", "false");
    }
    if (lastInBox) {
    	bundle_add_str(b, "inBox", "true");
    } else {
    	bundle_add_str(b, "inBox", "false");
    }
    message_port_send_message("DaCMXXuEWw.Z9Web", "PIG", b);
    bundle_free(b);
}

static void resetRun(void) {
	runStartEpoch = getTime();
	doNotRegisterLapUntilEpoch = 0;
	lastLapEpoch = 0;
	lastPausedEpoch = 0;
	totalPausedSec = 0;
	lapCount = 0;
	active = true;
	sendState();
}

static void pauseRun(void) {
	lastPausedEpoch = getTime();
	active = false;
	sendState();
}

static void resumeRun(void) {
	// If run has not yet started, start it now.
	if (runStartEpoch == 0) {
		resetRun();
		return;
	}
	if (active) {
		// Nothing to do...already active.
		sendState();
		return;
	}
	// normal resume when paused.
	time_t pausedSec = getTime() - lastPausedEpoch;
	totalPausedSec = totalPausedSec + pausedSec;
	if (doNotRegisterLapUntilEpoch > 0) {
		// Enter box at time 5sec, pause for 1 minute, resume still in box...
		// lastLapEpoch will now be 1min5sec ... good.
		doNotRegisterLapUntilEpoch = doNotRegisterLapUntilEpoch + pausedSec;
	}
	active = true;
	sendState();
}

static void lapBuzz()
{
	buzz(200, 2, 0.4);
}

static void registerLap(time_t currTimeEpoch) {
	if (!active) return;

	if (doNotRegisterLapUntilEpoch > currTimeEpoch) return;
	if (hitsOutsideBox < minRequiredHitsOutsideBox) return;

	// COUNT IT!
	lastLapEpoch = getTime();
	hitsOutsideBox = 0;
	doNotRegisterLapUntilEpoch = currTimeEpoch + minTimeBetweenLaps;
	lapCount = lapCount + 1;
	if (lapCount % 10 == 0) {
		// every tenth
		buzz(2000, 1, 0);
	} else if (lapCount % 5 == 0) {
		// every fifth
		buzz(1500, 1, 0);
	} else {
		// every lap
		lapBuzz();
	}
}



static Eina_Bool timerCb(void *data EINA_UNUSED)
{

	if (!active) return ECORE_CALLBACK_RENEW;

	// If we have been ACTIVE for over "forcePauseAfterSecWithoutLap" and we have
	// not registered a lap in that time, FORCE PAUSE!
	time_t currTime = getTime();

	// If never finished a lap, use start of run
	// If never paused, use start of run.
	time_t t1 = lastLapEpoch == 0 ? runStartEpoch : lastLapEpoch;
	time_t t2 = lastPausedEpoch == 0 ? runStartEpoch : lastPausedEpoch;
	if (currTime - t1 > forcePauseAfterSecWithoutLap && currTime - t2 > forcePauseAfterSecWithoutLap)
	{
		pauseRun();
		return ECORE_CALLBACK_RENEW;
	}

	time_t timestamp; // second since 1970
	// double altitude;
	// double latitude;
	// double longitude;
	location_manager_get_position(manager, &lastAlt, &lastLat, &lastLng, &timestamp);
	lastInBox = isInside(lastLat, lastLng);
	if (lastInBox) {
		registerLap(timestamp);
	} else {
		hitsOutsideBox = hitsOutsideBox + 1;
	}
	sendState();
	return ECORE_CALLBACK_RENEW;
}

void locationSetup(void)
{

	/*
		LOCATIONS_METHOD_GPS
		LOCATIONS_METHOD_HYBRID
		LOCATIONS_METHOD_WPS
		LOCATIONS_METHOD_PASSIVE
		LOCATIONS_METHOD_FUSED
	 */

	int ret = 0;
	ret = location_manager_create(LOCATIONS_METHOD_GPS, &manager);
	if (ret != LOCATIONS_ERROR_NONE) {
		sendError("Could not start location service 0");
	}

	ret = location_manager_start(manager);

	if (ret == LOCATIONS_ERROR_NONE) {
		// send("GOOD START!!");
	} else if (ret == LOCATIONS_ERROR_INVALID_PARAMETER) {
		sendError("Could not start location service 1");
	} else if (ret == LOCATIONS_ERROR_SERVICE_NOT_AVAILABLE) {
		sendError("Could not start location service 2");
	} else if (ret == LOCATIONS_ERROR_NETWORK_FAILED) {
		sendError("Could not start location service 3");
	} else if (ret == LOCATIONS_ERROR_GPS_SETTING_OFF) {
		sendError("Could not start location service 4");
	} else if (ret == LOCATIONS_ERROR_ACCESSIBILITY_NOT_ALLOWED) {
		sendError("Could not start location service 5");
	} else if (ret == LOCATIONS_ERROR_NOT_SUPPORTED) {
		sendError("Could not start location service 6");
	}
    ecore_timer_add(2, timerCb, NULL); // Every TWO seconds
}


void message_port_cb(int local_port_id, const char *remote_app_id, const char *remote_port, bool trusted_remote_port, bundle *message, void *user_data)
{
    char *v = NULL;
    bundle_get_str(message, "command", &v);
    if (strncmp(v, "reset", 5) == 0) {
    	resetRun();
    } else if (strncmp(v, "pause", 5) == 0) {
    	pauseRun();
    } else if (strncmp(v, "refresh", 6) == 0) {
    	sendState();
    } else if (strncmp(v, "resume", 5) == 0) {
    	// This will start the run if the run has never started
    	resumeRun();
    } else if (strncmp(v, "shit", 4) == 0) {
    	// sendKeyVal("TEST", "SHIT");
		double lat = 34.3511430127;
		double lng= -89.6082113217;
		if (isInside(lat, lng)) {
			sendKeyVal("TEST", "INSIDE");
		} else {
			sendKeyVal("TEST", "outside");
		}
    }
}

void listenPortSetup(void)
{
	int port_id = message_port_register_local_port("GUG", message_port_cb, NULL);
	if (port_id < 0) {
		sendError("cannot listen to GUG");
	}
}

bool service_app_create(void *data)
{
	lapBuzz();
    locationSetup();
    listenPortSetup();
    return true;
}

void service_app_terminate(void *data)
{
    // Todo: add your code here.
    return;
}

void service_app_control(app_control_h app_control, void *data)
{
    // Todo: add your code here.
    return;
}

static void
service_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LANGUAGE_CHANGED*/
	return;
}

static void
service_app_region_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

static void
service_app_low_battery(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_BATTERY*/
}

static void
service_app_low_memory(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_MEMORY*/
}

int main(int argc, char* argv[])
{
    char ad[50] = {0,};
	service_app_lifecycle_callback_s event_callback;
	app_event_handler_h handlers[5] = {NULL, };

	event_callback.create = service_app_create;
	event_callback.terminate = service_app_terminate;
	event_callback.app_control = service_app_control;

	service_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, service_app_low_battery, &ad);
	service_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, service_app_low_memory, &ad);
	service_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, service_app_lang_changed, &ad);
	service_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED], APP_EVENT_REGION_FORMAT_CHANGED, service_app_region_changed, &ad);

	return service_app_main(argc, argv, &event_callback, ad);
}
