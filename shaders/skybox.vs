#version 330 core
// Arena Limpia - Skybox (basado en 10_vertex_cubemap.vs del curso)

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;   // sin traslación: el cielo se mueve con la cámara

void main()
{
    TexCoords = aPos;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
