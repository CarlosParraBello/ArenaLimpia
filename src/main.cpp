/*
* Arena Limpia - Torneo de Voleibol de Playa
* Proyecto final del Laboratorio de Computación Gráfica e Interacción Humano-Computadora
* Facultad de Ingeniería, UNAM. Semestre 2027-1.
*
* Proyecto base: ventana OpenGL 3.3 core, cámara libre, skybox, piso de arena,
* iluminación de Phong con una luz direccional y carga de modelos OBJ con Assimp.
*
* Controles:
*   W A S D        mover la cámara
*   Espacio / C    subir / bajar
*   Shift          moverse más rápido
*   Mouse          mirar alrededor
*   Rueda          zoom
*   Tab            mostrar / ocultar el cursor
*   M / N          malla de alambre / relleno
*   F12            captura de pantalla en docs/capturas/
*   Esc            salir
*
* Argumentos opcionales (para pruebas):
*   --captura <archivo.bmp>   dibuja unos cuadros, guarda la imagen y termina
*   --camara x y z yaw pitch  posición y orientación inicial de la cámara
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
bool saveScreenshot(const std::string& path);

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
// Iluminación (sol)
// ---------------------------------------------------------------------------
glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
glm::vec3 sunColor(1.0f, 0.97f, 0.9f);

// ---------------------------------------------------------------------------
// Escena
// ---------------------------------------------------------------------------
Shader*  phongShader  = nullptr;
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
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
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
    }

    if (!findAssetsRoot()) {
        std::cout << "No se encontró la carpeta shaders/ junto al ejecutable." << std::endl;
        return -1;
    }

    if (!Start()) {
        glfwTerminate();
        return -1;
    }

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

    return true;
}

void drawObject(const SceneObject& obj)
{
    // Apoyar el modelo sobre la arena y centrarlo en su posición
    glm::vec3 center = 0.5f * (obj.boundsMin + obj.boundsMax);
    glm::vec3 pivot(center.x, obj.boundsMin.y, center.z);

    glm::mat4 model(1.0f);
    model = glm::translate(model, obj.position);
    model = glm::rotate(model, glm::radians(obj.rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
    float sizeY = obj.boundsMax.y - obj.boundsMin.y;
    float scale = (sizeY > 0.001f && obj.height > 0.0f) ? obj.height / sizeY : 1.0f;
    model = glm::scale(model, scale * obj.stretch);
    model = glm::translate(model, -pivot);

    phongShader->setMat4("model", model);
    obj.model->Draw(*phongShader);
}

bool Update()
{
    float currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput(window);

    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (screenHeight > 0) ? (float)screenWidth / (float)screenHeight : 1.0f;
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), aspect, 0.1f, 1000.0f);
    glm::mat4 view = camera.GetViewMatrix();

    // Skybox: sin la traslación de la cámara para que siempre quede "al infinito"
    glm::mat4 skyView = glm::mat4(glm::mat3(view));
    skybox->drawCubeMap(*skyboxShader, projection, skyView);

    phongShader->use();
    phongShader->setMat4("projection", projection);
    phongShader->setMat4("view", view);
    phongShader->setVec3("viewPos", camera.Position);
    phongShader->setVec3("sunDirection", sunDirection);
    phongShader->setVec3("sunColor", sunColor);
    phongShader->setFloat("ambientStrength", 0.35f);
    phongShader->setFloat("specularStrength", 0.25f);
    phongShader->setFloat("shininess", 32.0f);
    phongShader->setVec2("uvScale", glm::vec2(1.0f));

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
    for (SceneObject& obj : sceneObjects)
        unique.insert(obj.model);
    for (Model* m : unique)
        delete m;
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

    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
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
