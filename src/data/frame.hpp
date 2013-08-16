/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_FRAME_H_
#define _AFK_FRAME_H_

#include <sstream>

/* The frame tracker. */
class AFK_Frame
{
protected:
    unsigned long long id;

    /* This means "it never happened" when tracking things by
     * which frame they happened on.
     */
    bool never;

public:
    AFK_Frame(): id(0), never(true) {}
    AFK_Frame(const AFK_Frame& f): id(f.id), never(f.never) {}
    AFK_Frame& operator=(const AFK_Frame& f) { id = f.id; never = f.never; return *this; }

    void increment()
    {
        ++id; /* hopefully this is guaranteed to wrap */
        never = false;
    }

    bool operator==(const AFK_Frame& f) const
    {
        if (never) return f.never;
        else return id == f.id;
    }

    bool operator!=(const AFK_Frame& f) const
    {
        if (never) return !f.never;
        else return (f.never || id != f.id);
    }

    /* Returns true if our frame is less recent than the operand frame.
     * Because of rotating frame IDs, this means that any frame ID not
     * equal to the operand frame ID is assumed to be earlier (just
     * ones numerically greater are much much earlier...)
     */
    bool operator<(const AFK_Frame& f) const
    {
        if (never)
        {
            return true;
        }
        else
        {
            if (f.never) return false;
            else return (f.id != id);
        }
    }

    unsigned long long operator-(const AFK_Frame& f) const
    {
        if (never)
            return 0xffffffffffffffffull; /* as long ago as possible */
        else
            return id - f.id; /* hopefully more useful wrapping behaviour */
    }

    const long long get() const { return id; }
    const bool getNever() const { return never; }

    friend std::ostream& operator<<(std::ostream& os, const AFK_Frame& frame);
};

std::ostream& operator<<(std::ostream& os, const AFK_Frame& frame);

#endif /* _AFK_FRAME_H_ */

