/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_TERRAIN_H_
#define _AFK_TERRAIN_H_

/* Terrain-describing functions. */

/* TODO: Feature computation is an obvious candidate for
 * doing in a geometry shader, or in OpenCL.  Consider
 * this.  I'd need to build a long queue of features to
 * be computed, I guess.
 */

#include "def.hpp"

/* The list of possible terrain features.
 * TODO: Include some better ones!
 */
enum AFK_TerrainType
{
    AFK_TERRAIN_SQUARE_PYRAMID          = 0
};

/* TODO Keep this updated with the current feature
 * count.
 */
#define AFK_TERRAIN_FEATURE_TYPES 1

/* The encapsulation of any terrain feature.
 * A feature is computed at a particular location
 * (in world co-ordinates) based on the (x,y,z)
 * position that I want to compute it at, and
 * returns a displaced y co-ordinate for that position.
 *
 * I'm doing it like this so that I can pre-allocate
 * a single vector for all landscape terrain in the
 * landscape module, and don't end up thrashing the
 * heap making new objects for each feature.  This
 * restricts all features to the same parameters,
 * but I think that's okay.
 *
 * TODO:
 * Right now the feature is not allowed to displace
 * the x or z co-ordinates, only the y co-ordinate.
 * Clearly features for objects (??) would displace
 * all 3, but that's incompatible with the current
 * test for remaining within cell boundaries.
 * Do I want a different system for moving objects
 * (not cell restricted in the same way)?
 * Have a think about this.
 */
class AFK_TerrainFeature
{
protected:
    enum AFK_TerrainType    type;
    Vec3<float>             location;
    Vec3<float>             scale;

    /* The methods for computing each individual
     * terrain type.
     */
    float compute_squarePyramid(const Vec3<float>& c) const;

public:
    AFK_TerrainFeature() {}
    AFK_TerrainFeature(const AFK_TerrainFeature& f);
    AFK_TerrainFeature(
        const enum AFK_TerrainType _type,
        const Vec3<float>& _location,
        const Vec3<float>& _scale);

    AFK_TerrainFeature& operator=(const AFK_TerrainFeature& f);

    float compute(const Vec3<float>& c) const;
};

#endif /* _AFK_TERRAIN_H_ */

