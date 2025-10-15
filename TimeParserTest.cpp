#include <gtest/gtest.h>
#include "../TimeParser.h"

// null
TEST(TimeParserTest, TestNullPointer) {
    ASSERT_EQ(time_parse(nullptr), TIME_ARRAY_ERROR);   
    // valid
    char ok[] = "000001";                               
    EXPECT_EQ(time_parse(ok), 1);

}

//80s = 1min20s
TEST(TimeParserTest, TestToSeconds) {
    char t[] = "000120";
    EXPECT_EQ(time_parse(t), 80);                       
}


//before full
TEST(TimeParserTest, TestUpperBoundary) {
    char t[] = "235959";
    EXPECT_EQ(time_parse(t), 23*3600 + 59*60 + 59);
}


//full
TEST(TimeParserTest, TestHours23) {
    char t[] = "240000";                  
    EXPECT_EQ(time_parse(t), TIME_VALUE_ERROR);
}


//60min
TEST(TimeParserTest, TestMinutes59) {
    char t[] = "126000";  
    EXPECT_EQ(time_parse(t), TIME_VALUE_ERROR);
}


//60sec
TEST(TimeParserTest, TestSeconds59) {
    char t[] = "120060";              
    EXPECT_EQ(time_parse(t), TIME_VALUE_ERROR);
}


//non-numbers mixed in
TEST(TimeParserTest, TestNonDigits) {  
    char t[] = "12a045";
    EXPECT_EQ(time_parse(t), TIME_LEN_ERROR);
}

//different lengths
TEST(TimeParserTest, TestLenght) { 

    char s[] = "111";
    EXPECT_EQ(time_parse(s), TIME_LEN_ERROR);
    
    char l[] = "9999999";
    EXPECT_EQ(time_parse(l), TIME_LEN_ERROR);
}


//neg values
TEST(TimeParserTest, RejectsMinusSign) {
    char t[] = "-10010"; // '-'
    EXPECT_EQ(time_parse(t), TIME_LEN_ERROR);
}

// https://google.github.io/googletest/reference/testing.html
// https://google.github.io/googletest/reference/assertions.html