// AFK (c) Alex Holloway 2013
//
// This is the landscape geometry shader.
// It accepts VertexData as produced by the landscape vertex
// shader and culls triangles that are outside of the
// world cell boundaries.
// TODO: Stop the nasty stitching when two different LoDs
// cross at a y boundary by splitting the triangle into two
// here, each stopping at said y boundary.

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
    vec2 jigsawCoord;
} outData;

void emitTriangle(int one, int two, int three)
{
    gl_Position = ClipTransform * gl_in[one].gl_Position;
    outData.jigsawCoord = inData[one].jigsawCoord;
    EmitVertex();

    gl_Position = ClipTransform * gl_in[two].gl_Position;
    outData.jigsawCoord = inData[two].jigsawCoord;
    EmitVertex();

    gl_Position = ClipTransform * gl_in[three].gl_Position;
    outData.jigsawCoord = inData[three].jigsawCoord;
    EmitVertex();
}

void main()
{
    if (inData[0].withinBounds || inData[1].withinBounds || inData[2].withinBounds)
    {
        // Fix the winding order.
        vec3 crossP = cross(gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz,
            gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz);

        if (crossP.y >= 0.0)
        {
            emitTriangle(0, 2, 1);
        }
        else
        {
            emitTriangle(0, 1, 2);
        }

        EndPrimitive();
    }
}

