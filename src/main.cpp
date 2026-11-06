/*
* Arena Limpia - Torneo de Voleibol de Playa
* Proyecto final del Laboratorio de Computación Gráfica e Interacción Humano-Computadora
* Facultad de Ingeniería, UNAM. Semestre 2027-1.
*
* Proyecto base: ventana OpenGL 3.3 core, cámara libre, skybox, piso de arena,
* iluminación de Phong con una luz direccional y carga de modelos OBJ con Assimp.
*
* Controles:
*   V              cambiar de cámara: libre / tercera persona
*
*   Cámara libre:
*   W A S D        mover la cámara
*   Espacio / C    subir / bajar
*   Shift          moverse más rápido
*   Mouse          mirar alrededor
*   Rueda          zoom
*
*   Tercera persona:
*   W A S D        caminar con el jugador (relativo a la cámara)
*   Shift          correr
*   Mouse          girar la cámara alrededor del jugador
*   Rueda          acercar / alejar
*   1 2 3 4        elegir jugador (MX, PR, HA, JA)
*   Q / E / F      saque / remate / festejo
*
*   Generales:
*   Tab            mostrar / ocultar el cursor
*   M / N          malla de alambre / relleno
*   F12            captura de pantalla en docs/capturas/
*   P              partido automático 2 contra 2 (MX y PR contra HA y JA)
*   L              día / noche (de noche se nota la luz de la fogata)
*
*   Interacción con 15 objetos (cámara libre o tercera persona):
*   R              seleccionar el siguiente objeto (el seleccionado brilla)
*   G              tomar / soltar el objeto (solo cámara libre)
*   Flechas        mover el objeto en el piso
*   RePág / AvPág  subir / bajar el objeto
*   Z / X          rotar el objeto
*   + / -          escalar el objeto
*   Retroceso      regresar el objeto a como estaba
*   Esc            salir
*
* Argumentos opcionales (para pruebas):
*   --captura <archivo.bmp>   dibuja unos cuadros, guarda la imagen y termina
*   --camara x y z yaw pitch  posición y orientación inicial de la cámara
*   --tercera [1-4]           empieza en tercera persona siguiendo a ese jugador
*   --tiempo <s>              empieza la simulación en el segundo s (viento, fuego)
*   --simular <Saque|Remate> <s>  el jugador seguido hace la acción y se adelantan s segundos
*   --simular Partido <s>     inicia el partido automático y adelanta s segundos
*   --seleccionar <objeto>    empieza con ese objeto interactivo seleccionado
*   --pose <anim> <s>         fija la animación <anim> en el segundo s en los personajes que la tengan
*/

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <ctime>
#include <cfloat>

#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <shader_m.h>
#include <camera.h>
#include <model.h>
#include <cubemap.h>
#include <skinnedmodel.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Declaraciones
// ---------------------------------------------------------------------------
bool Start();
bool Update();
void Shutdown();
void processInput(GLFWwindow* window);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
bool findAssetsRoot();
enum class CameraMode;
void setCameraMode(CameraMode mode);
bool saveScreenshot(const std::string& path);
void runSimulationTest();
void toggleAutoMatch();
void initBall();
void initPalm();
void initSimpleAnims();
void initInteraction();
void initSea();
void drawSea(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& eye);
extern float simTime;

// ---------------------------------------------------------------------------
// Ventana y tiempo
// ---------------------------------------------------------------------------
GLFWwindow* window = nullptr;
int screenWidth = 1280;
int screenHeight = 720;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ---------------------------------------------------------------------------
// Cámara libre
// ---------------------------------------------------------------------------
Camera camera(glm::vec3(0.0f, 7.0f, 26.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -15.0f);
float lastX = 0.0f;
float lastY = 0.0f;
bool firstMouse = true;
bool cursorCaptured = true;
const float CAMERA_SPEED = 5.0f;
const float CAMERA_SPEED_FAST = 15.0f;

// ---------------------------------------------------------------------------
// Cámara en tercera persona: sigue a un jugador y orbita a su alrededor
// ---------------------------------------------------------------------------
enum class CameraMode { Free, ThirdPerson };
CameraMode cameraMode = CameraMode::Free;

const char* PLAYER_NAMES[] = { "JugadorMX", "JugadorPR", "JugadoraHA", "JugadoraJA" };
const int   PLAYER_COUNT = 4;
int   followedPlayer = 0;        // índice en PLAYER_NAMES
float orbitYaw = -90.0f;   // grados, alrededor del jugador
float orbitPitch = 20.0f;    // grados, positivo = cámara por encima
float orbitDistance = 5.0f;     // metros
const float ORBIT_MIN_DISTANCE = 2.0f;
const float ORBIT_MAX_DISTANCE = 15.0f;
const float PLAYER_SPEED = 2.0f;   // m/s caminando
const float PLAYER_SPEED_RUN = 4.5f;   // m/s corriendo (Shift)
const float WALK_CYCLE_METERS = 1.5f;   // distancia que avanza un ciclo de "Caminar" (dos pasos)
const float PLAYER_TURN_SPEED = 540.0f; // grados/s
const float WORLD_LIMIT = 55.0f;  // el piso mide 120 x 120 m
glm::vec3 thirdPersonEye(0.0f);

// ---------------------------------------------------------------------------
// Iluminación (sol)
// ---------------------------------------------------------------------------
glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
glm::vec3 sunColor(1.0f, 0.97f, 0.9f);

// Segunda técnica de iluminación: luz puntual de la fogata con atenuación (práctica 8).
// Con la tecla L se hace de noche (el sol baja mucho) para que se note la fogata.
bool      nightMode = false;
glm::vec3 fireLightPos(0.0f);
glm::vec3 fireLightColor(0.0f);
const glm::vec3 FIRE_COLOR(1.0f, 0.55f, 0.2f);

// ---------------------------------------------------------------------------
// Escena
// ---------------------------------------------------------------------------
Shader* phongShader = nullptr;
Shader* skinShader = nullptr;   // phong con skinning para personajes animados
Shader* skyboxShader = nullptr;
Shader* waterShader = nullptr;
CubeMap* skybox = nullptr;
Mesh* ground = nullptr;

// Un objeto de la escena: un modelo con su transformación.
// Los modelos no comparten unidades (ver docs/modelos.md), así que en lugar de una escala
// se da la altura real en metros y la escala se calcula con la caja envolvente.
// "stretch" permite deformar un eje (por ejemplo, la red para que coincida con los postes).
// position.y es la altura de la superficie donde se apoya (0 = arena).
struct SceneObject {
    std::string name;
    std::string path;
    glm::vec3 position;
    float rotationY;   // grados
    float height;      // metros
    glm::vec3 stretch = glm::vec3(1.0f);
    Model* model = nullptr;
    // Personajes con esqueleto (animado/<Modelo>.gltf): se dibujan con skinning
    SkinnedModel* skinned = nullptr;
    int   animIdle = -1, animWalk = -1;
    float animTime = 0.0f;   // segundos dentro de la animación actual
    float speed = 0.0f;   // m/s en este cuadro (0 = parado)
    int   oneShot = -1;     // animación que se reproduce una vez (saque, remate...) o -1
    float oneShotTime = 0.0f;
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
    // Para objetos con física (el balón): position es el centro y "spin" su orientación
    bool      centerPivot = false;
    glm::mat4 spin = glm::mat4(1.0f);
    // Animación por piezas: una matriz y una emisión por malla (vacío = modelo rígido)
    std::vector<glm::mat4> parts;
    std::vector<float>     partEmission;
    std::vector<float>     partFresnel;     // brillo Fresnel en los bordes (llamas)
};

#define CP(m) "assets/models/CarlosParra/" m "/" m ".obj"
#define GY(m) "assets/models/GabrielYukioayax/" m "/" m ".obj"
#define GYF(dir, file) "assets/models/GabrielYukioayax/" dir "/" file
#define CPF(dir, file) "assets/models/CarlosParra/" dir "/" file

// Cancha: x en [-4.5, 4.5], z en [-9, 9], red en z = 0 con postes en x = ±5.07.
// La mesa de jueces está en z = +12 (lado de la entrada) y el podio en z = -13.
const float MESA_Y = 0.75f;   // altura de la cubierta de la mesa
std::vector<SceneObject> sceneObjects = {
    // --- Cancha y juego ---
    // La cancha es plana (solo las zonas de arena, ya en metros): altura 0 = escala 1.
    // Sus postes y bandas se quitaron del .blend; la red y los postes son los de RedVoley.
    { "Cancha",             CP("VolleyballCourt"),      {   0.0f, 0.0f,   0.0f },    0.0f,  0.0f  },
    { "Red",                GY("RedVoley"),             {   0.0f, 0.0f,   0.0f },    0.0f,  2.43f, { 1.88f, 1.0f, 1.0f } },
    { "SillaArbitro",       GY("SillaArbitro"),         {   6.4f, 0.0f,   0.0f },  -90.0f,  2.2f  },
    { "Arbitro",            GY("Arbitro"),              {   7.4f, 0.0f,   1.4f },  -90.0f,  1.75f },
    { "JugadorMX",          GY("JugadorMX"),            {  -2.2f, 0.0f,   4.5f },  180.0f,  1.85f },
    { "JugadorPR",          GY("JugadorPR"),            {   2.2f, 0.0f,   6.0f },  180.0f,  1.83f },
    { "JugadoraHA",         GY("JugadoraHA"),           {  -2.2f, 0.0f,  -5.0f },    0.0f,  1.75f },
    { "JugadoraJA",         GY("JugadoraJA"),           {   2.2f, 0.0f,  -4.0f },    0.0f,  1.72f },
    { "BalonVoley",         GY("BalonVoley"),           {  -1.0f, 0.0f,   3.4f },    0.0f,  0.21f },
    { "Marcador",           GY("Marcador"),             {  -6.6f, 0.0f,  -1.5f },   90.0f,  1.8f  },
    { "GradasIzq",          CP("Stands"),               { -10.8f, 0.0f,   0.0f },   90.0f,  3.0f  },
    { "GradasDer",          CP("Stands"),               {  10.8f, 0.0f,   0.0f },  -90.0f,  3.0f  },
    { "Espectador",         GY("Espectador"),           {  -7.8f, 0.0f,   4.0f },   90.0f,  1.78f },
    { "Espectadora",        GY("Espectadora"),          {  -7.8f, 0.0f,   5.6f },   90.0f,  1.65f },
    { "Nina",               GY("Nina"),                 {   7.8f, 0.0f,  -4.0f },  -90.0f,  1.15f },
    { "Nino",               GY("Nino"),                 {   7.8f, 0.0f,  -5.4f },  -90.0f,  1.2f  },

    // --- Mesa de jueces y bancas (entrada) ---
    { "BancaLocal",         CP("MesaJugadores"),        {  -4.0f, 0.0f,  10.8f },    0.0f,  0.45f },
    { "BancaVisita",        CP("MesaJugadores"),        {   4.0f, 0.0f,  10.8f },    0.0f,  0.45f },
    { "Entrenador",         GY("Entrenador"),           {  -6.0f, 0.0f,  11.0f },  180.0f,  1.8f  },
    { "Mesa",               GY("Mesa"),                 {   0.0f, 0.0f,  12.4f },   90.0f,  MESA_Y },
    { "Botiquin",           CP("Medkit"),               {  -1.5f, MESA_Y, 12.4f },   0.0f,  0.12f },
    { "Termo",              GY("Termo"),                {  -0.6f, MESA_Y, 12.4f },   0.0f,  0.28f },
    { "Garrafon",           GY("Garrafon"),             {   0.2f, MESA_Y, 12.4f },   0.0f,  0.45f },
    { "Refresco",           GY("Refresco"),             {   0.9f, MESA_Y, 12.3f },   0.0f,  0.12f },
    { "BotellaAgua",        GY("BotellaAgua"),          {   1.3f, MESA_Y, 12.5f },   0.0f,  0.22f },
    { "BloqueadorSolar",    GY("BloqueadorSolar"),      {   1.7f, MESA_Y, 12.3f },   0.0f,  0.16f },
    { "Hielera",            GY("Hielera"),              {   2.9f, 0.0f,  12.6f },    0.0f,  0.45f },
    { "BalonTeoria",        CP("VolleyballBall"),       {  -2.8f, 0.0f,  12.8f },    0.0f,  0.21f },
    { "Bienvenida",         GY("Bienvenida"),           {  -6.0f, 0.0f,  17.0f },    0.0f,  4.0f  },
    { "BoteBasura",         GY("BoteBasura"),           {   3.5f, 0.0f,  15.5f },    0.0f,  0.9f  },
    { "BotellaPlastico",    CP("BotellaPlastico"),      {   4.4f, 0.0f,  15.8f },    0.0f,  0.25f },
    { "Gato",               GY("Gato"),                 {  -4.5f, 0.0f,  14.0f },   30.0f,  0.3f  },

    // --- Premiación ---
    { "Podio",              CP("Podio"),                {   0.0f, 0.0f, -13.0f },    0.0f,  1.2f  },
    { "Trofeo",             CP("Trofeo"),               {   0.0f, 1.2f, -13.0f },    0.0f,  0.55f },
    { "Bandera",            CPF("MasElementos", "Bandera.obj"), { -6.0f, 0.0f, -11.0f },  0.0f,  3.09f },

    // --- Zona de descanso (este) ---
    { "Sombrilla",          CP("Sombrilla"),            {  16.0f, 0.0f,   6.0f },    0.0f,  2.4f  },
    { "Camastro",           CP("Camastro"),             {  16.5f, 0.0f,   7.5f },  -20.0f,  0.9f  },
    { "Gorra",              CP("Gorra"),                {  14.8f, 0.0f,   8.0f },   20.0f,  0.12f },
    { "LentesSol",          CP("LentesSol"),            {  15.2f, 0.0f,   8.6f },    0.0f,  0.05f },
    { "Cubeta",             CP("Cubeta"),               {  14.0f, 0.0f,  10.0f },    0.0f,  0.25f },
    { "CosasDePlaya",       CP("CosasDePlaya"),         {  17.0f, 0.0f,  12.0f },    0.0f,  0.74f },
    { "AroSalvavidasNino",  GY("AroSalvavidasNino"),    {  13.0f, 0.0f,  11.5f },    0.0f,  0.08f },
    { "BalonPlaya",         GY("BalonPlaya"),           {  13.5f, 0.0f,   4.0f },    0.0f,  0.4f  },
    { "Frisbee",            GY("Frisbee"),              {  17.5f, 0.0f,   3.5f },    0.0f,  0.025f },
    { "Palmera",            CP("Palmera"),              {  19.0f, 0.0f,  -1.0f },    0.0f,  4.6f  },

    // --- Puesto de helados (oeste) ---
    { "PuestoHeladosBanca", CP("PuestoHeladosBanca"),   { -16.0f, 0.0f,   8.0f },   90.0f,  2.6f  },
    { "Globo",              CP("Globo"),                { -13.5f, 0.0f,  11.0f },    0.0f,  1.2f  },
    { "Barril",             CPF("MasElementos", "Barril.obj"),  { -14.0f, 0.0f,   5.5f },  0.0f,  0.9f  },

    // --- Fútbol playa y salvavidas ---
    { "Porteria",           GY("Porteria"),             { -19.0f, 0.0f,  -6.0f },   90.0f,  2.2f  },
    { "BalonFutbol",        GY("BalonFutbol"),          { -15.5f, 0.0f,  -6.0f },    0.0f,  0.22f },
    { "Escalera",           GY("Escalera"),             { -14.0f, 0.0f, -15.0f },    0.0f,  3.0f  },
    { "Salvavidas",         GY("Salvavidas"),           { -12.2f, 0.0f, -14.0f },    0.0f,  1.8f  },
    { "AroSalvavidas",      GY("AroSalvavidas"),        { -11.0f, 0.0f, -15.2f },    0.0f,  0.12f },

    // --- Orilla y fauna ---
    { "Tortuga",            GYF("Tortuga", "turtle.obj"),            {   8.0f, 0.0f, -18.0f },  30.0f, 0.35f },
    { "Cangrejo",           GYF("Cangrejo", "model.obj"),            {   5.0f, 0.0f, -20.0f },   0.0f, 0.1f  },
    { "Langosta",           GYF("Langosta", "model.obj"),            {  -4.0f, 0.0f, -19.0f },   0.0f, 0.12f },
    { "EstrellaMar",        GY("EstrellaMar"),                       {   2.0f, 0.0f, -21.0f },   0.0f, 0.02f },
    { "Remo",               CPF("MasElementos", "Remo.obj"),         {   9.5f, 0.0f, -22.0f },  30.0f, 0.04f },
    { "Gaviota",            GYF("Gaviota", "Mesh_Seagull.obj"),      {  -3.0f, 5.0f, -16.0f },   0.0f, 0.5f  },
    { "Puerto",             GY("Puerto"),                            {   0.0f, 0.0f, -36.0f },   0.0f, 2.5f  },
    { "Bote",               GYF("Bote", "Boat Texture 1.obj"),       {   7.0f, 0.0f, -34.0f },  20.0f, 1.5f  },
    { "Faro",               GYF("Faro", "Lighthouse singly.obj"),    { -40.0f, 0.0f, -45.0f },   0.0f, 20.0f },
};

#undef CP
#undef GY
#undef GYF
#undef CPF

// ---------------------------------------------------------------------------
// Materiales por objeto (práctica 7 y 8: propiedades materiales)
//
// Phong usa specularStrength (qué tanto brilla) y shininess (qué tan concentrado es el
// brillo). Con eso se distinguen los materiales:
//   Opaco      : poco brillo y disperso (los objetos que no están en la lista)
//   Plástico   : brillo medio y concentrado
//   Metálico   : brillo fuerte y muy concentrado
//   Traslúcido : además deja ver lo que está detrás (transparency < 1)
// ---------------------------------------------------------------------------
struct ObjectMaterial {
    const char* name;
    float specular;
    float shininess;
    float transparency;   // 1 = sólido, 0 = invisible
};

const float OPAQUE_SPECULAR = 0.25f;
const float OPAQUE_SHININESS = 32.0f;

std::vector<ObjectMaterial> objectMaterials = {
    // Plástico
    { "BalonVoley",        0.6f,   64.0f, 1.0f  },
    { "BalonTeoria",       0.6f,   64.0f, 1.0f  },
    { "BalonPlaya",        0.6f,   64.0f, 1.0f  },
    { "BalonFutbol",       0.6f,   64.0f, 1.0f  },
    { "Frisbee",           0.6f,   64.0f, 1.0f  },
    { "Hielera",           0.6f,   64.0f, 1.0f  },
    { "AroSalvavidas",     0.6f,   64.0f, 1.0f  },
    { "AroSalvavidasNino", 0.6f,   64.0f, 1.0f  },
    { "BloqueadorSolar",   0.6f,   64.0f, 1.0f  },
    { "BoteBasura",        0.6f,   64.0f, 1.0f  },
    { "Cubeta",            0.6f,   64.0f, 1.0f  },
    // Metálico
    { "Termo",             1.0f,  256.0f, 1.0f  },
    { "Refresco",          1.0f,  256.0f, 1.0f  },
    { "Trofeo",            1.0f,  256.0f, 1.0f  },
    // Traslúcido
    { "Garrafon",          0.8f,  128.0f, 0.45f },
    { "BotellaAgua",       0.8f,  128.0f, 0.45f },
    { "BotellaPlastico",   0.8f,  128.0f, 0.5f  },
};

const ObjectMaterial* materialOf(const SceneObject& obj)
{
    for (const ObjectMaterial& m : objectMaterials)
        if (obj.name == m.name)
            return &m;
    return nullptr;
}

bool isTranslucent(const SceneObject& obj)
{
    const ObjectMaterial* m = materialOf(obj);
    return m && m->transparency < 1.0f;
}

bool isSelected(const SceneObject& obj);

// Modo de captura para pruebas automáticas
std::string capturePath;
std::string simulateAction;      // --simular <Saque|Remate> <s>: adelanta la simulación (pruebas)
float simulateSeconds = 0.0f;
std::string selectArg;           // --seleccionar <objeto>: empieza con ese objeto seleccionado
std::string forcedAnim;          // --pose <anim> <s>: fija una animación (para capturas)
float forcedAnimTime = 0.0f;
int frameCount = 0;

// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--captura" && i + 1 < argc)
            capturePath = fs::absolute(argv[++i]).string();
        else if (arg == "--camara" && i + 5 < argc) {
            camera.Position = glm::vec3(std::stof(argv[i + 1]), std::stof(argv[i + 2]), std::stof(argv[i + 3]));
            camera.Yaw = std::stof(argv[i + 4]);
            camera.Pitch = std::stof(argv[i + 5]);
            camera.ProcessMouseMovement(0.0f, 0.0f);   // recalcula los vectores de la cámara
            i += 5;
        }
        else if (arg == "--seleccionar" && i + 1 < argc)
            selectArg = argv[++i];
        else if (arg == "--noche")
            nightMode = true;
        else if (arg == "--tiempo" && i + 1 < argc)
            simTime = std::stof(argv[++i]);
        else if (arg == "--simular" && i + 2 < argc) {
            simulateAction = argv[++i];
            simulateSeconds = std::stof(argv[++i]);
        }
        else if (arg == "--pose" && i + 2 < argc) {
            forcedAnim = argv[++i];
            forcedAnimTime = std::stof(argv[++i]);
        }
        else if (arg == "--tercera") {
            cameraMode = CameraMode::ThirdPerson;
            if (i + 1 < argc && argv[i + 1][0] >= '1' && argv[i + 1][0] <= '4')
                followedPlayer = argv[++i][0] - '1';
        }
    }

    if (!findAssetsRoot()) {
        std::cout << "No se encontró la carpeta shaders/ junto al ejecutable." << std::endl;
        return -1;
    }

    if (!Start()) {
        glfwTerminate();
        return -1;
    }

    if (!simulateAction.empty())
        runSimulationTest();

    while (!glfwWindowShouldClose(window)) {
        if (!Update())
            break;
    }

    Shutdown();
    glfwTerminate();
    return 0;
}

// Busca la raíz con shaders/ y assets/: primero el directorio actual, luego la carpeta
// del ejecutable y sus padres. Así funciona desde Visual Studio y desde el instalador.
bool findAssetsRoot()
{
    auto isRoot = [](const fs::path& p) {
        return fs::exists(p / "shaders" / "phong.vs") && fs::exists(p / "assets");
        };

    if (isRoot(fs::current_path()))
        return true;

    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    fs::path dir = fs::path(buffer).parent_path();
    for (int i = 0; i < 4 && !dir.empty(); ++i) {
        if (isRoot(dir)) {
            fs::current_path(dir);
            return true;
        }
        dir = dir.parent_path();
    }
    return false;
}

// Crea el piso de arena: un plano cuadrado usando la clase Mesh del curso, con la misma arena
// que la cancha. La foto original no es repetible (se notan cortes en cada repetición), así que
// se usa assets/textures/arena_repetible.jpg: la foto mezclada cerca de los bordes con una
// copia desplazada media imagen, para que el borde derecho continúe en el izquierdo. Las coordenadas UV salen de la posición en metros (una repetición de
// la textura cada tile.x por tile.y metros, igual que en la cancha), así se repite sin estirarse.
// Queda 1 cm abajo de y = 0 para no pelear en profundidad con los planos de la cancha.
const float GROUND_Y = -0.01f;

Mesh* createGround(float halfSize, glm::vec2 tile, const std::string& texDir, const char* texFile)
{
    std::vector<Vertex> vertices(4);
    glm::vec3 corners[4] = {
        { -halfSize, 0.0f, -halfSize }, { halfSize, 0.0f, -halfSize },
        {  halfSize, 0.0f,  halfSize }, { -halfSize, 0.0f,  halfSize }
    };
    for (int i = 0; i < 4; ++i) {
        Vertex v{};
        v.Position = corners[i] + glm::vec3(0.0f, GROUND_Y, 0.0f);
        v.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v.TexCoords = glm::vec2(corners[i].x / tile.x + 0.5f, corners[i].z / tile.y);
        v.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        v.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
        vertices[i] = v;
    }
    std::vector<unsigned int> indices = { 0, 2, 1, 0, 3, 2 };
    Texture sand;
    sand.id = TextureFromFile(texFile, texDir);
    sand.type = "texture_diffuse";
    sand.path = texFile;
    return new Mesh(vertices, indices, { sand }, glm::vec4(1.0f));
}

void computeBounds(SceneObject& obj)
{
    if (obj.skinned) {
        obj.boundsMin = obj.skinned->boundsMin;
        obj.boundsMax = obj.skinned->boundsMax;
        return;
    }
    glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
    for (const Mesh& mesh : obj.model->meshes) {
        for (const Vertex& v : mesh.vertices) {
            mn = glm::min(mn, v.Position);
            mx = glm::max(mx, v.Position);
        }
    }
    if (mn.x > mx.x) { mn = mx = glm::vec3(0.0f); }
    obj.boundsMin = mn;
    obj.boundsMax = mx;
}

bool Start()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    window = glfwCreateWindow(screenWidth, screenHeight, "Arena Limpia", nullptr, nullptr);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window" << std::endl;
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    if (capturePath.empty())
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
    glViewport(0, 0, screenWidth, screenHeight);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    phongShader = new Shader("shaders/phong.vs", "shaders/phong.fs");
    skinShader = new Shader("shaders/phong_skinning.vs", "shaders/phong.fs");
    skyboxShader = new Shader("shaders/skybox.vs", "shaders/skybox.fs");
    waterShader = new Shader("shaders/water.vs", "shaders/water.fs");

    std::vector<std::string> faces = {
        "assets/skybox/mondello/posx.png",
        "assets/skybox/mondello/negx.png",
        "assets/skybox/mondello/posy.png",
        "assets/skybox/mondello/negy.png",
        "assets/skybox/mondello/posz.png",
        "assets/skybox/mondello/negz.png"
    };
    skybox = new CubeMap();
    skybox->loadCubemap(faces);

    ground = createGround(60.0f, glm::vec2(9.0f, 6.0f),
        "assets/textures", "arena_repetible.jpg");

    // Un modelo usado varias veces (por ejemplo, las gradas) se carga una sola vez.
    std::map<std::string, Model*> loaded;
    double tTotal = glfwGetTime();
    for (SceneObject& obj : sceneObjects) {
        // ¿Tiene versión con esqueleto? assets/.../<Modelo>/animado/<Modelo>.gltf
        fs::path animPath = fs::path(obj.path).parent_path() / "animado" /
            (fs::path(obj.path).stem().string() + ".gltf");
        if (fs::exists(animPath)) {
            double t0 = glfwGetTime();
            std::cout << "Modelo animado " << obj.name << ":" << std::endl;
            SkinnedModel* sk = new SkinnedModel(animPath.generic_string());
            if (sk->valid()) {
                obj.skinned = sk;
                obj.animIdle = sk->findAnimation("Parado");
                obj.animWalk = sk->findAnimation("Caminar");
                if (obj.animIdle >= 0) sk->setPose(obj.animIdle, 0.0f);
                computeBounds(obj);
                std::cout << "  " << (glfwGetTime() - t0) << " s" << std::endl;
                continue;
            }
            delete sk;
        }

        auto it = loaded.find(obj.path);
        if (it != loaded.end()) {
            obj.model = it->second;
        }
        else {
            double t0 = glfwGetTime();
            obj.model = new Model(obj.path);
            loaded[obj.path] = obj.model;
            std::cout << "Modelo " << obj.name << ": " << obj.model->meshes.size() << " mallas, "
                << (glfwGetTime() - t0) << " s" << std::endl;
        }
        computeBounds(obj);
    }
    std::cout << loaded.size() << " modelos cargados en " << (glfwGetTime() - tTotal) << " s" << std::endl;

    initBall();
    initPalm();
    initSimpleAnims();
    initInteraction();
    initSea();
    setCameraMode(cameraMode);
    return true;
}

// Matriz de modelo: apoya el objeto sobre la arena (o lo centra, si centerPivot), lo gira y
// lo escala a su altura en metros.
glm::mat4 objectMatrix(const SceneObject& obj)
{
    glm::vec3 center = 0.5f * (obj.boundsMin + obj.boundsMax);
    glm::vec3 pivot = obj.centerPivot ? center : glm::vec3(center.x, obj.boundsMin.y, center.z);

    glm::mat4 model(1.0f);
    model = glm::translate(model, obj.position);
    model = glm::rotate(model, glm::radians(obj.rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = model * obj.spin;
    float sizeY = obj.boundsMax.y - obj.boundsMin.y;
    float scale = (sizeY > 0.001f && obj.height > 0.0f) ? obj.height / sizeY : 1.0f;
    model = glm::scale(model, scale * obj.stretch);
    model = glm::translate(model, -pivot);
    return model;
}

void drawObject(const SceneObject& obj)
{
    glm::mat4 model = objectMatrix(obj);

    // Material del objeto (opaco si no está en objectMaterials)
    const ObjectMaterial* mat = materialOf(obj);
    phongShader->setFloat("specularStrength", mat ? mat->specular : OPAQUE_SPECULAR);
    phongShader->setFloat("shininess", mat ? mat->shininess : OPAQUE_SHININESS);

    if (obj.skinned) {
        skinShader->use();
        skinShader->setMat4("model", model);
        obj.skinned->Draw(*skinShader);
        phongShader->use();
    }
    else if (!obj.parts.empty()) {
        for (size_t i = 0; i < obj.model->meshes.size(); i++) {
            phongShader->setMat4("model", model * obj.parts[i]);
            phongShader->setFloat("emission", obj.partEmission[i]);
            phongShader->setFloat("fresnelStrength", obj.partFresnel.empty() ? 0.0f : obj.partFresnel[i]);
            obj.model->meshes[i].Draw(*phongShader);
        }
        phongShader->setFloat("emission", 0.0f);
        phongShader->setFloat("fresnelStrength", 0.0f);
    }
    else {
        phongShader->setMat4("model", model);
        phongShader->setFloat("emission", isSelected(obj) ? 0.35f : 0.0f);   // el seleccionado brilla
        obj.model->Draw(*phongShader);
        phongShader->setFloat("emission", 0.0f);
    }
}

// Elige "Caminar" o "Parado" según la velocidad y avanza el tiempo de la animación.
// Al caminar, la animación va al ritmo de los pasos para que los pies no se resbalen.

// Inicia una animación que se reproduce una sola vez y luego regresa a "Parado".
// Devuelve false si el personaje no la tiene o ya está en otra.
bool playOneShot(SceneObject& obj, const std::string& name)
{
    if (!obj.skinned || obj.oneShot >= 0) return false;
    int anim = obj.skinned->findAnimation(name);
    if (anim < 0) return false;
    obj.oneShot = anim;
    obj.oneShotTime = 0.0f;
    return true;
}

void animateCharacter(SceneObject& obj, float dt)
{
    SkinnedModel* sk = obj.skinned;
    if (!sk) return;

    if (!forcedAnim.empty()) {
        int anim = sk->findAnimation(forcedAnim);
        if (anim >= 0) {
            sk->setPose(anim, forcedAnimTime);
            return;
        }
    }

    // Animación de una sola vez: al terminar se regresa a "Parado"
    if (obj.oneShot >= 0) {
        obj.oneShotTime += dt;
        float length = sk->duration(obj.oneShot);
        if (obj.oneShotTime < length) {
            sk->setPose(obj.oneShot, obj.oneShotTime);
            return;
        }
        obj.oneShot = -1;
        obj.animTime = 0.0f;
    }

    bool walking = obj.speed > 0.05f && obj.animWalk >= 0;
    int anim = walking ? obj.animWalk : obj.animIdle;
    if (anim < 0) return;

    if (walking)
        obj.animTime += dt * (obj.speed / WALK_CYCLE_METERS) * sk->duration(anim);
    else
        obj.animTime += dt;
    sk->setPose(anim, obj.animTime);
}

// El árbitro marca un punto cada cierto tiempo
SceneObject* findObject(const std::string& name);
const float REFEREE_INTERVAL = 8.0f;
float refereeTimer = 3.0f;

void updateReferee(float dt)
{
    refereeTimer -= dt;
    if (refereeTimer > 0.0f) return;
    refereeTimer = REFEREE_INTERVAL;
    if (SceneObject* referee = findObject("Arbitro"))
        playOneShot(*referee, "Marcar");
}

// ---------------------------------------------------------------------------
// Animación por piezas: jerarquía de transformaciones dentro de un modelo
//
// Un modelo OBJ trae una malla por objeto de Blender. Si un SceneObject tiene "parts", cada
// malla se dibuja con objectMatrix(obj) * parts[i], así se pueden mover piezas por separado.
// ---------------------------------------------------------------------------
int findMesh(const SceneObject& obj, const std::string& name)
{
    if (!obj.model) return -1;
    for (size_t i = 0; i < obj.model->meshNames.size(); i++)
        if (obj.model->meshNames[i] == name) return (int)i;
    return -1;
}

void meshBounds(const Mesh& mesh, glm::vec3& mn, glm::vec3& mx)
{
    mn = glm::vec3(FLT_MAX);
    mx = glm::vec3(-FLT_MAX);
    for (const Vertex& v : mesh.vertices) {
        mn = glm::min(mn, v.Position);
        mx = glm::max(mx, v.Position);
    }
}

// Rotación de "angle" grados alrededor de un eje que pasa por "pivot"
glm::mat4 rotateAround(const glm::vec3& pivot, float angle, const glm::vec3& axis)
{
    return glm::translate(glm::mat4(1.0f), pivot) *
        glm::rotate(glm::mat4(1.0f), glm::radians(angle), axis) *
        glm::translate(glm::mat4(1.0f), -pivot);
}

// Ruido suave en [-1, 1]: suma de senos de frecuencias que no son múltiplos entre sí,
// así el patrón no se repite de forma evidente.
float flicker(float t, float seed)
{
    return 0.5f * sin(7.3f * t + seed) + 0.3f * sin(13.1f * t + 2.1f * seed) +
        0.2f * sin(23.7f * t + 3.7f * seed);
}

// ---------------------------------------------------------------------------
// Palmera con viento y fuego de la fogata (animaciones procedurales)
const float FLAME_FRESNEL = 1.2f;   // intensidad del brillo Fresnel en el contorno de las llamas
//
// Jerarquía de la palmera:
//   Palmera -> Tronco (se mece desde la base) -> Hojas (aletean sobre su punto de unión)
//                                             -> Cocos (siguen la punta del tronco)
//   M_hoja = M_palmera * R_tronco(theta) * T(p_i) * R_hoja(theta_i) * T(-p_i)
// El fuego (en el mismo modelo) no cuelga del tronco: cada llama se estira desde su base
// con ruido y además brilla (emisión) con la misma intensidad.
// ---------------------------------------------------------------------------
const glm::vec3 WIND_AXIS = glm::normalize(glm::vec3(0.3f, 0.0f, 1.0f));  // eje de giro del tronco

struct Leaf { int mesh; glm::vec3 pivot; glm::vec3 axis; float phase; };
struct Flame { int mesh; glm::vec3 base; float seed; };

struct PalmRig {
    SceneObject* obj = nullptr;
    int trunk = -1;
    glm::vec3 trunkBase = glm::vec3(0.0f);
    std::vector<Leaf>  leaves;
    std::vector<int>   followers;   // cocos: siguen al tronco sin moverse por su cuenta
    std::vector<Flame> flames;
} palm;

void initPalm()
{
    palm.obj = findObject("Palmera");
    if (!palm.obj || !palm.obj->model) return;
    SceneObject& o = *palm.obj;
    o.parts.assign(o.model->meshes.size(), glm::mat4(1.0f));
    o.partEmission.assign(o.model->meshes.size(), 0.0f);
    o.partFresnel.assign(o.model->meshes.size(), 0.0f);

    palm.trunk = findMesh(o, "Tronco");
    if (palm.trunk < 0) return;
    glm::vec3 mn, mx;
    meshBounds(o.model->meshes[palm.trunk], mn, mx);
    palm.trunkBase = glm::vec3(0.5f * (mn.x + mx.x), mn.y, 0.5f * (mn.z + mx.z));
    glm::vec3 trunkTop(palm.trunkBase.x, mx.y, palm.trunkBase.z);

    for (size_t i = 0; i < o.model->meshNames.size(); i++) {
        const std::string& name = o.model->meshNames[i];
        const Mesh& mesh = o.model->meshes[i];
        if (name.rfind("Hoja", 0) == 0) {
            // Punto de unión: el vértice de la hoja más cercano a la punta del tronco
            glm::vec3 pivot = trunkTop, center(0.0f);
            float best = FLT_MAX;
            for (const Vertex& v : mesh.vertices) {
                float d = glm::length(v.Position - trunkTop);
                if (d < best) { best = d; pivot = v.Position; }
                center += v.Position;
            }
            center /= (float)std::max<size_t>(1, mesh.vertices.size());
            // Eje de aleteo: horizontal y perpendicular a la dirección de la hoja
            glm::vec3 dir(center.x - pivot.x, 0.0f, center.z - pivot.z);
            glm::vec3 axis = glm::length(dir) > 1e-4f
                ? glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), dir))
                : glm::vec3(1.0f, 0.0f, 0.0f);
            palm.leaves.push_back({ (int)i, pivot, axis, 1.7f * (float)palm.leaves.size() });
        }
        else if (name.rfind("Coco", 0) == 0) {
            palm.followers.push_back((int)i);
        }
        else if (name.rfind("Fogata_Fuego", 0) == 0) {
            meshBounds(mesh, mn, mx);
            glm::vec3 base(0.5f * (mn.x + mx.x), mn.y, 0.5f * (mn.z + mx.z));
            palm.flames.push_back({ (int)i, base, 2.3f * (float)palm.flames.size() + 0.5f });
            o.partFresnel[i] = FLAME_FRESNEL;
        }
    }
    std::cout << "Palmera: " << palm.leaves.size() << " hojas, " << palm.followers.size()
        << " cocos, " << palm.flames.size() << " llamas" << std::endl;
}

void updatePalm(float t)
{
    if (!palm.obj || palm.trunk < 0) return;
    SceneObject& o = *palm.obj;

    // Tronco: ráfagas de viento con dos senos de frecuencias distintas
    float trunkAngle = 2.5f * sin(0.9f * t) + 1.0f * sin(2.3f * t + 1.0f);
    glm::mat4 trunk = rotateAround(palm.trunkBase, trunkAngle, WIND_AXIS);
    o.parts[palm.trunk] = trunk;
    for (int c : palm.followers)
        o.parts[c] = trunk;

    // Hojas: hijas del tronco, cada una aletea con su propio desfase
    for (const Leaf& leaf : palm.leaves) {
        float angle = 6.0f * sin(2.5f * t + leaf.phase) + 2.0f * sin(5.3f * t + 0.7f * leaf.phase);
        o.parts[leaf.mesh] = trunk * rotateAround(leaf.pivot, angle, leaf.axis);
    }

    // Llamas: se estiran desde su base con ruido; la emisión parpadea igual
    for (const Flame& f : palm.flames) {
        float n = flicker(t, f.seed);
        float sy = 1.0f + 0.25f * n;
        float sxz = 1.0f + 0.08f * flicker(t, f.seed + 4.0f);
        o.parts[f.mesh] = glm::translate(glm::mat4(1.0f), f.base) *
            glm::scale(glm::mat4(1.0f), glm::vec3(sxz, sy, sxz)) *
            glm::translate(glm::mat4(1.0f), -f.base);
        o.partEmission[f.mesh] = 0.45f + 0.2f * n;
    }
}

// La luz de la fogata se coloca en la base de la primera llama y parpadea igual que el fuego
void updateFireLight()
{
    if (!palm.obj || palm.flames.empty()) {
        fireLightColor = glm::vec3(0.0f);
        return;
    }
    const Flame& f = palm.flames[0];
    fireLightPos = glm::vec3(objectMatrix(*palm.obj) * glm::vec4(f.base, 1.0f)) + glm::vec3(0.0f, 0.4f, 0.0f);
    float intensity = palm.obj->partEmission[f.mesh] / 0.45f;   // ~1, sube y baja con el fuego
    fireLightColor = FIRE_COLOR * (nightMode ? 6.0f : 2.0f) * intensity;
}

// ---------------------------------------------------------------------------
// Balón: tiro parabólico (animación procedural)
//
// El balón sigue a la mano mientras el jugador lo sostiene. En el lanzamiento y en el golpe
// se calcula la velocidad inicial que lo lleva a un punto en un tiempo dado:
//     p(T) = p0 + v0 T + g T^2 / 2   =>   v0 = (p1 - p0) / T - g T / 2
// Después vuela con gravedad, rebota en la arena perdiendo energía, choca con la red y rueda
// hasta detenerse. Gira según su velocidad.
// ---------------------------------------------------------------------------
const glm::vec3 GRAVITY(0.0f, -9.81f, 0.0f);
const float BALL_RADIUS = 0.105f;
const float BALL_RESTITUTION = 0.55f;   // fracción de la velocidad vertical que conserva al botar
const float BALL_FRICTION = 0.75f;   // fracción de la velocidad horizontal que conserva al botar
const float BALL_ROLL_DAMP = 1.5f;    // frenado al rodar (1/s)
const float NET_HALF_WIDTH = 5.25f;   // red de RedVoley en z = 0
const float NET_BOTTOM = 1.44f;
const float NET_TOP = 2.43f;

// Un golpe: en qué momento de la animación se lanza/golpea y a dónde va el balón
struct ShotDef {
    const char* anim;
    bool  heldAtStart;     // true: el balón empieza en la mano izquierda (saque)
    float tossTime;        // s: se suelta (saque) o lo pasa un compañero (remate)
    float hitTime;         // s: contacto con la mano derecha
    float targetDistance;  // m al frente del jugador donde cae
    float flightTime;      // s desde el golpe hasta caer
};
const ShotDef SHOT_SAQUE = { "Saque",  true,  0.55f, 22.0f / 24.0f, 13.0f, 1.25f };
const ShotDef SHOT_REMATE = { "Remate", false, 0.05f, 21.0f / 24.0f,  6.5f, 0.45f };

enum class BallState { Resting, Held, Flying };
struct Ball {
    SceneObject* obj = nullptr;
    BallState state = BallState::Resting;
    glm::vec3 pos = glm::vec3(0.0f);
    glm::vec3 vel = glm::vec3(0.0f);
    glm::mat4 orientation = glm::mat4(1.0f);
    // Golpe en curso
    SceneObject* shooter = nullptr;
    const ShotDef* shot = nullptr;
    int            phase = 0;   // 0 = antes de lanzar, 1 = en el aire hacia la mano, 2 = golpeado
} ball;

void initBall()
{
    ball.obj = findObject("BalonVoley");
    if (!ball.obj) return;
    ball.obj->centerPivot = true;
    ball.pos = ball.obj->position + glm::vec3(0.0f, BALL_RADIUS, 0.0f);
    ball.obj->position = ball.pos;
}

glm::vec3 forwardOf(const SceneObject& obj)
{
    float a = glm::radians(obj.rotationY);   // el modelo mira hacia +Z con rotationY = 0
    return glm::vec3(sin(a), 0.0f, cos(a));
}

// Posición en el mundo de un hueso del personaje en el instante t de una animación
glm::vec3 boneWorldAt(SceneObject& obj, int anim, float t, const char* bone)
{
    obj.skinned->setPose(anim, t);
    glm::vec3 local = obj.skinned->bonePosition(bone);
    return glm::vec3(objectMatrix(obj) * glm::vec4(local, 1.0f));
}

glm::vec3 launchVelocity(const glm::vec3& from, const glm::vec3& to, float T)
{
    return (to - from) / T - 0.5f * GRAVITY * T;
}

// ¿El tiro de from con velocidad v pasa por encima de la red (con margen)?
bool clearsNet(const glm::vec3& from, const glm::vec3& v)
{
    if (fabs(v.z) < 1e-4f) return true;
    float tNet = -from.z / v.z;                  // instante en que cruza z = 0
    if (tNet <= 0.0f) return true;               // se aleja de la red
    float x = from.x + v.x * tNet;
    float y = from.y + v.y * tNet + 0.5f * GRAVITY.y * tNet * tNet;
    return fabs(x) > NET_HALF_WIDTH || y > NET_TOP + 0.3f;
}

// Velocidad para que el balón caiga a "distance" metros al frente en "T" segundos.
// Si así chocaría con la red (ayuda de puntería):
//   1) alarga el tiro manteniendo la rapidez horizontal (remate más profundo), hasta el doble;
//   2) si no basta, le da más parábola.
glm::vec3 aimedVelocity(const glm::vec3& from, const glm::vec3& origin, const glm::vec3& forward,
    float distance, float T)
{
    const float maxDistance = 2.0f * distance, speed = distance / T;
    glm::vec3 to, v;
    for (int i = 0; i < 80; i++) {
        to = origin + forward * distance;
        to.y = BALL_RADIUS;
        v = launchVelocity(from, to, T);
        if (clearsNet(from, v)) break;
        if (distance < maxDistance) {
            distance += 0.5f;
            T = distance / speed;
        }
        else {
            T += 0.05f;
        }
    }
    return v;
}

void startShot(SceneObject& player, const ShotDef& shot)
{
    if (!ball.obj) return;
    ball.shooter = &player;
    ball.shot = &shot;
    ball.phase = 0;
    ball.state = shot.heldAtStart ? BallState::Held : BallState::Flying;
    ball.vel = glm::vec3(0.0f);
}

void updateShot()
{
    SceneObject* p = ball.shooter;
    if (!p) return;
    int anim = p->skinned ? p->skinned->findAnimation(ball.shot->anim) : -1;
    if (anim < 0 || p->oneShot != anim) {   // la animación terminó o se interrumpió
        ball.shooter = nullptr;
        return;
    }
    float t = p->oneShotTime;
    const ShotDef& s = *ball.shot;

    if (ball.phase == 0) {
        if (s.heldAtStart) {
            ball.state = BallState::Held;
            ball.pos = boneWorldAt(*p, anim, t, "mano.L") + glm::vec3(0.0f, BALL_RADIUS, 0.0f);
        }
        if (t >= s.tossTime) {
            glm::vec3 from = ball.pos;
            if (!s.heldAtStart) {
                // Pase de un compañero: llega desde la izquierda y un poco al frente
                glm::vec3 fwd = forwardOf(*p);
                glm::vec3 right(-fwd.z, 0.0f, fwd.x);
                from = p->position - right * 3.0f + fwd * 1.5f + glm::vec3(0.0f, 1.5f, 0.0f);
            }
            glm::vec3 hand = boneWorldAt(*p, anim, s.hitTime, "mano.R");
            ball.pos = from;
            ball.vel = launchVelocity(from, hand, s.hitTime - t);
            ball.state = BallState::Flying;
            ball.phase = 1;
        }
    }
    else if (ball.phase == 1 && t >= s.hitTime) {
        ball.vel = aimedVelocity(ball.pos, p->position, forwardOf(*p), s.targetDistance, s.flightTime);
        ball.phase = 2;
        ball.shooter = nullptr;   // a partir de aquí solo física
    }
}

void updateBall(float dt)
{
    if (!ball.obj) return;
    updateShot();

    if (ball.state == BallState::Flying) {
        glm::vec3 prev = ball.pos;
        // Integración exacta para aceleración constante
        ball.pos += ball.vel * dt + 0.5f * GRAVITY * dt * dt;
        ball.vel += GRAVITY * dt;

        // Red: si cruza z = 0 a la altura de la red, rebota hacia atrás
        if ((prev.z > 0.0f) != (ball.pos.z > 0.0f) && fabs(ball.pos.x) < NET_HALF_WIDTH &&
            ball.pos.y > NET_BOTTOM - BALL_RADIUS && ball.pos.y < NET_TOP + BALL_RADIUS) {
            ball.pos.z = (prev.z > 0.0f ? 1.0f : -1.0f) * BALL_RADIUS;
            ball.vel.z = -ball.vel.z * 0.2f;
            ball.vel.x *= 0.5f;
        }

        // Arena: bota perdiendo energía; si ya casi no bota, rueda
        if (ball.pos.y < BALL_RADIUS) {
            ball.pos.y = BALL_RADIUS;
            if (ball.vel.y < 0.0f) {
                ball.vel.y = -ball.vel.y * BALL_RESTITUTION;
                ball.vel.x *= BALL_FRICTION;
                ball.vel.z *= BALL_FRICTION;
                if (ball.vel.y < 0.6f) ball.vel.y = 0.0f;
            }
            if (ball.vel.y == 0.0f) {
                ball.vel *= exp(-BALL_ROLL_DAMP * dt);
                if (glm::length(ball.vel) < 0.05f) {
                    ball.vel = glm::vec3(0.0f);
                    ball.state = BallState::Resting;
                }
            }
        }
        ball.pos.x = glm::clamp(ball.pos.x, -WORLD_LIMIT, WORLD_LIMIT);
        ball.pos.z = glm::clamp(ball.pos.z, -WORLD_LIMIT, WORLD_LIMIT);

        // Giro: alrededor del eje perpendicular a la velocidad horizontal
        glm::vec3 vh(ball.vel.x, 0.0f, ball.vel.z);
        float speed = glm::length(vh);
        if (speed > 0.01f) {
            glm::vec3 axis = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), vh));
            float angle = speed / BALL_RADIUS * dt;
            ball.orientation = glm::rotate(glm::mat4(1.0f), angle, axis) * ball.orientation;
        }
    }

    ball.obj->position = ball.pos;
    ball.obj->spin = ball.orientation;
}

// ---------------------------------------------------------------------------
// Olas del mar (animación procedural)
//
// El mar es una cuadrícula en x-z que empieza en la orilla (z = SEA_SHORE_Z) y llega hasta el
// horizonte. El vertex shader (shaders/water.vs) sube cada vértice con una suma de senos:
//   h(p, t) = sum_i A_i sin(k_i (d_i . p) - w_i t + phi_i),   w_i = sqrt(g k_i)
// d_i es la dirección en que viaja la ola (todas hacia la playa, +z), k_i = 2 pi / largo y
// w_i sale de la relación de dispersión del agua profunda: las olas largas van más rápido.
// En la orilla las olas desaparecen poco a poco (smoothstep en SEA_SHORE_RAMP metros).
// La misma función se evalúa aquí en el CPU para que el bote suba, baje y se incline.
// ---------------------------------------------------------------------------
struct Wave {
    glm::vec2 dir;      // hacia donde viaja (se normaliza)
    float     length;   // largo de onda en metros
    float     amp;      // amplitud en metros
    float     phase;
};

const Wave SEA_WAVES[4] = {
    { {  0.2f, 1.0f }, 14.0f, 0.12f,  0.0f },
    { { -0.5f, 1.0f },  7.5f, 0.07f,  1.3f },
    { {  0.8f, 0.6f },  3.2f, 0.03f,  2.1f },
    { { -0.7f, 0.7f },  1.4f, 0.012f, 0.4f },
};
const float SEA_GRAVITY = 9.8f;
const float SEA_SHORE_Z = -26.0f;   // orilla: el mar está en z < -26
const float SEA_FAR_Z = -420.0f;
const float SEA_HALF_X = 220.0f;
const float SEA_SHORE_RAMP = 8.0f;     // metros en los que crecen las olas desde la orilla
const float SEA_LEVEL = 0.06f;    // altura del agua en la orilla (arriba de la arena)
const float SEA_RISE = 0.24f;    // el nivel sube esto mar adentro
const float BOAT_DRAFT = 0.35f;    // qué tanto se hunde el casco

GLuint seaVAO = 0, seaVBO = 0, seaEBO = 0;
GLsizei seaIndexCount = 0;

float seaShore(float z)
{
    return glm::smoothstep(0.0f, SEA_SHORE_RAMP, SEA_SHORE_Z - z);
}

// Altura del agua y su pendiente (dh/dx, dh/dz) en el punto p = (x, z)
float seaHeight(glm::vec2 p, float t, glm::vec2* grad = nullptr)
{
    float s = seaShore(p.y);
    float h = 0.0f;
    glm::vec2 g(0.0f);
    for (const Wave& w : SEA_WAVES) {
        glm::vec2 d = glm::normalize(w.dir);
        float k = 2.0f * glm::pi<float>() / w.length;
        float arg = k * glm::dot(d, p) - sqrt(SEA_GRAVITY * k) * t + w.phase;
        h += w.amp * sin(arg);
        g += w.amp * k * cos(arg) * d;
    }
    if (grad) *grad = s * g;
    return SEA_LEVEL + s * (SEA_RISE + h);
}

void initSea()
{
    // Más filas cerca de la orilla, donde se ve de cerca; más separadas hacia el horizonte
    const int NX = 221, NZ = 161;
    std::vector<glm::vec2> verts;
    verts.reserve(NX * NZ);
    for (int j = 0; j < NZ; j++) {
        float t = (float)j / (NZ - 1);
        float z = SEA_SHORE_Z + (SEA_FAR_Z - SEA_SHORE_Z) * pow(t, 1.6f);
        for (int i = 0; i < NX; i++)
            verts.push_back({ -SEA_HALF_X + 2.0f * SEA_HALF_X * i / (NX - 1), z });
    }
    std::vector<unsigned int> idx;
    for (int j = 0; j + 1 < NZ; j++)
        for (int i = 0; i + 1 < NX; i++) {
            unsigned int a = j * NX + i, b = a + 1, c = a + NX, d = c + 1;
            idx.insert(idx.end(), { a, c, b, b, c, d });
        }
    seaIndexCount = (GLsizei)idx.size();

    glGenVertexArrays(1, &seaVAO);
    glGenBuffers(1, &seaVBO);
    glGenBuffers(1, &seaEBO);
    glBindVertexArray(seaVAO);
    glBindBuffer(GL_ARRAY_BUFFER, seaVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec2), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, seaEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glBindVertexArray(0);
}

// El bote flota: su altura es la del agua y se inclina con la pendiente de la ola
void updateSea(float t)
{
    SceneObject* boat = findObject("Bote");
    if (!boat) return;
    glm::vec2 grad;
    glm::vec2 p(boat->position.x, boat->position.z);
    boat->position.y = seaHeight(p, t, &grad) - BOAT_DRAFT;

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 n = glm::normalize(glm::vec3(-grad.x, 1.0f, -grad.y));
    glm::vec3 axis = glm::cross(up, n);
    glm::mat4 tilt(1.0f);
    if (glm::length(axis) > 1e-5f)
        tilt = glm::rotate(glm::mat4(1.0f), acos(glm::clamp(n.y, -1.0f, 1.0f)), glm::normalize(axis));
    // objectMatrix aplica spin después de rotationY; así la inclinación queda en ejes del mundo
    glm::mat4 ry = glm::rotate(glm::mat4(1.0f), glm::radians(boat->rotationY), up);
    boat->spin = glm::inverse(ry) * tilt * ry;
}

void drawSea(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& eye)
{
    if (!seaVAO) return;
    waterShader->use();
    waterShader->setMat4("projection", projection);
    waterShader->setMat4("view", view);
    waterShader->setVec3("viewPos", eye);
    waterShader->setVec3("sunDirection", sunDirection);
    waterShader->setVec3("sunColor", nightMode ? sunColor * 0.12f : sunColor);   // de noche el mar también se oscurece
    waterShader->setFloat("waveTime", simTime);
    waterShader->setFloat("gravity", SEA_GRAVITY);
    waterShader->setFloat("shoreZ", SEA_SHORE_Z);
    waterShader->setFloat("shoreRamp", SEA_SHORE_RAMP);
    waterShader->setFloat("seaLevel", SEA_LEVEL);
    waterShader->setFloat("seaRise", SEA_RISE);
    for (int i = 0; i < 4; i++) {
        const Wave& w = SEA_WAVES[i];
        std::string n = "[" + std::to_string(i) + "]";
        waterShader->setVec2("waveDir" + n, glm::normalize(w.dir));
        waterShader->setVec3("waveParam" + n, glm::vec3(2.0f * glm::pi<float>() / w.length, w.amp, w.phase));
    }
    glBindVertexArray(seaVAO);
    glDrawElements(GL_TRIANGLES, seaIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Animaciones simples (10)
//
// Como la puerta corrediza y la puerta con bisagra de la práctica 9: solo se cambia la
// traslación o la rotación del objeto con el tiempo. Cada objeto guarda su posición
// original (basePoses) y en cada cuadro se le suma un desplazamiento o un ángulo.
// ---------------------------------------------------------------------------
struct BasePose {
    glm::vec3 position;
    float     rotationY;
};
std::map<std::string, BasePose> basePoses;   // posición y rotación originales de los objetos animados

const char* SIMPLE_ANIMATED[] = {
    "Frisbee", "EstrellaMar", "AroSalvavidas", "Globo",      // giran
    "BalonPlaya", "Bote", "Cangrejo",                        // van y vienen en X
    "Langosta", "Tortuga",                                   // van y vienen en Z
    "Gato",                                                  // voltea de un lado a otro
    "Gaviota"                                                // procedural: vuela en círculo
};
const int SIMPLE_ANIMATED_COUNT = 11;

// Avanza de 0 a "largo" y regresa a 0, siempre a la misma velocidad
float idaYVuelta(float x, float largo)
{
    float m = fmod(x, 2.0f * largo);
    return m < largo ? m : 2.0f * largo - m;
}

void initSimpleAnims()
{
    for (int i = 0; i < SIMPLE_ANIMATED_COUNT; i++) {
        SceneObject* o = findObject(SIMPLE_ANIMATED[i]);
        if (o)
            basePoses[o->name] = { o->position, o->rotationY };
    }
}

// Coloca el objeto en su posición original + desplazamiento, y rotación original + ángulo
void setAnimatedPose(const char* name, glm::vec3 offset, float angle)
{
    SceneObject* o = findObject(name);
    auto it = basePoses.find(name);
    if (!o || it == basePoses.end())
        return;
    o->position = it->second.position + offset;
    o->rotationY = it->second.rotationY + angle;
}

void updateSimpleAnims(float t)
{
    // 1 a 4. Rotación a velocidad constante (grados por segundo)
    setAnimatedPose("Frisbee", glm::vec3(0.0f), 180.0f * t);
    setAnimatedPose("EstrellaMar", glm::vec3(0.0f), 20.0f * t);
    setAnimatedPose("AroSalvavidas", glm::vec3(0.0f), 30.0f * t);
    setAnimatedPose("Globo", glm::vec3(0.0f), 25.0f * t);

    // 5 a 7. Traslación de ida y vuelta en X (metros por segundo, recorrido en metros)
    setAnimatedPose("BalonPlaya", glm::vec3(idaYVuelta(0.8f * t, 3.0f) - 1.5f, 0.0f, 0.0f), 0.0f);
    setAnimatedPose("Bote", glm::vec3(idaYVuelta(1.5f * t, 8.0f) - 4.0f, 0.0f, 0.0f), 0.0f);
    setAnimatedPose("Cangrejo", glm::vec3(idaYVuelta(0.4f * t, 1.5f) - 0.75f, 0.0f, 0.0f), 0.0f);

    // 8 y 9. Traslación de ida y vuelta en Z; al regresar se voltean 180 grados
    float langosta = fmod(0.3f * t, 2.4f);
    setAnimatedPose("Langosta", glm::vec3(0.0f, 0.0f, idaYVuelta(0.3f * t, 1.2f) - 0.6f),
        langosta < 1.2f ? 0.0f : 180.0f);
    float tortuga = fmod(0.15f * t, 2.0f);
    setAnimatedPose("Tortuga", glm::vec3(0.0f, 0.0f, idaYVuelta(0.15f * t, 1.0f) - 0.5f),
        tortuga < 1.0f ? 0.0f : 180.0f);

    // 10. Rotación de ida y vuelta: el gato voltea de -30 a 30 grados
    setAnimatedPose("Gato", glm::vec3(0.0f), idaYVuelta(40.0f * t, 60.0f) - 30.0f);

    // ---------------------------------------------------------------------------
    // Animación procedural: la gaviota vuela en círculo
    // Como la luna de la práctica 9: posición calculada con un radio y el tiempo.
    //   x = r cos(wt),  z = r sin(wt),  y = 0.5 sin(2wt)  (sube y baja)
    // y gira para mirar hacia donde vuela.
    // ---------------------------------------------------------------------------
    const float radio = 4.0f;               // metros
    const float periodo = 6.0f;               // segundos por vuelta
    float w = 2.0f * 3.14159265f / periodo;   // velocidad angular
    glm::vec3 vuelo(radio * cos(w * t), 0.5f * sin(2.0f * w * t), radio * sin(w * t));
    float direccion = glm::degrees(atan2(-sin(w * t), cos(w * t)));
    setAnimatedPose("Gaviota", vuelo, direccion);
}

// ---------------------------------------------------------------------------
// Interacción con 15 objetos: seleccionar, tomar, trasladar, rotar y escalar
// (como el movimiento del objeto con flechas y el escalamiento con +/- de la práctica 2)
// ---------------------------------------------------------------------------
const char* INTERACTIVE_NAMES[] = {
    "Frisbee", "BalonPlaya", "BalonFutbol", "AroSalvavidas", "Hielera",
    "Termo", "Garrafon", "Refresco", "BotellaAgua", "BloqueadorSolar",
    "BoteBasura", "Cubeta", "Gorra", "LentesSol", "Sombrilla"
};
const int INTERACTIVE_COUNT = 15;
int  selectedIndex = -1;    // -1 = ningún objeto seleccionado
bool holding = false;       // el objeto está tomado (flota frente a la cámara)

std::vector<BasePose> originalPoses;     // para regresar los objetos a como estaban
std::vector<float>    originalHeights;

SceneObject* selectedObject()
{
    if (selectedIndex < 0)
        return nullptr;
    return findObject(INTERACTIVE_NAMES[selectedIndex]);
}

bool isSelected(const SceneObject& obj)
{
    SceneObject* s = selectedObject();
    return s != nullptr && s == &obj;
}

// Si el objeto tiene animación simple se mueve su posición original (la animación se suma
// encima); si no, se mueve directamente.
glm::vec3* editablePosition(SceneObject* o)
{
    auto it = basePoses.find(o->name);
    return it != basePoses.end() ? &it->second.position : &o->position;
}

float* editableRotation(SceneObject* o)
{
    auto it = basePoses.find(o->name);
    return it != basePoses.end() ? &it->second.rotationY : &o->rotationY;
}

void initInteraction()
{
    for (int i = 0; i < INTERACTIVE_COUNT; i++) {
        SceneObject* o = findObject(INTERACTIVE_NAMES[i]);
        if (o) {
            originalPoses.push_back({ *editablePosition(o), *editableRotation(o) });
            originalHeights.push_back(o->height);
        }
        else {
            originalPoses.push_back({ glm::vec3(0.0f), 0.0f });
            originalHeights.push_back(0.0f);
        }
        if (selectArg == INTERACTIVE_NAMES[i])
            selectedIndex = i;
    }
}

void updateSelectionTitle()
{
    std::string title = std::string("Arena Limpia - ") +
        (cameraMode == CameraMode::Free ? std::string("camara libre")
            : std::string("tercera persona: ") + PLAYER_NAMES[followedPlayer]);
    if (SceneObject* o = selectedObject())
        title += " | seleccionado: " + o->name + (holding ? " (tomado)" : "");
    glfwSetWindowTitle(window, title.c_str());
}

// Teclas que se mantienen presionadas (se llama en cada cuadro desde processInput)
void interactionInput(GLFWwindow* window)
{
    SceneObject* o = selectedObject();
    if (!o)
        return;
    glm::vec3* p = editablePosition(o);
    float* r = editableRotation(o);

    if (holding && cameraMode == CameraMode::Free) {
        // Tomado: el objeto flota 2.5 m frente a la cámara
        *p = camera.Position + camera.Front * 2.5f;
        p->y = std::max(p->y - 0.5f * o->height, 0.0f);
    }
    else {
        // Traslación (2 metros por segundo)
        float step = 2.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) p->x -= step;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) p->x += step;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) p->z -= step;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) p->z += step;
        if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS) p->y += step;
        if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) p->y -= step;
        p->y = std::max(p->y, 0.0f);
    }

    // Rotación (90 grados por segundo)
    float turn = 90.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) *r += turn;
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) *r -= turn;

    // Escalamiento (crece o se encoge poco a poco)
    float grow = 1.0f + deltaTime;
    if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
        o->height *= grow;
    if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
        o->height /= grow;
    o->height = glm::clamp(o->height, 0.01f, 10.0f);
}

// Teclas de un solo toque (se llama desde key_callback)
void interactionKey(int key)
{
    if (key == GLFW_KEY_R) {
        // Siguiente objeto: -1 (ninguno), 0, 1, ..., 14, y otra vez -1
        selectedIndex = (selectedIndex + 2) % (INTERACTIVE_COUNT + 1) - 1;
        holding = false;
        updateSelectionTitle();
    }

    SceneObject* o = selectedObject();
    if (!o)
        return;

    if (key == GLFW_KEY_G && cameraMode == CameraMode::Free) {
        holding = !holding;
        if (!holding)
            editablePosition(o)->y = 0.0f;   // al soltarlo cae a la arena
        updateSelectionTitle();
    }

    if (key == GLFW_KEY_BACKSPACE) {
        *editablePosition(o) = originalPoses[selectedIndex].position;
        *editableRotation(o) = originalPoses[selectedIndex].rotationY;
        o->height = originalHeights[selectedIndex];
        holding = false;
        updateSelectionTitle();
    }
}

// ---------------------------------------------------------------------------
// Partido automático (tecla P): 2 contra 2
//   Equipo 0: JugadorMX y JugadorPR      Equipo 1: JugadoraHA y JugadoraJA
//
// Usa las mismas animaciones y la misma física del balón que el saque y el remate manuales:
//   1. JugadorMX saca.
//   2. Mientras el balón vuela, del otro equipo corre hacia él la jugadora más cercana
//      (sin salirse de su mitad de la cancha); su compañera regresa a su lugar.
//   3. Cuando el balón baja en su lado, remata de regreso y le toca al otro equipo.
// Si el balón se queda atorado (por ejemplo en la red), a los 5 s se sigue igual.
// ---------------------------------------------------------------------------
SceneObject* followedObject();

const char* AUTO_TEAMS[2][2] = { { "JugadorMX",  "JugadorPR"  },
                                 { "JugadoraHA", "JugadoraJA" } };
const float AUTO_FACING[2] = { 180.0f, 0.0f };   // cada equipo mira hacia la red
const float AUTO_MAX_WAIT = 5.0f;               // segundos máximos esperando el balón
bool      autoMatch = false;
int       autoTurn = 0;        // equipo al que le toca golpear
bool      autoServed = false;    // false = el siguiente golpe es el saque
float     autoDelay = 0.0f;     // pausa después de cada golpe
float     autoWait = 0.0f;     // tiempo esperando a que el balón llegue
glm::vec3 autoHome[2][2];

// ¿Este jugador lo está controlando el usuario en tercera persona?
bool isUserControlled(SceneObject* p)
{
    return cameraMode == CameraMode::ThirdPerson && p == followedObject();
}

// Mueve al jugador hacia un punto, sin salirse de su mitad de la cancha
void autoWalkTo(SceneObject* p, int side, glm::vec3 target, float speed)
{
    if (!p || isUserControlled(p) || p->oneShot >= 0) return;
    target.x = glm::clamp(target.x, -4.0f, 4.0f);
    target.z = (side == 0) ? glm::clamp(target.z, 1.5f, 8.5f) : glm::clamp(target.z, -8.5f, -1.5f);
    glm::vec3 d(target.x - p->position.x, 0.0f, target.z - p->position.z);
    float dist = glm::length(d);
    if (dist < 0.15f) return;
    d = glm::normalize(d);
    float step = std::min(speed * deltaTime, dist);
    p->position += d * step;
    p->speed = speed;
    p->rotationY = glm::degrees(atan2(d.x, d.z));   // mira hacia donde camina
}

// De un equipo, el jugador más cercano al balón (antes del saque: el primero)
int autoHitterIndex(int side)
{
    if (!autoServed) return 0;
    SceneObject* a = findObject(AUTO_TEAMS[side][0]);
    SceneObject* b = findObject(AUTO_TEAMS[side][1]);
    if (!a) return 1;
    if (!b) return 0;
    glm::vec3 pa(a->position.x - ball.pos.x, 0.0f, a->position.z - ball.pos.z);
    glm::vec3 pb(b->position.x - ball.pos.x, 0.0f, b->position.z - ball.pos.z);
    return glm::length(pa) <= glm::length(pb) ? 0 : 1;
}

// Golpe sin animación (para personajes que no tienen "Saque" ni "Remate"):
// el balón sale de arriba de su cabeza hacia el otro lado y el personaje da un saltito.
SceneObject* autoJumper = nullptr;
float        autoJumpTime = 0.0f;
const float  AUTO_JUMP_DURATION = 0.5f;

void autoSimpleHit(SceneObject* p)
{
    if (!ball.obj) return;
    glm::vec3 fwd = forwardOf(*p);
    ball.shooter = nullptr;
    ball.pos = p->position + fwd * 0.4f + glm::vec3(0.0f, p->height + 0.3f, 0.0f);
    float distance = autoServed ? 9.0f : 13.0f;   // remate : saque
    ball.vel = aimedVelocity(ball.pos, p->position, fwd, distance, 1.1f);
    ball.state = BallState::Flying;
    autoJumper = p;
    autoJumpTime = 0.0f;
}

void updateAutoJump(float dt)
{
    if (!autoJumper) return;
    autoJumpTime += dt;
    if (autoJumpTime >= AUTO_JUMP_DURATION) {
        autoJumper->position.y = 0.0f;
        autoJumper = nullptr;
        return;
    }
    // Sube y baja como medio seno (0.35 m de altura)
    autoJumper->position.y = 0.35f * sin(3.14159265f * autoJumpTime / AUTO_JUMP_DURATION);
}

void toggleAutoMatch()
{
    autoMatch = !autoMatch;
    if (!autoMatch) return;
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < 2; i++)
            if (SceneObject* p = findObject(AUTO_TEAMS[s][i]))
                autoHome[s][i] = p->position;
    autoTurn = 0;
    autoServed = false;
    autoDelay = 0.5f;
    autoWait = 0.0f;
}

void updateAutoMatch(float dt)
{
    updateAutoJump(dt);
    if (!autoMatch) return;
    int h = autoHitterIndex(autoTurn);
    SceneObject* hitter = findObject(AUTO_TEAMS[autoTurn][h]);
    if (!hitter) return;

    // Todos los demás regresan a su lugar caminando
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < 2; i++)
            if (!(s == autoTurn && i == h))
                autoWalkTo(findObject(AUTO_TEAMS[s][i]), s, autoHome[s][i], PLAYER_SPEED);

    // El que va a golpear corre hacia el balón mientras vuela
    if (autoServed && ball.state != BallState::Resting)
        autoWalkTo(hitter, autoTurn, ball.pos, PLAYER_SPEED_RUN);

    autoDelay -= dt;
    if (autoDelay > 0.0f || hitter->oneShot >= 0 || ball.shooter || autoJumper)
        return;

    if (autoServed) {
        // Esperar a que el balón esté bajando en su lado (o quieto), con tiempo máximo
        autoWait += dt;
        bool onSide = (autoTurn == 0) ? ball.pos.z > 0.0f : ball.pos.z < 0.0f;
        bool reached = onSide && (ball.state == BallState::Resting || (ball.vel.y < 0.0f && ball.pos.y < 1.6f));
        if (!reached && autoWait < AUTO_MAX_WAIT)
            return;
    }

    // Golpear: primero se voltea hacia la red
    hitter->rotationY = AUTO_FACING[autoTurn];
    const ShotDef& shot = autoServed ? SHOT_REMATE : SHOT_SAQUE;
    bool animated = playOneShot(*hitter, shot.anim);
    if (animated)
        startShot(*hitter, shot);
    else
        autoSimpleHit(hitter);   // no tiene la animación: golpe sencillo con saltito
    {
        autoServed = true;
        autoTurn = 1 - autoTurn;
        autoDelay = 1.2f;
        autoWait = 0.0f;
    }
}

// Avanza todo lo que se mueve solo: balón, personajes y árbitro.
// El balón va primero porque lee el tiempo de la animación del jugador antes de que avance;
// además boneWorldAt cambia la pose y animateCharacter la deja como corresponde.
float simTime = 0.0f;   // segundos de simulación (--tiempo lo adelanta para capturas)

void stepSimulation(float dt)
{
    simTime += dt;
    updatePalm(simTime);
    updateFireLight();
    updateSimpleAnims(simTime);
    updateSea(simTime);
    updateAutoMatch(dt);
    updateBall(dt);
    for (SceneObject& obj : sceneObjects)
        animateCharacter(obj, dt);
    updateReferee(dt);
}

// --simular: el jugador seguido hace la acción (o empieza el partido automático) y se adelanta
// la simulación (para capturas)
void runSimulationTest()
{
    const float dt = 1.0f / 120.0f;
    if (simulateAction == "Partido") {
        toggleAutoMatch();
    }
    else {
        SceneObject* player = findObject(PLAYER_NAMES[followedPlayer]);
        if (!player) return;
        const ShotDef& shot = (simulateAction == "Remate") ? SHOT_REMATE : SHOT_SAQUE;
        if (!playOneShot(*player, shot.anim)) return;
        startShot(*player, shot);
    }
    deltaTime = dt;
    int steps = (int)(simulateSeconds / dt);
    for (int i = 0; i <= steps; i++) {
        if (i % 12 == 0)
            printf("t=%.2f balon=(%.2f, %.2f, %.2f) estado=%d\n", i * dt,
                ball.pos.x, ball.pos.y, ball.pos.z, (int)ball.state);
        if (i < steps) {
            for (SceneObject& obj : sceneObjects) obj.speed = 0.0f;
            stepSimulation(dt);
        }
    }
    lastFrame = (float)glfwGetTime();
}

// ---------------------------------------------------------------------------
// Tercera persona
// ---------------------------------------------------------------------------
SceneObject* findObject(const std::string& name)
{
    for (SceneObject& obj : sceneObjects)
        if (obj.name == name)
            return &obj;
    return nullptr;
}

SceneObject* followedObject()
{
    return findObject(PLAYER_NAMES[followedPlayer]);
}

// Coloca la cámara detrás del jugador, a la altura de los hombros, y apunta a él.
glm::mat4 thirdPersonView(glm::vec3& eye)
{
    SceneObject* player = followedObject();
    glm::vec3 base = player ? player->position : glm::vec3(0.0f);
    float h = player ? player->height : 1.8f;
    glm::vec3 target = base + glm::vec3(0.0f, 0.85f * h, 0.0f);

    float yaw = glm::radians(orbitYaw), pitch = glm::radians(orbitPitch);
    glm::vec3 dir(cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw));
    eye = target - dir * orbitDistance;
    eye.y = std::max(eye.y, 0.3f);   // que no se meta bajo la arena
    thirdPersonEye = eye;
    return glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

// Al entrar a tercera persona la cámara queda detrás del jugador.
void placeOrbitBehindPlayer()
{
    SceneObject* player = followedObject();
    if (!player) return;
    // El modelo mira hacia +Z cuando rotationY = 0
    float facing = glm::radians(player->rotationY);
    glm::vec3 forward(sin(facing), 0.0f, cos(facing));
    orbitYaw = glm::degrees(atan2(forward.z, forward.x));
}

void movePlayer(GLFWwindow* window)
{
    SceneObject* player = followedObject();
    if (!player || player->oneShot >= 0) return;   // no camina mientras saca o remata

    // Adelante = hacia donde mira la cámara, proyectado en el piso
    float yaw = glm::radians(orbitYaw);
    glm::vec3 forward(cos(yaw), 0.0f, sin(yaw));
    glm::vec3 right(-forward.z, 0.0f, forward.x);

    glm::vec3 move(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move -= forward;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += right;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= right;
    if (glm::length(move) < 0.001f)
        return;
    move = glm::normalize(move);

    bool run = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    player->speed = run ? PLAYER_SPEED_RUN : PLAYER_SPEED;
    player->position += move * player->speed * deltaTime;
    player->position.x = glm::clamp(player->position.x, -WORLD_LIMIT, WORLD_LIMIT);
    player->position.z = glm::clamp(player->position.z, -WORLD_LIMIT, WORLD_LIMIT);

    // Girar al jugador poco a poco hacia donde camina
    float target = glm::degrees(atan2(move.x, move.z));
    float diff = fmod(target - player->rotationY + 540.0f, 360.0f) - 180.0f;
    float step = PLAYER_TURN_SPEED * deltaTime;
    player->rotationY += glm::clamp(diff, -step, step);
}

void setCameraMode(CameraMode mode)
{
    cameraMode = mode;
    if (mode == CameraMode::ThirdPerson)
        placeOrbitBehindPlayer();
    std::string title = std::string("Arena Limpia - ") +
        (mode == CameraMode::Free ? "camara libre" : std::string("tercera persona: ") + PLAYER_NAMES[followedPlayer]);
    glfwSetWindowTitle(window, title.c_str());
}

bool Update()
{
    float currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    for (SceneObject& obj : sceneObjects)
        obj.speed = 0.0f;
    processInput(window);
    stepSimulation(deltaTime);

    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (screenHeight > 0) ? (float)screenWidth / (float)screenHeight : 1.0f;
    glm::mat4 view;
    glm::vec3 eye;
    float fov;
    if (cameraMode == CameraMode::ThirdPerson) {
        view = thirdPersonView(eye);
        fov = 50.0f;
    }
    else {
        view = camera.GetViewMatrix();
        eye = camera.Position;
        fov = camera.Zoom;
    }
    glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, 0.1f, 1000.0f);

    // Skybox: sin la traslación de la cámara para que siempre quede "al infinito"
    glm::mat4 skyView = glm::mat4(glm::mat3(view));
    skybox->drawCubeMap(*skyboxShader, projection, skyView);

    phongShader->use();
    phongShader->setMat4("projection", projection);
    phongShader->setMat4("view", view);
    phongShader->setVec3("viewPos", eye);
    phongShader->setVec3("sunDirection", sunDirection);
    glm::vec3 sunNow = nightMode ? sunColor * 0.12f : sunColor;   // de noche casi no hay sol
    float ambientNow = nightMode ? 0.12f : 0.35f;
    phongShader->setVec3("sunColor", sunNow);
    phongShader->setFloat("ambientStrength", ambientNow);
    phongShader->setVec3("pointPosition", fireLightPos);
    phongShader->setVec3("pointColor", fireLightColor);
    phongShader->setFloat("specularStrength", 0.25f);
    phongShader->setFloat("shininess", 32.0f);
    phongShader->setVec2("uvScale", glm::vec2(1.0f));
    phongShader->setFloat("emission", 0.0f);

    skinShader->use();
    skinShader->setMat4("projection", projection);
    skinShader->setMat4("view", view);
    skinShader->setVec3("viewPos", eye);
    skinShader->setVec3("sunDirection", sunDirection);
    skinShader->setVec3("sunColor", sunNow);
    skinShader->setFloat("ambientStrength", ambientNow);
    skinShader->setVec3("pointPosition", fireLightPos);
    skinShader->setVec3("pointColor", fireLightColor);
    skinShader->setFloat("specularStrength", 0.25f);
    skinShader->setFloat("shininess", 32.0f);
    skinShader->setVec2("uvScale", glm::vec2(1.0f));
    skinShader->setFloat("emission", 0.0f);
    phongShader->use();

    // Piso de arena
    phongShader->setMat4("model", glm::mat4(1.0f));
    ground->Draw(*phongShader);

    // Primero los objetos sólidos y al final los traslúcidos, para que se vea lo que está detrás.
    // Los traslúcidos se mezclan con una transparencia fija (glBlendColor) y no escriben
    // profundidad, así no tapan a los demás traslúcidos.
    for (const SceneObject& obj : sceneObjects)
        if (!isTranslucent(obj))
            drawObject(obj);

    // Mar: después de los sólidos para que se vea la arena y los postes del muelle debajo
    drawSea(projection, view, eye);
    phongShader->use();

    glDepthMask(GL_FALSE);
    glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    for (const SceneObject& obj : sceneObjects) {
        if (isTranslucent(obj)) {
            glBlendColor(0.0f, 0.0f, 0.0f, materialOf(obj)->transparency);
            drawObject(obj);
        }
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);

    glUseProgram(0);

    ++frameCount;
    if (!capturePath.empty() && frameCount == 5) {
        saveScreenshot(capturePath);
        return false;
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
    return true;
}

void Shutdown()
{
    std::set<Model*> unique;
    for (SceneObject& obj : sceneObjects) {
        if (obj.model) unique.insert(obj.model);
        delete obj.skinned;
    }
    for (Model* m : unique)
        delete m;
    delete skinShader;
    delete ground;
    delete skybox;
    delete phongShader;
    delete skyboxShader;
    delete waterShader;
    glDeleteVertexArrays(1, &seaVAO);
    glDeleteBuffers(1, &seaVBO);
    glDeleteBuffers(1, &seaEBO);
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    interactionInput(window);

    if (cameraMode == CameraMode::ThirdPerson) {
        movePlayer(window);
        return;
    }

    camera.MovementSpeed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        ? CAMERA_SPEED_FAST : CAMERA_SPEED;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.Position.y += camera.MovementSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        camera.Position.y -= camera.MovementSpeed * deltaTime;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS)
        return;

    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, true);

    if (key == GLFW_KEY_TAB) {
        cursorCaptured = !cursorCaptured;
        glfwSetInputMode(window, GLFW_CURSOR, cursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        firstMouse = true;
    }

    interactionKey(key);

    if (key == GLFW_KEY_P)
        toggleAutoMatch();

    if (key == GLFW_KEY_L)
        nightMode = !nightMode;   // día / noche (para ver la luz de la fogata)

    if (key == GLFW_KEY_V)
        setCameraMode(cameraMode == CameraMode::Free ? CameraMode::ThirdPerson : CameraMode::Free);

    // Acciones del jugador en tercera persona
    if (cameraMode == CameraMode::ThirdPerson) {
        if (SceneObject* player = followedObject()) {
            if (key == GLFW_KEY_Q && playOneShot(*player, "Saque"))  startShot(*player, SHOT_SAQUE);
            if (key == GLFW_KEY_E && playOneShot(*player, "Remate")) startShot(*player, SHOT_REMATE);
            if (key == GLFW_KEY_F) playOneShot(*player, "Festejo");
        }
    }

    if (cameraMode == CameraMode::ThirdPerson && key >= GLFW_KEY_1 && key < GLFW_KEY_1 + PLAYER_COUNT) {
        followedPlayer = key - GLFW_KEY_1;
        setCameraMode(CameraMode::ThirdPerson);
    }

    if (key == GLFW_KEY_F12) {
        fs::create_directories("docs/capturas");
        char name[64];
        std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &now);
        std::strftime(name, sizeof(name), "docs/capturas/captura_%Y%m%d_%H%M%S.bmp", &tm);
        if (saveScreenshot(name))
            std::cout << "Captura guardada en " << name << std::endl;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    screenWidth = width;
    screenHeight = height;
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!cursorCaptured)
        return;

    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    lastX = (float)xpos;
    lastY = (float)ypos;

    if (cameraMode == CameraMode::ThirdPerson) {
        const float sensitivity = 0.15f;
        orbitYaw += xoffset * sensitivity;
        orbitPitch = glm::clamp(orbitPitch + yoffset * sensitivity, -10.0f, 75.0f);   // ya no está invertida
        return;
    }
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (cameraMode == CameraMode::ThirdPerson) {
        orbitDistance = glm::clamp(orbitDistance - (float)yoffset * 0.5f,
            ORBIT_MIN_DISTANCE, ORBIT_MAX_DISTANCE);
        return;
    }
    camera.ProcessMouseScroll((float)yoffset);
}

// ---------------------------------------------------------------------------
// Captura de pantalla en BMP de 24 bits (sin dependencias extra)
// ---------------------------------------------------------------------------
bool saveScreenshot(const std::string& path)
{
    int w = screenWidth, h = screenHeight;
    int rowSize = (w * 3 + 3) & ~3;
    std::vector<unsigned char> pixels(rowSize * h);

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, pixels.data());

    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;

    unsigned int dataSize = rowSize * h;
    unsigned int fileSize = 54 + dataSize;
    unsigned char header[54] = { 'B', 'M' };
    auto put32 = [&](int offset, unsigned int value) {
        for (int i = 0; i < 4; ++i) header[offset + i] = (value >> (8 * i)) & 0xFF;
        };
    put32(2, fileSize);
    put32(10, 54);
    put32(14, 40);
    put32(18, (unsigned int)w);
    put32(22, (unsigned int)h);   // alto positivo: filas de abajo hacia arriba, como OpenGL
    header[26] = 1;
    header[28] = 24;
    put32(34, dataSize);

    out.write((const char*)header, 54);
    out.write((const char*)pixels.data(), dataSize);
    return true;
}