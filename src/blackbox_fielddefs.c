#include "blackbox_fielddefs.h"

const char * const FLIGHT_LOG_FLIGHT_MODE_NAME[] = {
    "ARM",
    "ANGLE",
    "HORIZON",
    "MAG",
    "ALTHOLD",
    "HEADFREE",
    "CHIRP",
    "PASSTHRU",
    "FAILSAFE",
    "POSHOLD",
    "GPSRESCUE",
    "ANTIGRAVITY",
    "HEADADJ",
    "CAMSTAB",
    "BEEPER",
    "LEDLOW",
    "CALIB",
    "OSD",
    "TELEMETRY",
    "SERVO1",
    "SERVO2",
    "SERVO3",
    "BLACKBOX",
    "AIRMODE",
    "3D",
    "FPVANGLEMIX",
    "BLACKBOXERASE",
    "CAMERA1",
    "CAMERA2",
    "CAMERA3",
    "FLIPOVERAFTERCRASH",
    "PREARM",
    "BEEPGPSCOUNT",
    "VTXPITMODE",
    "USER1",
    "USER2",
    "USER3",
    "USER4",
    "PIDAUDIO",
    "ACROTRAINER",
    "VTXCONTROLDISABLE",
    "LAUNCHCONTROL"
};

const char * const FLIGHT_LOG_FLIGHT_STATE_NAME[] = {
    "GPS_FIX_HOME",
    "GPS_FIX",
    "CALIBRATE_MAG",
    "SMALL_ANGLE",
    "FIXED_WING"
};

const char * const FLIGHT_LOG_FAILSAFE_PHASE_NAME[] = {
    "IDLE",
    "RX_LOSS_DETECTED",
    "LANDING",
    "LANDED",
    "FAILSAFE_RX_LOSS_MONITORING",
    "FAILSAFE_RX_LOSS_RECOVERED"
};
