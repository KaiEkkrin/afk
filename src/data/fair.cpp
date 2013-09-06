/* AFK (c) Alex Holloway 2013 */

int afk_combineTwoPuzzleFairQueue(int puzzle1, int puzzle2)
{
    int queue = 1;
    for (int pc1 = 0; pc1 < puzzle1; ++pc1) queue = queue * 2;
    for (int pc2 = 0; pc2 < puzzle2; ++pc2) queue = queue * 3;
    return queue;
}

void afk_splitTwoPuzzleFairQueue(int queue, int& o_puzzle1, int& o_puzzle2)
{
    o_puzzle1 = 0;
    o_puzzle2 = 0;

    while (queue > 0 && (queue % 2) == 0)
    {
        queue = queue / 2;
        ++o_puzzle1;
    }

    while (queue > 0 && (queue % 3) == 0)
    {
        queue = queue / 3;
        ++o_puzzle2;
    }
}

