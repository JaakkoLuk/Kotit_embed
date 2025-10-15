#include <gtest/gtest.h>
#include "TimeParser.h"


TEST(TimeParserTest, ValidTimeString_000120) {
    char time[] = "000120";  // 1 min 20 sec = 80 seconds
    int result = time_parse(time);
    EXPECT_EQ(result, 80);
}

TEST(TimeParserTest, ValidTimeString_001000) {
    char time[] = "001000";  // 10 min 0 sec = 600 seconds
    int result = time_parse(time);
    EXPECT_EQ(result, 600);
}

TEST(TimeParserTest, ValidTimeString_000001) {
    char time[] = "000001";  // 0 min 1 sec = 1 second
    int result = time_parse(time);
    EXPECT_EQ(result, 1);
}

TEST(TimeParserTest, BoundaryValue_Seconds_Min) {
    char time[] = "000000";  // 0 seconds 
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_ZERO_TIME);
}

TEST(TimeParserTest, BoundaryValue_Seconds_Valid_1) {
    char time[] = "000001";  // 1 second - minimum valid
    int result = time_parse(time);
    EXPECT_EQ(result, 1);
}

TEST(TimeParserTest, BoundaryValue_Seconds_Valid_59) {
    char time[] = "000059";  // 59 seconds - maximum valid
    int result = time_parse(time);
    EXPECT_EQ(result, 59);
}

TEST(TimeParserTest, BoundaryValue_Seconds_Invalid_60) {
    char time[] = "000060";  // 60 seconds - invalid
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_INVALID_SECONDS);
}

TEST(TimeParserTest, BoundaryValue_Seconds_Invalid_99) {
    char time[] = "000099";  // 99 seconds - invalid
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_INVALID_SECONDS);
}

// Test Suite: Boundary Value Tests for Minutes
TEST(TimeParserTest, BoundaryValue_Minutes_Min) {
    char time[] = "000001";  // 0 minutes - valid
    int result = time_parse(time);
    EXPECT_EQ(result, 1);
}

TEST(TimeParserTest, BoundaryValue_Minutes_Valid_59) {
    char time[] = "005959";  // 59 min 59 sec - maximum valid
    int result = time_parse(time);
    EXPECT_EQ(result, 3599);  // 59*60 + 59 = 3599
}

TEST(TimeParserTest, BoundaryValue_Minutes_Invalid_60) {
    char time[] = "006001";  // 60 minutes - invalid
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_INVALID_MINUTES);
}

TEST(TimeParserTest, BoundaryValue_Minutes_Invalid_99) {
    char time[] = "009901";  // 99 minutes - invalid
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_INVALID_MINUTES);
}

// Test Suite: Boundary Value Tests for Hours
TEST(TimeParserTest, BoundaryValue_Hours_Min) {
    char time[] = "000101";  // 0 hours - valid
    int result = time_parse(time);
    EXPECT_EQ(result, 61);  // 1*60 + 1
}

TEST(TimeParserTest, BoundaryValue_Hours_Valid_23) {
    char time[] = "230101";  // 23 hours 
    int result = time_parse(time);
    EXPECT_EQ(result, 61);  // Only counts 1 min 1 sec
}

TEST(TimeParserTest, BoundaryValue_Hours_Invalid_24) {
    char time[] = "240101";  // 24 hours - invalid
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_INVALID_HOURS);
}

TEST(TimeParserTest, BoundaryValue_Hours_Invalid_99) {
    char time[] = "990101";  // 99 hours - invalid
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_INVALID_HOURS);
}

// Test Suite: String Length Validation
TEST(TimeParserTest, StringLength_Valid_Exactly6) {
    char time[] = "001234";  // Exactly 6 characters
    int result = time_parse(time);
    EXPECT_EQ(result, 12*60 + 34);  // Valid
}

TEST(TimeParserTest, StringLength_Invalid_TooShort_5) {
    char time[] = "00123";  // Only 5 characters
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_INVALID_LENGTH);
}

TEST(TimeParserTest, StringLength_Invalid_TooShort_1) {
    char time[] = "1";  // Only 1 character
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_INVALID_LENGTH);
}

TEST(TimeParserTest, StringLength_Invalid_TooLong_7) {
    char time[] = "0012345";  // 7 characters
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_INVALID_LENGTH);
}

TEST(TimeParserTest, StringLength_Invalid_Empty) {
    char time[] = "";  // Empty string
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_INVALID_LENGTH);
}

TEST(TimeParserTest, ZeroTime_AllZeros) {
    char time[] = "000000";  // 0 minutes, 0 seconds
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_ZERO_TIME);
}

TEST(TimeParserTest, ZeroTime_WithHours) {
    char time[] = "120000";  // 12 hours but 0 min 0 sec
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_ZERO_TIME);
}

TEST(TimeParserTest, NonZeroTime_OnlySeconds) {
    char time[] = "000001";  // 1 second - valid
    int result = time_parse(time);
    EXPECT_EQ(result, 1);
}

TEST(TimeParserTest, NonZeroTime_OnlyMinutes) {
    char time[] = "000100";  // 1 minute - valid
    int result = time_parse(time);
    EXPECT_EQ(result, 60);
}

TEST(TimeParserTest, OnlyDigits_Valid) {
    char time[] = "123456";  // All digits
    int result = time_parse(time);
    EXPECT_NE(result, ERROR_NON_DIGIT);  // Should not fail on digit check
}

TEST(TimeParserTest, ContainsLetter_Invalid) {
    char time[] = "12A456";  // Contains letter 'A'
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_NON_DIGIT);
}

TEST(TimeParserTest, ContainsSpecialChar_Invalid) {
    char time[] = "12:456";  // Contains ':'
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_NON_DIGIT);
}

TEST(TimeParserTest, ContainsSpace_Invalid) {
    char time[] = "12 456";  // Contains space
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_NON_DIGIT);
}

TEST(TimeParserTest, ContainsNegative_Invalid) {
    char time[] = "12-456";  // Contains '-'
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_NON_DIGIT);
}

// Test Suite: Null String Validation
TEST(TimeParserTest, NullString_Invalid) {
    char *time = nullptr;
    int result = time_parse(time);
    EXPECT_EQ(result, ERROR_NULL_STRING);
}

TEST(TimeParserTest, ValidString_NotNull) {
    char time[] = "000123";
    int result = time_parse(time);
    EXPECT_NE(result, ERROR_NULL_STRING);  // Should not fail on null check
}


TEST(SequenceParserTest, SimpleFormat_Valid_RYG) {
    char sequence[] = "RYG";
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, 0);  // Valid
}

TEST(SequenceParserTest, SimpleFormat_Valid_RYGRYG) {
    char sequence[] = "RYGRYG";
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, 0);  // Valid
}

TEST(SequenceParserTest, SimpleFormat_Valid_Lowercase) {
    char sequence[] = "ryg";
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, 0);  // Valid
}

TEST(SequenceParserTest, SimpleFormat_Valid_MixedCase) {
    char sequence[] = "RyGrYg";
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, 0);  // Valid
}

TEST(SequenceParserTest, SimpleFormat_Invalid_ContainsB) {
    char sequence[] = "RYGB";  // B is not valid
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, ERROR_INVALID_COLOR);
}

TEST(SequenceParserTest, SimpleFormat_Invalid_ContainsNumber) {
    char sequence[] = "RYG1";  // Number without comma format
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, ERROR_INVALID_COLOR);
}

TEST(SequenceParserTest, SimpleFormat_Invalid_ContainsSpace) {
    char sequence[] = "RY G";  // Space not allowed
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, ERROR_INVALID_COLOR);
}

TEST(SequenceParserTest, TimedFormat_Valid_Simple) {
    char sequence[] = "R,1000,Y,500,G,1000";
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, 0);  // Valid
}

TEST(SequenceParserTest, TimedFormat_Valid_ShortTimes) {
    char sequence[] = "R,100,Y,50";
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, 0);  // Valid
}

TEST(SequenceParserTest, TimedFormat_Valid_SingleColor) {
    char sequence[] = "R,5000";
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, 0);  // Valid
}

TEST(SequenceParserTest, TimedFormat_Invalid_MissingComma) {
    char sequence[] = "R1000,Y,500";  // Missing comma after R
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, ERROR_INVALID_SEQUENCE);
}

TEST(SequenceParserTest, TimedFormat_Invalid_DoubleComma) {
    char sequence[] = "R,,1000";  // Double comma
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, ERROR_INVALID_SEQUENCE);
}

TEST(SequenceParserTest, TimedFormat_Invalid_EndsWithComma) {
    char sequence[] = "R,1000,";  // Ends with comma
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, ERROR_INVALID_SEQUENCE);
}

TEST(SequenceParserTest, TimedFormat_Invalid_InvalidColor) {
    char sequence[] = "R,1000,B,500";  // B is invalid
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, ERROR_INVALID_COLOR);
}

TEST(SequenceParserTest, EdgeCase_NullSequence) {
    char *sequence = nullptr;
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, ERROR_NULL_STRING);
}

TEST(SequenceParserTest, EdgeCase_EmptySequence) {
    char sequence[] = "";
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, ERROR_EMPTY_SEQUENCE);
}

TEST(SequenceParserTest, EdgeCase_TooLong) {
    char sequence[150];
    memset(sequence, 'R', 149);
    sequence[149] = '\0';
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, ERROR_SEQUENCE_TOO_LONG);
}

TEST(SequenceParserTest, EdgeCase_ValidLength_100) {
    char sequence[101];
    memset(sequence, 'R', 100);
    sequence[100] = '\0';
    int result = sequence_parse(sequence);
    EXPECT_EQ(result, 0);  // Valid at exactly 100
}