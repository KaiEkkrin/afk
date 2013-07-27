// AFK (c) Alex Holloway 2013
//
// Shape vertex shader.  Looks up the world transform of each instance
// in a texture buffer.

#version 330

layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Vcol;
layout (location = 2) in vec3 Normal;

uniform samplerBuffer WorldTransformTBO;

uniform mat4 ProjectionTransform;

out vec3 VcolF;
out vec3 NormalF;

void main()
{
    // Reconstruct the world transform matrix for this instance.
    // Of course, AFK is row-major...  :/
    vec4 WTRow1 = texelFetch(WorldTransformTBO, gl_InstanceID * 4);
    vec4 WTRow2 = texelFetch(WorldTransformTBO, gl_InstanceID * 4 + 1);
    vec4 WTRow3 = texelFetch(WorldTransformTBO, gl_InstanceID * 4 + 2);
    vec4 WTRow4 = texelFetch(WorldTransformTBO, gl_InstanceID * 4 + 3);

    mat4 WorldTransform = mat4(
        vec4(WTRow1.x, WTRow2.x, WTRow3.x, WTRow4.x),
        vec4(WTRow1.y, WTRow2.y, WTRow3.y, WTRow4.y),
        vec4(WTRow1.z, WTRow2.z, WTRow3.z, WTRow4.z),
        vec4(WTRow1.w, WTRow2.w, WTRow3.w, WTRow4.w));

    gl_Position = (ProjectionTransform * WorldTransform) * vec4(Position, 1.0);
    VcolF = Vcol;
    NormalF = (WorldTransform * vec4(Normal, 0.0)).xyz;
}

