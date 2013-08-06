// AFK (c) Alex Holloway 2013
//
// Vertex colour and ambient and diffuse lighting for the landscape.
// It accepts a GeometryData as output by the landscape geometry
// shader.

#version 330

// This is the jigsaw.  It's a float4: (3 colours, y displacement).
uniform sampler2D JigsawTex;

in GeometryData
{
    vec3 normal;
    vec2 jigsawCoord;
} inData;

out vec4 FragColor;

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
    vec4 jigsawTexel = textureLod(JigsawTex, inData.jigsawCoord, 0);
    vec3 colour = normalize(jigsawTexel.xyz);
    vec3 normal = normalize(inData.normal);

    vec3 AmbientColour = gLight.Colour * gLight.Ambient;
    vec3 DiffuseColour = gLight.Colour * gLight.Diffuse * max(dot(normal, -gLight.Direction), 0.0);
    float DiffuseFactor = dot(normal, -gLight.Direction);
    // TODO fix this when I'm getting all the properties
    // in once more.
    FragColor = vec4(colour * (AmbientColour + DiffuseColour), 1.0);
}

