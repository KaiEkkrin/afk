// Vertex colour and ambient and diffuse lighting for the landscape.
// It accepts a GeometryData as output by the landscape geometry
// shader.

#version 330

in GeometryData
{
    vec3 colour;
    vec3 normal;
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
    vec3 colour = normalize(inData.colour);
    vec3 normal = normalize(inData.normal);

    vec3 AmbientColour = gLight.Colour * gLight.Ambient;
    vec3 DiffuseColour = gLight.Colour * gLight.Diffuse * max(dot(normal, -gLight.Direction), 0.0);
    float DiffuseFactor = dot(normal, -gLight.Direction);
    FragColor = vec4(colour * (AmbientColour + DiffuseColour), 1.0);
}

