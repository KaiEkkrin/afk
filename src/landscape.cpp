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

unsigned int AFK_Cell::subdivide(
    AFK_Cell *subCells,
    const size_t subCellsSize,
    int factor,
    int stride,
    int points) const
{
    /* Check whether we're at smallest subdivision */
    if (coord.v[3] == 1) return 0;

    /* Check for programming error */
    if (subCellsSize != (size_t)CUBE(points))
    {
        std::ostringstream ss;
        ss << "Supplied " << subCellsSize << " subcells for " << points << " points";
        throw AFK_Exception(ss.str());
    }

    AFK_Cell *subCellPos = subCells;
    unsigned int subCellCount = 0;
    for (int i = coord.v[0]; i < coord.v[0] + stride * points; i += stride)
    {
        for (int j = coord.v[1]; j < coord.v[1] + stride * points; j += stride)
        {
            for (int k = coord.v[2]; k < coord.v[2] + stride * points; k += stride)
            {
                *(subCellPos++) = AFK_Cell(Vec4<int>(i, j, k, stride));
                ++subCellCount;
            }
        }
    }

    return subCellCount;
}

unsigned int AFK_Cell::subdivide(AFK_Cell *subCells, const size_t subCellsSize) const
{
    return subdivide(
        subCells,
        subCellsSize,
        (int)afk_core.landscape->subdivisionFactor,
        coord.v[3] / afk_core.landscape->subdivisionFactor,
        (int)afk_core.landscape->subdivisionFactor);
}

unsigned int AFK_Cell::augmentedSubdivide(AFK_Cell *augmentedSubcells, const size_t augmentedSubcellsSize) const
{
    return subdivide(
        augmentedSubcells,
        augmentedSubcellsSize,
        (int)afk_core.landscape->subdivisionFactor,
        coord.v[3] / afk_core.landscape->subdivisionFactor,
        (int)afk_core.landscape->subdivisionFactor + 1);
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

bool AFK_Cell::isParent(const AFK_Cell& parent) const
{
    /* The given cell could be parent if this cell falls
     * within its boundaries and the scale is correct
     */
    return (
        coord.v[0] >= parent.coord.v[0] &&
        coord.v[0] < (parent.coord.v[0] + parent.coord.v[3]) &&
        coord.v[1] >= parent.coord.v[1] &&
        coord.v[1] < (parent.coord.v[1] + parent.coord.v[3]) &&
        coord.v[2] >= parent.coord.v[2] &&
        coord.v[2] < (parent.coord.v[2] + parent.coord.v[3]));
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
    /* Using the overall landscape shader program. */
    shaderProgram = NULL;
    transformLocation = afk_core.landscape->transformLocation;
    fixedColorLocation = afk_core.landscape->fixedColorLocation;

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
        glUseProgram(afk_core.landscape->shaderProgram->program);
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

    /* Set up the landscape shader. */
    shaderProgram = new AFK_ShaderProgram();
    *shaderProgram << "basic_fragment" << "basic_vertex";
    shaderProgram->Link();

    transformLocation = glGetUniformLocation(shaderProgram->program, "transform");
    fixedColorLocation = glGetUniformLocation(shaderProgram->program, "fixedColor");

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

    std::cout << "AFK: Landscape using minCellSize " << minCellSize << std::endl;

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
    landQueue.clear();

    delete shaderProgram;
}

float AFK_Landscape::getCellDetailPitch(const AFK_Cell& cell, const Vec3<float>& viewerLocation) const
{
    Vec4<float> realCoord = cell.realCoord();
    Vec3<float> realLocation = Vec3<float>(realCoord.v[0], realCoord.v[1], realCoord.v[2]);
    Vec3<float> realFacing = realLocation - viewerLocation;

    float distanceToViewer = realFacing.magnitude();
    return (float)afk_core.camera.windowHeight * realCoord.v[3] /
        (afk_core.camera.tanHalfFov * distanceToViewer);
}

bool AFK_Landscape::testCellDetailPitch(const AFK_Cell& cell, const Vec3<float>& viewerLocation) const
{
    /* At this point, it's sane to assume that I have a
     * configured camera.  I'm going to sample the cell twice,
     * at opposite vertices, and take the average distance from
     * those vertices to the viewer in order to decide its LoD.
     */
#if 0
    float cellDetailPitch1 = getCellDetailPitch(cell, viewerLocation);
    float cellDetailPitch2 = getCellDetailPitch(AFK_Cell(Vec4<int>(
        cell.coord.v[0] + cell.coord.v[3],
        cell.coord.v[1] + cell.coord.v[3],
        cell.coord.v[2] + cell.coord.v[3],
        cell.coord.v[3])), viewerLocation);
    float avgCellDetailPitch = (cellDetailPitch1 + cellDetailPitch2) / 2.0f;

    return avgCellDetailPitch < (float)detailPitch;
#else
    /* TODO Different plan -- I bet this fails if the camera
     * gets too close to the middle of a big cell (?!)
     * Sample only once, in the centre
     * TODO To fix it properly, I need to pick three points
     * displaced along the 3 axes by the dot pitch from the
     * centre of the cell, project them through the camera,
     * and compare those distances to the detail pitch,
     * no?
     * (in fact I'd probably get away with just the x and
     * z axes)
     */
    float cellDetailPitch = getCellDetailPitch(AFK_Cell(Vec4<int>(
        cell.coord.v[0] + cell.coord.v[3] / 2,
        cell.coord.v[1] + cell.coord.v[3] / 2,
        cell.coord.v[2] + cell.coord.v[3] / 2,
        cell.coord.v[3])), viewerLocation);
    return cellDetailPitch < (float)detailPitch;
#endif
}

void AFK_Landscape::testPointVisible(
        const Vec3<float>& point,
        const Vec3<float>& viewerLocation,
        const Vec3<float>& viewerFacing,
        bool& io_someVisible,
        bool& io_allVisible) const
{
    /* TODO Right now I'm just checking whether the
     * point is in the correct hemisphere.
     * Ideally, I should project the point with the
     * camera transform, and check using that.
     */
    Vec3<float> viewerToPoint = point - viewerLocation;
    bool visible = viewerToPoint.dot(viewerFacing) > 0.0f;

    io_someVisible |= visible;
    io_allVisible &= visible;
}

void AFK_Landscape::enqueueSubcells(
    const AFK_Cell& cell,
    const AFK_Cell& parent,
    const Vec3<float>& viewerLocation,
    const Vec3<float>& viewerFacing,
    bool parentEntirelyVisible)
{
    /* If this cell is outside the given parent, stop right
     * away.
     */
    if (!cell.isParent(parent)) return;

    /* Find out if it's enqueued already */
    std::pair<bool, AFK_LandscapeCell*>& enqueued = landQueue[cell];
    
    /* If we've seen it already, stop */
    if (enqueued.first) return;

    /* Otherwise, flag this cell as having been looked at
     * (regardless of whether we add a landscape, we don't
     * want to come back here)
     */
    enqueued.first = true;
    enqueued.second = NULL;

    bool entirelyVisible = parentEntirelyVisible;

    if (!parentEntirelyVisible)
    {
        /* Check whether this cell is actually visible, by
         * testing all 8 vertices.
         */
        bool someVisible = false;
        entirelyVisible = true;
        Vec4<float> realCoord = cell.realCoord();

        testPointVisible(Vec3<float>(
            realCoord.v[0],
            realCoord.v[1],
            realCoord.v[2]),
            viewerLocation, viewerFacing, someVisible, entirelyVisible);
        testPointVisible(Vec3<float>(
            realCoord.v[0] + realCoord.v[3],
            realCoord.v[1],
            realCoord.v[2]),
            viewerLocation, viewerFacing, someVisible, entirelyVisible);
        testPointVisible(Vec3<float>(
            realCoord.v[0],
            realCoord.v[1] + realCoord.v[3],
            realCoord.v[2]),
            viewerLocation, viewerFacing, someVisible, entirelyVisible);
        testPointVisible(Vec3<float>(
            realCoord.v[0] + realCoord.v[3],
            realCoord.v[1] + realCoord.v[3],
            realCoord.v[2]),
            viewerLocation, viewerFacing, someVisible, entirelyVisible);
        testPointVisible(Vec3<float>(
            realCoord.v[0],
            realCoord.v[1],
            realCoord.v[2] + realCoord.v[3]),
            viewerLocation, viewerFacing, someVisible, entirelyVisible);
        testPointVisible(Vec3<float>(
            realCoord.v[0] + realCoord.v[3],
            realCoord.v[1],
            realCoord.v[2] + realCoord.v[3]),
            viewerLocation, viewerFacing, someVisible, entirelyVisible);
        testPointVisible(Vec3<float>(
            realCoord.v[0],
            realCoord.v[1] + realCoord.v[3],
            realCoord.v[2] + realCoord.v[3]),
            viewerLocation, viewerFacing, someVisible, entirelyVisible);
        testPointVisible(Vec3<float>(
            realCoord.v[0] + realCoord.v[3],
            realCoord.v[1] + realCoord.v[3],
            realCoord.v[2] + realCoord.v[3]),
            viewerLocation, viewerFacing, someVisible, entirelyVisible);

        /* If it can't be seen at all, we can
         * drop out now.
         */
        if (!someVisible)
        {
            ++landscapeCellsInvisible;
            return;
        }
    }

    /* If it's the smallest possible cell, or its detail pitch
     * is at the target detail pitch, include it as-is
     */
    if (cell.coord.v[3] == 1 || testCellDetailPitch(cell, viewerLocation))
    {
        /* Find out if it's already in the cache */
        AFK_LandscapeCell*& landscapeCell = landMap[cell];
        if (!landscapeCell)
            landscapeCell = new AFK_LandscapeCell(cell, cell.realCoord());
        else
            ++landscapeCellsCached;

        /* Now, push it into the queue as well */
        enqueued.second = landscapeCell;
        ++landscapeCellsQueued;
        return;
    }

    /* Pull out the augmented set of subcells.  This set
     * includes all direct subcells and also the shell of
     * subcells one step + along each axis; I've done this
     * because I want to enqueue a cell if any of its
     * vertices are within fov, and a cell is described
     * by its lowest vertex (along each axis).
     */
    size_t augmentedSubcellsSize = CUBE(subdivisionFactor + 1);
    AFK_Cell *augmentedSubcells = new AFK_Cell[augmentedSubcellsSize];
    unsigned int augmentedSubcellsCount = cell.augmentedSubdivide(augmentedSubcells, augmentedSubcellsSize);

    if (augmentedSubcellsCount == augmentedSubcellsSize)
    {
        for (unsigned int i = 0; i < augmentedSubcellsCount; ++i)
        {
            /* Recurse down the subcells that fall strictly
             * within the parent cell.
             */
            enqueueSubcells(AFK_Cell(Vec4<int>(
                augmentedSubcells[i].coord.v[0] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[1] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[2] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[3])), cell, viewerLocation, viewerFacing, entirelyVisible);
            enqueueSubcells(AFK_Cell(Vec4<int>(
                augmentedSubcells[i].coord.v[0] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[1] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[2],
                augmentedSubcells[i].coord.v[3])), cell, viewerLocation, viewerFacing, entirelyVisible);
            enqueueSubcells(AFK_Cell(Vec4<int>(
                augmentedSubcells[i].coord.v[0] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[1],
                augmentedSubcells[i].coord.v[2] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[3])), cell, viewerLocation, viewerFacing, entirelyVisible);
            enqueueSubcells(AFK_Cell(Vec4<int>(
                augmentedSubcells[i].coord.v[0] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[1],
                augmentedSubcells[i].coord.v[2],
                augmentedSubcells[i].coord.v[3])), cell, viewerLocation, viewerFacing, entirelyVisible);
            enqueueSubcells(AFK_Cell(Vec4<int>(
                augmentedSubcells[i].coord.v[0],
                augmentedSubcells[i].coord.v[1] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[2] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[3])), cell, viewerLocation, viewerFacing, entirelyVisible);
            enqueueSubcells(AFK_Cell(Vec4<int>(
                augmentedSubcells[i].coord.v[0],
                augmentedSubcells[i].coord.v[1] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[2],
                augmentedSubcells[i].coord.v[3])), cell, viewerLocation, viewerFacing, entirelyVisible);
            enqueueSubcells(AFK_Cell(Vec4<int>(
                augmentedSubcells[i].coord.v[0],
                augmentedSubcells[i].coord.v[1],
                augmentedSubcells[i].coord.v[2] - augmentedSubcells[i].coord.v[3],
                augmentedSubcells[i].coord.v[3])), cell, viewerLocation, viewerFacing, entirelyVisible);
            enqueueSubcells(augmentedSubcells[i], cell, viewerLocation, viewerFacing, entirelyVisible);
        }
    }
    else
    {
        /* That's clearly a bug :P */
        std::ostringstream ss;
        ss << "Cell " << cell << " subdivided into " << augmentedSubcellsCount << " not " << augmentedSubcellsSize;
        throw AFK_Exception(ss.str());
    }

    delete[] augmentedSubcells;
}

void AFK_Landscape::enqueueSubcells(
    const AFK_Cell& cell,
    const Vec3<int>& modifier,
    const Vec3<float>& viewerLocation,
    const Vec3<float>& viewerFacing)
{
    AFK_Cell modifiedCell(Vec4<int>(
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[0],
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[1],
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[2],
        cell.coord.v[3]));
    enqueueSubcells(modifiedCell, modifiedCell.parent(), viewerLocation, viewerFacing, false);
}

void AFK_Landscape::updateLandMap(void)
{
    /* Do the statistics thing for this pass. */
    landscapeCellsInvisible = 0;
    landscapeCellsCached = 0;
    landscapeCellsQueued = 0;

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

    /* Wind up the cell tree and find its largest parent. */
    AFK_Cell cell;
    for (cell = protagonistCell;
        (float)cell.coord.v[3] < maxDistance;
        cell = cell.parent());

    /* Draw that cell and the other cells around it.
     * TODO Can I optimise this?  It's going to attempt
     * cells that are very very far away.  Mind you,
     * that might be okay.
     */
    for (int i = -1; i <= 1; ++i)
        for (int j = -1; j <= 1; ++j)
            for (int k = -1; k <= 1; ++k)
                enqueueSubcells(cell, Vec3<int>(i, j, k), protagonistLocation, protagonistFacing);

#ifdef PROTAGONIST_CELL_DEBUG
    {
        std::ostringstream ss;
        ss << "Queued " << landscapeCellsQueued << " cells (" <<
            (100 * landscapeCellsCached / landscapeCellsQueued) << "\% cached, " <<
            landscapeCellsInvisible << " rejected for invisibility)";
        afk_core.occasionallyPrint(ss.str());
    }
#endif
}

void AFK_Landscape::display(const Mat4<float>& projection)
{
    for (boost::unordered_map<const AFK_Cell, std::pair<bool, AFK_LandscapeCell*> >::iterator lqIt = landQueue.begin(); lqIt != landQueue.end(); ++lqIt)
        if (lqIt->second.second) lqIt->second.second->display(projection);

    landQueue.clear();
}

