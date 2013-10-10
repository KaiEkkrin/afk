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

#include <sstream>

#include "stage_timer.hpp"

AFK_StageTimer::AFK_StageTimer(const std::string& _timerName, const std::vector<std::string>& _stageNames, unsigned int _printFrequency):
    timerName(_timerName),
    printFrequency(_printFrequency),
    roundsSincePrint(0)
{
    unsigned int longestNameLength = 0;
    for (unsigned int i = 0; i < _stageNames.size(); ++i)
    {
        timesInMicros.push_back(new AFK_MovingAverage<unsigned int>(_printFrequency, 0));
        if (_stageNames[i].size() > longestNameLength) longestNameLength = _stageNames[i].size();
    }

    for (unsigned int i = 0; i < _stageNames.size(); ++i)
    {
        std::ostringstream stageNameSS;
        stageNameSS << _stageNames[i];
        for (unsigned int j = longestNameLength; j > _stageNames[i].size(); --j)
            stageNameSS << " ";
        stageNames.push_back(stageNameSS.str());
    }

    /* I always add a "Finish" stage, which takes up until the restart
     * is called.
     */
    stageNames.push_back("Finish");
    timesInMicros.push_back(new AFK_MovingAverage<unsigned int>(_printFrequency, 0));
}

AFK_StageTimer::~AFK_StageTimer()
{
    for (auto t : timesInMicros)
    {
        delete t;
    }
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
        unsigned int totalTime = 0;
        for (auto t : timesInMicros)
        {
            totalTime += t->get();
        }

        std::cout << "  " << timerName << " stage timer: " << std::endl;
        for (unsigned int i = 0; i < stageNames.size(); ++i)
        {
            unsigned int timeInMicros = timesInMicros[i]->get();
            unsigned int timePercent = 100 * timeInMicros / totalTime;
            std::cout << "    " << stageNames[i] << ": " << timeInMicros << " micros (" << timePercent << "\% total)" << std::endl;
        }

        roundsSincePrint = 0;
    }
}

void AFK_StageTimer::hitStage(unsigned int stage)
{
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
    timesInMicros[stage]->push((now - lastStage).total_microseconds());
    lastStage = now;
}

