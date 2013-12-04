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

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <iostream>

#include "debug.hpp"
#include "detail_adjuster.hpp"


/* AFK_DetailAdjuster implementation */

AFK_DetailAdjuster::AFK_DetailAdjuster(const AFK_Config *config):
    frameTimeTarget(config->targetFrameTimeMillis),
    wiggle(0.9f),
    detailPitchMax(config->maxDetailPitch),
    detailPitchMin(config->minDetailPitch),
    errorPerDeviation(240.0f), /* magic */
    stickiness(config->detailPitchStickiness),
    haveFirstMeasurement(false),
    haveLastFrameTime(false),
    detailPitch(config->startingDetailPitch),
    lastDetailPitch(config->startingDetailPitch),
    logLastDetailPitch(log(config->startingDetailPitch)),
    deviation(config->framesPerCalibration, 0.0f)
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
        lastFrameTime = std::chrono::duration_cast<afk_duration_mfl>(
            startOfFrame - lastStartOfFrame);
        haveLastFrameTime = true;

        float boundedError = std::max<float>(std::min<float>(deviation.get() / errorPerDeviation, 1.0f), -1.0f);
        float detailFactor = -(1.0f / (boundedError / 2.0f - 1.0f)); /* between 0.5 and 2 */
        //AFK_DEBUG_PRINTL("Detail factor: " << detailFactor)
        detailPitch = std::max<float>(std::min<float>(detailPitch * detailFactor, detailPitchMax), detailPitchMin);
    }

    lastStartOfFrame = startOfFrame;
    haveFirstMeasurement = true;
}

void AFK_DetailAdjuster::computeFinished(void)
{
    lastComputeFinish = afk_clock::now();

    /* Push the time left before the end of this frame into
     * the "deviation" figure.
     * This logic works remarkably well with vsync on or off,
     * target frame times matching or not matching the vsync,
     * etc.  Wow!
     * (To get vsync actually switched off on nvidia, use
     * nvidia-settings, "X Screen <number>" -> "OpenGL Settings"
     * -> "Sync to VBlank".)
     */
    afk_duration_mfl lastFrameComputeTime = std::chrono::duration_cast<afk_duration_mfl>(
        lastComputeFinish - lastStartOfFrame);
    deviation.push(lastFrameComputeTime.count() - frameTimeTarget);
}

void AFK_DetailAdjuster::computeTimedOut(void)
{
    afk_duration_mfl timeoutTime = std::chrono::duration_cast<afk_duration_mfl>(
        afk_clock::now() - lastStartOfFrame);
    deviation.push(timeoutTime.count());
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

float AFK_DetailAdjuster::getDetailPitch(void)
{
    /* I'm going to apply a "stickiness",
     * by which the detail pitch returned is the previous one unless the
     * difference is larger by a log-based threshold.
     */
    float logDetailPitch = log(detailPitch);
    if (fabs(logDetailPitch - logLastDetailPitch) > stickiness)
    {
        //AFK_DEBUG_PRINTL("detail pitch " << detailPitch << "; using new")
        lastDetailPitch = detailPitch;
        logLastDetailPitch = logDetailPitch;
        return detailPitch;
    }
    else
    {
        //AFK_DEBUG_PRINTL("detail pitch " << detailPitch << ": using last, " << lastDetailPitch)
        return lastDetailPitch;
    }
}

void AFK_DetailAdjuster::checkpoint(const afk_duration_mfl& sinceLastCheckpoint)
{
    std::cout << "AFK Detail Adjuster: detail pitch: " << getDetailPitch() << std::endl;
}

