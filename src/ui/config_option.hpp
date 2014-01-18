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

#ifndef _AFK_UI_CONFIG_OPTION_H_
#define _AFK_UI_CONFIG_OPTION_H_

#include <cassert>
#include <functional>
#include <sstream>
#include <list>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "../exception.hpp"


/* How to name config options. */
class AFK_ConfigOptionName
{
protected:
    std::string name;
    std::list<std::string> spellings;

public:
    AFK_ConfigOptionName(const std::string& _name);

    const std::string& getName(void) const { return name; }
    std::list<std::string>::const_iterator spellingsBegin(void) const { return spellings.cbegin(); }
    std::list<std::string>::const_iterator spellingsEnd(void) const { return spellings.cend(); }

    virtual bool matches(const std::string& arg);
    virtual bool subMatches(const std::string& arg, size_t start, size_t& o_matchedLength);

    friend std::ostream& operator<<(std::ostream& os, const AFK_ConfigOptionName& optionName);
};

std::ostream& operator<<(std::ostream& os, const AFK_ConfigOptionName& optionName);

/* A fixed-type interface all config options implement. */
class AFK_ConfigOptionBase
{
protected:
    AFK_ConfigOptionName name;
    bool noSave;

    /* Implement this to do the save -- stops subclasses from needing to
     * worry about the noSave flag
     */
    virtual void saveInternal(std::ostream& os) const = 0;

public:
    AFK_ConfigOptionBase(const std::string& _name, std::list<AFK_ConfigOptionBase *> *options, bool _noSave = false);

    /* Check the name matches.
     * In this function, we abstract away the concept that
     * the caller is iterating over a list of strings (in such a way as
     * to no longer require a template) by providing functions to get
     * the current string, and to increment the iterator.
     * We call nextArg() only if we matched successfully.
     */
    virtual bool nameMatches(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg);

    /* How to set the value.  Return true if the value was valid
     * (and use nextArg() to consume it).  Of course, the base class
     * doesn't have a value so this must remain abstract!
     */
    virtual bool matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) = 0;

    /* The command line parser consumes either one (for bool) or two
    * (otherwise) strings from the iterator.
    */
    template<typename Iterable>
    bool parseCmdArgs(Iterable& here, Iterable& end)
    {
        bool hasMatched = false;

        if (here != end)
        {
            std::function<std::string(void)> getArg = [&here, &end]()
            {
                if (here != end) return std::string(*here);
                else throw std::out_of_range("parseCmdArgs");
            };

            std::function<void(void)> nextArg = [&here]() { ++here; };

            if (nameMatches(getArg, nextArg))
            {
                hasMatched = matched(getArg, nextArg);
            }
        }

        return hasMatched;
    }

    /* The file parser expects you to have already split the string,
     * and thence also takes an iterable that it increments.
     * The difference here is it chucks whitespace out automatically...
     */
    template<typename Iterable>
    bool parseFileLine(Iterable& here, Iterable& end)
    {
        bool hasMatched = false;

        if (here != end)
        {
            std::function<std::string(void)> getArg = [&here, &end]()
            {
                if (here != end) return boost::algorithm::trim_copy(std::string(*here));
                else throw std::out_of_range("parseFileLine");
            };

            std::function<void(void)> nextArg = [&here]() { ++here; };

            if (nameMatches(getArg, nextArg))
            {
                hasMatched = matched(getArg, nextArg);
            }
        }

        return hasMatched;
    }

    void save(std::ostream& os) const;
};

/* A basic single-value configuration option of any value type.
 */
template<typename T>
class AFK_ConfigOption : public AFK_ConfigOptionBase
{
protected:
    T field;

    void saveInternal(std::ostream& os) const override
    {
        os << name << "=" << boost::lexical_cast<std::string, T>(field) << std::endl;
    }

public:
    AFK_ConfigOption(const std::string& _name, std::list<AFK_ConfigOptionBase *> *options, const T& defaultValue, bool _noSave = false) :
        AFK_ConfigOptionBase(_name, options, _noSave),
        field(defaultValue)
    {
    }

    bool matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) override
    {
        try
        {
            field = boost::lexical_cast<T>(getArg());
            nextArg();
            return true;
        }
        catch (std::out_of_range&)
        {
            return false;
        }
    }

    /* To be used when I get in-program UI */
    T& operator=(const T& value) { field = value; return field; }
    void set(const T& value)
    {
        field = value;
    }

    /* The inevitable getters. */
    operator T(void) const { return field; }
    const T& get(void) const { return field; }
};

/* A specialisation for boolean options.  Allows --thing (sets to true) and
 * --no-thing (sets to false).
 */
template<>
class AFK_ConfigOption<bool> : public AFK_ConfigOptionBase
{
protected:
    // The name of the negated version.
    AFK_ConfigOptionName antiName;

    // A bit of parser state tracking.  Gets set to true when we find the
    // positive option, false when we find the negative.
    bool found;

    bool field;

    std::string makeAntiName(const std::string& _name) const
    {
        std::ostringstream antiNameSS;
        antiNameSS << "no";
        antiNameSS << static_cast<char>(toupper(_name[0]));
        antiNameSS << _name.substr(1, _name.size() - 1);
        return antiNameSS.str();
    }

    void saveInternal(std::ostream& os) const override
    {
        if (field)
        {
            os << name << std::endl;
        }
        else
        {
            os << antiName << std::endl;
        }
    }

public:
    AFK_ConfigOption(const std::string& _name, std::list<AFK_ConfigOptionBase *> *options, const bool& defaultValue, bool _noSave = false) :
        AFK_ConfigOptionBase(_name, options, _noSave),
        antiName(makeAntiName(_name)),
        field(defaultValue)
    {
    }

    bool nameMatches(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) override
    {
        if (AFK_ConfigOptionBase::nameMatches(getArg, nextArg))
        {
            found = true;
            return true;
        }
        else
        {
            if (antiName.matches(getArg()))
            {
                nextArg();
                found = false;
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    bool matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) override
    {
        field = found;
        return true;
    }

    /* To be used when I get in-program UI */
    bool operator=(bool value) { field = value; return field; }
    void set(const bool& value)
    {
        field = value;
    }

    /* The inevitable getters. */
    operator bool(void) const { return field; }
    const bool& get(void) const { return field; }
};

#endif /* _AFK_UI_CONFIG_OPTION_H_*/
