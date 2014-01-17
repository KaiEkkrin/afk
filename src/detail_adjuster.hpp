/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#ifndef _AFK_DETAIL_ADJUSTER_H_
#define _AFK_DETAIL_ADJUSTER_H_

#include "afk.hpp"

#include "clock.hpp"
#include "config.hpp"
#include "data/moving_average.hpp"

/* This module is a new place to implement AFK's dynamic detail
 * adjustment system.
 *
 * I was going to try to do something complicated involving a
 * model of a damped spring system to fix the oscillation I saw
 * with the old detail pitch model; however, with some tweaking and
 * updates every frame here, I find the oscillation is gone anyway.
 * So no extra complexity.
 *
 * The detail pitch adjuster is a compromise between getting close
 * to the frame time target, and getting close to no flicker or
 * distracting random detail changes.  I don't think it's possible
 * to get perfect both.
 */

class AFK_DetailAdjuster
{
protected:
    /* The target frame time. */
    const float frameTimeTarget;
    const float wiggle;

    const float detailPitchMax;
    const float detailPitchMin;

    /* This divides out the deviation figure to create an error
     * value.
     */
    const float errorPerDeviation;

    /* This value determines the detail pitch sticky threshold
     * by the log of the detail pitch.
     */
    const float stickiness;

    /* The current sequence of times we're tracking. */
    afk_clock::time_point lastStartOfFrame;
    afk_clock::time_point lastComputeWait;
    afk_clock::time_point lastComputeFinish;
    bool haveFirstMeasurement;

    /* The amount of time the last frame took (for simulation delay.) */
    afk_duration_mfl lastFrameTime;
    bool haveLastFrameTime;

    float detailPitch;
    float lastDetailPitch;
    float logLastDetailPitch;

    AFK_MovingAverage<float> deviation;

public:
    AFK_DetailAdjuster(const AFK_Config *config);
    virtual ~AFK_DetailAdjuster();

    /* Call these functions to tell the detail adjuster to take a
     * time measurement.
     */
    void startOfFrame(void);
    void computeFinished(void);
    void computeTimedOut(void);

    /* For reference. */
    const afk_clock::time_point getStartOfFrameTime(void) const;

    /* Output a measured delay time to use for the simulation,
     * in millis, of course.
     * Returns false if we don't yet have enough measurements
     * (at start of simulation only).
     */
    bool getFrameInterval(float& o_interval) const;

    /* Call this when deciding how long to wait for the compute
     * phase before timing it out and passing control back to the
     * window.
     */
    afk_duration_mfl getComputeWaitTime(void);

    /* Output detail pitch for the world to use. */
    float getDetailPitch(void);

    /* Print a checkpoint. */
    void checkpoint(const afk_duration_mfl& sinceLastCheckpoint);
};

#endif /* _AFK_DETAIL_ADJUSTER_H_ */

