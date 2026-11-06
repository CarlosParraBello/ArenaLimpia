#version 330 core
// Arena Limpia - Olas del mar (fragmentos)
// La normal se calcula en cada fragmento con la derivada exacta de la suma de senos, así el
// brillo del sol se ve detallado aunque la cuadrícula sea gruesa:
//   h  = sum A sin(k d.p - w t + phi)
//   dh/dx = sum A k d.x cos(...),  dh/dz = sum A k d.z cos(...),  n = normalize(-dh/dx, 1, -dh/dz)
// Color: agua clara en la orilla y más profunda mar adentro; en ángulos rasantes refleja más
// el cielo (aproximación de Fresnel). En la orilla se vuelve transparente y tiene espuma.

out vec4 FragColor;

in vec3  FragPos;
in float Shore;

const int WAVES = 4;
uniform vec2  waveDir[WAVES];
uniform vec3  waveParam[WAVES];
uniform float waveTime;
uniform float gravity;
uniform float shoreZ;

uniform vec3  viewPos;
uniform vec3  sunDirection;
uniform vec3  sunColor;

const vec3 SHALLOW = vec3(0.30, 0.72, 0.74);
const vec3 DEEP    = vec3(0.04, 0.32, 0.48);
const vec3 SKY     = vec3(0.62, 0.80, 0.95);

void main()
{
    vec2 p = FragPos.xz;
    vec2 grad = vec2(0.0);
    for (int i = 0; i < WAVES; i++) {
        float k = waveParam[i].x;
        float w = sqrt(gravity * k);
        float c = cos(k * dot(waveDir[i], p) - w * waveTime + waveParam[i].z);
        grad += waveParam[i].y * k * c * waveDir[i];
    }
    grad *= Shore;
    vec3 n = normalize(vec3(-grad.x, 1.0, -grad.y));
    vec3 l = normalize(-sunDirection);
    vec3 v = normalize(viewPos - FragPos);

    vec3 water = mix(SHALLOW, DEEP, Shore);
    float diffuse = max(dot(n, l), 0.0);
    vec3 color = (0.45 + 0.55 * diffuse) * water * sunColor;

    float fresnel = pow(1.0 - max(dot(n, v), 0.0), 4.0);
    color = mix(color, SKY, 0.6 * fresnel);

    vec3 r = reflect(-l, n);
    color += 0.9 * pow(max(dot(v, r), 0.0), 160.0) * sunColor;

    // Espuma: una franja blanca en la orilla que avanza y regresa con las olas
    float d = shoreZ - FragPos.z;
    float foamLine = 0.8 + 0.6 * sin(1.3 * waveTime + 0.35 * FragPos.x);
    float foam = 1.0 - smoothstep(0.0, 0.8, abs(d - foamLine));
    color = mix(color, vec3(0.95), 0.7 * foam);

    float alpha = mix(0.0, 0.88, smoothstep(0.0, 2.0, d)) + 0.6 * foam * smoothstep(0.0, 0.4, d);
    FragColor = vec4(color, clamp(alpha, 0.0, 0.95));
}
