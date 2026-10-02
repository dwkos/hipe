/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Test.h"
#include <wtf/DateMath.h>

namespace TestWebKitAPI {

// Note: The results of these function look weird if you do not understand the following mappings:
// dayOfWeek: [0, 6] 0 being Monday, day: [1, 31], month: [0, 11], year: ex: 2011,
// hours: [0, 23], minutes: [0, 59], seconds: [0, 59], utcOffset: [-720,720].

TEST(WTF_DateMath, dateToDaysFrom1970)
{
    EXPECT_EQ(0.0, dateToDaysFrom1970(1970, 0, 1));
    EXPECT_EQ(157.0, dateToDaysFrom1970(1970, 5, 7));
    EXPECT_EQ(-145.0, dateToDaysFrom1970(1969, 7, 9));
    EXPECT_EQ(16322, dateToDaysFrom1970(2014, 8, 9));
}


TEST(WTF_DateMath, isLeapYear)
{
    EXPECT_TRUE(isLeapYear(1804));
    EXPECT_FALSE(isLeapYear(1900));
    EXPECT_TRUE(isLeapYear(1968));
    EXPECT_TRUE(isLeapYear(1976));
    EXPECT_TRUE(isLeapYear(2000));
    EXPECT_FALSE(isLeapYear(2010));
    EXPECT_TRUE(isLeapYear(2012));
    EXPECT_FALSE(isLeapYear(2100));
}

TEST(WTF_DateMath, msToYear)
{
    EXPECT_EQ(1962, msToYear(-220953600000));
    EXPECT_EQ(1970, msToYear(0));
    EXPECT_EQ(1970, msToYear(100));
    EXPECT_EQ(1977, msToYear(220953600000));
    EXPECT_EQ(2013, msToYear(1365318000000));
}

TEST(WTF_DateMath, msToDays)
{
    EXPECT_EQ(0, msToDays(0));
    EXPECT_EQ(2557, msToDays(220953600000));
    EXPECT_EQ(255, msToDays(22095360000));
    EXPECT_EQ(25, msToDays(2209536000));
    EXPECT_EQ(2, msToDays(220953600));
    EXPECT_EQ(0, msToDays(22095360));
    EXPECT_EQ(0, msToDays(2209536));
}

TEST(WTF_DateMath, msToMinutes)
{
    EXPECT_EQ(0, msToMinutes(0));
    EXPECT_EQ(0, msToMinutes(220953600000));
    EXPECT_EQ(36, msToMinutes(22095360000));
    EXPECT_EQ(36, msToMinutes(22095360000));
    EXPECT_EQ(45, msToMinutes(2209536000));
    EXPECT_EQ(22, msToMinutes(220953600));
    EXPECT_EQ(8, msToMinutes(22095360));
    EXPECT_EQ(36, msToMinutes(2209536));
}

TEST(WTF_DateMath, msToHours)
{
    EXPECT_EQ(0, msToHours(0));
    EXPECT_EQ(8, msToHours(220953600000));
    EXPECT_EQ(17, msToHours(22095360000));
    EXPECT_EQ(13, msToHours(2209536000));
    EXPECT_EQ(13, msToHours(220953600));
    EXPECT_EQ(6, msToHours(22095360));
    EXPECT_EQ(0, msToHours(2209536));
}

TEST(WTF_DateMath, dayInYear)
{
    EXPECT_EQ(59, dayInYear(2015, 2, 1));
    EXPECT_EQ(60, dayInYear(2012, 2, 1));
    EXPECT_EQ(0, dayInYear(2015, 0, 1));
    EXPECT_EQ(31, dayInYear(2015, 1, 1));
}

TEST(WTF_DateMath, monthFromDayInYear)
{
    EXPECT_EQ(2, monthFromDayInYear(59, false));
    EXPECT_EQ(1, monthFromDayInYear(59, true));
    EXPECT_EQ(2, monthFromDayInYear(60, true));
    EXPECT_EQ(0, monthFromDayInYear(0, false));
    EXPECT_EQ(0, monthFromDayInYear(0, true));
    EXPECT_EQ(1, monthFromDayInYear(31, true));
    EXPECT_EQ(1, monthFromDayInYear(31, false));
}

TEST(WTF_DateMath, dayInMonthFromDayInYear)
{
    EXPECT_EQ(1, dayInMonthFromDayInYear(0, false));
    EXPECT_EQ(1, dayInMonthFromDayInYear(0, true));
    EXPECT_EQ(1, dayInMonthFromDayInYear(59, false));
    EXPECT_EQ(29, dayInMonthFromDayInYear(59, true));
    EXPECT_EQ(1, dayInMonthFromDayInYear(60, true));
    EXPECT_EQ(1, dayInMonthFromDayInYear(0, false));
    EXPECT_EQ(1, dayInMonthFromDayInYear(0, true));
    EXPECT_EQ(31, dayInMonthFromDayInYear(30, true));
    EXPECT_EQ(31, dayInMonthFromDayInYear(30, false));
    EXPECT_EQ(31, dayInMonthFromDayInYear(365, true));
    EXPECT_EQ(32, dayInMonthFromDayInYear(365, false));
    EXPECT_EQ(32, dayInMonthFromDayInYear(366, true));
}

} // namespace TestWebKitAPI
