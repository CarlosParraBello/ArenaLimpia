#version 330 core
// Arena Limpia - Skybox (basado en 10_fragment_cubemap.fs del curso)

out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{
    FragColor = texture(skybox, TexCoords);
}
