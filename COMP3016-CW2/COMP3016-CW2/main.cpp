#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// stb_image for texture loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------
static const int WIDTH = 1280;
static const int HEIGHT = 720;

GLFWwindow* gWindow = nullptr;

// framebuffer size (can differ from window size with DPI / fullscreen)
int gFBWidth = WIDTH;
int gFBHeight = HEIGHT;

// Camera state
glm::vec3 cameraPos = glm::vec3(0.0f, 3.0f, 8.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.3f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float yaw = -90.0f;
float pitch = -15.0f;

double lastX = WIDTH * 0.5;
double lastY = HEIGHT * 0.5;

bool firstMouse = true;
bool mouseLocked = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// keep player inside terrain
float worldLimit = 50.0f;

// fullscreen toggle state
bool fullscreen = false;
int  windowedX = 100;
int  windowedY = 100;
int  windowedW = WIDTH;
int  windowedH = HEIGHT;

// Terrain settings
int   gTerrainSize = 100;   // quads per side
float gTerrainStep = 1.0f;  // spacing
float gHeightScale = 1.5f;  // hills

// Player movement & physics
float walkSpeed = 5.0f;
float runMultiplier = 2.0f;
float gravity = 20.0f;
float jumpSpeed = 8.0f;
float verticalVelocity = 0.0f;
bool  isGrounded = false;
float eyeHeight = 1.5f;

// -----------------------------------------------------------------------------
// Shaders – lighting + alpha cutout for leaves
// -----------------------------------------------------------------------------
const char* vertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 u_Model;
uniform mat4 u_MVP;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
    gl_Position = u_MVP * vec4(aPos, 1.0);
    FragPos = vec3(u_Model * vec4(aPos, 1.0));
    Normal  = mat3(transpose(inverse(u_Model))) * aNormal;
    TexCoord = aTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uViewPos;

void main()
{
    vec4 texSample = texture(uTexture, TexCoord);

    // Alpha cutout for leaf textures (PNG alpha). Safe for trunk too.
    if (texSample.a < 0.1) discard;

    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(-uLightDir); // direction TO light
    float diff    = max(dot(norm, lightDir), 0.0);

    vec3 viewDir    = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

    vec3 ambient  = 0.30 * uLightColor;
    vec3 diffuse  = 0.70 * diff * uLightColor;
    vec3 specular = 0.20 * spec * uLightColor;

    vec3 texColor = texSample.rgb;
    vec3 result   = (ambient + diffuse + specular) * texColor;

    FragColor = vec4(result, texSample.a);
}
)";

GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << "\n";
    }
    return shader;
}

GLuint createShaderProgram()
{
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking failed: " << infoLog << "\n";
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}

// -----------------------------------------------------------------------------
// Terrain height + generation with normals
// -----------------------------------------------------------------------------
float sampleTerrainHeight(float worldX, float worldZ)
{
    float h =
        sinf(worldX * 0.2f) * cosf(worldZ * 0.2f) * gHeightScale +
        sinf(worldX * 0.05f + worldZ * 0.1f) * gHeightScale * 0.5f;
    return h;
}

// vertices layout: pos(3), normal(3), uv(2) = 8 floats per vertex
void generateTerrain(int size, float spacing, float heightScale,
    std::vector<float>& vertices,
    std::vector<unsigned int>& indices)
{
    vertices.clear();
    indices.clear();

    int gridSize = size;
    int vertPerSide = gridSize + 1;
    int vertCount = vertPerSide * vertPerSide;

    std::vector<glm::vec3> positions(vertCount);
    std::vector<glm::vec3> normals(vertCount, glm::vec3(0.0f));
    std::vector<glm::vec2> uvs(vertCount);

    float uvScale = 0.2f;

    for (int z = 0; z < vertPerSide; ++z)
    {
        for (int x = 0; x < vertPerSide; ++x)
        {
            int i = z * vertPerSide + x;

            float worldX = (x - gridSize / 2.0f) * spacing;
            float worldZ = (z - gridSize / 2.0f) * spacing;
            float h = sampleTerrainHeight(worldX, worldZ);

            positions[i] = glm::vec3(worldX, h, worldZ);
            uvs[i] = glm::vec2(worldX * uvScale, worldZ * uvScale);
        }
    }

    for (int z = 0; z < vertPerSide; ++z)
    {
        for (int x = 0; x < vertPerSide; ++x)
        {
            int i = z * vertPerSide + x;

            int xL = std::max(x - 1, 0);
            int xR = std::min(x + 1, gridSize);
            int zD = std::max(z - 1, 0);
            int zU = std::min(z + 1, gridSize);

            glm::vec3 left = positions[z * vertPerSide + xL];
            glm::vec3 right = positions[z * vertPerSide + xR];
            glm::vec3 down = positions[zD * vertPerSide + x];
            glm::vec3 up = positions[zU * vertPerSide + x];

            glm::vec3 dx = right - left;
            glm::vec3 dz = up - down;

            glm::vec3 normal = glm::normalize(glm::cross(dz, dx));
            normals[i] = normal;
        }
    }

    vertices.reserve(vertCount * 8);
    for (int i = 0; i < vertCount; ++i)
    {
        vertices.push_back(positions[i].x);
        vertices.push_back(positions[i].y);
        vertices.push_back(positions[i].z);

        vertices.push_back(normals[i].x);
        vertices.push_back(normals[i].y);
        vertices.push_back(normals[i].z);

        vertices.push_back(uvs[i].x);
        vertices.push_back(uvs[i].y);
    }

    for (int z = 0; z < gridSize; ++z)
    {
        for (int x = 0; x < gridSize; ++x)
        {
            int topLeft = z * vertPerSide + x;
            int topRight = z * vertPerSide + x + 1;
            int bottomLeft = (z + 1) * vertPerSide + x;
            int bottomRight = (z + 1) * vertPerSide + x + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
}

// -----------------------------------------------------------------------------
// Fullscreen toggle
// -----------------------------------------------------------------------------
void toggleFullscreen()
{
    fullscreen = !fullscreen;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    if (fullscreen)
    {
        glfwGetWindowPos(gWindow, &windowedX, &windowedY);
        glfwGetWindowSize(gWindow, &windowedW, &windowedH);

        glfwSetWindowMonitor(
            gWindow,
            monitor,
            0, 0,
            mode->width,
            mode->height,
            mode->refreshRate
        );
        glfwSwapInterval(1);
    }
    else
    {
        glfwSetWindowMonitor(
            gWindow,
            nullptr,
            windowedX,
            windowedY,
            windowedW,
            windowedH,
            0
        );
        glfwSwapInterval(1);
    }
}

// -----------------------------------------------------------------------------
// Callbacks
// -----------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    gFBWidth = w;
    gFBHeight = h;
    glViewport(0, 0, w, h);
}

void cursor_pos_callback(GLFWwindow*, double xpos, double ypos)
{
    if (!mouseLocked) return;

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos - lastX);
    float yoffset = static_cast<float>(lastY - ypos);

    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cosf(glm::radians(yaw)) * cosf(glm::radians(pitch));
    front.y = sinf(glm::radians(pitch));
    front.z = sinf(glm::radians(yaw)) * cosf(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void key_callback(GLFWwindow* window, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        if (mouseLocked)
        {
            mouseLocked = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        else
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    if (key == GLFW_KEY_F11 && action == GLFW_PRESS)
    {
        toggleFullscreen();
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        if (!mouseLocked)
        {
            mouseLocked = true;
            firstMouse = true;

            double cx, cy;
            glfwGetCursorPos(window, &cx, &cy);
            lastX = cx;
            lastY = cy;

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}

void window_focus_callback(GLFWwindow*, int focused)
{
    if (focused && mouseLocked)
    {
        firstMouse = true;
        glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

// -----------------------------------------------------------------------------
// Movement (walk / run / jump grounded)
// -----------------------------------------------------------------------------
void processMovement(float dt)
{
    float speed = walkSpeed;
    if (glfwGetKey(gWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        speed *= runMultiplier;

    glm::vec3 moveDir(0.0f);

    if (glfwGetKey(gWindow, GLFW_KEY_W) == GLFW_PRESS)
        moveDir += glm::vec3(cameraFront.x, 0.0f, cameraFront.z);
    if (glfwGetKey(gWindow, GLFW_KEY_S) == GLFW_PRESS)
        moveDir -= glm::vec3(cameraFront.x, 0.0f, cameraFront.z);

    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
    if (glfwGetKey(gWindow, GLFW_KEY_A) == GLFW_PRESS)
        moveDir -= glm::vec3(right.x, 0.0f, right.z);
    if (glfwGetKey(gWindow, GLFW_KEY_D) == GLFW_PRESS)
        moveDir += glm::vec3(right.x, 0.0f, right.z);

    if (glm::length(moveDir) > 0.0f)
        moveDir = glm::normalize(moveDir);

    cameraPos += moveDir * speed * dt;

    if (cameraPos.x > worldLimit) cameraPos.x = worldLimit;
    if (cameraPos.x < -worldLimit) cameraPos.x = -worldLimit;
    if (cameraPos.z > worldLimit) cameraPos.z = worldLimit;
    if (cameraPos.z < -worldLimit) cameraPos.z = -worldLimit;

    if (glfwGetKey(gWindow, GLFW_KEY_SPACE) == GLFW_PRESS && isGrounded)
    {
        isGrounded = false;
        verticalVelocity = jumpSpeed;
    }

    verticalVelocity -= gravity * dt;
    cameraPos.y += verticalVelocity * dt;

    float terrainY = sampleTerrainHeight(cameraPos.x, cameraPos.z) + eyeHeight;

    if (cameraPos.y <= terrainY)
    {
        cameraPos.y = terrainY;
        verticalVelocity = 0.0f;
        isGrounded = true;
    }
}

// -----------------------------------------------------------------------------
// Texture loading
// -----------------------------------------------------------------------------
GLuint loadTexture(const char* path)
{
    int width, height, channels;

    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);

    if (!data)
    {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLenum format = GL_RGB;
    if (channels == 1)      format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexImage2D(GL_TEXTURE_2D,
        0,
        format,
        width,
        height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return texID;
}

// -----------------------------------------------------------------------------
// Path helpers (portable relative texture loading)
// -----------------------------------------------------------------------------
static std::string getDirectory(const std::string& filepath)
{
    size_t slash = filepath.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    return filepath.substr(0, slash);
}

static std::string joinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    char last = a.back();
    if (last == '/' || last == '\\') return a + b;
    return a + "/" + b;
}

// -----------------------------------------------------------------------------
// Mesh struct (per-mesh texture from MTL)
// -----------------------------------------------------------------------------
struct Mesh
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLsizei indexCount = 0;
    GLuint diffuseTex = 0; // loaded from MTL (aiTextureType_DIFFUSE) if present
};

std::vector<Mesh> loadAllMeshesAssimp(const std::string& path)
{
    std::vector<Mesh> meshes;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes |
        aiProcess_FlipUVs
    );

    if (!scene || !scene->mRootNode || scene->mNumMeshes == 0)
    {
        std::cerr << "ASSIMP failed to load model: " << path
            << " (" << importer.GetErrorString() << ")\n";
        return meshes;
    }

    std::string dir = getDirectory(path);
    meshes.reserve(scene->mNumMeshes);

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* aMesh = scene->mMeshes[m];
        Mesh mesh;

        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        vertices.reserve(aMesh->mNumVertices * 8);

        for (unsigned int i = 0; i < aMesh->mNumVertices; ++i)
        {
            const aiVector3D& pos = aMesh->mVertices[i];

            aiVector3D norm(0, 1, 0);
            if (aMesh->HasNormals())
                norm = aMesh->mNormals[i];

            aiVector3D uv(0, 0, 0);
            if (aMesh->HasTextureCoords(0))
                uv = aMesh->mTextureCoords[0][i];

            vertices.push_back(pos.x);
            vertices.push_back(pos.y);
            vertices.push_back(pos.z);

            vertices.push_back(norm.x);
            vertices.push_back(norm.y);
            vertices.push_back(norm.z);

            vertices.push_back(uv.x);
            vertices.push_back(uv.y);
        }

        for (unsigned int f = 0; f < aMesh->mNumFaces; ++f)
        {
            const aiFace& face = aMesh->mFaces[f];
            if (face.mNumIndices != 3) continue;
            indices.push_back(face.mIndices[0]);
            indices.push_back(face.mIndices[1]);
            indices.push_back(face.mIndices[2]);
        }

        mesh.indexCount = static_cast<GLsizei>(indices.size());

        glGenVertexArrays(1, &mesh.VAO);
        glGenBuffers(1, &mesh.VBO);
        glGenBuffers(1, &mesh.EBO);

        glBindVertexArray(mesh.VAO);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
        glBufferData(GL_ARRAY_BUFFER,
            vertices.size() * sizeof(float),
            vertices.data(),
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(unsigned int),
            indices.data(),
            GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            8 * sizeof(float),
            (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
            8 * sizeof(float),
            (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
            8 * sizeof(float),
            (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        // Load diffuse texture from MTL (if present)
        if (aMesh->mMaterialIndex >= 0 && scene->mNumMaterials > 0)
        {
            aiMaterial* mat = scene->mMaterials[aMesh->mMaterialIndex];
            aiString texPath;
            if (mat && mat->GetTextureCount(aiTextureType_DIFFUSE) > 0 &&
                mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
            {
                std::string fullPath = joinPath(dir, texPath.C_Str());
                mesh.diffuseTex = loadTexture(fullPath.c_str());
                if (mesh.diffuseTex == 0)
                {
                    std::cerr << "Warning: could not load diffuse texture: "
                        << fullPath << " (from " << path << ")\n";
                }
            }
        }

        std::cout << "Loaded mesh " << m << " from " << path
            << " verts: " << aMesh->mNumVertices
            << " indices: " << mesh.indexCount
            << " tex: " << (mesh.diffuseTex ? "yes" : "no") << "\n";

        meshes.push_back(mesh);
    }

    return meshes;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main()
{
    if (!glfwInit())
    {
        std::cerr << "GLFW init failed\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    gWindow = glfwCreateWindow(WIDTH, HEIGHT, "Interactive 3D Scene Explorer", nullptr, nullptr);
    if (!gWindow)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(gWindow);
    glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(gWindow, framebuffer_size_callback);
    glfwSetCursorPosCallback(gWindow, cursor_pos_callback);
    glfwSetKeyCallback(gWindow, key_callback);
    glfwSetMouseButtonCallback(gWindow, mouse_button_callback);
    glfwSetWindowFocusCallback(gWindow, window_focus_callback);
    glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        glfwDestroyWindow(gWindow);
        glfwTerminate();
        return 1;
    }

    glfwGetFramebufferSize(gWindow, &gFBWidth, &gFBHeight);
    glViewport(0, 0, gFBWidth, gFBHeight);

    glEnable(GL_DEPTH_TEST);

    // Leaf textures often use alpha. This makes them render correctly.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";

    GLuint shaderProgram = createShaderProgram();

    // -------- Terrain setup --------
    std::vector<float>        terrainVertices;
    std::vector<unsigned int> terrainIndices;

    generateTerrain(gTerrainSize, gTerrainStep, gHeightScale,
        terrainVertices, terrainIndices);

    worldLimit = gTerrainSize * gTerrainStep * 0.5f - 2.0f;

    GLuint terrainVAO, terrainVBO, terrainEBO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glGenBuffers(1, &terrainEBO);

    glBindVertexArray(terrainVAO);

    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER,
        terrainVertices.size() * sizeof(float),
        terrainVertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        terrainIndices.size() * sizeof(unsigned int),
        terrainIndices.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        8 * sizeof(float),
        (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
        8 * sizeof(float),
        (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
        8 * sizeof(float),
        (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    // -------------------------------

    // Load textures
    GLuint grassTex = loadTexture("assets/grass.png");
    if (grassTex == 0)
    {
        std::cerr << "Warning: grass texture not loaded, you'll see white terrain.\n";
    }

    // Tree model (OBJ + MTL). Loads *all* meshes (leaves + trunk).
    std::vector<Mesh> treeMeshes = loadAllMeshesAssimp("assets/tree.obj");
    bool hasTree = !treeMeshes.empty();

    glUseProgram(shaderProgram);
    GLint texLoc = glGetUniformLocation(shaderProgram, "uTexture");
    glUniform1i(texLoc, 0);

    float startTerrainY = sampleTerrainHeight(cameraPos.x, cameraPos.z) + eyeHeight;
    cameraPos.y = startTerrainY;
    isGrounded = true;
    verticalVelocity = 0.0f;

    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.2f));
    glm::vec3 lightColor = glm::vec3(1.0f, 0.96f, 0.9f);

    GLint lightDirLoc = glGetUniformLocation(shaderProgram, "uLightDir");
    GLint lightColorLoc = glGetUniformLocation(shaderProgram, "uLightColor");
    GLint viewPosLoc = glGetUniformLocation(shaderProgram, "uViewPos");
    GLint modelLoc = glGetUniformLocation(shaderProgram, "u_Model");
    GLint mvpLoc = glGetUniformLocation(shaderProgram, "u_MVP");

    while (!glfwWindowShouldClose(gWindow))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        processMovement(deltaTime);

        glClearColor(0.05f, 0.05f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 view = glm::lookAt(
            cameraPos,
            cameraPos + cameraFront,
            cameraUp
        );

        float aspect = (gFBHeight > 0)
            ? static_cast<float>(gFBWidth) / static_cast<float>(gFBHeight)
            : 16.0f / 9.0f;

        glm::mat4 projection = glm::perspective(
            glm::radians(60.0f),
            aspect,
            0.1f,
            300.0f
        );

        glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));
        glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
        glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));

        // ---------- draw terrain ----------
        glm::mat4 terrainModel = glm::mat4(1.0f);
        glm::mat4 terrainMVP = projection * view * terrainModel;

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(terrainModel));
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(terrainMVP));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTex);

        glBindVertexArray(terrainVAO);
        glDrawElements(GL_TRIANGLES,
            static_cast<GLsizei>(terrainIndices.size()),
            GL_UNSIGNED_INT,
            0);
        glBindVertexArray(0);

        // ---------- draw tree ----------
        if (hasTree)
        {
            float tx = 5.0f;
            float tz = 5.0f;
            float ty = sampleTerrainHeight(tx, tz);

            glm::mat4 treeModel = glm::mat4(1.0f);
            treeModel = glm::translate(treeModel, glm::vec3(tx, ty, tz));
            treeModel = glm::scale(treeModel, glm::vec3(2.0f)); // tweak if needed

            glm::mat4 treeMVP = projection * view * treeModel;

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(treeModel));
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(treeMVP));

            glActiveTexture(GL_TEXTURE0);

            for (const Mesh& m : treeMeshes)
            {
                GLuint texToUse = (m.diffuseTex != 0) ? m.diffuseTex : grassTex;
                glBindTexture(GL_TEXTURE_2D, texToUse);

                glBindVertexArray(m.VAO);
                glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
            }

            glBindVertexArray(0);
        }

        glfwSwapBuffers(gWindow);
    }

    glDeleteTextures(1, &grassTex);

    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteBuffers(1, &terrainEBO);

    for (Mesh& m : treeMeshes)
    {
        if (m.diffuseTex != 0)
            glDeleteTextures(1, &m.diffuseTex);

        if (m.VAO != 0)
        {
            glDeleteVertexArrays(1, &m.VAO);
            glDeleteBuffers(1, &m.VBO);
            glDeleteBuffers(1, &m.EBO);
        }
    }

    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(gWindow);
    glfwTerminate();
    return 0;
}
