#pragma once

#include <stdint.h>

typedef enum FlightLogFieldCondition {
    FLIGHT_LOG_FIELD_CONDITION_ALWAYS = 0,
    FLIGHT_LOG_FIELD_CONDITION_AT_LEAST_MOTORS_1,
    FLIGHT_LOG_FIELD_CONDITION_AT_LEAST_MOTORS_2,
    FLIGHT_LOG_FIELD_CONDITION_AT_LEAST_MOTORS_3,
    FLIGHT_LOG_FIELD_CONDITION_AT_LEAST_MOTORS_4,
    FLIGHT_LOG_FIELD_CONDITION_AT_LEAST_MOTORS_5,
    FLIGHT_LOG_FIELD_CONDITION_AT_LEAST_MOTORS_6,
    FLIGHT_LOG_FIELD_CONDITION_AT_LEAST_MOTORS_7,
    FLIGHT_LOG_FIELD_CONDITION_AT_LEAST_MOTORS_8,
    FLIGHT_LOG_FIELD_CONDITION_TRICOPTER,

    FLIGHT_LOG_FIELD_CONDITION_MAG,
    FLIGHT_LOG_FIELD_CONDITION_BARO,
    FLIGHT_LOG_FIELD_CONDITION_VBAT,
    FLIGHT_LOG_FIELD_CONDITION_AMPERAGE_ADC,
    FLIGHT_LOG_FIELD_CONDITION_SONAR,
    FLIGHT_LOG_FIELD_CONDITION_RSSI,

    FLIGHT_LOG_FIELD_CONDITION_NONZERO_PID_D_0,
    FLIGHT_LOG_FIELD_CONDITION_NONZERO_PID_D_1,
    FLIGHT_LOG_FIELD_CONDITION_NONZERO_PID_D_2,

    FLIGHT_LOG_FIELD_CONDITION_NOT_LOGGING_EVERY_FRAME,

    FLIGHT_LOG_FIELD_CONDITION_NEVER,

    FLIGHT_LOG_FIELD_CONDITION_FIRST = FLIGHT_LOG_FIELD_CONDITION_ALWAYS,
    FLIGHT_LOG_FIELD_CONDITION_LAST = FLIGHT_LOG_FIELD_CONDITION_NEVER
} FlightLogFieldCondition;

typedef enum FlightLogFieldPredictor {
    //No prediction:
    FLIGHT_LOG_FIELD_PREDICTOR_0              = 0,

    //Predict that the field is the same as last frame:
    FLIGHT_LOG_FIELD_PREDICTOR_PREVIOUS       = 1,

    //Predict that the slope between this field and the previous item is the same as that between the past two history items:
    FLIGHT_LOG_FIELD_PREDICTOR_STRAIGHT_LINE  = 2,

    //Predict that this field is the same as the average of the last two history items:
    FLIGHT_LOG_FIELD_PREDICTOR_AVERAGE_2      = 3,

    //Predict that this field is minthrottle
    FLIGHT_LOG_FIELD_PREDICTOR_MINTHROTTLE    = 4,

    //Predict that this field is the same as motor 0
    FLIGHT_LOG_FIELD_PREDICTOR_MOTOR_0        = 5,

    //This field always increments
    FLIGHT_LOG_FIELD_PREDICTOR_INC            = 6,

    //Predict this GPS co-ordinate is the GPS home co-ordinate (or no prediction if that coordinate is not set)
    FLIGHT_LOG_FIELD_PREDICTOR_HOME_COORD     = 7,

    //Predict 1500
    FLIGHT_LOG_FIELD_PREDICTOR_1500           = 8,

    //Predict vbatref, the reference ADC level stored in the header
    FLIGHT_LOG_FIELD_PREDICTOR_VBATREF        = 9,

    //Predict the last time value written in the main stream
    FLIGHT_LOG_FIELD_PREDICTOR_LAST_MAIN_FRAME_TIME = 10,

  	//Predict that this field is the minimum motor output
 	FLIGHT_LOG_FIELD_PREDICTOR_MINMOTOR       = 11,

    //Home coord predictors appear in pairs (two copies of FLIGHT_LOG_FIELD_PREDICTOR_HOME_COORD). Rewrite the second
    //one we see to this to make parsing easier
    FLIGHT_LOG_FIELD_PREDICTOR_HOME_COORD_1   = 256,

} FlightLogFieldPredictor;

typedef enum FlightLogFieldEncoding {
    FLIGHT_LOG_FIELD_ENCODING_SIGNED_VB       = 0, // Signed variable-byte
    FLIGHT_LOG_FIELD_ENCODING_UNSIGNED_VB     = 1, // Unsigned variable-byte
    FLIGHT_LOG_FIELD_ENCODING_NEG_14BIT       = 3, // Unsigned variable-byte but we negate the value before storing, value is 14 bits
    FLIGHT_LOG_FIELD_ENCODING_ELIAS_DELTA_U32 = 4,
    FLIGHT_LOG_FIELD_ENCODING_ELIAS_DELTA_S32 = 5,
    FLIGHT_LOG_FIELD_ENCODING_TAG8_8SVB       = 6,
    FLIGHT_LOG_FIELD_ENCODING_TAG2_3S32       = 7,
    FLIGHT_LOG_FIELD_ENCODING_TAG8_4S16       = 8,
    FLIGHT_LOG_FIELD_ENCODING_NULL            = 9, // Nothing is written to the file, take value to be zero
    FLIGHT_LOG_FIELD_ENCODING_ELIAS_GAMMA_U32 = 10,
    FLIGHT_LOG_FIELD_ENCODING_ELIAS_GAMMA_S32 = 11
} FlightLogFieldEncoding;

typedef enum FlightLogFieldSign {
    FLIGHT_LOG_FIELD_UNSIGNED = 0,
    FLIGHT_LOG_FIELD_SIGNED   = 1
} FlightLogFieldSign;

// Flight mode flags - using uint64_t to support all 42 modes cleanly
typedef uint64_t flightModeFlags_t;

#define ARM_MODE                    (1ULL << 0)
#define ANGLE_MODE                  (1ULL << 1)
#define HORIZON_MODE                (1ULL << 2)
#define MAG_MODE                    (1ULL << 3)
#define ALTHOLD_MODE                (1ULL << 4)
#define HEADFREE_MODE               (1ULL << 5)
#define CHIRP_MODE                  (1ULL << 6)
#define PASSTHRU_MODE               (1ULL << 7)
#define FAILSAFE_MODE               (1ULL << 8)
#define POSHOLD_MODE                (1ULL << 9)
#define GPSRESCUE_MODE              (1ULL << 10)
#define ANTIGRAVITY_MODE            (1ULL << 11)
#define HEADADJ_MODE                (1ULL << 12)
#define CAMSTAB_MODE                (1ULL << 13)
#define BEEPER_MODE                 (1ULL << 14)
#define LEDLOW_MODE                 (1ULL << 15)
#define CALIB_MODE                  (1ULL << 16)
#define OSD_MODE                    (1ULL << 17)
#define TELEMETRY_MODE              (1ULL << 18)
#define SERVO1_MODE                 (1ULL << 19)
#define SERVO2_MODE                 (1ULL << 20)
#define SERVO3_MODE                 (1ULL << 21)
#define BLACKBOX_MODE               (1ULL << 22)
#define AIRMODE_MODE                (1ULL << 23)
#define THREED_MODE                 (1ULL << 24)
#define FPVANGLEMIX_MODE            (1ULL << 25)
#define BLACKBOXERASE_MODE          (1ULL << 26)
#define CAMERA1_MODE                (1ULL << 27)
#define CAMERA2_MODE                (1ULL << 28)
#define CAMERA3_MODE                (1ULL << 29)
#define FLIPOVERAFTERCRASH_MODE     (1ULL << 30)
#define PREARM_MODE                 (1ULL << 31)
#define BEEPGPSCOUNT_MODE           (1ULL << 32)
#define VTXPITMODE_MODE             (1ULL << 33)
#define USER1_MODE                  (1ULL << 34)
#define USER2_MODE                  (1ULL << 35)
#define USER3_MODE                  (1ULL << 36)
#define USER4_MODE                  (1ULL << 37)
#define PIDAUDIO_MODE               (1ULL << 38)
#define ACROTRAINER_MODE            (1ULL << 39)
#define VTXCONTROLDISABLE_MODE      (1ULL << 40)
#define LAUNCHCONTROL_MODE          (1ULL << 41)

#define FLIGHT_LOG_FLIGHT_MODE_COUNT 42

extern const char * const FLIGHT_LOG_FLIGHT_MODE_NAME[];

typedef enum {
    GPS_FIX_HOME   = (1 << 0),
    GPS_FIX        = (1 << 1),
    CALIBRATE_MAG  = (1 << 2),
    SMALL_ANGLE    = (1 << 3),
    FIXED_WING     = (1 << 4),                   // set when in flying_wing or airplane mode. currently used by althold selection code
} stateFlags_t;

extern const char * const FLIGHT_LOG_FLIGHT_STATE_NAME[];

#define FLIGHT_LOG_FLIGHT_STATE_COUNT 5

typedef enum {
    FAILSAFE_IDLE = 0,
    FAILSAFE_RX_LOSS_DETECTED,
    FAILSAFE_LANDING,
    FAILSAFE_LANDED,
    FAILSAFE_RX_LOSS_MONITORING,
    FAILSAFE_RX_LOSS_RECOVERED
} failsafePhase_e;

extern const char * const FLIGHT_LOG_FAILSAFE_PHASE_NAME[];

#define FLIGHT_LOG_FAILSAFE_PHASE_COUNT 4

typedef enum FlightLogEvent {
    FLIGHT_LOG_EVENT_SYNC_BEEP = 0,
    FLIGHT_LOG_EVENT_INFLIGHT_ADJUSTMENT = 13,
    FLIGHT_LOG_EVENT_LOGGING_RESUME = 14,
    FLIGHT_LOG_EVENT_FLIGHTMODE = 30, // Add new event type for flight mode status.
    FLIGHT_LOG_EVENT_LOG_END = 255
} FlightLogEvent;

typedef struct flightLogEvent_syncBeep_t {
    int64_t time;
} flightLogEvent_syncBeep_t;

typedef struct flightLogEvent_inflightAdjustment_t {
    uint8_t adjustmentFunction;
    int32_t newValue;
    float newFloatValue;
} flightLogEvent_inflightAdjustment_t;

typedef struct flightLogEvent_loggingResume_t {
    uint32_t logIteration;
    int64_t currentTime;
} flightLogEvent_loggingResume_t;

typedef union flightLogEventData_t
{
    flightLogEvent_syncBeep_t syncBeep;
    flightLogEvent_inflightAdjustment_t inflightAdjustment;
    flightLogEvent_loggingResume_t loggingResume;
} flightLogEventData_t;

typedef struct flightLogEvent_t
{
    FlightLogEvent event;
    flightLogEventData_t data;
} flightLogEvent_t;
