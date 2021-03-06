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

#include <iostream>
#include <sstream>

#include "../file/logstream.hpp"
#include "stage_timer.hpp"

AFK_StageTimer::AFK_StageTimer(const std::string& _timerName, const std::vector<std::string>& _stageNames, int _printFrequency):
    timerName(_timerName),
    printFrequency(_printFrequency),
    roundsSincePrint(0)
{
    size_t longestNameLength = 0;
    for (size_t i = 0; i < _stageNames.size(); ++i)
    {
        timesInMicros.push_back(AFK_MovingAverage<int64_t>(_printFrequency, 0));
        if (_stageNames[i].size() > longestNameLength) longestNameLength = _stageNames[i].size();
    }

    for (size_t i = 0; i < _stageNames.size(); ++i)
    {
        std::ostringstream stageNameSS;
        stageNameSS << _stageNames[i];
        for (size_t j = longestNameLength; j > _stageNames[i].size(); --j)
            stageNameSS << " ";
        stageNames.push_back(stageNameSS.str());
    }

    /* I always add a "Finish" stage, which takes up until the restart
     * is called.
     */
    stageNames.push_back("Finish");
    timesInMicros.push_back(AFK_MovingAverage<int64_t>(_printFrequency, 0));
}

AFK_StageTimer::~AFK_StageTimer()
{
}

void AFK_StageTimer::restart(void)
{
    /* Account for the "finish" stage. */
    hitStage(stageNames.size() - 1);

    if (++roundsSincePrint == printFrequency)
    {
        /* Work out a total time, so that I can display percentages
         * of each round that the various stages appear to take.
         */
        int64_t totalTime = 0;
        for (auto timesIt = timesInMicros.begin(); timesIt != timesInMicros.end(); ++timesIt)
        {
            totalTime += timesIt->get();
        }

        afk_out << "  " << timerName << " stage timer: " << std::endl;
        for (unsigned int i = 0; i < stageNames.size(); ++i)
        {
            int64_t timeInMicros = timesInMicros[i].get();
            int64_t timePercent = 100 * timeInMicros / totalTime;
            afk_out << "    " << stageNames[i] << ": " << timeInMicros << " micros (" << timePercent << "% total)" << std::endl;
        }

        roundsSincePrint = 0;
    }
}

void AFK_StageTimer::hitStage(size_t stage)
{
    afk_clock::time_point now = afk_clock::now();
    std::chrono::microseconds timeInMicros =
        std::chrono::duration_cast<std::chrono::microseconds>(now - lastStage);
    timesInMicros[stage].push(timeInMicros.count());
    lastStage = now;
}

