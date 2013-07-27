/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_RENDER_LIST_H_
#define _AFK_DATA_RENDER_LIST_H_

#include <exception>
#include <vector>

/* A render list works like a render queue (the workers feed
 * the list, a display thread takes stuff off it to render,
 * it's "double-buffered" so that the workers can work on one
 * side while the display thread works on the other).
 * It's different because rather than using the lockfree
 * queue it actually has one vector per thread.  Doing this
 * allows the entire list to be fed into the GL at once at
 * the display stage (well, once per thread, which should
 * still be quite effective).
 */

class AFK_RenderListException: public std::exception {};

template<typename Renderable>
class AFK_RenderList
{
protected:
    const unsigned int threadCount;

    std::vector<Renderable> *l[2];
    unsigned int updateList;
    unsigned int drawList;

public:
    AFK_RenderList(const unsigned int _threadCount):
        threadCount(_threadCount), updateList(0), drawList(1)
    {
        for (unsigned int i = 0; i < 2; ++i)
        {
            l[i] = new std::vector<Renderable>[threadCount];
        }
    }

    virtual ~AFK_RenderList()
    {
        delete[] l[0];
        delete[] l[1];
    }

    /* Unlike the render queue's flipQueues, I won't really have
     * any idea whether the draw list got used, because it
     * will no doubt have been accessed all at once using
     * getDrawListBase().
     * So just assume. :P
     */
    void flipLists(void)
    {
        updateList = (updateList == 1 ? 0 : 1);
        drawList = (drawList == 1 ? 0 : 1);

        for (unsigned int threadId = 0; threadId < threadCount; ++threadId)
            l[updateList][threadId].clear();
    }

    void updatePush(unsigned int threadId, const Renderable& r)
    {
        if (threadId >= threadCount) throw new AFK_RenderListException();
        l[updateList][threadId].push_back(r);
    }

    unsigned int getThreadCount(void) const
    {
        return threadCount;
    }

    Renderable *getDrawListBase(unsigned int threadId) const
    {
        if (threadId >= threadCount) throw new AFK_RenderListException();
        return &l[drawList][threadId][0];
    }

    size_t getDrawListSize(unsigned int threadId) const
    {
        if (threadId >= threadCount) throw new AFK_RenderListException();
        return l[drawList][threadId].size();
    }
};

#endif /* _AFK_DATA_RENDER_LIST_H_ */

