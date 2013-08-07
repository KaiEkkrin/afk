// AFK (c) Alex Holloway 2013
//
// This is the landscape geometry shader.
// It accepts VertexData as produced by the landscape vertex
// shader and culls triangles that are outside of the
// world cell boundaries.
// TODO Adapt this to accept GL_TRIANGLES_ADJACENCY, and
// calculate the correct normal for each vertex as well.
// TODO *2: Stop the nasty stitching when two different LoDs
// cross at a y boundary by splitting the triangle into two
// here, each stopping at said y boundary.
// TODO *3: Throw away the current base geometry entirely,
// and use the tessellator!

#version 330

layout (triangles) in;
layout (triangle_strip) out;
layout (max_vertices = 3) out;

in VertexData
{
    vec2 jigsawCoord;
    bool withinBounds;
} inData[];

uniform mat4 ClipTransform;

out GeometryData
{
    vec3 normal;
    vec2 jigsawCoord;
} outData;

void main()
{
    if (inData[0].withinBounds || inData[1].withinBounds || inData[2].withinBounds)
    {
        int i;
        for (i = 0; i < 3; ++i)
        {
            gl_Position = ClipTransform * gl_in[i].gl_Position;
            outData.normal = vec3(0.0, 1.0, 0.0); // TODO Needs fixing :P
            outData.jigsawCoord = inData[i].jigsawCoord;
            EmitVertex();
        }
        EndPrimitive();
    }
}

