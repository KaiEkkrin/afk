/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_FILE_FILTER_H_
#define _AFK_FILE_FILTER_H_

#include <vector>

#include <boost/regex.hpp>

/* This is a trivial utility that runs through a list of a list of strings
 * (e.g. read by calls to afk_readFileContents() ) and applies a collection
 * of regular expression replaces to it.
 * I've made this because the GLSL compiler doesn't have its own preprocessor,
 * and this utility will let me do simple stuff like replacing a label with a
 * runtime number.
 */

class AFK_FileFilter
{
protected:
    class FilterReplace
    {
    protected:
        boost::regex r;
        std::string f;

    public:
        FilterReplace(const std::string& _regex, const std::string& _fmt);
        FilterReplace(const FilterReplace& _r);

        FilterReplace& operator=(const FilterReplace& _r);

        std::string replace(const std::string& source) const;
    };

    std::vector<FilterReplace> rep;

public:
    /* Adds a regular expression and format string to the filter. */
    void augment(const std::string& _regex, const std::string& _fmt);

    /* Applies this filter to the given list of sources.
     * This function re-allocates the supplied source array to
     * match, assuming that each element of `sources' was
     * allocated with `malloc'.
     */
    void filter(int count, char **sources, size_t *sourceLengths) const;
};

#endif /* _AFK_FILE_FILTER_H_ */

