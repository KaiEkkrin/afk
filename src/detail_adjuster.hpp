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
    /* ... TODO ... */

public:
    AFK_DetailAdjuster(const AFK_Config *config);

    /* Call these functions to tell the detail adjuster to take a
     * time measurement.
     */
    void startOfFrame(void);
    void computeFinished(void);
    void graphicsTimedOut(void);
    void graphicsFinished(void);

    /* Output a measured delay time to use for the simulation. */
    float getLastDelay(void) const;

    /* Output detail pitch for the world to use. */
    float getDetailPitch(void) const;
};

#endif /* _AFK_DETAIL_ADJUSTER_H_ */

