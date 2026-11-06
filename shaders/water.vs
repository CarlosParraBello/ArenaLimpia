#version 330 core
// Arena Limpia - Olas del mar (vértices)
// La malla es una cuadrícula plana en x-z (coordenadas de mundo). Cada cuadro se calcula la
// altura de cada vértice con una suma de senos (ondas que viajan hacia la playa).
// Las mismas ondas se evalúan en el CPU (seaHeight en main.cpp) para que el bote flote.

layout (location = 0) in vec2 aPos;   // (x, z) en metros

out vec3  FragPos;
out float Shore;    // 0 en la orilla, 1 mar adentro

uniform mat4 view;
uniform mat4 projection;

const int WAVES = 4;
uniform vec2  waveDir[WAVES];     // dirección (unitaria) hacia donde viaja la ola
uniform vec3  waveParam[WAVES];   // (número de onda k, amplitud A, fase)
uniform float waveTime;
uniform float gravity;
uniform float shoreZ;             // z de la orilla; el mar está en z < shoreZ
uniform float shoreRamp;          // metros en los que las olas crecen desde la orilla
uniform float seaLevel;           // altura del agua en la orilla
uniform float seaRise;            // cuánto sube el nivel mar adentro

void main()
{
    float d = shoreZ - aPos.y;                     // distancia a la orilla
    float s = smoothstep(0.0, shoreRamp, d);
    float h = 0.0;
    for (int i = 0; i < WAVES; i++) {
        float k = waveParam[i].x;
        float w = sqrt(gravity * k);               // relación de dispersión en agua profunda
        h += waveParam[i].y * sin(k * dot(waveDir[i], aPos) - w * waveTime + waveParam[i].z);
    }
    vec3 world = vec3(aPos.x, seaLevel + s * (seaRise + h), aPos.y);
    FragPos = world;
    Shore = s;
    gl_Position = projection * view * vec4(world, 1.0);
}
