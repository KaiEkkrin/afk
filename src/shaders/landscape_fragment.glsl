// AFK (c) Alex Holloway 2013
//
// Landscape colour and lighting.

#version 330

// This is the jigsaw colour texture.
uniform sampler2D JigsawColourTex;

// ...and the normal
uniform sampler2D JigsawNormalTex;

// The sky colour, which I fade out to at long distance.
uniform vec3 SkyColour;

// The far clip plane.  (I don't think I need to worry about the near
// one for fade-to-sky.)
uniform float FarClipDistance;

in GeometryData
{
    vec2 jigsawCoord;
} inData;

struct Light
{
    vec3 Colour;
    vec3 Direction; 
    float Ambient;
    float Diffuse;
};

uniform Light gLight;

void main()
{
    vec3 colour = textureLod(JigsawColourTex, inData.jigsawCoord, 0).xyz;
    vec3 normal = textureLod(JigsawNormalTex, inData.jigsawCoord, 0).xyz;

    vec3 AmbientColour = gLight.Colour * gLight.Ambient;
    vec3 DiffuseColour = gLight.Colour * gLight.Diffuse * max(dot(normal, -gLight.Direction), 0.0);
    float DiffuseFactor = dot(normal, -gLight.Direction);
    vec3 CombinedColour = colour * (AmbientColour + DiffuseColour * DiffuseFactor);

    float depth = (gl_FragCoord.z / gl_FragCoord.w) / FarClipDistance;
    gl_FragColor = mix(
        vec4(CombinedColour, 1.0),
        vec4(SkyColour, 1.0),
        depth);
}

