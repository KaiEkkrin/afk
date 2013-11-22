/* AFK
 * Copyright (C) 2013, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#include "afk.hpp"

#include <cassert>
#include <cfloat>
#include <cmath>
#include <iostream>

#include "debug.hpp"
#include "detail_adjuster.hpp"


/* Let's define a "k" in terms of the detail pitch. */
constexpr float kFromDetailPitch(float detailPitch)
{
    return 1.0f / (detailPitch * detailPitch);
}

constexpr float detailPitchFromK(float k)
{
    return sqrt(1.0f / k);
}

/* AFK_DetailAdjuster implementation */

AFK_DetailAdjuster::AFK_DetailAdjuster(const AFK_Config *config):
    frameTimeTarget(config->targetFrameTimeMillis),
    wiggle(0.9f),
    xTarget(1.0f / config->targetFrameTimeMillis),
    vsync(config->vsync),
    detailPitchMax(config->maxDetailPitch),
    detailPitchMin(config->minDetailPitch),
    haveFirstMeasurement(false),
    haveLastFrameTime(false),
    lastV(-FLT_MAX),
    detailPitch(config->startingDetailPitch),
    k(kFromDetailPitch(config->startingDetailPitch)),
    deviation(config->framesPerCalibration, 0.0f),
    consistency(config->framesPerCalibration, 1.0f)
{
}

AFK_DetailAdjuster::~AFK_DetailAdjuster()
{
}

void AFK_DetailAdjuster::startOfFrame(void)
{
    afk_clock::time_point startOfFrame = afk_clock::now();

    if (haveFirstMeasurement)
    {
        /* Update my readings. */
        float lastX = x;
        lastFrameTime = std::chrono::duration_cast<afk_duration_mfl>(
            startOfFrame - lastStartOfFrame);
        haveLastFrameTime = true;
        x = (1.0f / lastFrameTime.count()) - xTarget;

        lastV = v;
        v = x - lastX;

        a = v - lastV;

        /* ... and update the detail pitch.
         * To test: I'm going to set m=1, and try to solve the
         * equation for a value of k satisfying c^2-4mk=0.
         * Firstly, from the formula ma + cv + kx = 0,
         * find the current c:
         * o c = -(ma + kx) / v
         */
        c = -(m * a + k * x) / v;

        /* Now, make a new value for k such that c^2-4mk=0,
         * i.e. k = (c * c) / (4 * m)
         */
        k = (c * c) / (4.0f * m);

#if 0
        /* The above isn't doing anything like what I hoped,
         * although that's not surprising, because I was
         * fumbling in the dark.
         * Let's mess with updating the DP as an independent
         * entity, like so:
         */
        detailPitch -= x;

        /* TODO:
         * Of course, I can't govern the detail pitch like
         * that, because it ends up ruled by single-frame
         * glitches.
         * What I should try doing is push in a system for
         * governing the detail pitch on an averaged basis
         * (just like the oscillation-prone current one),
         * and then try seeing the detail pitch change factor
         * as `k' and optimising it with the above logic.?
         * Also it would be really useful to be able to remove
         * the frame rate cap, which makes adjustment much harder
         * (is it still present in full screen mode?  Can I
         * create a switchable overlay to print my checkpoint
         * messages so I can see them fullscreen?)
         */
#endif
        /* TODO: With non-vsync, deal with lastFrameTime as a
         * whole
         */

        /* Now, adjust the detail pitch to match... */
        float boundedError = std::max(std::min(deviation.get() / 240.0f, 1.0f), -1.0f);
        float detailFactor = -(1.0f / (boundedError / 2.0f - 1.0f)); /* between 0.5 and 2 */
        //AFK_DEBUG_PRINTL("Detail factor: " << detailFactor)
        detailPitch = std::max(std::min(detailPitch * detailFactor, detailPitchMax), detailPitchMin);
    }

    lastStartOfFrame = startOfFrame;
    haveFirstMeasurement = true;
}

void AFK_DetailAdjuster::computeFinished(void)
{
    lastComputeFinish = afk_clock::now();

    /* Push the time left before the end of this frame into
     * the "deviation" figure.
     * TODO: I'm going to need logic that's a bit different
     * (probably simpler -- just use lastFrameTime?) in the
     * non-vsync case.  But I don't appear to be able to    
     * turn vsync off, even fullscreen, right now? :(
     */
    afk_duration_mfl lastFrameComputeTime = std::chrono::duration_cast<afk_duration_mfl>(
        lastComputeFinish - lastStartOfFrame);
    deviation.push(lastFrameComputeTime.count() - frameTimeTarget);
    consistency.push(1.0f);
}

void AFK_DetailAdjuster::computeTimedOut(void)
{
    afk_duration_mfl timeoutTime = std::chrono::duration_cast<afk_duration_mfl>(
        afk_clock::now() - lastStartOfFrame);
    deviation.push(timeoutTime.count());
    consistency.push(0.0f);
}

const afk_clock::time_point AFK_DetailAdjuster::getStartOfFrameTime(void) const
{
    assert(haveFirstMeasurement);
    return lastStartOfFrame;
}

bool AFK_DetailAdjuster::getFrameInterval(float& o_interval) const
{
    if (haveLastFrameTime)
    {
        o_interval = lastFrameTime.count();
        return true;
    }
    else return false;
}

afk_duration_mfl AFK_DetailAdjuster::getComputeWaitTime(void)
{
    /* This is the amount of time I think ought to be left in
     * the frame, between start-of-frame and now.
     */
    assert(haveFirstMeasurement);   
    lastComputeWait = afk_clock::now();
    afk_duration_mfl frameTimeSoFar = std::chrono::duration_cast<afk_duration_mfl>(
        lastComputeWait - lastStartOfFrame);
    return afk_duration_mfl((frameTimeTarget - frameTimeSoFar.count()) * wiggle);
}

float AFK_DetailAdjuster::getDetailPitch(void) const
{
    /* TODO: Adjustment! :P
     * But first, I want to take some measurements and look
     * at the kind of problem I'm dealing with, and of course
     * make sure the code infrastructure changes are OK.
     */
    //return 512.0f; //detailPitchFromK(k);
    return detailPitch;
}

void AFK_DetailAdjuster::checkpoint(const afk_duration_mfl& sinceLastCheckpoint)
{
    std::cout << "AFK Detail Adjuster: x=" << x << ", v=" << v << ", a=" << a << std::endl;
    std::cout << "AFK Detail Adjuster: k=" << k << ", c=" << c << ", m=" << m << std::endl;
    std::cout << "AFK Detail Adjuster: deviation=" << deviation.get() << ", consistency=" << consistency.get() << std::endl;
    std::cout << "AFK Detail Adjuster: detail pitch: " << getDetailPitch() << std::endl;
}

