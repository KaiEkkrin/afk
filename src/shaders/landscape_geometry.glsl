// This is the landscape geometry shader.
// It accepts VertexData as produced by the landscape vertex
// shader and culls triangles that are outside of the
// world cell boundaries.
// TODO: Can I come up with a cunning data format to pass into
// this geometry shader, such that it can create the stitch
// triangles where needed (in the places where I'm crossing a
// level of detail)?
// Because maintaining stitch structures on the CPU side would
// be an embuggerance...
// For now, though, I'm going to continue to produce triangles on
// the CPU side and just use this shader to cull outside-of-cell
// triangles.

#version 330

layout (triangles) in;
layout (triangle_strip) out;
layout (max_vertices = 3) out;

in VertexData
{
    vec3 colour;
    vec3 normal;
} inData[];

uniform mat4 ClipTransform;

uniform float yCellMin;
uniform float yCellMax;

out GeometryData
{
    vec3 colour;
    vec3 normal;
} outData;

void main()
{
    // Cell is determined by the first vertex of each triangle.
    if (gl_in[0].gl_Position.y >= yCellMin && gl_in[0].gl_Position.y < yCellMax)
    {
        int i;
        for (i = 0; i < 3; ++i)
        {
            gl_Position = ClipTransform * gl_in[i].gl_Position;
            outData.colour = inData[i].colour;
            outData.normal = inData[i].normal;
            EmitVertex();
        }
        EndPrimitive();
    }
}

