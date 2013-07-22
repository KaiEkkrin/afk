/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DISPLAYED_LANDSCAPE_TILE_H_
#define _AFK_DISPLAYED_LANDSCAPE_TILE_H_

#include "afk.hpp"

#include <boost/shared_ptr.hpp>

#include "def.hpp"
#include "landscape_tile.hpp"
#include "light.hpp"
#include "shader.hpp"

/* Enables a render of a pre-prepared landscape tile, as fitting into
 * one world cell.
 * This object is not a DisplayedObject!  It doesn't fit.  The landscape
 * is entirely in world space, so there is no movement matrix to
 * track.
 */
class AFK_DisplayedLandscapeTile
{
protected:
    /* This comes from LandscapeTile.  I don't own it. */
    AFK_LandscapeGeometry **geometry;

    float cellBoundLower;
    float cellBoundUpper;

public:
    /* The LandscapeTile sets up a DisplayedLandscapeTile by sharing
     * its buffer pointers.
     */
    AFK_DisplayedLandscapeTile(
        AFK_LandscapeGeometry **_geometry,
        float _cellBoundLower,
        float _cellBoundUpper);

    /* The world display step initialises this object with OpenGL
     * (if necessary) by calling this.
     * Calling it again doesn't do anything bad.
     */
    void initGL(void);

    /* This makes the GL render calls.  The landscape shader
     * program is supplied here (there is a single one for
     * the whole landscape).
     */
    void display(
        AFK_ShaderProgram *shaderProgram,
        AFK_ShaderLight *shaderLight,
        const struct AFK_Light& globalLight,
        GLuint clipTransformLocation,
        GLuint yCellMinLocation,
        GLuint yCellMaxLocation,
        const Mat4<float>& projection);
};

#endif /* _AFK_DISPLAYED_LANDSCAPE_TILE_H_ */

