#version 330 core
// Arena Limpia - Phong básico (fragmentos)
// Una luz direccional (el sol) con componentes ambiental, difusa y especular.
// Si la malla no tiene textura difusa se usa el color Kd del material (.mtl).
// Segunda técnica: luz puntual (la fogata) con atenuación por distancia (práctica 8).

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D texture_diffuse1;
uniform bool  hasDiffuseTexture;
uniform vec4  meshDiffuseColor;
uniform vec2  uvScale;

uniform vec3  viewPos;
uniform vec3  sunDirection;   // dirección hacia donde viaja la luz
uniform vec3  sunColor;
uniform float ambientStrength;
uniform float specularStrength;
uniform float shininess;
uniform float emission;       // 0 = normal; > 0 = la superficie brilla con su propio color (fuego)

// Luz puntual: la fogata (si pointColor es negro no aporta nada)
uniform vec3  pointPosition;
uniform vec3  pointColor;

void main()
{
    vec4 base = hasDiffuseTexture ? texture(texture_diffuse1, TexCoords * uvScale)
                                  : meshDiffuseColor;
    if (base.a < 0.1)
        discard;

    vec3 n = normalize(Normal);
    vec3 l = normalize(-sunDirection);
    vec3 v = normalize(viewPos - FragPos);

    // Iluminar ambas caras: varias mallas de Blender son planos de una sola cara
    if (dot(n, v) < 0.0)
        n = -n;

    vec3 ambient  = ambientStrength * sunColor;
    vec3 diffuse  = max(dot(n, l), 0.0) * sunColor;
    vec3 r        = reflect(-l, n);
    vec3 specular = specularStrength * pow(max(dot(v, r), 0.0), shininess) * sunColor;

    // Luz puntual con atenuación: entre más lejos de la fogata, menos luz
    vec3  lp    = pointPosition - FragPos;
    float d     = length(lp);
    vec3  lpDir = lp / max(d, 0.0001);
    float att   = 1.0 / (1.0 + 0.35 * d + 0.44 * d * d);
    vec3  rp    = reflect(-lpDir, n);
    diffuse  += att * max(dot(n, lpDir), 0.0) * pointColor;
    specular += att * specularStrength * pow(max(dot(v, rp), 0.0), shininess) * pointColor;

    vec3 lit = (ambient + diffuse) * base.rgb + specular;
    FragColor = vec4(lit + emission * base.rgb, base.a);
}