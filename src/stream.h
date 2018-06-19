#ifndef STREAM_H_
#define STREAM_H_

#include "platform.h"
#define FLIGHT_LOG_MAX_FRAME_LENGTH 256
#define FLIGHT_LOG_MAX_FRAME_HEADER_LENGTH 1024


typedef struct mmapStream_t {
    fileMapping_t mapping;

    //The start of the entire data block
    const char *data;

    //The length of the entire data block
    size_t size;

    //The section of the file which is currently being examined by the stream:
    const char *start, *end, *pos;

    //When reading bit-by-bit, the index of the next bit to be read within the byte at pos (from the high bit of index 7..0)
    int bitPos;

    //Set to true if we attempt to read from the log when it is already exhausted
    bool eof;
} mmapStream_t;

typedef enum ParserState {
    PARSER_STATE_HEADER = 0,
    PARSER_STATE_DATA
} ParserState;

void fillSerialBuffer(mmapStream_t *stream,size_t bytesParsedDataSize, ParserState *parserState);
mmapStream_t* streamCreate(int fd);
void streamDestroy(mmapStream_t *stream);

int streamPeekChar(mmapStream_t *stream);
char streamReadChar(mmapStream_t *stream);
int streamReadByte(mmapStream_t *stream);
void streamUnreadChar(mmapStream_t *stream);

void streamRead(mmapStream_t *stream, void *buf, int len);

uint32_t streamReadBits(mmapStream_t *stream, int numBits);
int streamReadBit(mmapStream_t *stream);
void streamByteAlign(mmapStream_t *stream);

uint32_t streamReadUnsignedVB(mmapStream_t *stream);
int32_t streamReadSignedVB(mmapStream_t *stream);

#endif
