// AFK (c) Alex Holloway 2013
//
// Shape vertex colour and ambient and diffuse lighting.

#version 330

// TODO Jigsaw colour and normal textures should appear here.

in VertexData
{
    vec2 jigsawCoord;

    // TODO Temporary, as per shape_vertex.
    vec2 tempColourRG;
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
    //vec3 AmbientColour = gLight.Colour * gLight.Ambient;
    //vec3 DiffuseColour = gLight.Colour * gLight.Diffuse * max(dot(normalize(NormalF), -gLight.Direction), 0.0);
    //float DiffuseFactor = dot(normalize(NormalF), -gLight.Direction);
    //FragColor = vec4(VcolF * (AmbientColour + DiffuseColour), 1.0);
    FragColor = vec4(tempColourRG.x, tempColourRG.y, 0.0, 1.0);
}

