/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "async/async.hpp"
#include "async/work_queue.hpp"
#include "compute_test_long.hpp"
#include "computer.hpp"
#include "exception.hpp"

struct testComputeParam
{
    AFK_Computer *computer;
};

bool testComputeWorker(unsigned int id, const struct testComputeParam& param, AFK_WorkQueue<struct testComputeParam, bool>& queue)
{
    bool success = false;
    try
    {
        param.computer->test(id);
        success = true;
    }
    catch (AFK_Exception)
    {
        std::cout << "F";
    }

    return success;
}

void afk_testComputeLong(void)
{
    AFK_AsyncGang<struct testComputeParam, bool> testComputeGang(
        boost::function<bool (unsigned int, struct testComputeParam, AFK_WorkQueue<struct testComputeParam, bool>&)>(testComputeWorker),
        10);

    AFK_Computer computer(testComputeGang.getConcurrency());
    for (unsigned int i = 0; i < (testComputeGang.getConcurrency() * 100); ++i)
    {
        struct testComputeParam p;
        p.computer = &computer;
        testComputeGang << p;
    }

    boost::unique_future<bool> finished = testComputeGang.start();
    finished.wait();
    std::cout << "Finished with " << finished.get() << std::endl;
}

