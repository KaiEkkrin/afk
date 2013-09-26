// AFK (c) Alex Holloway 2013
//
// Shape geometry shader.
// The point of this right now is to cull triangles that shouldn't
// exist (the edge kernel will have written NAN to those vertices.)

#version 330

layout (lines_adjacency) in;
layout (triangle_strip) out;
layout (max_vertices = 4) out;

in VertexData
{
    vec3 normal;
    vec2 jigsawCoord;
} inData[];

out GeometryData
{
    vec3 normal;
    vec2 jigsawCoord;
} outData;

void main()
{
    /* TODO Remove the nan check and replace it with a lookup
     * of the overlap texture.
     */
    bool haveNan = false;
    for (int i = 0; i < 4; ++i)
    {
        if (isnan(gl_in[i].gl_Position.x) ||
            isnan(gl_in[i].gl_Position.y) ||
            isnan(gl_in[i].gl_Position.z) ||
            isnan(gl_in[i].gl_Position.w))
        {
            haveNan = true;
        }
    }

    if (!haveNan)
    {
        for (int i = 0; i < 4; ++i)
        {
            gl_Position = gl_in[i].gl_Position;
            outData.normal = inData[i].normal;
            outData.jigsawCoord = inData[i].jigsawCoord;
            EmitVertex();
        }

        EndPrimitive();
    }
}

