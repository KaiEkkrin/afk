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

#include <cstdlib>
#include <sstream>

#include <boost/tokenizer.hpp>

#include "filter.hpp"


AFK_FileFilter::FilterReplace::FilterReplace(const std::string& _regex, const std::string& _fmt):
    r(_regex), f(_fmt)
{
}

AFK_FileFilter::FilterReplace::FilterReplace(const AFK_FileFilter::FilterReplace& _r):
    r(_r.r), f(_r.f)
{
}

AFK_FileFilter::FilterReplace& AFK_FileFilter::FilterReplace::operator=(const AFK_FileFilter::FilterReplace& _r)
{
    r = _r.r;
    f = _r.f;
    return *this;
}

std::string AFK_FileFilter::FilterReplace::replace(const std::string& source) const
{
    return boost::regex_replace(source, r, f);
}

AFK_FileFilter::AFK_FileFilter(std::initializer_list<std::string> init)
{
    for (auto initIt = init.begin(); initIt != init.end(); ++initIt)
    {
        std::string r = *initIt;
        ++initIt;
        std::string f = *initIt;
        rep.push_back(FilterReplace(r, f));
    }
}

void AFK_FileFilter::filter(int count, char **sources, size_t *sourceLengths) const
{
    for (int i = 0; i < count; ++i)
    {
        /* Split this string apart into its component lines, because
         * I want to replace line-by-line.
         */
        std::string source(sources[i], sourceLengths[i]);
        std::ostringstream repss;

        boost::char_separator<char> lineSep("\n");
        boost::tokenizer<boost::char_separator<char> > sourceTok(source, lineSep);
        for (boost::tokenizer<boost::char_separator<char> >::iterator sourceIt = sourceTok.begin();
            sourceIt != sourceTok.end(); ++sourceIt)
        {
            std::string sourceLine = *sourceIt;

            for (std::vector<FilterReplace>::const_iterator repIt = rep.begin(); repIt != rep.end(); ++repIt)
            {
                sourceLine = repIt->replace(sourceLine);
            }

            repss << sourceLine << "\n";
        }

        /* Now, splice the replaced string back into the source vector.
         */
        std::string repstr = repss.str();
        if (sourceLengths[i] < repstr.size())
            sources[i] = (char *)realloc(sources[i], repstr.size() + 1);

        strcpy(sources[i], repstr.c_str());
        sourceLengths[i] = repstr.size();
    }
}

std::ostream& operator<<(std::ostream& os, const AFK_FileFilter::FilterReplace& r)
{
    os << r.r << " -> " << r.f;
    return os;
}

std::ostream& operator<<(std::ostream& os, const AFK_FileFilter& f)
{
    os << "FileFilter(";
    for (auto r : f.rep)
    {
        os << r << ", ";
    }
    os << ")";
    return os;
}

