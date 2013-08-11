/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_STAGE_TIMER_H_
#define _AFK_DATA_STAGE_TIMER_H_

#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "moving_average.hpp"

/* The stage timer is a device for reporting average times taken
 * across the various stages of a multi-stage computation.
 */

class AFK_StageTimer
{
protected:
    /* The average times taken for each stage. */
    std::vector<AFK_MovingAverage<unsigned int>*> timesInMicros;

    /* When we hit the last stage. */
    boost::posix_time::ptime lastStage;

    /* The name of each stage, for printing. */
    std::vector<std::string> stageNames;
    std::string timerName;

    const unsigned int printFrequency;
    unsigned int roundsSincePrint;

public:
    AFK_StageTimer(const std::string& _timerName, const std::vector<std::string>& _stageNames, unsigned int _printFrequency);
    virtual ~AFK_StageTimer();

    /* Tells the timer to restart.
     * This function also prints the current averages,
     * if it's hit `printFrequency'.
     */
    void restart(void);

    /* Tells the timer we just hit a stage. */
    void hitStage(unsigned int stage);
};

#endif /* _AFK_DATA_STAGE_TIMER_H_ */

