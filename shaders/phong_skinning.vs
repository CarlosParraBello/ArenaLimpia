#version 330 core
// Arena Limpia - Phong con skinning (vértices)
// Igual que phong.vs, pero antes deforma cada vértice con hasta 4 huesos.
// Se usa con phong.fs, así los personajes animados tienen la misma luz que el resto.

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 5) in vec4 bIDs1;
layout (location = 8) in vec4 bWeights1;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];

void main()
{
    mat4 skin = gBones[int(bIDs1.x)] * bWeights1.x
              + gBones[int(bIDs1.y)] * bWeights1.y
              + gBones[int(bIDs1.z)] * bWeights1.z
              + gBones[int(bIDs1.w)] * bWeights1.w;
    // Vértices sin pesos se quedan donde están
    float total = bWeights1.x + bWeights1.y + bWeights1.z + bWeights1.w;
    if (total < 0.001)
        skin = mat4(1.0);

    vec4 worldPos = model * skin * vec4(aPos, 1.0);
    FragPos   = worldPos.xyz;
    Normal    = mat3(transpose(inverse(model * skin))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * worldPos;
}
