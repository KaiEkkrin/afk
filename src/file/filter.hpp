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

#ifndef _AFK_FILE_FILTER_H_
#define _AFK_FILE_FILTER_H_

#include <vector>

/* TODO: Can I use the regex in the C++11 standard library instead? */
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

        friend std::ostream& operator<<(std::ostream& os, const AFK_FileFilter::FilterReplace& r);
    };

    std::vector<FilterReplace> rep;

public:
    /* Initializes the filter with a sequence of (regexp, format string).
     * -- which means you must provide an even number of strings!
     */
    AFK_FileFilter(std::initializer_list<std::string> init);

    /* Applies this filter to the given list of sources.
     * This function re-allocates the supplied source array to
     * match, assuming that each element of `sources' was
     * allocated with `malloc'.
     */
    void filter(size_t count, char **sources, size_t *sourceLengths) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_FileFilter::FilterReplace& r);
    friend std::ostream& operator<<(std::ostream& os, const AFK_FileFilter& f);
};

std::ostream& operator<<(std::ostream& os, const AFK_FileFilter::FilterReplace& r);
std::ostream& operator<<(std::ostream& os, const AFK_FileFilter& f);

#endif /* _AFK_FILE_FILTER_H_ */

