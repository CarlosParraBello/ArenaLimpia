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
*   Esc            salir
*
* Argumentos opcionales (para pruebas):
*   --captura <archivo.bmp>   dibuja unos cuadros, guarda la imagen y termina
*   --camara x y z yaw pitch  posición y orientación inicial de la cámara
*   --tercera [1-4]           empieza en tercera persona siguiendo a ese jugador
*   --simular <Saque|Remate> <s>  el jugador seguido hace la acción y se adelantan s segundos
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
void initBall();

// ---------------------------------------------------------------------------
// Ventana y tiempo
// ---------------------------------------------------------------------------
GLFWwindow* window = nullptr;
int screenWidth  = 1280;
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
const float CAMERA_SPEED      = 5.0f;
const float CAMERA_SPEED_FAST = 15.0f;

// ---------------------------------------------------------------------------
// Cámara en tercera persona: sigue a un jugador y orbita a su alrededor
// ---------------------------------------------------------------------------
enum class CameraMode { Free, ThirdPerson };
CameraMode cameraMode = CameraMode::Free;

const char* PLAYER_NAMES[] = { "JugadorMX", "JugadorPR", "JugadoraHA", "JugadoraJA" };
const int   PLAYER_COUNT   = 4;
int   followedPlayer = 0;        // índice en PLAYER_NAMES
float orbitYaw       = -90.0f;   // grados, alrededor del jugador
float orbitPitch     = 20.0f;    // grados, positivo = cámara por encima
float orbitDistance  = 5.0f;     // metros
const float ORBIT_MIN_DISTANCE = 2.0f;
const float ORBIT_MAX_DISTANCE = 15.0f;
const float PLAYER_SPEED       = 2.0f;   // m/s caminando
const float PLAYER_SPEED_RUN   = 4.5f;   // m/s corriendo (Shift)
const float WALK_CYCLE_METERS  = 1.5f;   // distancia que avanza un ciclo de "Caminar" (dos pasos)
const float PLAYER_TURN_SPEED  = 540.0f; // grados/s
const float WORLD_LIMIT        = 55.0f;  // el piso mide 120 x 120 m
glm::vec3 thirdPersonEye(0.0f);

// ---------------------------------------------------------------------------
// Iluminación (sol)
// ---------------------------------------------------------------------------
glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
glm::vec3 sunColor(1.0f, 0.97f, 0.9f);

// ---------------------------------------------------------------------------
// Escena
// ---------------------------------------------------------------------------
Shader*  phongShader  = nullptr;
Shader*  skinShader   = nullptr;   // phong con skinning para personajes animados
Shader*  skyboxShader = nullptr;
CubeMap* skybox       = nullptr;
Mesh*    ground       = nullptr;

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
    float speed    = 0.0f;   // m/s en este cuadro (0 = parado)
    int   oneShot  = -1;     // animación que se reproduce una vez (saque, remate...) o -1
    float oneShotTime = 0.0f;
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
    // Para objetos con física (el balón): position es el centro y "spin" su orientación
    bool      centerPivot = false;
    glm::mat4 spin = glm::mat4(1.0f);
};

#define CP(m) "assets/models/CarlosParra/" m "/" m ".obj"
#define GY(m) "assets/models/GabrielYukioayax/" m "/" m ".obj"
#define GYF(dir, file) "assets/models/GabrielYukioayax/" dir "/" file

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
    { "Gaviota",            GYF("Gaviota", "Mesh_Seagull.obj"),      {  -3.0f, 5.0f, -16.0f },   0.0f, 0.5f  },
    { "Puerto",             GY("Puerto"),                            {   0.0f, 0.0f, -36.0f },   0.0f, 2.5f  },
    { "Bote",               GYF("Bote", "Boat Texture 1.obj"),       {   7.0f, 0.0f, -34.0f },  20.0f, 1.5f  },
    { "Faro",               GYF("Faro", "Lighthouse singly.obj"),    { -40.0f, 0.0f, -45.0f },   0.0f, 20.0f },
};

#undef CP
#undef GY
#undef GYF

// Modo de captura para pruebas automáticas
std::string capturePath;
std::string simulateAction;      // --simular <Saque|Remate> <s>: adelanta la simulación (pruebas)
float simulateSeconds = 0.0f;
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
            camera.Yaw   = std::stof(argv[i + 4]);
            camera.Pitch = std::stof(argv[i + 5]);
            camera.ProcessMouseMovement(0.0f, 0.0f);   // recalcula los vectores de la cámara
            i += 5;
        }
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

// Crea un plano cuadrado en y = 0 usando la clase Mesh del curso.
Mesh* createGround(float halfSize, glm::vec4 color)
{
    std::vector<Vertex> vertices(4);
    glm::vec3 corners[4] = {
        { -halfSize, 0.0f, -halfSize }, { halfSize, 0.0f, -halfSize },
        {  halfSize, 0.0f,  halfSize }, { -halfSize, 0.0f,  halfSize }
    };
    glm::vec2 uvs[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    for (int i = 0; i < 4; ++i) {
        Vertex v{};
        v.Position  = corners[i];
        v.Normal    = glm::vec3(0.0f, 1.0f, 0.0f);
        v.TexCoords = uvs[i];
        v.Tangent   = glm::vec3(1.0f, 0.0f, 0.0f);
        v.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
        vertices[i] = v;
    }
    std::vector<unsigned int> indices = { 0, 2, 1, 0, 3, 2 };
    return new Mesh(vertices, indices, {}, color);
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

    phongShader  = new Shader("shaders/phong.vs", "shaders/phong.fs");
    skinShader   = new Shader("shaders/phong_skinning.vs", "shaders/phong.fs");
    skyboxShader = new Shader("shaders/skybox.vs", "shaders/skybox.fs");

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

    ground = createGround(60.0f, glm::vec4(0.86f, 0.76f, 0.57f, 1.0f));

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
                obj.skinned  = sk;
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
        } else {
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

    if (obj.skinned) {
        skinShader->use();
        skinShader->setMat4("model", model);
        obj.skinned->Draw(*skinShader);
        phongShader->use();
    } else {
        phongShader->setMat4("model", model);
        obj.model->Draw(*phongShader);
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
// Balón: tiro parabólico (animación procedural)
//
// El balón sigue a la mano mientras el jugador lo sostiene. En el lanzamiento y en el golpe
// se calcula la velocidad inicial que lo lleva a un punto en un tiempo dado:
//     p(T) = p0 + v0 T + g T^2 / 2   =>   v0 = (p1 - p0) / T - g T / 2
// Después vuela con gravedad, rebota en la arena perdiendo energía, choca con la red y rueda
// hasta detenerse. Gira según su velocidad.
// ---------------------------------------------------------------------------
const glm::vec3 GRAVITY(0.0f, -9.81f, 0.0f);
const float BALL_RADIUS      = 0.105f;
const float BALL_RESTITUTION = 0.55f;   // fracción de la velocidad vertical que conserva al botar
const float BALL_FRICTION    = 0.75f;   // fracción de la velocidad horizontal que conserva al botar
const float BALL_ROLL_DAMP   = 1.5f;    // frenado al rodar (1/s)
const float NET_HALF_WIDTH   = 5.25f;   // red de RedVoley en z = 0
const float NET_BOTTOM       = 1.44f;
const float NET_TOP          = 2.43f;

// Un golpe: en qué momento de la animación se lanza/golpea y a dónde va el balón
struct ShotDef {
    const char* anim;
    bool  heldAtStart;     // true: el balón empieza en la mano izquierda (saque)
    float tossTime;        // s: se suelta (saque) o lo pasa un compañero (remate)
    float hitTime;         // s: contacto con la mano derecha
    float targetDistance;  // m al frente del jugador donde cae
    float flightTime;      // s desde el golpe hasta caer
};
const ShotDef SHOT_SAQUE  = { "Saque",  true,  0.55f, 22.0f / 24.0f, 13.0f, 1.25f };
const ShotDef SHOT_REMATE = { "Remate", false, 0.05f, 21.0f / 24.0f,  6.5f, 0.45f };

enum class BallState { Resting, Held, Flying };
struct Ball {
    SceneObject* obj = nullptr;
    BallState state = BallState::Resting;
    glm::vec3 pos = glm::vec3(0.0f);
    glm::vec3 vel = glm::vec3(0.0f);
    glm::mat4 orientation = glm::mat4(1.0f);
    // Golpe en curso
    SceneObject*   shooter = nullptr;
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
        } else {
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
    } else if (ball.phase == 1 && t >= s.hitTime) {
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

// Avanza todo lo que se mueve solo: balón, personajes y árbitro.
// El balón va primero porque lee el tiempo de la animación del jugador antes de que avance;
// además boneWorldAt cambia la pose y animateCharacter la deja como corresponde.
void stepSimulation(float dt)
{
    updateBall(dt);
    for (SceneObject& obj : sceneObjects)
        animateCharacter(obj, dt);
    updateReferee(dt);
}

// --simular: el jugador seguido hace la acción y se adelanta la simulación (para capturas)
void runSimulationTest()
{
    SceneObject* player = findObject(PLAYER_NAMES[followedPlayer]);
    if (!player) return;
    const ShotDef& shot = (simulateAction == "Remate") ? SHOT_REMATE : SHOT_SAQUE;
    if (!playOneShot(*player, shot.anim)) return;
    startShot(*player, shot);
    const float dt = 1.0f / 120.0f;
    int steps = (int)(simulateSeconds / dt);
    for (int i = 0; i <= steps; i++) {
        if (i % 12 == 0)
            printf("t=%.2f balon=(%.2f, %.2f, %.2f) estado=%d\n", i * dt,
                   ball.pos.x, ball.pos.y, ball.pos.z, (int)ball.state);
        if (i < steps)
            stepSimulation(dt);
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
    } else {
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
    phongShader->setVec3("sunColor", sunColor);
    phongShader->setFloat("ambientStrength", 0.35f);
    phongShader->setFloat("specularStrength", 0.25f);
    phongShader->setFloat("shininess", 32.0f);
    phongShader->setVec2("uvScale", glm::vec2(1.0f));

    skinShader->use();
    skinShader->setMat4("projection", projection);
    skinShader->setMat4("view", view);
    skinShader->setVec3("viewPos", eye);
    skinShader->setVec3("sunDirection", sunDirection);
    skinShader->setVec3("sunColor", sunColor);
    skinShader->setFloat("ambientStrength", 0.35f);
    skinShader->setFloat("specularStrength", 0.25f);
    skinShader->setFloat("shininess", 32.0f);
    skinShader->setVec2("uvScale", glm::vec2(1.0f));
    phongShader->use();

    // Piso de arena
    phongShader->setMat4("model", glm::mat4(1.0f));
    ground->Draw(*phongShader);

    for (const SceneObject& obj : sceneObjects)
        drawObject(obj);

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
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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
        orbitYaw   += xoffset * sensitivity;
        orbitPitch  = glm::clamp(orbitPitch - yoffset * sensitivity, -10.0f, 75.0f);
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
