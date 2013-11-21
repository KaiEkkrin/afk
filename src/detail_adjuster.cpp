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
#include <iostream>

#include "detail_adjuster.hpp"


/* AFK_DetailAdjuster implementation */

AFK_DetailAdjuster::AFK_DetailAdjuster(const AFK_Config *config):
    frameTimeTarget(config->targetFrameTimeMillis),
    wiggle(0.9f),
    xTarget(1.0f / config->targetFrameTimeMillis),
    haveFirstMeasurement(false),
    haveLastFrameTime(false),
    lastV(-FLT_MAX),
    detailPitch(config->startingDetailPitch)
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
        x = 1.0f / lastFrameTime.count(); 

        lastV = v;
        v = x - lastX;

        a = v - lastV;
    }

    lastStartOfFrame = startOfFrame;
    haveFirstMeasurement = true;
}

void AFK_DetailAdjuster::computeFinished(void)
{
    lastComputeFinish = afk_clock::now();
}

void AFK_DetailAdjuster::computeTimedOut(void)
{
    /* TODO ? */
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
    return detailPitch;
}

void AFK_DetailAdjuster::checkpoint(const afk_duration_mfl& sinceLastCheckpoint)
{
    std::cout << "AFK Detail Adjuster: x=" << x << ", v=" << v << ", a=" << a << std::endl;
    std::cout << "AFK Detail Adjuster: Detail pitch: " << detailPitch << std::endl;
}

