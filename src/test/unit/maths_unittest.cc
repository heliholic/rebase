/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>

#include <limits.h>

#include <math.h>

#define USE_BARO

extern "C" {
    #include "common/maths.h"
    #include "common/vector.h"
}

#include "unittest_macros.h"
#include "gtest/gtest.h"


TEST(MathsUnittest, TestScaleRange)
{
    // Within bounds
    EXPECT_EQ(scaleRange(0, 0, 10, 0, 100), 0);
    EXPECT_EQ(scaleRange(10, 0, 10, 0, 100), 100);
    EXPECT_EQ(scaleRange(0, 0, 100, 0, 10), 0);
    EXPECT_EQ(scaleRange(100, 0, 100, 0, 10), 10);

    // Scale up
    EXPECT_EQ(scaleRange(1, 0, 10, 0, 100), 10);
    EXPECT_EQ(scaleRange(2, 0, 10, 0, 100), 20);
    EXPECT_EQ(scaleRange(5, 0, 10, 0, 100), 50);

    // Scale down
    EXPECT_EQ(scaleRange(10, 0, 100, 0, 10), 1);
    EXPECT_EQ(scaleRange(20, 0, 100, 0, 10), 2);
    EXPECT_EQ(scaleRange(50, 0, 100, 0, 10), 5);
}

TEST(MathsUnittest, TestScaleRangeNegatives)
{
    // Within bounds
    EXPECT_EQ(scaleRange(0, -10, 0, -100, 0), 0);
    EXPECT_EQ(scaleRange(-10, -10, 0, -100, 0), -100);
    EXPECT_EQ(scaleRange(0, -100, 0, -10, 0), 0);
    EXPECT_EQ(scaleRange(-100, -100, 0, -10, 0), -10);

    // Scale up
    EXPECT_EQ(scaleRange(-1, -10, 0, -100, 0), -10);
    EXPECT_EQ(scaleRange(-2, -10, 0, -100, 0), -20);
    EXPECT_EQ(scaleRange(-5, -10, 0, -100, 0), -50);

    // Scale down
    EXPECT_EQ(scaleRange(-10, -100, 0, -10, 0), -1);
    EXPECT_EQ(scaleRange(-20, -100, 0, -10, 0), -2);
    EXPECT_EQ(scaleRange(-50, -100, 0, -10, 0), -5);
}

TEST(MathsUnittest, TestScaleRangeNegativePositive)
{
    // Within bounds
    EXPECT_EQ(scaleRange(0, -10, 0, 0, 100), 100);
    EXPECT_EQ(scaleRange(-10, -10, 0, 0, 100), 0);
    EXPECT_EQ(scaleRange(0, -100, 0, 0, 10), 10);
    EXPECT_EQ(scaleRange(-100, -100, 0, 0, 10), 0);

    // Scale up
    EXPECT_EQ(scaleRange(-1, -10, 0, 0, 100), 90);
    EXPECT_EQ(scaleRange(-2, -10, 0, 0, 100), 80);
    EXPECT_EQ(scaleRange(-5, -10, 0, 0, 100), 50);

    // Scale down
    EXPECT_EQ(scaleRange(-10, -100, 0, 0, 10), 9);
    EXPECT_EQ(scaleRange(-20, -100, 0, 0, 10), 8);
    EXPECT_EQ(scaleRange(-50, -100, 0, 0, 10), 5);
}

TEST(MathsUnittest, TestScaleRangeReverse)
{
    // Within bounds
    EXPECT_EQ(scaleRange(0, 0, 10, 100, 0), 100);
    EXPECT_EQ(scaleRange(10, 0, 10, 100, 0), 0);
    EXPECT_EQ(scaleRange(0, 0, 100, 10, 0), 10);
    EXPECT_EQ(scaleRange(100, 0, 100, 10, 0), 0);

    // Scale up
    EXPECT_EQ(scaleRange(1, 0, 10, 100, 0), 90);
    EXPECT_EQ(scaleRange(2, 0, 10, 100, 0), 80);
    EXPECT_EQ(scaleRange(5, 0, 10, 100, 0), 50);

    // Scale down
    EXPECT_EQ(scaleRange(10, 0, 100, 10, 0), 9);
    EXPECT_EQ(scaleRange(20, 0, 100, 10, 0), 8);
    EXPECT_EQ(scaleRange(50, 0, 100, 10, 0), 5);
}

TEST(MathsUnittest, TestConstrain)
{
    // Within bounds
    EXPECT_EQ(constrain(0, 0, 0), 0);
    EXPECT_EQ(constrain(1, 1, 1), 1);
    EXPECT_EQ(constrain(1, 0, 2), 1);

    // Equal to bottom bound.
    EXPECT_EQ(constrain(1, 1, 2), 1);
    // Equal to top bound.
    EXPECT_EQ(constrain(1, 0, 1), 1);

    // Equal to both bottom and top bound.
    EXPECT_EQ(constrain(1, 1, 1), 1);

    // Above top bound.
    EXPECT_EQ(constrain(2, 0, 1), 1);
    // Below bottom bound.
    EXPECT_EQ(constrain(0, 1, 2), 1);
}

TEST(MathsUnittest, TestConstrainNegatives)
{
    // Within bounds.
    EXPECT_EQ(constrain(-1, -1, -1), -1);
    EXPECT_EQ(constrain(-1, -2, 0), -1);

    // Equal to bottom bound.
    EXPECT_EQ(constrain(-1, -1, 0), -1);
    // Equal to top bound.
    EXPECT_EQ(constrain(-1, -2, -1), -1);

    // Equal to both bottom and top bound.
    EXPECT_EQ(constrain(-1, -1, -1), -1);

    // Above top bound.
    EXPECT_EQ(constrain(-1, -3, -2), -2);
    // Below bottom bound.
    EXPECT_EQ(constrain(-3, -2, -1), -2);
}

TEST(MathsUnittest, TestConstrainf)
{
    // Within bounds.
    EXPECT_FLOAT_EQ(constrainf(1.0f, 0.0f, 2.0f), 1.0f);

    // Equal to bottom bound.
    EXPECT_FLOAT_EQ(constrainf(1.0f, 1.0f, 2.0f), 1.0f);
    // Equal to top bound.
    EXPECT_FLOAT_EQ(constrainf(1.0f, 0.0f, 1.0f), 1.0f);

    // Equal to both bottom and top bound.
    EXPECT_FLOAT_EQ(constrainf(1.0f, 1.0f, 1.0f), 1.0f);

    // Above top bound.
    EXPECT_FLOAT_EQ(constrainf(2.0f, 0.0f, 1.0f), 1.0f);
    // Below bottom bound.
    EXPECT_FLOAT_EQ(constrainf(0, 1.0f, 2.0f), 1.0f);

    // Above bouth bounds.
    EXPECT_FLOAT_EQ(constrainf(2.0f, 0.0f, 1.0f), 1.0f);
    // Below bouth bounds.
    EXPECT_FLOAT_EQ(constrainf(0, 1.0f, 2.0f), 1.0f);
}

TEST(MathsUnittest, TestDegreesToRadians)
{
    EXPECT_FLOAT_EQ(degreesToRadians(0), 0.0f);
    EXPECT_FLOAT_EQ(degreesToRadians(90), 0.5f * M_PIf);
    EXPECT_FLOAT_EQ(degreesToRadians(180), M_PIf);
    EXPECT_FLOAT_EQ(degreesToRadians(-180), - M_PIf);
}

TEST(MathsUnittest, TestApplyDeadband)
{
    EXPECT_EQ(applyDeadband(0, 0), 0);
    EXPECT_EQ(applyDeadband(1, 0), 1);
    EXPECT_EQ(applyDeadband(-1, 0), -1);

    EXPECT_EQ(applyDeadband(0, 10), 0);
    EXPECT_EQ(applyDeadband(1, 10), 0);
    EXPECT_EQ(applyDeadband(10, 10), 0);

    EXPECT_EQ(applyDeadband(11, 10), 1);
    EXPECT_EQ(applyDeadband(-11, 10), -1);
}

void expectVectorsAreEqual(vector3_t *a, vector3_t *b, float absTol)
{
    EXPECT_NEAR(a->x, b->x, absTol);
    EXPECT_NEAR(a->y, b->y, absTol);
    EXPECT_NEAR(a->z, b->z, absTol);
}
