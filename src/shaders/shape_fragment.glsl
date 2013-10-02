// AFK (c) Alex Holloway 2013
//
// Shape vertex colour and ambient and diffuse lighting.

#version 400

// This is the jigsaw colour texture.
uniform sampler2D JigsawColourTex;

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
    vec3 colour = textureLod(JigsawColourTex, inData.jigsawCoord, 0).xyz;
    vec3 normal = normalize(inData.normal);

    vec3 AmbientColour = gLight.Colour * gLight.Ambient;
    vec3 DiffuseColour = gLight.Colour * gLight.Diffuse * max(dot(normal, -gLight.Direction), 0.0);
    float DiffuseFactor = dot(normal, -gLight.Direction);
    FragColor = vec4(colour * (AmbientColour + DiffuseColour), 1.0);
}

