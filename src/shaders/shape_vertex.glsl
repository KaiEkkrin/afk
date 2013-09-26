// AFK (c) Alex Holloway 2013
//
// Shape vertex shader.  Looks up the world transform of each instance
// in a texture buffer.

#version 400

layout (location = 0) in vec2 TexCoord;

out VertexData
{
    int instanceId; /* Because gl_InstanceID isn't accessible in a
                     * geometry shader
                     */
    vec2 texCoord;
} outData;

void main()
{
    /* All the hard work is now done by the geometry shader.  This
     * vertex shader is a passthrough only.
     */
    outData.instanceId = gl_InstanceID;
    outData.texCoord = TexCoord.st;
}

