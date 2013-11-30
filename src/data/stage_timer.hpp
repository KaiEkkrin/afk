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

#ifndef _AFK_DATA_STAGE_TIMER_H_
#define _AFK_DATA_STAGE_TIMER_H_

#include <vector>

#include "moving_average.hpp"
#include "../clock.hpp"

/* The stage timer is a device for reporting average times taken
 * across the various stages of a multi-stage computation.
 */

class AFK_StageTimer
{
protected:
    /* The average times taken for each stage. */
    std::vector<AFK_MovingAverage<int64_t>*> timesInMicros;

    /* When we hit the last stage. */
    afk_clock::time_point lastStage;

    /* The name of each stage, for printing. */
    std::vector<std::string> stageNames;
    std::string timerName;

    const int printFrequency;
    int roundsSincePrint;

public:
    AFK_StageTimer(const std::string& _timerName, const std::vector<std::string>& _stageNames, int _printFrequency);
    virtual ~AFK_StageTimer();

    /* Tells the timer to restart.
     * This function also prints the current averages,
     * if it's hit `printFrequency'.
     */
    void restart(void);

    /* Tells the timer we just hit a stage. */
    void hitStage(int stage);
};

#endif /* _AFK_DATA_STAGE_TIMER_H_ */

