// Vertex colour and ambient and diffuse lighting.

#version 330

in vec3 VcolF;
in vec3 NormalF;

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
    vec3 AmbientColour = gLight.Colour * gLight.Ambient;
    vec3 DiffuseColour = gLight.Colour * gLight.Diffuse * max(dot(normalize(NormalF), -gLight.Direction), 0.0);
    float DiffuseFactor = dot(normalize(NormalF), -gLight.Direction);
    gl_FragColor = vec4(VcolF * (AmbientColour + DiffuseColour * DiffuseFactor), 1.0);
}

