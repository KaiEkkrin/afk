/* AFK (c) Alex Holloway 2013 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#define REQUIRE_ARGUMENT(option) \
    ++argi;\
    if (argi == *argcp)\
    {\
        fprintf(stderr, "%s without argument\n", option);\
        exit(1);\
    }

AFK_Config::AFK_Config(int *argcp, char **argv)
{
    shadersDir  = NULL;
    fov         = 90.0f;
    zNear       = 0.5f;
    zFar        = 16.0f;

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

    /* Print a little dump. */
    printf("AFK: Loaded configuration:\n");
    printf("  shadersDir:   %s\n", shadersDir);
}

AFK_Config::~AFK_Config()
{
    if (shadersDir) free(shadersDir);
}

