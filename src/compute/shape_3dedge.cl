/* AFK (c) Alex Holloway 2013 */

/* This program computes a 3D shape by accumulating a 3D
 * number field and tracing the edge of some particular
 * value, rendering this trace into a displacement
 * texture.
 * This program replaces `shape_shrinkform' and its
 * ancillaries in the 3dedgeshape branch.  I'm hoping it
 * turns out to work better and can be integrated into
 * master and become the definitive random-shape device.
 */

/* TODO: For now the comments are all about planning
 * the design of this compute kernel, which is a bit more
 * complicated than the ones I've made before.
 */

/* It looks like `jigsawDisp' and `jigsawColour' will
 * each consist of six squares in a row, corresponding
 * to the six faces of the cube.
 * I also expect that `jigsawDisp' can be a one-channel
 * image again, displacing only the co-ordinate that is
 * all-zero in the base geometry (y for bottom and top,
 * z for front and back, x for left and right).
 */

/* TODO: Do I want to split this kernel into two, with
 * the first writing an image3d_t number volume, and
 * the second one reading it to do the edge detection
 * and face reduction?
 * (Depends whether I can retain the number volume in
 * local memory reasonably or not.)
 */

/* Note: The laptop GPU claims about 48K of local
 * memory, which should be considered about the
 * usual amount.
 * An 11x11x11 cube is 1331 units; one float per
 * unit would make 5324 bytes.  Which _ought_ to fit;
 * (including the colour as well would double that
 * requirement; which would mean fewer wavefronts
 * scheduled, which would mess up latency hiding.)
 * I'm going to need local memory for the reduce
 * operations as well (actually *need* as opposed to just
 * *would like*), so I think I'm best off using global
 * images for the number volumes, and two kernels,
 * using local memory in the reduce kernel to pull out
 * the displacements.
 */

__kernel void makeShape3dedge(
    __global const struct AFK_3DEdgeFeature *features,
    __global const struct AFK_3DEdgeCube *cubes,
    __global const struct AFK_3DEdgeComputeUnit *units,
    __write_only image2d_t jigsawDisp,
    __write_only image2d_t jigsawColour)
{
    /* We're necessarily going to operate across the
     * three dimensions of a cube.
     * The first dimension should be multiplied up by
     * the unit offset, like so.
     */
    const int unitOffset = get_global_id(0) / TDIM;
    const int xdim = get_global_id(0) % TDIM;
    const int ydim = get_global_id(1);
    const int zdim = get_global_id(2);

    /* First of all, compute the number field by 
     * iterating across all of the cubes and features.
     */

    /* Next, edge-detect by comparing my point in the
     * number grid with the other three points to its
     * top, right and back.
     * (The top and right sides are exempt from this.)
     * This comparison should give me one or more faces
     * complete with information about which direction
     * they are facing in.
     */

    /* Thirdly, reduce in turn across each of the six
     * potential faces that could contain my edge-
     * detected geometry: bottom, top, left, right,
     * front, back.  The edge closest to a face
     * becomes the one that influences its geometry
     * at the relevant 2D points (and also its colour
     * there.)
     * The worker closest to the base face in the
     * reduction (the one at 0 or TDIM) will be responsible
     * for writing the displacement into the image.
     */

    /* Each time a point has "won" and been used for one
     * edge, it is removed from the detection so it can't
     * be used for another one.  This should allow
     * diagonal sides that were occluded in one direction
     * but not the other to influence the geometry of the
     * second one.
     */

    /* Fourthly: `surface' isn't going to work for these
     * shapes, not unless I put lots of overlap around all
     * the edges (fail).  However, the number field gives
     * me lots of information to make lovely normals,
     * by using the numbers themselves to compute a more
     * detailed slant.  Consider how I'd implement this.
     */
}

