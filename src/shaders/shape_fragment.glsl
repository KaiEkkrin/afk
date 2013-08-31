// AFK (c) Alex Holloway 2013
//
// Shape vertex colour and ambient and diffuse lighting.

#version 330

// This is the jigsaw colour texture.
uniform sampler2D JigsawColourTex;

// ...and the normal
uniform sampler2D JigsawNormalTex;

in GeometryData
{
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
    /* TODO This normal is for the orientation of the base face -- we
     * need to rotate it to the object's current orientation!
     */
    vec3 normal = textureLod(JigsawNormalTex, inData.jigsawCoord, 0).xyz;

    vec3 AmbientColour = gLight.Colour * gLight.Ambient;
    vec3 DiffuseColour = gLight.Colour * gLight.Diffuse * max(dot(normal, -gLight.Direction), 0.0);
    float DiffuseFactor = dot(normal, -gLight.Direction);
    FragColor = vec4(colour * (AmbientColour + DiffuseColour), 1.0);
}

