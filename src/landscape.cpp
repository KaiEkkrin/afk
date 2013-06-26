/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <stdlib.h>
#include <time.h>

#include "core.hpp"
#include "exception.hpp"
#include "landscape.hpp"


/* TODO remove debug?  (or something) */
#define PROTAGONIST_CELL_DEBUG 1


/* AFK_Cell implementation */

AFK_Cell::AFK_Cell()
{
    /* A cell size number of 0 indicates this AFK_Cell is invalid:
     * being able to express such an invalid value might be
     * useful.
     */
    coord = Vec4<int>(0, 0, 0, 0);
}

AFK_Cell::AFK_Cell(const AFK_Cell& _cell)
{
    coord = _cell.coord;
}

AFK_Cell::AFK_Cell(const Vec4<int>& _coord)
{
    coord = _coord;
}

AFK_Cell& AFK_Cell::operator=(const AFK_Cell& _cell)
{
    coord = _cell.coord;
    return *this;
}

bool AFK_Cell::operator==(const AFK_Cell& _cell) const
{
    return coord == _cell.coord;
}

bool AFK_Cell::operator!=(const AFK_Cell& _cell) const
{
    return coord != _cell.coord;
}

Vec4<float> AFK_Cell::realCoord() const
{
    return Vec4<float>(
        (float)coord.v[0] * afk_core.landscape->minCellSize,
        (float)coord.v[1] * afk_core.landscape->minCellSize,
        (float)coord.v[2] * afk_core.landscape->minCellSize,
        (float)coord.v[3] * afk_core.landscape->minCellSize);
}

unsigned int AFK_Cell::subdivide(AFK_Cell *subCells, const size_t subCellsSize) const
{
    std::ostringstream ss;

    /* Check whether we're at smallest subdivision */
    if (coord.v[3] == 1) return 0;

    /* Check for programming error */
    if (subCellsSize < CUBE(afk_core.landscape->subdivisionFactor))
    {
        ss << "Supplied " << subCellsSize << " subcells on a landscape with subdivision factor " << afk_core.landscape->subdivisionFactor;
        throw AFK_Exception(ss.str());
    }

    /* Split things up. */
    int subScale = coord.v[3] / afk_core.landscape->subdivisionFactor;
    AFK_Cell *subCellPos = subCells;
    unsigned int subCellCount = 0;
    for (int i = coord.v[0]; i < coord.v[0] + coord.v[3]; i += subScale)
    {
        for (int j = coord.v[1]; j < coord.v[1] + coord.v[3]; j += subScale)
        {
            for (int k = coord.v[2]; k < coord.v[2] + coord.v[3]; k += subScale)
            {
                /* TODO Remove paranoia? */
                if (subCellPos >= subCells + subCellsSize)
                {
                    ss << "Ran off the end of the subcells array with i " << i << ", j " << j << ", k " << k << ", subScale " << subScale;
                    throw AFK_Exception(ss.str());
                }

                *subCellPos = AFK_Cell(Vec4<int>(i, j, k, subScale));

                /* TODO _DEFINITELY_ remove paranoia */
                if (subCellPos->parent() != *this)
                {
                    ss << "Subdivided " << *this << " into " << *subCellPos << " (parent " << subCellPos->parent() << ")";
                    throw AFK_Exception(ss.str());
                }

                ++subCellPos;
                ++subCellCount;
            }
        }
    }

    /* TODO Remove paranoia? */
    if (subCellCount != CUBE(afk_core.landscape->subdivisionFactor))
    {
        ss << "Somehow made " << subCellCount << " subcells from a subdivision factor of " << afk_core.landscape->subdivisionFactor;
        throw AFK_Exception(ss.str());
    }

    return subCellCount;
}

/* The C++ integer modulus operator's behaviour with
 * negative numbers is just shocking
 */
#define ROUND_TO_CELL_SCALE(coord, scale) \
    (coord) - ((coord) >= 0 ? \
                ((coord) % (scale)) : \
                ((scale) + (((coord) % (scale)) != 0 ? \
                            ((coord) % (scale)) : \
                            -(scale))))

AFK_Cell AFK_Cell::parent(void) const
{
    int parentCellScale = coord.v[3] * afk_core.landscape->subdivisionFactor;
    return AFK_Cell(Vec4<int>(
        ROUND_TO_CELL_SCALE(coord.v[0], parentCellScale),
        ROUND_TO_CELL_SCALE(coord.v[1], parentCellScale),
        ROUND_TO_CELL_SCALE(coord.v[2], parentCellScale),
        parentCellScale));
}

size_t hash_value(const AFK_Cell& cell)
{
    size_t yr, zr, sr;

    yr = (size_t)cell.coord.v[1];
    zr = (size_t)cell.coord.v[2];
    sr = (size_t)cell.coord.v[3];

    asm("rol $7, %0\n" :"=r"(yr) :"0"(yr));
    asm("rol $14, %0\n" :"=r"(zr) :"0"(zr));
    asm("rol $21, %0\n" :"=r"(sr) :"0"(sr));

    return (size_t)cell.coord.v[0] ^ yr ^ zr ^ sr;
}


/* The AFK_Cell print overload. */
std::ostream& operator<<(std::ostream& os, const AFK_Cell& cell)
{
    return os << "Cell(" <<
        cell.coord.v[0] << ", " <<
        cell.coord.v[1] << ", " <<
        cell.coord.v[2] << ", scale " <<
        cell.coord.v[3] << ")";
}


/* AFK_LandscapeCell implementation */

AFK_LandscapeCell::AFK_LandscapeCell(const AFK_Cell& cell, const Vec4<float>& _coord)
{
    /* Sort out the shader program.
     * TODO This is highly inefficient.  Once I've made sure
     * that things are working, try to share a single shader
     * program between all landscape cells.
     */
    shaderProgram << "basic_fragment" << "basic_vertex";
    shaderProgram.Link();

    transformLocation = glGetUniformLocation(shaderProgram.program, "transform");
    fixedColorLocation = glGetUniformLocation(shaderProgram.program, "fixedColor");

    coord = _coord;

    object.displace(Vec3<float>(coord.v[0], coord.v[1], coord.v[2]));

    /* TODO Maybe I can come up with something neater; OTOH, maybe
     * I don't want to :P
     */
    colour = Vec3<float>(
        (float)rand() / (float)RAND_MAX,
        (float)rand() / (float)RAND_MAX,
        (float)rand() / (float)RAND_MAX);

    float *rawVertices = NULL;
    size_t rawVerticesSize = sizeof(float) * 3 * SQUARE(afk_core.landscape->pointSubdivisionFactor);
    rawVertices = (float *)malloc(rawVerticesSize);
    if (!rawVertices)
    {
        std::ostringstream ss;
        ss << "Unable to allocate vertex array for landscape cell " << cell;
        throw AFK_Exception(ss.str());
    }

    /* Populate the vertex array. */
    /* TODO This is where the random landscape point
     * displacement code will go.  But in order to test that
     * my cells work, I'll just make everything flat.
     * Which means that for cells that don't have zero Y,
     * there's no geometry at all.
     */
    if (cell.coord.v[1] == 0)
    {
        float *rawVerticesPos = rawVertices;

        for (unsigned int xi = 0; xi < afk_core.landscape->pointSubdivisionFactor; ++xi)
        {
            for (unsigned int zi = 0; zi < afk_core.landscape->pointSubdivisionFactor; ++zi)
            {
                /* TODO REMOVEME Check for programming error */
                if (rawVerticesPos >= (rawVertices + rawVerticesSize))
                {
                    std::ostringstream ss;
                    ss << "Exceeded rawVertices array populating landscape cell " << *this;
                    throw AFK_Exception(ss.str());
                }

                /* The geometry in a cell goes from its (0,0,0) point to
                 * just before its (coord.v[3], coord.v[3], coord.v[3]
                 * point (in cell space)
                 */
                float xf = (float)xi * coord.v[3] / (float)afk_core.landscape->pointSubdivisionFactor;
                float zf = (float)zi * coord.v[3] / (float)afk_core.landscape->pointSubdivisionFactor;

                *(rawVerticesPos++) = xf;
                *(rawVerticesPos++) = 0.0f;
                *(rawVerticesPos++) = zf;
            }
        }

        /* Turn this into an OpenGL vertex buffer */
        glGenBuffers(1, &vertices);
        glBindBuffer(GL_ARRAY_BUFFER, vertices);
        glBufferData(GL_ARRAY_BUFFER, rawVerticesSize, rawVertices, GL_STATIC_DRAW);

        free(rawVertices);
    }
    else
    {
        vertices = 0;
    }
}

AFK_LandscapeCell::~AFK_LandscapeCell()
{
    glDeleteBuffers(1, &vertices);
}

void AFK_LandscapeCell::display(const Mat4<float>& projection)
{
    if (vertices != 0)
    {
        glUseProgram(shaderProgram.program);
        glUniform3f(fixedColorLocation, colour.v[0], colour.v[1], colour.v[2]);

        updateTransform(projection);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertices);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glDrawArrays(GL_POINTS, 0, SQUARE(afk_core.landscape->pointSubdivisionFactor));

        glDisableVertexAttribArray(0);
    }
}


std::ostream& operator<<(std::ostream& os, const AFK_LandscapeCell& cell)
{
    return os << "LandscapeCell(" <<
        cell.coord.v[0] << ", " <<
        cell.coord.v[1] << ", " <<
        cell.coord.v[2] << ", scale " <<
        cell.coord.v[3] << ")";
}


/* AFK_Landscape implementation */

AFK_Landscape::AFK_Landscape(size_t _cacheSize, float _maxDistance, unsigned int _subdivisionFactor, unsigned int _detailPitch)
{
    cacheSize           = _cacheSize;
    maxDistance         = _maxDistance;
    subdivisionFactor   = _subdivisionFactor;
    detailPitch         = _detailPitch;

    /* TODO Make this a parameter. */
    pointSubdivisionFactor = 8;

    /* Guess at the minimum point separation.
     * Explanation on a piece of paper I'll lose quickly
     * TODO: This changes with window dimensions.  Move
     * this calculation elsewhere so that it can be
     * changed as we go too :/
     */
    float tanHalfFov = tanf((afk_core.config->fov / 2.0f) * M_PI / 180.0f);
    float minPointSeparation = ((float)_detailPitch * afk_core.config->zNear * tanHalfFov) / (float)glutGet(GLUT_WINDOW_HEIGHT);

    /* The target minimum cell size is that multiplied by
     * the point subdivision factor.
     */
    float targetMinCellSize = minPointSeparation * pointSubdivisionFactor;

    /* Now, start with the obvious max cell size and subdivide
     * until I've got a minimum cell size that roughly matches
     * the intended detail pitch.
     */
    maxSubdivisions = 1;

    for (minCellSize = maxDistance; minCellSize > targetMinCellSize; minCellSize = minCellSize / subdivisionFactor)
        ++maxSubdivisions;

    std::cout << "AFK: Landscape using minCellSize " << minCellSize;

    /* TODO I'm using this, albeit right now only for the
     * test colours.  Sort out the RNG system; this is almost
     * certainly not what I want to end up with.
     */
    srand(time(NULL));
}

AFK_Landscape::~AFK_Landscape()
{
    for (boost::unordered_map<const AFK_Cell, AFK_LandscapeCell*>::iterator lmIt = landMap.begin(); lmIt != landMap.end(); ++lmIt)
        delete lmIt->second;

    landMap.clear();    
    displayQueue.clear();
}

void AFK_Landscape::enqueueCellForDrawing(const AFK_Cell& cell, const Vec4<float>& realCoord)
{
    AFK_LandscapeCell*& landscapeCell = landMap[cell];
    if (!landscapeCell) landscapeCell = new AFK_LandscapeCell(cell, realCoord);

    /* Don't enqueue empty landscape cells */
    if (landscapeCell->vertices != 0)
        displayQueue.push_back(landscapeCell);
}

void AFK_Landscape::updateLandMap(void)
{
    /* Make the largest cell that contains the protagonist.
     * TODO: Maybe I should consider the ones around it,
     * especially if we're near the edge.  In future.  Mhm.
     * This cell is pretty large.
     */

    /* First, transform the protagonist location and its facing
     * into integer cell-space.
     * TODO: This is *really* going to want arbitrary
     * precision arithmetic, eventually.
     */
    Mat4<float> protagonistTransformation = afk_core.protagonist->object.getTransformation();
    Vec4<float> hgProtagonistLocation = protagonistTransformation *
        Vec4<float>(0.0f, 0.0f, 0.0f, 1.0f);
    Vec3<float> protagonistLocation = Vec3<float>(
        hgProtagonistLocation.v[0] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[1] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[2] / hgProtagonistLocation.v[3]);
    Vec4<int> csProtagonistLocation = Vec4<int>(
        (int)(protagonistLocation.v[0] / minCellSize),
        (int)(protagonistLocation.v[1] / minCellSize),
        (int)(protagonistLocation.v[2] / minCellSize),
        1);

    AFK_Cell protagonistCell(csProtagonistLocation);

    Vec4<float> hgProtagonistNose = protagonistTransformation *
        Vec4<float>(0.0f, 0.0f, 1.0f, 1.0f);
    Vec3<float> protagonistFacing = Vec3<float>(
        hgProtagonistNose.v[0] * hgProtagonistNose.v[3],
        hgProtagonistNose.v[1] * hgProtagonistNose.v[3],
        hgProtagonistNose.v[2] * hgProtagonistNose.v[3]) - protagonistLocation;

#ifdef PROTAGONIST_CELL_DEBUG
    {
        std::ostringstream ss;
        ss << "Protagonist in cell: " << protagonistCell << std::endl;
        ss << "Protagonist facing direction: (" <<
            protagonistFacing.v[0] << ", " <<
            protagonistFacing.v[1] << ", " <<
            protagonistFacing.v[2] << ")";
        afk_core.occasionallyPrint(ss.str());
    }
#endif
        
    /* I definitely want to draw that cell. */
    enqueueCellForDrawing(protagonistCell, protagonistCell.realCoord());

    /*
     * Now, wander up through the cell's family tree
     * deciding which other cells to draw.
     */
    size_t siblingCellsSize = CUBE(subdivisionFactor);
    AFK_Cell *siblingCells = new AFK_Cell[siblingCellsSize];

    do
    {
        AFK_Cell parentCell = protagonistCell.parent();

#ifdef PROTAGONIST_CELL_DEBUG
        {
            std::ostringstream ss;
            ss << "Considering parent cell: " << parentCell;
            afk_core.occasionallyPrint(ss.str());
        }
#endif

        parentCell.subdivide(siblingCells, siblingCellsSize);

        /* TODO: I want to be a lot cleverer about this:
         * - for each sibling cell, check whether there will
         * be anything in it -- if not, discard it right away.
         * - if not, check whether it will appear outside the
         * camera fov -- discard it right away if so.
         * - if not, check how close it will appear.  I might
         * want to subdivide it further.
         * - some day, I'll want to check for occlusion too
         * maybe (although that could be quite hard).
         * - once I've reached the biggest cell, maybe
         * investigate its neighbours too, or will I definitely
         * never need that?
         */

        /* For now, just add everything and keep hopping up. */
        for (unsigned int i = 0; i < siblingCellsSize; ++i)
        {
            if (siblingCells[i] != protagonistCell)
            {
                Vec4<float> siblingRealCoord = siblingCells[i].realCoord();

                /* I want to show this cell if it's on the side
                 * the protagonist is looking at, otherwise not.
                 * TODO I'm checking only one vertex here; I
                 * should be checking the other seven too!
                 * Change this thing to check vertices, not cells.
                 */
                Vec3<float> siblingCellFacing = Vec3<float>(
                    siblingRealCoord.v[0],
                    siblingRealCoord.v[1],
                    siblingRealCoord.v[2]) - protagonistLocation;

                /* TODO Move this logic into the recursive-descent
                 * cell finder that I should write.
                 * And finesse it to take into account fov
                 * (right now it's 180 degrees).
                 */
                if (protagonistFacing.dot(siblingCellFacing) > 0.0f)
                    enqueueCellForDrawing(siblingCells[i], siblingRealCoord);
            }
        }

        protagonistCell = parentCell;
    }
    while (protagonistCell.coord.v[3] < maxDistance);

    /* TODO: The protagonist cell is now the largest cell.
     * Go through its vertices; if any of them is close
     * enough to render (and is in front of the camera),
     * recurse through it enqueueing it.
     * This is inevitably going to result in trying to
     * re-enqueue the same cell a few times (after picking
     * a new largest cell I want to sub-divide it, and there
     * may be more sub-cells than are reasonable to pick
     * through with static logic).
     * Therefore, what I want to do is to replace the queue
     * (it doesn't need to be a queue anyway) with another
     * unordered_map, so that I can uniquely add each one.
     * As a finesse, I should replace the above code that
     * actually queues sibling cells, with code that simply
     * finds the biggest cell right away, and then calls the
     * recursive-descent cell finder on the protagonist
     * super-cell too.
     */
    

    delete[] siblingCells;
}

void AFK_Landscape::display(const Mat4<float>& projection)
{
    while (!displayQueue.empty())
    {
        AFK_LandscapeCell *landscapeCell = displayQueue.front();
        landscapeCell->display(projection);
        displayQueue.pop_front();
    }
}

