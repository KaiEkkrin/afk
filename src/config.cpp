/* AFK (c) Alex Holloway 2013 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

AFK_Config::AFK_Config(int *argcp, char **argv)
{
    shadersDir = NULL;

    /* Some hand rolled command line parsing, because it's not very
     * hard, and there's no good cross platform one by default */
    for (int argi = 0; argi < *argcp; ++argi)
    {
        if (strcmp(argv[argi], "--shaders-dir") == 0)
        {
            ++argi;
            if (argi == *argcp)
            {
                fprintf(stderr, "--shaders-dir without argument\n");
                exit(1);
            }

            shadersDir = strdup(argv[argi]);
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

