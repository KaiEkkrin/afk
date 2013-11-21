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

#ifndef _AFK_DETAIL_ADJUSTER_H_
#define _AFK_DETAIL_ADJUSTER_H_

#include "afk.hpp"

#include "clock.hpp"
#include "config.hpp"

/* This module is a new place to implement AFK's dynamic detail
 * adjustment system.
 * 
 * Thought: Can I use critical damping of springs as a model?
 * e.g. http://hyperphysics.phy-astr.gsu.edu/hbase/oscda.html
 * `x' is the difference between the measured frame time and the
 * target (e.g. 16ms)
 * `v' is the rate of change of frame time
 * `a' is the rate of change of that rate of change :P
 * `c', `k' and `m' are constants of the model (try to arrange for
 * c*c-4mk=0)
 * The question is: what do I adjust in order to get good
 * behaviour?  What does the detail pitch represent?  Since that's
 * the thing that needs to keep changing...
 *
 * Also consider the behaviour of suspension systems, where the
 * detail pitch might be the road surface (similar?  closer
 * analogy?)
 */

class AFK_DetailAdjuster
{
protected:
    /* The target frame time. */
    const float frameTimeTarget;
    const float wiggle;

    /* The target frame rate. */
    const float xTarget;

    /* The current sequence of times we're tracking. */
    afk_clock::time_point lastStartOfFrame;
    afk_clock::time_point lastComputeWait;
    afk_clock::time_point lastComputeFinish;
    bool haveFirstMeasurement;

    /* The amount of time the last frame took (for simulation delay.) */
    afk_duration_mfl lastFrameTime;
    bool haveLastFrameTime;

    /* Instantaneous measurements of frame rate deviation, and its first and
     * second order derivatives (rate of change == "speed", rate of
     * change of change == "acceleration").
     * These numbers are in units of 1/milliseconds
     */
    float x, v, a;
    float lastV; /* -FLT_MAX for "no measurement yet" */

    /* ... TODO ... */

    float detailPitch;

    /* Here are my variables for the damping formula */
    float k, c;

    /* Again; this is tweakable, I think */
    const float m = 1.0f;

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
    float getDetailPitch(void) const;

    /* Print a checkpoint. */
    void checkpoint(const afk_duration_mfl& sinceLastCheckpoint);
};

#endif /* _AFK_DETAIL_ADJUSTER_H_ */

