/* AFK (c) Alex Holloway 2013 */

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

void AFK_FileFilter::augment(const std::string& _regex, const std::string& _fmt)
{
    rep.push_back(AFK_FileFilter::FilterReplace(_regex, _fmt));
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

