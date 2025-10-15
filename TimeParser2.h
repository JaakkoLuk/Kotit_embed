#ifndef TIMEPARSER_H
#define TIMEPARSER_H

// Error codes
#define ERROR_INVALID_LENGTH -1
#define ERROR_INVALID_HOURS -2
#define ERROR_INVALID_MINUTES -3
#define ERROR_INVALID_SECONDS -4
#define ERROR_NULL_STRING -5
#define ERROR_ZERO_TIME -6
#define ERROR_NON_DIGIT -7

int time_parse(char *time);

#define ERROR_INVALID_SEQUENCE -10
#define ERROR_INVALID_COLOR -11
#define ERROR_SEQUENCE_TOO_LONG -12
#define ERROR_EMPTY_SEQUENCE -13


int sequence_parse(char *sequence);

#endif // TIMEPARSER_H