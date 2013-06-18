/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_CONFIG_H_
#define _AFK_CONFIG_H_

class AFK_Config
{
public:
    char *shadersDir;

    /* Initialises AFK configuration, based on command line
     * arguments. */
    AFK_Config(int *argcp, char **argv);

    ~AFK_Config();
};

#endif /* _AFK_CONFIG_H_ */

