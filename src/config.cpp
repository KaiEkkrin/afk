/* AFK (c) Alex Holloway 2013 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <utility>

#include "config.h"

#define REQUIRE_ARGUMENT(option) \
    ++argi;\
    if (argi == *argcp)\
    {\
        fprintf(stderr, "%s without argument\n", option);\
        exit(1);\
    }

#define DEFAULT_CONTROL(chr, ctrl) \
    controlMapping.insert(std::pair<char, enum AFK_Controls>((chr), (ctrl)));\
    controlMapping.insert(std::pair<char, enum AFK_Controls>(toupper((chr)), (ctrl)));

AFK_Config::AFK_Config(int *argcp, char **argv)
{
    shadersDir  = NULL;
    fov         = 90.0f;
    zNear       = 0.5f;
    zFar        = 128.0f;

    keyboardRotateSensitivity   = 0.01f;
    keyboardThrottleSensitivity = 0.01f;

    /* Some hand rolled command line parsing, because it's not very
     * hard, and there's no good cross platform one by default */
    for (int argi = 0; argi < *argcp; ++argi)
    {
        if (strcmp(argv[argi], "--shaders-dir") == 0)
        {
            REQUIRE_ARGUMENT("--shaders-dir")
            shadersDir = strdup(argv[argi]);
        }
        else if (strcmp(argv[argi], "--fov") == 0)
        {
            REQUIRE_ARGUMENT("--fov")
            fov = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--zNear") == 0)
        {
            REQUIRE_ARGUMENT("--zNear")
            zNear = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--zFar") == 0)
        {
            REQUIRE_ARGUMENT("--zFar")
            zFar = strtof(argv[argi], NULL);
        }

        /* Ignore other arguments. */
    }

    /* Apply defaults. */
    if (!shadersDir)
    {
        char *currentDir;
        const char *shadersDirLeafName = "shaders";
        size_t shadersDirLength;

        currentDir = get_current_dir_name();
        shadersDirLength = strlen(currentDir) + 1 + strlen(shadersDirLeafName) + 1;
        shadersDir = (char *) malloc(sizeof(char) * shadersDirLength);
        snprintf(shadersDir, shadersDirLength, "%s/%s", currentDir, shadersDirLeafName);
        free(currentDir);
    }

    /* TODO: Come up with a way of inputting the control mapping.
     * (and editing it within AFK and saving it later!)
     */
    if (controlMapping.empty())
    {
        /* TODO Swap wsad to mouse controls by default, so I can put
         * the others somewhere more sensible!
         */
        DEFAULT_CONTROL('s', CTRL_PITCH_UP)
        DEFAULT_CONTROL('w', CTRL_PITCH_DOWN)
        DEFAULT_CONTROL('e', CTRL_YAW_RIGHT)
        DEFAULT_CONTROL('q', CTRL_YAW_LEFT)
        DEFAULT_CONTROL('d', CTRL_ROLL_RIGHT)
        DEFAULT_CONTROL('a', CTRL_ROLL_LEFT)
        DEFAULT_CONTROL(' ', CTRL_OPEN_THROTTLE)
        DEFAULT_CONTROL('z', CTRL_CLOSE_THROTTLE)
    }

    /* Print a little dump. */
    printf("AFK: Loaded configuration:\n");
    printf("  shadersDir:   %s\n", shadersDir);
}

AFK_Config::~AFK_Config()
{
    if (shadersDir) free(shadersDir);
}

