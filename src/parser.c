#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//For msvcrt to define M_PI:
#define _USE_MATH_DEFINES
#include <math.h>

#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#if defined(__MINGW64__) || defined(__MINGW32__)
    #include <time.h>
#endif

#include "parser.h"
#include "tools.h"
#include "decoders.h"

#define LOG_START_MARKER "H Product:Blackbox flight data recorder by Nicholas Sherlock\n"

//Assume that even in the most woeful logging situation, we won't miss 10 seconds of frames
#define MAXIMUM_TIME_JUMP_BETWEEN_FRAMES (10 * 1000000)

//Likewise for iteration count
#define MAXIMUM_ITERATION_JUMP_BETWEEN_FRAMES (500 * 10)

union {
    float f;
    uint32_t u;
} floatConvert;

typedef void (*FlightLogFrameParse)(flightLog_t *log, mmapStream_t *stream, bool raw);
typedef bool (*FlightLogFrameComplete)(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw);

typedef struct flightLogFrameType_t {
    uint8_t marker;
    FlightLogFrameParse parse;
    FlightLogFrameComplete complete;
} flightLogFrameType_t;

static void parseIntraframe(flightLog_t *log, mmapStream_t *stream, bool raw);
static void parseInterframe(flightLog_t *log, mmapStream_t *stream, bool raw);
static void parseGPSFrame(flightLog_t *log, mmapStream_t *stream, bool raw);
static void parseGPSHomeFrame(flightLog_t *log, mmapStream_t *stream, bool raw);
static void parseEventFrame(flightLog_t *log, mmapStream_t *stream, bool raw);
static void parseSlowFrame(flightLog_t *log, mmapStream_t *stream, bool raw);

static bool completeIntraframe(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw);
static bool completeInterframe(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw);
static bool completeEventFrame(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw);
static bool completeGPSFrame(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw);
static bool completeGPSHomeFrame(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw);
static bool completeSlowFrame(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw);

static const flightLogFrameType_t frameTypes[] = {
    {.marker = 'I', .parse = parseIntraframe,   .complete = completeIntraframe},
    {.marker = 'P', .parse = parseInterframe,   .complete = completeInterframe},
    {.marker = 'G', .parse = parseGPSFrame,     .complete = completeGPSFrame},
    {.marker = 'H', .parse = parseGPSHomeFrame, .complete = completeGPSHomeFrame},
    {.marker = 'E', .parse = parseEventFrame,   .complete = completeEventFrame},
    {.marker = 'S', .parse = parseSlowFrame,    .complete = completeSlowFrame}
};

/**
 * Parse a comma-separated list of field names into the given frame definition. Sets the fieldCount field based on the
 * number of names parsed.
 */
static void parseFieldNames(const char *line, flightLogFrameDef_t *frameDef)
{
    char *start, *end;
    bool done = false;

    //Make a copy of the line so we can manage its lifetime (and write to it to null terminate the fields)
    frameDef->namesLine = strdup(line);
    frameDef->fieldCount = 0;

    start = frameDef->namesLine;

    while (!done && *start) {
        end = start;

        do {
            end++;
        } while (*end != ',' && *end != 0);

        frameDef->fieldName[frameDef->fieldCount++] = start;

        if (*end == 0) {
            done = true;
        }

        *end = 0;

        start = end + 1;
    }
}

static void parseCommaSeparatedIntegers(char *line, int *target, int maxCount)
{
    char *start, *end;
    bool done = false;

    start = line;

    while (!done && *start && maxCount > 0) {
        end = start + 1;

        while (*end != ',' && *end != 0)
            end++;

        if (*end == 0) {
            done = true;
        }

        *end = 0;

        *target = atoi(start);
        target++;
        maxCount--;

        start = end + 1;
    }
}

static void identifyMainFields(flightLog_t *log, flightLogFrameDef_t *frameDef)
{
    int fieldIndex;

    for (fieldIndex = 0; fieldIndex < frameDef->fieldCount; fieldIndex++) {
        const char *fieldName = frameDef->fieldName[fieldIndex];

        if (startsWith(fieldName, "motor[")) {
            int motorIndex = atoi(fieldName + strlen("motor["));

            if (motorIndex >= 0 && motorIndex < FLIGHT_LOG_MAX_MOTORS) {
                log->mainFieldIndexes.motor[motorIndex] = fieldIndex;
            }
        } else if (startsWith(fieldName, "rcCommand[")) {
            int rcCommandIndex = atoi(fieldName + strlen("rcCommand["));

            if (rcCommandIndex >= 0 && rcCommandIndex < 4) {
                log->mainFieldIndexes.rcCommand[rcCommandIndex] = fieldIndex;
            }
        } else if (startsWith(fieldName, "axis")) {
            int axisIndex = atoi(fieldName + strlen("axisX["));

            switch (fieldName[strlen("axis")]) {
            case 'P':
                log->mainFieldIndexes.pid[0][axisIndex] = fieldIndex;
                break;
            case 'I':
                log->mainFieldIndexes.pid[1][axisIndex] = fieldIndex;
                break;
            case 'D':
                log->mainFieldIndexes.pid[2][axisIndex] = fieldIndex;
                break;
            }
        } else if (startsWith(fieldName, "gyroData[")) {
            int axisIndex = atoi(fieldName + strlen("gyroData["));

            log->mainFieldIndexes.gyroADC[axisIndex] = fieldIndex;
        } else if (startsWith(fieldName, "gyroADC[")) {
            int axisIndex = atoi(fieldName + strlen("gyroADC["));

            log->mainFieldIndexes.gyroADC[axisIndex] = fieldIndex;
        } else if (startsWith(fieldName, "magADC[")) {
            int axisIndex = atoi(fieldName + strlen("magADC["));

            log->mainFieldIndexes.magADC[axisIndex] = fieldIndex;
        } else if (startsWith(fieldName, "accSmooth[")) {
            int axisIndex = atoi(fieldName + strlen("accSmooth["));

            log->mainFieldIndexes.accSmooth[axisIndex] = fieldIndex;
        } else if (startsWith(fieldName, "servo[")) {
            int servoIndex = atoi(fieldName + strlen("servo["));

            log->mainFieldIndexes.servo[servoIndex] = fieldIndex;
        } else if (strcmp(fieldName, "vbatLatest") == 0) {
            log->mainFieldIndexes.vbatLatest = fieldIndex;
        } else if (strcmp(fieldName, "amperageLatest") == 0) {
            log->mainFieldIndexes.amperageLatest = fieldIndex;
        } else if (strcmp(fieldName, "BaroAlt") == 0) {
            log->mainFieldIndexes.BaroAlt = fieldIndex;
        } else if (strcmp(fieldName, "sonarRaw") == 0) {
            log->mainFieldIndexes.sonarRaw = fieldIndex;
        } else if (strcmp(fieldName, "rssi") == 0) {
            log->mainFieldIndexes.rssi = fieldIndex;
        } else if (strcmp(fieldName, "loopIteration") == 0) {
            log->mainFieldIndexes.loopIteration = fieldIndex;
        } else if (strcmp(fieldName, "time") == 0) {
            log->mainFieldIndexes.time = fieldIndex;
        }
    }
}

static void clearFieldIdents(flightLog_t *log)
{
    /*
     * Start off all the field indexes as -1 so we can use that as a not-present identifier.
     * Conveniently, -1 has all bits set so we can just byte-fill
     */

    memset(&log->mainFieldIndexes, (char) 0xFF, sizeof(log->mainFieldIndexes));
    memset(&log->gpsFieldIndexes, (char) 0xFF, sizeof(log->gpsFieldIndexes));
    memset(&log->gpsHomeFieldIndexes, (char) 0xFF, sizeof(log->gpsHomeFieldIndexes));
}

static void identifyGPSFields(flightLog_t *log, flightLogFrameDef_t *frameDef)
{
    int i;

    for (i = 0; i < frameDef->fieldCount; i++) {
        const char *fieldName = frameDef->fieldName[i];

        if (strcmp(fieldName, "time") == 0) {
            log->gpsFieldIndexes.time = i;
        } else if (strcmp(fieldName, "GPS_numSat") == 0) {
            log->gpsFieldIndexes.GPS_numSat = i;
        } else if (strcmp(fieldName, "GPS_altitude") == 0)  {
            log->gpsFieldIndexes.GPS_altitude = i;
        } else if (strcmp(fieldName, "GPS_speed") == 0) {
            log->gpsFieldIndexes.GPS_speed = i;
        } else if (strcmp(fieldName, "GPS_ground_course") == 0) {
            log->gpsFieldIndexes.GPS_ground_course = i;
        } else if (startsWith(fieldName, "GPS_coord[")) {
            int coordIndex = atoi(fieldName + strlen("GPS_coord["));

            log->gpsFieldIndexes.GPS_coord[coordIndex] = i;
        }
    }
}

static void identifyGPSHomeFields(flightLog_t *log, flightLogFrameDef_t *frameDef)
{
    int i;

    for (i = 0; i < frameDef->fieldCount; i++) {
        const char *fieldName = frameDef->fieldName[i];

        if (strcmp(fieldName, "GPS_home[0]") == 0) {
            log->gpsHomeFieldIndexes.GPS_home[0] = i;
        } else if (strcmp(fieldName, "GPS_home[1]") == 0) {
            log->gpsHomeFieldIndexes.GPS_home[1] = i;
        }
    }
}

static void identifySlowFields(flightLog_t *log, flightLogFrameDef_t *frameDef)
{
    int i;

    for (i = 0; i < frameDef->fieldCount; i++) {
        const char *fieldName = frameDef->fieldName[i];

        if (strcmp(fieldName, "flightModeFlags") == 0) {
            log->slowFieldIndexes.flightModeFlags = i;
        } else if (strcmp(fieldName, "stateFlags") == 0) {
            log->slowFieldIndexes.stateFlags = i;
        } else if (strcmp(fieldName, "failsafePhase") == 0) {
            log->slowFieldIndexes.failsafePhase = i;
        }
    }
}

static void identifyFields(flightLog_t * log, uint8_t frameType, flightLogFrameDef_t *frameDef)
{
    switch (frameType) {
        case 'I':
            identifyMainFields(log, frameDef);
        break;
        case 'G':
            identifyGPSFields(log, frameDef);
        break;
        case 'H':
            identifyGPSHomeFields(log, frameDef);
        break;
        case 'S':
            identifySlowFields(log, frameDef);
        break;
        default:
            ;
    }
}

static time_t parseDateTime(char* fieldValue) {
    int year, month, day;
    struct tm parsedTime;
    if(sscanf(fieldValue, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &parsedTime.tm_hour, &parsedTime.tm_min, &parsedTime.tm_sec) != EOF){
        // tm_year is years since 1900
        parsedTime.tm_year = year - 1900;
        // tm_months is months since january
        parsedTime.tm_mon = month - 1;
        parsedTime.tm_mday = day;
        parsedTime.tm_isdst = 0; //Ignore daylight savings time for GPS time
    }
    return mktime(&parsedTime);;
}

// Parses a single header line. Does not change parserState.
// Assumes stream->pos points to 'H' of a configuration header. Consumes the entire line.
static size_t parseHeaderLine(flightLog_t *log, mmapStream_t *stream) {

    if (streamReadByte(stream) != 'H') {
        // This case should ideally not be hit if called after peeking 'H'
        return 0;
    }

    if (streamReadByte(stream) != ' ') {
        // Malformed header line, missing space after 'H'
        return 1; // Consumed 'H'
    }

    const char *lineStart = stream->pos; // Position after "H "
    char valueBuffer[FLIGHT_LOG_MAX_FRAME_HEADER_LENGTH];
    const char *separatorPos = 0;
    size_t i = 0; // Characters read for the current line content (after "H ")
    for ( ; i < FLIGHT_LOG_MAX_FRAME_HEADER_LENGTH; ++i) {
        char c = streamReadChar(stream); // Consumes character

        if (c == ':' && !separatorPos) {
            separatorPos = stream->pos - 1; // stream->pos is now after ':', so -1 points to ':'
        }

        if (c == '\n') {
            // i is current char index, so i+1 is count of chars including this newline
            i++;
            break;
        }

        if (c == EOF || c == '\0') {
            // Line ended prematurely or contains null byte
            // i is count of chars read before EOF/null
            break;
        }
        valueBuffer[i] = c;
    }
    // Total bytes consumed for this line = 2 ("H ") + i (content including newline or up to EOF/error)
    size_t totalLineBytesConsumed = 2 + i;

    if (!separatorPos) {
        // No colon found, malformed header or just a comment line starting with "H "
        return totalLineBytesConsumed;
    }

    char *fieldName = valueBuffer;
    valueBuffer[separatorPos - lineStart] = '\0'; // Null-terminate fieldName

    char *fieldValue = valueBuffer + (separatorPos - lineStart) + 1; // Points to char after ':'
    // fieldValue needs to be null-terminated before the newline.
    if (i > 0 && valueBuffer[i-1] == '\n') {
        valueBuffer[i-1] = '\0';
    } else if (i < FLIGHT_LOG_MAX_FRAME_HEADER_LENGTH) {
        // Line ended without newline (e.g. EOF, or filled buffer)
        valueBuffer[i] = '\0';
    } else {
        // Buffer full, ensure null termination if possible (should be rare)
        valueBuffer[FLIGHT_LOG_MAX_FRAME_HEADER_LENGTH -1] = '\0';
    }


    if (startsWith(fieldName, "Field ")) {
        uint8_t frameTypeMarker = (uint8_t) fieldName[strlen("Field ")];
        flightLogFrameDef_t *frameDef = &log->frameDefs[frameTypeMarker];

        if (endsWith(fieldName, " name")) {
            parseFieldNames(fieldValue, frameDef);
            identifyFields(log, frameTypeMarker, frameDef);

            if (frameTypeMarker == 'I') {
                // P frames are derived from I frames so copy common data over to the P frame:
                memcpy(log->frameDefs['P'].fieldName, frameDef->fieldName, sizeof(frameDef->fieldName));
                log->frameDefs['P'].fieldCount = frameDef->fieldCount;
            }
        } else if (endsWith(fieldName, " signed")) {
            parseCommaSeparatedIntegers(fieldValue, frameDef->fieldSigned, FLIGHT_LOG_MAX_FIELDS);

            if (frameTypeMarker == 'I') {
                memcpy(log->frameDefs['P'].fieldSigned, frameDef->fieldSigned, sizeof(frameDef->fieldSigned));
            }
        } else if (endsWith(fieldName, " predictor")) {
            parseCommaSeparatedIntegers(fieldValue, frameDef->predictor, FLIGHT_LOG_MAX_FIELDS);
        } else if (endsWith(fieldName, " encoding")) {
            parseCommaSeparatedIntegers(fieldValue, frameDef->encoding, FLIGHT_LOG_MAX_FIELDS);
        }
    } else if (strcmp(fieldName, "I interval") == 0) {
        log->frameIntervalI = atoi(fieldValue);
        if (log->frameIntervalI < 1)
            log->frameIntervalI = 1;
    } else if (strcmp(fieldName, "P interval") == 0) {
        char *slashPos = strchr(fieldValue, '/');

        if (slashPos) {
            log->frameIntervalPNum = atoi(fieldValue);
            log->frameIntervalPDenom = atoi(slashPos + 1);
        }
    } else if (strcmp(fieldName, "Data version") == 0) {
        log->private->dataVersion = atoi(fieldValue);
    } else if (strcmp(fieldName, "Firmware type") == 0) {
        if (strcmp(fieldValue, "Cleanflight") == 0)
            log->sysConfig.firmwareType = FIRMWARE_TYPE_CLEANFLIGHT;
        else
            log->sysConfig.firmwareType = FIRMWARE_TYPE_BASEFLIGHT;
    } else if (strcmp(fieldName, "Firmware revision") == 0) {
        char fieldCopy[200];
        strncpy(fieldCopy, fieldValue, sizeof(fieldCopy) - 1);
        fieldCopy[sizeof(fieldCopy) - 1] = '\0';
        char* fcName = strtok(fieldCopy, " "); //Read firmware name
        if (fcName && !strcmp(fcName, "Betaflight")) { //Version text location known for Betaflight firmware
            char* fcVersion = strtok(NULL, " "); //Firmware version text
            if (fcVersion) strncpy(log->private->fcVersion, fcVersion, sizeof(log->private->fcVersion) - 1);
            log->private->fcVersion[sizeof(log->private->fcVersion) - 1] = '\0';
        } else {
            log->private->fcVersion[0] = 0; //Indicate that firmware version unknown
        }
    } else if (strcmp(fieldName, "minthrottle") == 0) {
        log->sysConfig.minthrottle = atoi(fieldValue);
        log->sysConfig.motorOutputLow = log->sysConfig.minthrottle;
    } else if (strcmp(fieldName, "maxthrottle") == 0) {
        log->sysConfig.maxthrottle = atoi(fieldValue);
        log->sysConfig.motorOutputHigh = log->sysConfig.maxthrottle;
    } else if (strcmp(fieldName, "rcRate") == 0) {
        log->sysConfig.rcRate = atoi(fieldValue);
    } else if (strcmp(fieldName, "vbatscale") == 0) {
        log->sysConfig.vbatscale = atoi(fieldValue);
    } else if (strcmp(fieldName, "vbatref") == 0) {
        log->sysConfig.vbatref = atoi(fieldValue);
    } else if (strcmp(fieldName, "vbatcellvoltage") == 0) {
        int vbatcellvoltage[3];
        parseCommaSeparatedIntegers(fieldValue, vbatcellvoltage, 3);
        log->sysConfig.vbatmincellvoltage = vbatcellvoltage[0];
        log->sysConfig.vbatwarningcellvoltage = vbatcellvoltage[1];
        log->sysConfig.vbatmaxcellvoltage = vbatcellvoltage[2];
    } else if (strcmp(fieldName, "currentMeter") == 0) {
        int currentMeterParams[2];
        parseCommaSeparatedIntegers(fieldValue, currentMeterParams, 2);
        log->sysConfig.currentMeterOffset = currentMeterParams[0];
        log->sysConfig.currentMeterScale = currentMeterParams[1];
    } else if (strcmp(fieldName, "gyro.scale") == 0 || strcmp(fieldName, "gyro_scale") == 0) {
        floatConvert.u = strtoul(fieldValue, 0, 16);
        log->sysConfig.gyroScale = floatConvert.f;
        if (log->sysConfig.firmwareType != FIRMWARE_TYPE_BASEFLIGHT) {
            log->sysConfig.gyroScale = (float) (log->sysConfig.gyroScale * (M_PI / 180.0) * 0.000001);
        }
    } else if (strcmp(fieldName, "acc_1G") == 0) {
        log->sysConfig.acc_1G = atoi(fieldValue);
    } else if (strcmp(fieldName, "motorOutput") == 0) {
        int motorOutputs[2];
        parseCommaSeparatedIntegers(fieldValue, motorOutputs, 2);
        log->sysConfig.motorOutputLow = motorOutputs[0];
        log->sysConfig.motorOutputHigh = motorOutputs[1];
     } else if (startsWith(fieldName,"Log start datetime"))  {
        log->dateTime = parseDateTime(fieldValue);
    }

     return totalLineBytesConsumed;
} // End of parseHeaderLine

/**
 * Should a frame with the given index exist in this log (based on the user's selection of sampling rates)?
 */
static int shouldHaveFrame(flightLog_t *log, int32_t frameIndex)
{
    return (frameIndex % log->frameIntervalI + log->frameIntervalPNum - 1) % log->frameIntervalPDenom < log->frameIntervalPNum;
}

/**
 * Take the raw value for a a field, apply the prediction that is configured for it, and return it.
 */
static int64_t applyPrediction(flightLog_t *log, int fieldIndex, int predictor, int64_t value, int64_t *current, int64_t *previous, int64_t *previous2)
{
    flightLogPrivate_t *private = log->private;

    // First see if we have a prediction that doesn't require a previous frame as reference:
    switch (predictor) {
        case FLIGHT_LOG_FIELD_PREDICTOR_0:
            // No correction to apply
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_MINTHROTTLE:
            value += log->sysConfig.minthrottle;
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_1500:
            value += 1500;
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_MOTOR_0:
            if (log->mainFieldIndexes.motor[0] < 0) {
                fprintf(stderr, "Attempted to base prediction on motor[0] without that field being defined\n");
                exit(-1);
            }
            value += current[log->mainFieldIndexes.motor[0]];
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_VBATREF:
            value += log->sysConfig.vbatref;
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_PREVIOUS:
            if (!previous)
                break;

            value += previous[fieldIndex];
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_STRAIGHT_LINE:
            if (!previous)
                break;

            value += 2 * previous[fieldIndex] - previous2[fieldIndex];
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_AVERAGE_2:
            if (!previous)
                break;

            value += (previous[fieldIndex] + previous2[fieldIndex]) / 2;
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_HOME_COORD:
            if (log->gpsHomeFieldIndexes.GPS_home[0] < 0) {
                fprintf(stderr, "Attempted to base prediction on GPS home position without GPS home frame definition\n");
                exit(-1);
            }

            value += private->gpsHomeHistory[1][log->gpsHomeFieldIndexes.GPS_home[0]];
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_HOME_COORD_1:
            if (log->gpsHomeFieldIndexes.GPS_home[1] < 1) {
                fprintf(stderr, "Attempted to base prediction on GPS home position without GPS home frame definition\n");
                exit(-1);
            }

            value += private->gpsHomeHistory[1][log->gpsHomeFieldIndexes.GPS_home[1]];
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_LAST_MAIN_FRAME_TIME:
            if (private->mainHistory[1])
                value += private->mainHistory[1][FLIGHT_LOG_FIELD_INDEX_TIME];
        break;
        case FLIGHT_LOG_FIELD_PREDICTOR_MINMOTOR:
            value += log->sysConfig.motorOutputLow;
        break;
        default:
            fprintf(stderr, "Unsupported field predictor %d\n", predictor);
            exit(-1);
    }

    return value;
}

/**
 * Attempt to parse the frame of the given `frameType` into the supplied `frame` buffer using the encoding/predictor
 * definitions from log->frameDefs[`frameType`].
 *
 * raw - Set to true to disable predictions (and so store raw values)
 * skippedFrames - Set to the number of field iterations that were skipped over by rate settings since the last frame.
 */
static void parseFrame(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, int64_t *frame, int64_t *previous, int64_t *previous2, int skippedFrames, bool raw)
{
    flightLogFrameDef_t *frameDef = &log->frameDefs[frameType];

    int *predictor = frameDef->predictor;
    int *encoding = frameDef->encoding;
    int *fieldSigned = frameDef->fieldSigned;
    int *fieldWidth = frameDef->fieldWidth;

    int i, j, groupCount;

    i = 0;
    while (i < frameDef->fieldCount) {
        int64_t value;
        int64_t values[8];

        if (predictor[i] == FLIGHT_LOG_FIELD_PREDICTOR_INC) {
            frame[i] = skippedFrames + 1;

            if (previous)
                frame[i] += previous[i];

            i++;
        } else {
            switch (encoding[i]) {
                case FLIGHT_LOG_FIELD_ENCODING_SIGNED_VB:
                    streamByteAlign(stream);

                    value = streamReadSignedVB(stream);
                break;
                case FLIGHT_LOG_FIELD_ENCODING_UNSIGNED_VB:
                    streamByteAlign(stream);

                    value = streamReadUnsignedVB(stream);
                break;
                case FLIGHT_LOG_FIELD_ENCODING_NEG_14BIT:
                    streamByteAlign(stream);

                    value = -signExtend14Bit(streamReadUnsignedVB(stream));
                break;
                case FLIGHT_LOG_FIELD_ENCODING_TAG8_4S16:
                    streamByteAlign(stream);

                    if (log->private->dataVersion < 2)
                        streamReadTag8_4S16_v1(stream, values);
                    else
                        streamReadTag8_4S16_v2(stream, values);

                    //Apply the predictors for the fields:
                    for (j = 0; j < 4; j++, i++)
                        frame[i] = applyPrediction(log, i, raw ? FLIGHT_LOG_FIELD_PREDICTOR_0 : predictor[i], values[j], frame, previous, previous2);

                    continue;
                break;
                case FLIGHT_LOG_FIELD_ENCODING_TAG2_3S32:
                    streamByteAlign(stream);

                    streamReadTag2_3S32(stream, values);

                    //Apply the predictors for the fields:
                    for (j = 0; j < 3; j++, i++)
                        frame[i] = applyPrediction(log, i, raw ? FLIGHT_LOG_FIELD_PREDICTOR_0 : predictor[i], values[j], frame, previous, previous2);

                    continue;
                break;
                case FLIGHT_LOG_FIELD_ENCODING_TAG8_8SVB:
                    streamByteAlign(stream);

                    //How many fields are in this encoded group? Check the subsequent field encodings:
                    for (j = i + 1; j < i + 8 && j < frameDef->fieldCount; j++)
                        if (encoding[j] != FLIGHT_LOG_FIELD_ENCODING_TAG8_8SVB)
                            break;

                    groupCount = j - i;

                    streamReadTag8_8SVB(stream, values, groupCount);

                    for (j = 0; j < groupCount; j++, i++)
                        frame[i] = applyPrediction(log, i, raw ? FLIGHT_LOG_FIELD_PREDICTOR_0 : predictor[i], values[j], frame, previous, previous2);

                    continue;
                break;
                case FLIGHT_LOG_FIELD_ENCODING_ELIAS_DELTA_U32:
                    value = streamReadEliasDeltaU32(stream);

                    /*
                     * Reading this bitvalue may cause the stream's bit pointer to no longer lie on a byte boundary, so be sure to call
                     * streamByteAlign() if you want to read a byte from the stream later.
                     */
                break;
                case FLIGHT_LOG_FIELD_ENCODING_ELIAS_DELTA_S32:
                    value = streamReadEliasDeltaS32(stream);
                break;
                case FLIGHT_LOG_FIELD_ENCODING_ELIAS_GAMMA_U32:
                    value = streamReadEliasGammaU32(stream);
                break;
                case FLIGHT_LOG_FIELD_ENCODING_ELIAS_GAMMA_S32:
                    value = streamReadEliasGammaS32(stream);
                break;
                case FLIGHT_LOG_FIELD_ENCODING_NULL:
                    //Nothing to read
                    value = 0;
                break;
                default:
                    fprintf(stderr, "Unsupported field encoding %d\n", encoding[i]);
                    exit(-1);
            }

            value = applyPrediction(log, i, raw ? FLIGHT_LOG_FIELD_PREDICTOR_0 : predictor[i], value, frame, previous, previous2);

            if (fieldWidth[i] != 8) {
                // Assume 32-bit...
                if (fieldSigned[i]) {
                    value = (int32_t) value; // Sign extend the lower 32-bits
                } else {
                    value = (uint32_t) value;
                }
            }

            frame[i] = value;

            i++;
        }
    }

    streamByteAlign(stream);
}

/*
 * Based on the log sampling rate, work out how many frames would have been skipped after the last frame that was
 * parsed until we get to the next logged iteration.
 */
static uint32_t countIntentionallySkippedFrames(flightLog_t *log)
{
    flightLogPrivate_t *private = log->private;
    uint32_t count = 0, frameIndex;

    if (private->lastMainFrameIteration == (uint32_t) -1) {
        // Haven't parsed a frame yet so there's no frames to skip
        return 0;
    } else {
        for (frameIndex = private->lastMainFrameIteration + 1; !shouldHaveFrame(log, frameIndex); frameIndex++) {
            count++;
        }
    }

    return count;
}

/*
 * Based on the log sampling rate, work out how many frames would have been skipped after the last frame that was
 * parsed until we get to the iteration with the given index.
 */
static uint32_t countIntentionallySkippedFramesTo(flightLog_t *log, uint32_t targetIteration)
{
    flightLogPrivate_t *private = log->private;
    uint32_t count = 0, frameIndex;

    if (private->lastMainFrameIteration == (uint32_t) -1) {
        // Haven't parsed a frame yet so there's no frames to skip
        return 0;
    } else {
        for (frameIndex = private->lastMainFrameIteration + 1; frameIndex < targetIteration; frameIndex++) {
            if (!shouldHaveFrame(log, frameIndex)) {
                count++;
            }
        }
    }

    return count;
}

/**
 * Attempt to parse the Intraframe at the current log position into the history buffer at mainHistory[0].
 */
static void parseIntraframe(flightLog_t *log, mmapStream_t *stream, bool raw)
{
    flightLogPrivate_t *private = log->private;

    int64_t *current = private->mainHistory[0];
    int64_t *previous = private->mainHistory[1];

    parseFrame(log, stream, 'I', current, previous, NULL, 0, raw);
}

/**
 * Attempt to parse the interframe at the current log position into the history buffer at mainHistory[0].
 */
static void parseInterframe(flightLog_t *log, mmapStream_t *stream, bool raw)
{
    flightLogPrivate_t *private = log->private;

    int64_t *current = log->private->mainHistory[0];
    int64_t *previous = log->private->mainHistory[1];
    int64_t *previous2 = log->private->mainHistory[2];

    private->lastSkippedFrames = countIntentionallySkippedFrames(log);

    parseFrame(log, stream, 'P', current, previous, previous2, log->private->lastSkippedFrames, raw);
}

static void parseGPSFrame(flightLog_t *log, mmapStream_t *stream, bool raw)
{
    parseFrame(log, stream, 'G', log->private->lastGPS, NULL, NULL, 0, raw);
}

static void parseGPSHomeFrame(flightLog_t *log, mmapStream_t *stream, bool raw)
{
    parseFrame(log, stream, 'H', log->private->gpsHomeHistory[0], NULL, NULL, 0, raw);
}

static void parseSlowFrame(flightLog_t *log, mmapStream_t *stream, bool raw)
{
    parseFrame(log, stream, 'S', log->private->lastSlow, NULL, NULL, 0, raw);
}

/**
 * Attempt to parse an event frame at the current location into the log->private->lastEvent struct.
 * Return false if the event couldn't be parsed (e.g. unknown event ID), or true if it might have been
 * parsed successfully.
 */
static void parseEventFrame(flightLog_t *log, mmapStream_t *stream, bool raw)
{
    static const char END_OF_LOG_MESSAGE[] = "End of log\0";
    enum { END_OF_LOG_MESSAGE_LEN = 11};

    char endMessage[END_OF_LOG_MESSAGE_LEN];
    (void) raw;
    uint8_t eventType = streamReadByte(stream);

    flightLogEventData_t *data = &log->private->lastEvent.data;
    log->private->lastEvent.event = eventType;

    switch (eventType) {
        case FLIGHT_LOG_EVENT_SYNC_BEEP:
            data->syncBeep.time = streamReadUnsignedVB(stream) + log->private->timeRolloverAccumulator;
        break;
        case FLIGHT_LOG_EVENT_INFLIGHT_ADJUSTMENT:
             data->inflightAdjustment.adjustmentFunction = streamReadByte(stream);
             if (data->inflightAdjustment.adjustmentFunction > 127) {
                 data->inflightAdjustment.newFloatValue = streamReadRawFloat(stream);
             } else {
                 data->inflightAdjustment.newValue = streamReadSignedVB(stream);
             }
        break;
        case FLIGHT_LOG_EVENT_LOGGING_RESUME:
            data->loggingResume.logIteration = streamReadUnsignedVB(stream);
            data->loggingResume.currentTime = streamReadUnsignedVB(stream) + log->private->timeRolloverAccumulator;
        break;
        case FLIGHT_LOG_EVENT_LOG_END:
            streamRead(stream, endMessage, END_OF_LOG_MESSAGE_LEN);

            if (strncmp(endMessage, END_OF_LOG_MESSAGE, END_OF_LOG_MESSAGE_LEN) == 0) {
                //Adjust the end of stream so we stop reading, this log is done
                stream->end = stream->pos;
            } else {
                /*
                 * This isn't the real end of log message, it's probably just some bytes that happened to look like
                 * an event header.
                 */
                log->private->lastEvent.event = -1;
            }
        break;
        default:
            log->private->lastEvent.event = -1;
    }
}

static void updateMainFieldStatistics(flightLog_t *log, int64_t *fields)
{
    int i;
    flightLogFrameDef_t *frameDef = &log->frameDefs['I'];

    if (!log->stats.haveFieldStats) {
        //If this is the first frame, there are no minimums or maximums in the stats to compare with
        for (i = 0; i < frameDef->fieldCount; i++) {
            log->stats.field[i].max = fields[i];
            log->stats.field[i].min = fields[i];
        }

        log->stats.haveFieldStats = true;
    } else {
        for (i = 0; i < frameDef->fieldCount; i++) {
            log->stats.field[i].max = fields[i] > log->stats.field[i].max ? fields[i] : log->stats.field[i].max;
            log->stats.field[i].min = fields[i] < log->stats.field[i].min ? fields[i] : log->stats.field[i].min;
        }
    }
}

#define ADCVREF 33L

unsigned int flightLogVbatADCToMillivolts(flightLog_t *log, uint16_t vbatADC)
{
    // ADC is 12 bit (i.e. max 0xFFF), voltage reference is 3.3V, vbatscale is premultiplied by 100
    return (vbatADC * ADCVREF * 10 * log->sysConfig.vbatscale) / 0xFFF;
}

int flightLogAmperageADCToMilliamps(flightLog_t *log, uint16_t amperageADC)
{
    int32_t millivolts;

    millivolts = ((uint32_t)amperageADC * ADCVREF * 100) / 4095;
    millivolts -= log->sysConfig.currentMeterOffset;

    return ((int64_t) millivolts * 10000) / log->sysConfig.currentMeterScale;
}

int flightLogEstimateNumCells(flightLog_t *log)
{
    int i;
    int refVoltage;

    refVoltage = flightLogVbatADCToMillivolts(log, log->sysConfig.vbatref) / 100;

    for (i = 1; i < 8; i++) {
        if (refVoltage < i * log->sysConfig.vbatmaxcellvoltage)
            break;
    }

    return i;
}

double flightlogAccelerationRawToGs(flightLog_t *log, int32_t accRaw)
{
    return (double)accRaw / log->sysConfig.acc_1G;
}

double flightlogGyroToRadiansPerSecond(flightLog_t *log, int32_t gyroRaw)
{
    // gyroScale is set to give radians per microsecond, so multiply by 1,000,000 out to get the per-second value
    return (double)log->sysConfig.gyroScale * 1000000 * gyroRaw;
}

static void flightlogDecodeFlagsToString(uint32_t flags, int numFlags, const char * const *flagNames, char *dest, unsigned destLen)
{
    bool printedFlag = false;
    const char NO_FLAGS_MESSAGE[] = "0";

    // The buffer should at least be large enough for us to add the "no flags present" message in!
    if (destLen < strlen(NO_FLAGS_MESSAGE) + 1) {
        fprintf(stderr, "Flag buffer too short\n");
        exit(-1);
    }

    for (int i = 0; i < numFlags; i++) {
        if ((flags & (1 << i)) != 0) {
            const char *flagName = flagNames[i];
            unsigned flagNameLen = strlen(flagName);

            if (destLen < (printedFlag ? 1 : 0) + flagNameLen + 1 /* Null-terminator */) {
                // Not enough room in the dest string to fit this flag
                fprintf(stderr, "Flag buffer too short\n");
                exit(-1);
            }

            if (printedFlag) {
                *dest = '|';
                dest++;
                destLen--;
            } else {
                printedFlag = true;
            }

            strcpy(dest, flagName);

            dest += flagNameLen;
            destLen -= flagNameLen;
        }
    }

    if (!printedFlag) {
        strcpy(dest, NO_FLAGS_MESSAGE);
    }
}

void flightlogDecodeEnumToString(uint32_t value, unsigned numEnums, const char * const *enumNames, char *dest, unsigned destLen)
{
    assert(destLen > 1);

    if (value < numEnums) {
        const char *name = enumNames[value];

        if (strlen(name) < destLen) {
            strcpy(dest, name);
        } else {
            dest[0] = '\0';
        }
    } else {
        // Since we don't have a name for this value, print it as a raw integer instead

        snprintf(dest, destLen, "%u", value);
    }
}

void flightlogFlightModeToString(uint32_t flightMode, char *dest, int destLen)
{
    flightlogDecodeFlagsToString(flightMode, FLIGHT_LOG_FLIGHT_MODE_COUNT, FLIGHT_LOG_FLIGHT_MODE_NAME, dest, destLen);
}

void flightlogFlightStateToString(uint32_t flightState, char *dest, int destLen)
{
    flightlogDecodeFlagsToString(flightState, FLIGHT_LOG_FLIGHT_STATE_COUNT, FLIGHT_LOG_FLIGHT_STATE_NAME, dest, destLen);
}

void flightlogFailsafePhaseToString(uint8_t failsafePhase, char *dest, int destLen)
{
    flightlogDecodeEnumToString(failsafePhase, FLIGHT_LOG_FAILSAFE_PHASE_COUNT, FLIGHT_LOG_FAILSAFE_PHASE_NAME, dest, destLen);
}

flightLog_t * flightLogCreate(int fd)
{
    const char *logSearchStart;
    int logIndex;

    flightLog_t *log;
    flightLogPrivate_t *private;

    log = (flightLog_t *) malloc(sizeof(*log));
    private = (flightLogPrivate_t *) malloc(sizeof(*private));

    memset(log, 0, sizeof(*log));
    memset(private, 0, sizeof(*private));

    private->stream = streamCreate(fd);

    if (!private->stream) {
        free(log);
        free(private);

        return 0;
    }

    if (private->stream->size == 0 && (private->stream->mapping.stats.st_mode & S_IFREG) ==  S_IFREG) {
        fprintf(stderr, "Error: This log is zero-bytes long!\n");

        streamDestroy(private->stream);

        free(log);
        free(private);

        return 0;
    }

    if ((private->stream->mapping.stats.st_mode & S_IFMT) == S_IFREG) {
    //First check how many logs are in this one file (each time the FC is rearmed, a new log is appended)
    logSearchStart = private->stream->data;

    for (logIndex = 0; logIndex < FLIGHT_LOG_MAX_LOGS_IN_FILE && logSearchStart < private->stream->data + private->stream->size; logIndex++) {
        log->logBegin[logIndex] = memmem(logSearchStart, (private->stream->data + private->stream->size) - logSearchStart, LOG_START_MARKER, strlen(LOG_START_MARKER));

        if (!log->logBegin[logIndex])
            break; //No more logs found in the file

        //Search for the next log after this header ends
        logSearchStart = log->logBegin[logIndex] + strlen(LOG_START_MARKER);
    }

    log->logCount = logIndex;

    /*
     * Stick the end of the file as the beginning of the "one past end" log, so we can easily compute each log size.
     *
     * We have room for this because the logBegin array has an extra element on the end for it.
     */
    log->logBegin[log->logCount] = private->stream->data + private->stream->size;
    } else {
    log->logCount = 1; //one stream 1 log.
    log->logBegin[0] = private->stream->data;
    log->logBegin[1] = private->stream->end;
    }

    log->private = private;

    return log;
}

static const flightLogFrameType_t* getFrameType(uint8_t c)
{
    for (int i = 0; i < (int) ARRAY_LENGTH(frameTypes); i++)
        if (frameTypes[i].marker == c)
            return &frameTypes[i];

    return 0;
}

/**
 * Check that the values in the currently-decoded main frame (mainHistory[0]) don't look corrupted.
 *
 */
static bool flightLogValidateMainFrameValues(flightLog_t *log)
{
    flightLogPrivate_t *private = log->private;

    // Check that iteration count and time didn't move backwards, and didn't move forward too much.
    return
        (uint32_t) private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_ITERATION] >= private->lastMainFrameIteration
        && (uint32_t) private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_ITERATION] < private->lastMainFrameIteration + MAXIMUM_ITERATION_JUMP_BETWEEN_FRAMES
        && private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_TIME] >= private->lastMainFrameTime
        && private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_TIME] < private->lastMainFrameTime + MAXIMUM_TIME_JUMP_BETWEEN_FRAMES;
}

static void flightLogInvalidateStream(flightLog_t *log)
{
    log->private->mainStreamIsValid = false;
    log->private->mainHistory[1] = 0;
    log->private->mainHistory[2] = 0;
}

/**
 * Detects rollovers in the 32-bit system microtime that we use for frame timestamps. When a rollover is detected, it
 * is accumulated to allow the recovery of 64-bit timestamps.
 *
 * Returns the recovered 64-bit timestamp for the frame.
 */
static int64_t flightLogDetectAndApplyTimestampRollover(flightLog_t *log, int64_t timestamp)
{
    if (log->private->lastMainFrameTime != -1) {
        if (
            // If we appeared to travel backwards in time (modulo 32 bits)...
            (uint32_t) timestamp < (uint32_t) log->private->lastMainFrameTime
            // But we actually just incremented a reasonable amount (modulo 32-bits)...
            && (uint32_t) ((uint32_t) timestamp - (uint32_t) log->private->lastMainFrameTime) < MAXIMUM_TIME_JUMP_BETWEEN_FRAMES
        ) {
            // 32-bit time counter has wrapped, so add 2^32 to the timestamp
            log->private->timeRolloverAccumulator += 0x100000000LL;
        }
    }

    return (uint32_t) timestamp + log->private->timeRolloverAccumulator;
}

static void flightLogApplyMainFrameTimeRollover(flightLog_t *log)
{
    log->private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_TIME] = flightLogDetectAndApplyTimestampRollover(log, log->private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_TIME]);
}

static void flightLogApplyGPSFrameTimeRollover(flightLog_t *log)
{
    int timeFieldIndex = log->gpsFieldIndexes.time;

    if (timeFieldIndex != -1) {
        log->private->lastGPS[timeFieldIndex] = flightLogDetectAndApplyTimestampRollover(log, log->private->lastGPS[timeFieldIndex]);
    }
}

static bool completeIntraframe(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw)
{
    flightLogPrivate_t *private = log->private;

    flightLogApplyMainFrameTimeRollover(log);

    // Only attempt to validate the frame values if we have something to check it against
    if (!raw && private->lastMainFrameIteration != (uint32_t) -1 && !flightLogValidateMainFrameValues(log)) {
        flightLogInvalidateStream(log);
    } else {
        private->mainStreamIsValid = true;
    }

    if (private->mainStreamIsValid) {
        log->stats.intentionallyAbsentIterations += countIntentionallySkippedFramesTo(log, (uint32_t) private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_ITERATION]);

        private->lastMainFrameIteration = (uint32_t) private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_ITERATION];
        private->lastMainFrameTime = private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_TIME];

        updateMainFieldStatistics(log, log->private->mainHistory[0]);
    }

    if (private->onFrameReady) {
        private->onFrameReady(log, private->mainStreamIsValid, private->mainHistory[0], frameType, log->frameDefs[(int) frameType].fieldCount, frameStart - stream->data, frameEnd - frameStart);
    }

    if (private->mainStreamIsValid) {
        // Rotate history buffers

        // Both the previous and previous-previous states become the I-frame, because we can't look further into the past than the I-frame
        private->mainHistory[1] = private->mainHistory[0];
        private->mainHistory[2] = private->mainHistory[0];

        // And advance the current frame into an empty space ready to be filled
        private->mainHistory[0] += FLIGHT_LOG_MAX_FIELDS;
        if (private->mainHistory[0] >= &private->blackboxHistoryRing[3][0]) {
            private->mainHistory[0] = &private->blackboxHistoryRing[0][0];
        }
    }

    return private->mainStreamIsValid;
}

static bool completeInterframe(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw)
{
    flightLogPrivate_t *private = log->private;

    (void) frameType;
    (void) raw;

    flightLogApplyMainFrameTimeRollover(log);

    if (private->mainStreamIsValid && !raw && !flightLogValidateMainFrameValues(log)) {
        flightLogInvalidateStream(log);
    }

    if (private->mainStreamIsValid) {
        private->lastMainFrameIteration = (uint32_t) private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_ITERATION];
        private->lastMainFrameTime = private->mainHistory[0][FLIGHT_LOG_FIELD_INDEX_TIME];

        log->stats.intentionallyAbsentIterations += private->lastSkippedFrames;

        updateMainFieldStatistics(log, private->mainHistory[0]);
    }

    //Receiving a P frame can't resynchronise the stream so it doesn't set mainStreamIsValid to true

    if (private->onFrameReady) {
        private->onFrameReady(log, private->mainStreamIsValid, private->mainHistory[0], frameType, log->frameDefs['I'].fieldCount, frameStart - stream->data, frameEnd - frameStart);
    }

    if (private->mainStreamIsValid) {
        // Rotate history buffers
        private->mainHistory[2] = private->mainHistory[1];
        private->mainHistory[1] = private->mainHistory[0];

        // And advance the current frame into an empty space ready to be filled
        private->mainHistory[0] += FLIGHT_LOG_MAX_FIELDS;
        if (private->mainHistory[0] >= &private->blackboxHistoryRing[3][0])
            private->mainHistory[0] = &private->blackboxHistoryRing[0][0];
    }

    return private->mainStreamIsValid;
}

static bool completeEventFrame(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw)
{
    flightLogEvent_t *lastEvent = &log->private->lastEvent;

    (void) stream;
    (void) frameType;
    (void) frameStart;
    (void) frameEnd;
    (void) raw;

    //Don't bother reporting invalid event types since they're likely just garbage data that happened to look like an event
    if (lastEvent->event != (FlightLogEvent) -1) {
        switch (lastEvent->event) {
            case FLIGHT_LOG_EVENT_LOGGING_RESUME:
                /*
                 * Bring the "last time" and "last iteration" up to the new resume time so we accept the sudden jump into
                 * the future.
                 */
                log->private->lastMainFrameIteration = lastEvent->data.loggingResume.logIteration;
                log->private->lastMainFrameTime = lastEvent->data.loggingResume.currentTime;
            break;
            default:
                ;
        }

        if (log->private->onEvent) {
            log->private->onEvent(log, lastEvent);
        }

        return true;
    }

    return false;
}

static bool completeGPSHomeFrame(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw)
{
    (void) frameType;
    (void) frameStart;
    (void) frameEnd;
    (void) raw;

    //Copy the decoded frame into the "last state" entry of gpsHomeHistory to publish it:
    memcpy(&log->private->gpsHomeHistory[1], &log->private->gpsHomeHistory[0], sizeof(*log->private->gpsHomeHistory));
    log->private->gpsHomeIsValid = true;

    if (log->private->onFrameReady) {
        log->private->onFrameReady(log, true, log->private->gpsHomeHistory[1], frameType, log->frameDefs[frameType].fieldCount, frameStart - stream->data, frameEnd - frameStart);
    }

    return true;
}

static bool completeGPSFrame(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw)
{
    (void) frameType;
    (void) frameStart;
    (void) frameEnd;
    (void) raw;

    flightLogApplyGPSFrameTimeRollover(log);

    if (log->private->onFrameReady) {
        log->private->onFrameReady(log, log->private->gpsHomeIsValid, log->private->lastGPS, frameType, log->frameDefs[frameType].fieldCount, frameStart - stream->data, frameEnd - frameStart);
    }

    return true;
}

static bool completeSlowFrame(flightLog_t *log, mmapStream_t *stream, uint8_t frameType, const char *frameStart, const char *frameEnd, bool raw)
{
    (void) frameType;
    (void) frameStart;
    (void) frameEnd;
    (void) raw;

    if (log->private->onFrameReady) {
        log->private->onFrameReady(log, true, log->private->lastSlow, frameType, log->frameDefs[frameType].fieldCount, frameStart - stream->data, frameEnd - frameStart);
    }

    return true;
}

static void resetSysConfigToDefaults(flightLogSysConfig_t *config)
{
    config->minthrottle = 1150;
    config->maxthrottle = 1850;
    config->motorOutputLow = 1150;
    config->motorOutputHigh = 1850;

    config->vbatref = 4095;
    config->vbatscale = 110;
    config->vbatmincellvoltage = 33;
    config->vbatmaxcellvoltage = 43;
    config->vbatwarningcellvoltage = 35;

    config->currentMeterOffset = 0;
    config->currentMeterScale = 400;

    config->rcRate = 90;
    config->yawRate = 0;

    // Default these to silly numbers, because if we don't know the hardware we can't even begin to guess:
    config->acc_1G = 1;
    config->gyroScale = 1;

    config->firmwareType = FIRMWARE_TYPE_UNKNOWN;
}

bool flightLogParse(flightLog_t *log, int logIndex, FlightLogMetadataReady onMetadataReady, FlightLogFrameReady onFrameReady, FlightLogEventReady onEvent, bool raw) {
    ParserState parserState = PARSER_STATE_HEADER;
    const flightLogFrameType_t *frameType = 0;
    bool metadata_setup_and_ready_called = false; // To ensure one-time setup and onMetadataReady call

    flightLogPrivate_t *private = log->private;

    if (logIndex < 0 || logIndex >= log->logCount)
        return false;

    //Reset any parsed information from previous parses
    memset(&log->stats, 0, sizeof(log->stats));

    for (int frameC = 0; frameC < 256; frameC++) {
        free(log->frameDefs[frameC].namesLine);
        log->frameDefs[frameC].namesLine = NULL; // Avoid double free if parse called again on same log object
    }

    memset(log->frameDefs, 0, sizeof(log->frameDefs));

    // Apply default field widths (for older logging code that might omit the field width header)
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < FLIGHT_LOG_MAX_FIELDS; j++) {
            log->frameDefs[i].fieldWidth[j] = 4;
        }
    }

    private->gpsHomeIsValid = false;
    flightLogInvalidateStream(log);

    private->mainHistory[0] = private->blackboxHistoryRing[0];
    private->mainHistory[1] = NULL;
    private->mainHistory[2] = NULL;

    resetSysConfigToDefaults(&log->sysConfig);

    log->frameIntervalI = 32;
    log->frameIntervalPNum = 1;
    log->frameIntervalPDenom = 1;

    private->lastEvent.event = -1;

    clearFieldIdents(log);

    private->timeRolloverAccumulator = 0;
    private->lastSkippedFrames = 0;
    private->lastMainFrameIteration = (uint32_t) -1;
    private->lastMainFrameTime = -1;

    private->onMetadataReady = onMetadataReady;
    private->onFrameReady = onFrameReady;
    private->onEvent = onEvent;

    //Set parsing ranges up for the log the caller selected
    private->stream->start = log->logBegin[logIndex];
    private->stream->pos = private->stream->start;
    private->stream->end = log->logBegin[logIndex + 1];
    private->stream->eof = false;

    // Main parsing loop
    while (1) {
        char command = streamPeekChar(private->stream); // Peek once at the start of the loop

        if (parserState == PARSER_STATE_HEADER) {
            if (command == 'H') {
                bool is_config_header = false;
                // Peek ahead to see if this 'H' line is a "Key:Value" config header
                // A simple check: look for a colon within a reasonable distance, after "H "
                if (private->stream->pos + 1 < private->stream->end && *(private->stream->pos + 1) == ' ') {
                    const char *peek_ptr = private->stream->pos + 2; // Start peeking after "H "
                    int peek_count = 0;
                    // Max lookahead for colon in a typical header field name part
                    const int max_peek_for_colon = 60; // Adjust if field names can be longer
                    while (peek_ptr < private->stream->end && *peek_ptr != '\n' && peek_count < max_peek_for_colon) {
                        if (*peek_ptr == ':') {
                            is_config_header = true;
                            break;
                        }
                        peek_ptr++;
                        peek_count++;
                    }
                }

                if (is_config_header) {
                    // It's a configuration header, parse it
                    size_t frameSize = parseHeaderLine(log, private->stream); // parseHeaderLine now takes no parserState
                    if ((private->stream->mapping.stats.st_mode & S_IFMT) == S_IFCHR) {
                        fillSerialBuffer(private->stream, frameSize, &parserState);
                    }
                    continue; // Go back to top of loop to process next line (could be another header)
                } else {
                    // This 'H' is not a config header (no colon found quickly, or not "H "),
                    // so assume it's the start of an H-data frame. End of headers.
                    parserState = PARSER_STATE_TRANSITION;
                    // Fall through to PARSER_STATE_TRANSITION logic with current `command` ('H')
                }
            } else if (command == EOF) {
                fprintf(stderr, "Data file ended during header processing or contained no data.\n");
                goto done;
            } else {
                // Not 'H' and not EOF, so headers must be finished.
                parserState = PARSER_STATE_TRANSITION;
                // Fall through to PARSER_STATE_TRANSITION logic with current `command`
            }

            // If we are now in PARSER_STATE_TRANSITION (meaning headers are done),
            // perform one-time metadata setup.
            if (parserState == PARSER_STATE_TRANSITION && !metadata_setup_and_ready_called) {
                 if (log->frameDefs['I'].fieldCount == 0) {
                    fprintf(stderr, "Data file is missing I-frame field name definitions or has no valid headers.\n");
                    return false; // Critical definitions missing
                }
                // GPS predictor rewrite (moved from old PARSER_STATE_TRANSITION block)
                for (int i = 1; i < log->frameDefs['G'].fieldCount; i++) {
                    if (log->frameDefs['G'].predictor[i - 1] == FLIGHT_LOG_FIELD_PREDICTOR_HOME_COORD &&
                        log->frameDefs['G'].predictor[i] == FLIGHT_LOG_FIELD_PREDICTOR_HOME_COORD) {
                        log->frameDefs['G'].predictor[i] = FLIGHT_LOG_FIELD_PREDICTOR_HOME_COORD_1;
                    }
                }
                // Note: onMetadataReady will be called in PARSER_STATE_TRANSITION after first data frame type is known
            }
        } // End of PARSER_STATE_HEADER handling

        if (parserState == PARSER_STATE_TRANSITION) {
            // `command` is the first char of data section (e.g. 'I', 'P', or 'H' for H-data frame)
            frameType = getFrameType(command);

            if (frameType) { // Found a valid data frame type marker
                if (!metadata_setup_and_ready_called) { // Call onMetadataReady once
                    if (onMetadataReady) {
                        onMetadataReady(log);
                    }
                    metadata_setup_and_ready_called = true;
                }
                parserState = PARSER_STATE_DATA; // Switch to data parsing state
                // Fall through to PARSER_STATE_DATA block. `command` is the frame type.
            } else if (command == EOF) {
                fprintf(stderr, "EOF encountered after headers, before any valid data frame type.\n");
                goto done;
            } else { // `command` is not a frame type and not EOF - skip garbage
                fprintf(stderr, "Skipping unexpected character '%c' (0x%02X) after headers.\n", isprint(command) ? command : '?', command);
                streamReadByte(private->stream); // Consume the garbage character
                if ((private->stream->mapping.stats.st_mode & S_IFMT) == S_IFCHR) {
                    fillSerialBuffer(private->stream, 1, &parserState);
                }
                continue; // Go to top of loop and peek next character
            }
        } // End of PARSER_STATE_TRANSITION handling

        if (parserState == PARSER_STATE_DATA) {
            if (command == EOF) {
                goto done;
            }

            // `command` is the frame type character, peeked at the start of the loop.
            // `frameType` is set if first data frame from transition, or needs to be re-evaluated for subsequent frames.
            frameType = getFrameType((uint8_t) command);

            if (!frameType) { // Handles corruption or unexpected char in data stream
                fprintf(stderr, "Unexpected character '%c' (0x%02X) in data stream (expected frame type marker).\n", isprint(command) ? command : '?', command);
                streamReadByte(private->stream); // Consume the unexpected character
                log->stats.totalCorruptFrames++;
                private->mainStreamIsValid = false; // Mark I/P stream as invalid to stop P-frame predictions
                if ((private->stream->mapping.stats.st_mode & S_IFMT) == S_IFCHR) {
                    fillSerialBuffer(private->stream, 1, &parserState);
                }
                continue; // Try to find next valid frame marker
            }

            streamReadByte(private->stream); // Consume the frame type marker (`command`)

            const char *framePayloadStart = private->stream->pos; // Start of payload
            frameType->parse(log, private->stream, raw);   // Parses payload
            size_t framePayloadSize = private->stream->pos - framePayloadStart; // Size of payload

            bool prematureEofInPayload = private->stream->eof; // Check EOF *after* parse

            size_t totalFrameLengthForCallback = framePayloadSize + 1; // Payload + marker
            const char *actualFrameStartForCallback = (private->stream->pos - framePayloadSize) - 1; // Before marker

            if (!prematureEofInPayload && totalFrameLengthForCallback <= FLIGHT_LOG_MAX_FRAME_LENGTH) {
                bool frameAccepted = true;
                if (frameType->complete) {
                    frameAccepted = frameType->complete(log, private->stream, frameType->marker, actualFrameStartForCallback, private->stream->pos, raw);
                }

                if (frameAccepted) {
                    log->stats.frame[frameType->marker].bytes += totalFrameLengthForCallback;
                    if (totalFrameLengthForCallback < FLIGHT_LOG_MAX_FRAME_LENGTH) { // Check bounds for sizeCount array
                        log->stats.frame[frameType->marker].sizeCount[totalFrameLengthForCallback]++;
                    }
                    log->stats.frame[frameType->marker].validCount++;
                    if ((private->stream->mapping.stats.st_mode & S_IFMT) == S_IFCHR) {
                        // fillSerialBuffer needs total bytes consumed for this frame (marker + payload)
                        // streamReadByte consumed marker, parse consumed payload.
                        // So, totalFrameLengthForCallback is correct for fillSerialBuffer if it expects
                        // total bytes of the just-processed frame *including its marker*.
                        fillSerialBuffer(private->stream, totalFrameLengthForCallback, &parserState);
                    }
                } else {
                    log->stats.frame[frameType->marker].desyncCount++;
                }
            } else { // Frame corrupt (too long or EOF in payload)
                private->mainStreamIsValid = false; // Mark I/P stream invalid
                log->stats.frame[frameType->marker].corruptCount++;
                log->stats.totalCorruptFrames++;

                if (onFrameReady) {
                    onFrameReady(log, false, 0, frameType->marker, 0, actualFrameStartForCallback - private->stream->data, totalFrameLengthForCallback);
                }

                if (prematureEofInPayload) private->stream->eof = false; // Reset EOF to allow further peeking

                if ((private->stream->mapping.stats.st_mode & S_IFMT) == S_IFCHR) {
                    // Account for consumed marker + attempted payload for serial buffer
                    fillSerialBuffer(private->stream, totalFrameLengthForCallback, &parserState);
                }
                continue;
            }
        } // End of PARSER_STATE_DATA
    } // End of main while(1) loop

    done:
    log->stats.totalBytes = private->stream->end - private->stream->start;
    return true;
} // End of flightLogParse

void flightLogDestroy(flightLog_t *log)
{
    streamDestroy(log->private->stream);

    for (int i = 0; i < 256; i++) {
        free(log->frameDefs[i].namesLine);
    }

    free(log->private);
    free(log);
}
