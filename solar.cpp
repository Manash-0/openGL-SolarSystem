#include <glad/glad.h>
#include <GLFW/glfw3.h>

// GLM Mathematics
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// STB Image Loader
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h" // Use ../ to go up one directory

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <map> 
#include <iomanip> 
#include <sstream> // For formatting strings for ImGui

// ImGui Includes
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Settings ---
unsigned int SCR_WIDTH = 1920;
unsigned int SCR_HEIGHT = 1080;

// --- Camera and Interaction ---
float cameraDistance = 50.0f;
float cameraYaw = 90.0f; 
float cameraPitch = 20.0f;
int focusedPlanet = 0; 
bool isDragging = false;
double lastMouseX, lastMouseY;
int focusedLocationIndex = -1;  // -1 means no location focused
glm::vec3 locationCameraOffset = glm::vec3(0.0f, 0.0f, 5.0f);  // Camera offset from location

// --- Smooth Camera Movement to Location ---
bool isMovingToLocation = false;
float locationCameraDistance = 3.5f;  // Distance from location when focused
float cameraLerpSpeed = 0.08f;  // Smooth transition speed (0-1, higher = faster)
glm::vec3 targetCameraPos = glm::vec3(0.0f);
glm::vec3 currentCameraPos = glm::vec3(0.0f);

// --- Time Control ---
double g_simulationTime = 0.0;
float timeScale = 1.0f; 
const float g_daySpeed = 2.0f;

// --- Planet State ---
vector<glm::vec3> planetPositions(17); // 0-Sun, 1-8 planets, 9-16 moons

// --- Moon Structure ---
struct Moon {
    int parentPlanet;    // Which planet it orbits
    float orbitRadius;   // Distance from parent
    float orbitSpeed;    // Angular speed multiplier
    float size;          // Relative size
    GLuint texture;      // Moon texture
};
vector<Moon> moons;

// --- Asteroid Belt ---
struct Asteroid {
    float orbitRadius;
    float angle;
    float size;
    float yOffset;
};
vector<Asteroid> asteroidBelt;
const int ASTEROID_COUNT = 2000;
glm::mat4* asteroidMatrices;

// --- Geographic Locations on Earth ---
struct GeographicLocation {
    string name;
    string description;
    float latitude;      // In degrees
    float longitude;     // In degrees
    glm::vec3 color;     // Representative color
};

vector<GeographicLocation> earthLocations = {
    {"Ocean", "Pacific Ocean - Deep Blue Waters", 0.0f, -150.0f, glm::vec3(0.0f, 0.3f, 0.8f)},
    {"Mountain", "Himalayan Mountains - Snow Peaks", 28.0f, 84.0f, glm::vec3(0.9f, 0.9f, 0.95f)},
    {"Land", "Amazon Rainforest - Green Land", -5.0f, -65.0f, glm::vec3(0.1f, 0.6f, 0.2f)},
    {"Desert", "Sahara Desert - Golden Sand", 20.0f, 10.0f, glm::vec3(0.9f, 0.75f, 0.3f)}
};

vector<GeographicLocation> saturnLocations = {
    {"Diamond Mountain", "Diamond Mountain - Crystalline Peak", 45.0f, 0.0f, glm::vec3(0.85f, 0.85f, 0.9f)},
    {"Chloric Ocean", "Chloric Ocean - Hydrogen Seas", -30.0f, 90.0f, glm::vec3(0.2f, 0.7f, 0.5f)}
};

int currentLocationIndex = 0;
bool showEarthLocation = false;
int selectedLocationIndex = -1;  // For click selection

int currentSaturnLocationIndex = 0;
bool showSaturnLocation = false;
int selectedSaturnLocationIndex = -1;  // For click selection

// Function to convert latitude/longitude to 3D position on sphere
glm::vec3 latLonToSpherePosition(float latitude, float longitude, float radius) {
    float lat = glm::radians(latitude);
    float lon = glm::radians(longitude);
    float x = radius * cos(lat) * cos(lon);
    float y = radius * sin(lat);
    float z = radius * cos(lat) * sin(lon);
    return glm::vec3(x, y, z);
}

// --- Planet Info ---
struct PlanetInfo {
    string name;
    string size; 
    string rotation; 
    string revolution; 
    string atmosphere;
    string speciality;
    string moons;
};
map<int, PlanetInfo> planetDatabase;

void initializePlanetData() {
    planetDatabase[0] = {"Sun", "109x Earth", "27 Earth days", "N/A (Center)", "Hydrogen, Helium", "Core fusion, provides light and heat", "N/A"};
    planetDatabase[1] = {"Mercury", "0.38x Earth", "59 Earth days", "88 Earth days", "Thin (Sodium, Potassium)", "Extreme temperature swings", "0"};
    planetDatabase[2] = {"Venus", "0.95x Earth", "243 Earth days (Retrograde)", "225 Earth days", "Thick (CO2), Sulfuric acid clouds", "Runaway greenhouse effect, hottest planet", "0"};
    planetDatabase[3] = {"Earth", "1.0x Earth", "1 Earth day", "365.25 Earth days", "Nitrogen, Oxygen", "Supports liquid water and life", "1"};
    planetDatabase[4] = {"Mars", "0.53x Earth", "1.03 Earth days", "687 Earth days", "Thin (CO2)", "Iron oxide 'red' surface, largest volcano (Olympus Mons)", "2"};
    planetDatabase[5] = {"Jupiter", "11.2x Earth", "0.41 Earth days", "11.9 Earth years", "Hydrogen, Helium", "Great Red Spot, strong magnetic field", "79 (known)"};
    planetDatabase[6] = {"Saturn", "9.4x Earth", "0.45 Earth days", "29.5 Earth years", "Hydrogen, Helium", "Extensive and complex ring system", "82 (known)"};
    planetDatabase[7] = {"Uranus", "4.0x Earth", "0.72 Earth days (Retrograde)", "84 Earth years", "Hydrogen, Helium, Methane", "Tilted on its side (98 degrees)", "27 (known)"};
    planetDatabase[8] = {"Neptune", "3.9x Earth", "0.67 Earth days", "164.8 Earth years", "Hydrogen, Helium, Methane", "Strongest winds in solar system", "14 (known)"};
    planetDatabase[9] = {"Moon", "0.27x Earth", "27.3 Earth days (Tidal lock)", "27.3 Earth days (orbits Earth)", "Exosphere", "Stabilizes Earth's axial tilt", "0 (orbits Earth)"};
}

// --- Utility: Shader Class ---
class Shader {
public:
    unsigned int ID;
    Shader(const char* vertexSource, const char* fragmentSource) {
        unsigned int vertex, fragment;
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertexSource, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");
        
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragmentSource, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");
        
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");
        
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }
    void use() { glUseProgram(ID); }
    void setBool(const string &name, bool value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value); }
    void setInt(const string &name, int value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), value); }
    void setFloat(const string &name, float value) const { glUniform1f(glGetUniformLocation(ID, name.c_str()), value); }
    void setVec2(const string &name, const glm::vec2 &value) const { glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); }
    void setVec3(const string &name, const glm::vec3 &value) const { glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); }
    void setMat4(const string &name, const glm::mat4 &mat) const { glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]); }

private:
    void checkCompileErrors(unsigned int shader, string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- " << endl;
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- " << endl;
            }
        }
    }
};

// --- Utility: Sphere Geometry Class ---
class Sphere {
public:
    unsigned int VAO, VBO, EBO;
    unsigned int indexCount;
    Sphere(int sectorCount, int stackCount) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        vector<float> vertices;
        vector<unsigned int> indices;
        float x, y, z, xy;
        float s, t;
        float sectorStep = 2 * M_PI / sectorCount;
        float stackStep = M_PI / stackCount;
        float sectorAngle, stackAngle;
        for (int i = 0; i <= stackCount; ++i) {
            stackAngle = M_PI / 2 - i * stackStep;
            xy = cosf(stackAngle);
            z = sinf(stackAngle);
            for (int j = 0; j <= sectorCount; ++j) {
                sectorAngle = j * sectorStep;
                x = xy * cosf(sectorAngle);
                y = xy * sinf(sectorAngle);
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);
                s = (float)j / sectorCount;
                t = (float)i / stackCount;
                vertices.push_back(s);
                vertices.push_back(t);
            }
        }
        int k1, k2;
        for (int i = 0; i < stackCount; ++i) {
            k1 = i * (sectorCount + 1);
            k2 = k1 + sectorCount + 1;
            for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
                if (i != 0) {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }
                if (i != (stackCount - 1)) {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }
        indexCount = static_cast<unsigned int>(indices.size());
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    }
    void draw() {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }
};

// --- Utility: Ring Geometry ---
unsigned int ringVAO, ringVBO, ringIndexCount;
void createRing(float innerRadius, float outerRadius, int segments) {
    vector<float> vertices;
    vector<unsigned int> indices;
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / segments * 2.0f * M_PI;
        float s = (float)i / segments;
        vertices.push_back(cos(angle) * innerRadius);
        vertices.push_back(0.0f);
        vertices.push_back(sin(angle) * innerRadius);
        vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f);
        vertices.push_back(0.0f); vertices.push_back(s);
        vertices.push_back(cos(angle) * outerRadius);
        vertices.push_back(0.0f);
        vertices.push_back(sin(angle) * outerRadius);
        vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f);
        vertices.push_back(1.0f); vertices.push_back(s);
    }
    for (int i = 0; i < segments; ++i) {
        int i0 = i * 2; int i1 = i0 + 1; int i2 = i0 + 2; int i3 = i0 + 3;
        indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
        indices.push_back(i1); indices.push_back(i3); indices.push_back(i2);
    }
    ringIndexCount = static_cast<unsigned int>(indices.size());
    glGenVertexArrays(1, &ringVAO);
    glGenBuffers(1, &ringVBO);
    unsigned int ringEBO;
    glGenBuffers(1, &ringEBO);
    glBindVertexArray(ringVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ringEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ringIndexCount * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
}

// --- Orbit Globals ---
unsigned int orbitVAO[9], orbitVBO[9], orbitEBO[9], orbitIndexCount[9];

// Orbit data with eccentricity for elliptical paths
struct OrbitData {
    float semiMajor;
    float eccentricity;
    int segments;
};
OrbitData orbitParams[9] = {
    {12.0f, 0.45f, 100},   // Mercury - very elliptical
    {16.0f, 0.25f, 100},   // Venus - more elliptical
    {22.0f, 0.30f, 100},   // Earth - more elliptical
    {30.0f, 0.40f, 100},   // Mars - very elliptical
    {50.0f, 0.25f, 100},   // Jupiter - more elliptical
    {70.0f, 0.35f, 100},   // Saturn - more elliptical
    {85.0f, 0.30f, 100},   // Uranus - more elliptical
    {100.0f, 0.25f, 100},  // Neptune - more elliptical
    {2.5f, 0.25f, 64}      // Moon - more elliptical
};

void createEllipticalOrbit(int planetIndex) {
    vector<float> vertices;
    vector<unsigned int> indices;
    
    float a = orbitParams[planetIndex].semiMajor;  // semi-major axis
    float e = orbitParams[planetIndex].eccentricity; // eccentricity
    float b = a * sqrt(1.0f - e * e); // semi-minor axis
    int segments = orbitParams[planetIndex].segments;
    
    // Create dotted pattern like: . . . . . . . .
    // Draw 2 vertices for dot, skip 2 vertices for gap
    int dotSize = 2;      // vertices per dot
    int gapSize = 2;      // vertices per gap
    int patternSize = dotSize + gapSize;
    
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / segments * 2.0f * M_PI;
        float x = a * cos(angle);
        float z = b * sin(angle);
        
        vertices.push_back(x);
        vertices.push_back(0.0f);
        vertices.push_back(z);
    }
    
    // Create indices for dotted pattern
    for (int i = 0; i < segments; ++i) {
        int posInPattern = i % patternSize;
        // Only connect consecutive vertices within the dot (first 2 in pattern)
        if (posInPattern < dotSize - 1) {
            indices.push_back(i);
            indices.push_back(i + 1);
        }
    }
    
    orbitIndexCount[planetIndex] = static_cast<unsigned int>(indices.size());
    
    glGenVertexArrays(1, &orbitVAO[planetIndex]);
    glGenBuffers(1, &orbitVBO[planetIndex]);
    glGenBuffers(1, &orbitEBO[planetIndex]);
    
    glBindVertexArray(orbitVAO[planetIndex]);
    glBindBuffer(GL_ARRAY_BUFFER, orbitVBO[planetIndex]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, orbitEBO[planetIndex]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, orbitIndexCount[planetIndex] * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    glBindVertexArray(0);
}

// --- Utility: Texture Loader ---
unsigned int loadTexture(const char* path, bool hasAlpha) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format = GL_RGB;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    } else {
        cerr << "Texture failed to load at path: " << path << endl;
        stbi_image_free(data);
    }
    return textureID;
}

// --- Post-Processing Globals ---
unsigned int quadVAO = 0;
unsigned int quadVBO;
unsigned int fboScene, texSceneColor, texBrightMap, rboDepth;
unsigned int fboBloom[2], texBloom[2];
unsigned int fboGodRays, texGodRays;
unsigned int fboComposite, texComposite;
unsigned int fboFinal, texFinal; // We still declare them, just won't use fboFinal
unsigned int texNoise;

// --- Minimap FBO ---
unsigned int fboMinimap, texMinimap;
const int MINIMAP_WIDTH = 400;
const int MINIMAP_HEIGHT = 400;

// --- Setup Screen-Sized Quad ---
void setupScreenQuad() {
    float quadVertices[] = { 
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

// --- Create/Recreate all FBOs and Textures ---
void createFramebuffers(int width, int height) {
    glDeleteFramebuffers(1, &fboScene);
    glDeleteTextures(1, &texSceneColor);
    glDeleteTextures(1, &texBrightMap);
    glDeleteRenderbuffers(1, &rboDepth);
    glDeleteFramebuffers(2, fboBloom);
    glDeleteTextures(2, texBloom);
    glDeleteFramebuffers(1, &fboGodRays);
    glDeleteTextures(1, &texGodRays);
    glDeleteFramebuffers(1, &fboComposite);
    glDeleteTextures(1, &texComposite);
    glDeleteFramebuffers(1, &fboFinal);
    glDeleteTextures(1, &texFinal);

    // --- FBO Pass 1 (Scene) ---
    glGenFramebuffers(1, &fboScene);
    glBindFramebuffer(GL_FRAMEBUFFER, fboScene);
    
    glGenTextures(1, &texSceneColor);
    glBindTexture(GL_TEXTURE_2D, texSceneColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texSceneColor, 0);

    glGenTextures(1, &texBrightMap);
    glBindTexture(GL_TEXTURE_2D, texBrightMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texBrightMap, 0);

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        cerr << "ERROR::FRAMEBUFFER:: fboScene is not complete!" << endl;

    // --- FBO Pass 2 (Bloom) ---
    glGenFramebuffers(2, fboBloom);
    glGenTextures(2, texBloom);
    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, fboBloom[i]);
        glBindTexture(GL_TEXTURE_2D, texBloom[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texBloom[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            cerr << "ERROR::FRAMEBUFFER:: fboBloom[" << i << "] is not complete!" << endl;
    }

    // --- FBO Pass 3 (God Rays) ---
    glGenFramebuffers(1, &fboGodRays);
    glGenTextures(1, &texGodRays);
    glBindFramebuffer(GL_FRAMEBUFFER, fboGodRays);
    glBindTexture(GL_TEXTURE_2D, texGodRays);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texGodRays, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        cerr << "ERROR::FRAMEBUFFER:: fboGodRays is not complete!" << endl;

    // --- FBO Pass 4 (Composite) ---
    glGenFramebuffers(1, &fboComposite);
    glGenTextures(1, &texComposite);
    glBindFramebuffer(GL_FRAMEBUFFER, fboComposite);
    glBindTexture(GL_TEXTURE_2D, texComposite);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texComposite, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        cerr << "ERROR::FRAMEBUFFER:: fboComposite is not complete!" << endl;

    // --- FBO Pass 5 (Final) ---
    // We create it just to avoid delete errors, but it won't be used for rendering
    glGenFramebuffers(1, &fboFinal);
    glGenTextures(1, &texFinal);
    glBindFramebuffer(GL_FRAMEBUFFER, fboFinal);
    glBindTexture(GL_TEXTURE_2D, texFinal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texFinal, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        cerr << "ERROR::FRAMEBUFFER:: fboFinal is not complete!" << endl;

    // --- Minimap FBO ---
    glDeleteFramebuffers(1, &fboMinimap);
    glDeleteTextures(1, &texMinimap);
    glGenFramebuffers(1, &fboMinimap);
    glBindFramebuffer(GL_FRAMEBUFFER, fboMinimap);
    glGenTextures(1, &texMinimap);
    glBindTexture(GL_TEXTURE_2D, texMinimap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, MINIMAP_WIDTH, MINIMAP_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texMinimap, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        cerr << "ERROR::FRAMEBUFFER:: fboMinimap is not complete!" << endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// --- GLFW Callbacks ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (width > 0 && height > 0) {
        SCR_WIDTH = width;
        SCR_HEIGHT = height;
        createFramebuffers(width, height); // Recreate FBOs to match window size
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (isDragging) {
        float xoffset = static_cast<float>(lastMouseX - xpos); 
        float yoffset = static_cast<float>(ypos - lastMouseY);
        lastMouseX = xpos;
        lastMouseY = ypos;

        float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        cameraYaw += xoffset;
        cameraPitch += yoffset;

        if (cameraPitch > 89.0f) cameraPitch = 89.0f;
        if (cameraPitch < -89.0f) cameraPitch = 89.0f;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
            
            // Check for location marker clicks on Earth
            if (focusedPlanet == 3) {  // Earth is focused
                double mouseX, mouseY;
                glfwGetCursorPos(window, &mouseX, &mouseY);
                
                // Check if click is near screen center (where markers appear)
                float distFromCenter = sqrt((mouseX - SCR_WIDTH/2.0f)*(mouseX - SCR_WIDTH/2.0f) + 
                                           (mouseY - SCR_HEIGHT/2.0f)*(mouseY - SCR_HEIGHT/2.0f));
                
                // If clicking near center of screen, focus on a location
                if (distFromCenter < 300.0f) {  // Click radius
                    // Cycle to next location or start from first
                    int nextIndex = (focusedLocationIndex + 1) % earthLocations.size();
                    selectedLocationIndex = nextIndex;
                    currentLocationIndex = nextIndex;
                    focusedLocationIndex = nextIndex;
                    showEarthLocation = true;
                    cameraDistance = 2.5f;  // Zoom in on location
                    isDragging = false;
                } else {
                    isDragging = true;
                }
            } else {
                isDragging = true;
            }
        } else if (action == GLFW_RELEASE) {
            isDragging = false;
        }
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    cameraDistance -= (float)yoffset * (cameraDistance * 0.1f);
    if (cameraDistance < 1.0f) cameraDistance = 1.0f;
    if (cameraDistance > 800.0f) cameraDistance = 800.0f;
}

// Helper: remove all moons that orbit a given planet
void removeMoonsOfPlanet(int planetIndex) {
    for (auto it = moons.begin(); it != moons.end(); ) {
        if (it->parentPlanet == planetIndex) it = moons.erase(it);
        else ++it;
    }
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    static bool plusKeyPressed = false;
    static bool minusKeyPressed = false;
    
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS && !plusKeyPressed) {
        timeScale *= 1.1f;
        cout << "\n===== SPEED INCREASED =====" << endl;
        cout << "Time Scale: " << fixed << setprecision(2) << timeScale << "x" << endl;
        cout << "===========================\n" << endl;
        plusKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_RELEASE) {
        plusKeyPressed = false;
    }
    
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS && !minusKeyPressed) {
        timeScale *= 0.9f;
        if (timeScale < 0.01f) timeScale = 0.01f;
        cout << "\n===== SPEED DECREASED =====" << endl;
        cout << "Time Scale: " << fixed << setprecision(2) << timeScale << "x" << endl;
        cout << "===========================\n" << endl;
        minusKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_RELEASE) {
        minusKeyPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) focusedPlanet = 0;
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) focusedPlanet = 1;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) focusedPlanet = 2;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) focusedPlanet = 3;
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) focusedPlanet = 4;
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) focusedPlanet = 5;
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) focusedPlanet = 6;
    if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) focusedPlanet = 7;
    if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) focusedPlanet = 8;

    // Pressing 9: remove Mars' moon(s) and focus Earth's moon
    static bool nineKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS && !nineKeyPressed) {
        // Remove any moons whose parent is Mars (planet index 4)
        removeMoonsOfPlanet(4);

        // Find Earth's moon index (parentPlanet == 3)
        int earthMoonIndex = -1;
        for (int i = 0; i < (int)moons.size(); ++i) {
            if (moons[i].parentPlanet == 3) { earthMoonIndex = i; break; }
        }

        if (earthMoonIndex >= 0) {
            focusedPlanet = 9 + earthMoonIndex; // focus on that moon's position in planetPositions
            cout << "Focused on Earth's moon (index: " << (9 + earthMoonIndex) << ")" << endl;
        } else {
            // Fallback: focus Earth
            focusedPlanet = 3;
            cout << "Earth's moon not found — focusing on Earth instead." << endl;
        }

        nineKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_9) == GLFW_RELEASE) nineKeyPressed = false;
    
    // Geographic location controls when Earth is focused
    if (focusedPlanet == 3) {  // Earth is planet 3
        static bool locKeyPressed[4] = {false, false, false, false};
        
        // O for Ocean
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS && !locKeyPressed[0]) {
            currentLocationIndex = 0;
            focusedLocationIndex = 0;
            showEarthLocation = true;
            isMovingToLocation = true;  // Start smooth camera movement
            locKeyPressed[0] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_RELEASE) locKeyPressed[0] = false;
        
        // M for Mountain
        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS && !locKeyPressed[1]) {
            currentLocationIndex = 1;
            focusedLocationIndex = 1;
            showEarthLocation = true;
            isMovingToLocation = true;  // Start smooth camera movement
            locKeyPressed[1] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_RELEASE) locKeyPressed[1] = false;
        
        // D for Desert
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && !locKeyPressed[2]) {
            currentLocationIndex = 3;
            focusedLocationIndex = 3;
            showEarthLocation = true;
            isMovingToLocation = true;  // Start smooth camera movement
            locKeyPressed[2] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_RELEASE) locKeyPressed[2] = false;
        
        // A for Amazon (Land)
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && !locKeyPressed[3]) {
            currentLocationIndex = 2;
            focusedLocationIndex = 2;
            showEarthLocation = true;
            isMovingToLocation = true;  // Start smooth camera movement
            locKeyPressed[3] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_RELEASE) locKeyPressed[3] = false;
    } else {
        showEarthLocation = false;
        isMovingToLocation = false;
    }
    
    // Geographic location controls when Saturn is focused
    if (focusedPlanet == 6) {  // Saturn is planet 6
        static bool saturnLocKeyPressed[2] = {false, false};
        
        // H for Diamond Mountain
        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS && !saturnLocKeyPressed[0]) {
            currentSaturnLocationIndex = 0;
            selectedSaturnLocationIndex = 0;
            showSaturnLocation = true;
            saturnLocKeyPressed[0] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_RELEASE) saturnLocKeyPressed[0] = false;
        
        // C for Chloric Ocean
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !saturnLocKeyPressed[1]) {
            currentSaturnLocationIndex = 1;
            selectedSaturnLocationIndex = 1;
            showSaturnLocation = true;
            saturnLocKeyPressed[1] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) saturnLocKeyPressed[1] = false;
    } else {
        showSaturnLocation = false;
    }
}

// --- Shader Source Code (GLSL) ---

// --- ORIGINAL SHADERS ---
const char *litVertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoords;
    out vec2 TexCoords;
    out vec3 Normal;
    out vec3 FragPos;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        TexCoords = aTexCoords;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)glsl";

const char *litFragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoords;
    in vec3 Normal;
    in vec3 FragPos;
    uniform sampler2D mainTexture;
    uniform vec3 lightPos;
    uniform vec3 viewPos;
    uniform float ambientStrength;
    uniform bool hasTransparency;
    uniform float opacity;
    void main() {
        vec3 ambient = ambientStrength * vec3(1.0);
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * vec3(1.0);
        vec4 texColor = texture(mainTexture, TexCoords);
        vec3 result = (ambient + diffuse) * texColor.rgb;
        float finalOpacity = texColor.a * opacity;
        if (hasTransparency) {
            FragColor = vec4(result, finalOpacity);
        } else {
            FragColor = vec4(result, 1.0);
        }
    }
)glsl";

const char *skyboxVertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 2) in vec2 aTexCoords;
    out vec2 TexCoords;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    void main() {
        TexCoords = aTexCoords;
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)glsl";

const char *skyboxFragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoords;
    uniform sampler2D mainTexture;
    uniform float time; 
    float noise(vec2 st) {
        return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
    }
    void main() {
        vec4 starColor = texture(mainTexture, TexCoords);
        float twinkle = noise(TexCoords * 100.0 + time * 0.1);
        float twinkleFactor = smoothstep(0.8, 1.0, twinkle) * 0.3 + 1.0; 
        if(starColor.r > 0.1) {
            FragColor = vec4(starColor.rgb * twinkleFactor, 1.0);
        } else {
            FragColor = starColor; 
        }
    }
)glsl";

// --- SUN SHADER (Seamless) ---
const char *sunVertexSource = R"glsl(
#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 FragPos; 
out vec3 Normal;
out vec3 v_ModelPos; // <-- NEW: Pass model-space position

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float u_time;
uniform float u_displacementStrength;
uniform float u_noiseScale; 

// Using your simple noise function
float simpleNoise(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main()
{
    TexCoords = aTexCoords;
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    v_ModelPos = aPos;

    vec3 noisePos = aPos * u_noiseScale + (aNormal * u_time * 0.5);
    float noise = simpleNoise(noisePos);
    float displacement = (noise * 2.0 - 1.0) * u_displacementStrength;
    vec3 displacedPos = aPos + (aNormal * displacement);

    gl_Position = projection * view * model * vec4(displacedPos, 1.0);
}
)glsl";

const char *sunFragmentSource = R"glsl(
#version 450 core
// FBO Pass 1: Multiple Render Targets
layout (location = 0) out vec4 FragColor;   // To texSceneColor
layout (location = 1) out vec4 BrightColor; // To texBrightMap

in vec2 TexCoords;
in vec3 v_ModelPos; 

uniform float u_time;
uniform float u_noiseScale;
uniform float u_distortionStrength; 
uniform sampler2D u_colorRamp; // Sun texture acts as color ramp

// Noise functions from your prompt
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f); // Smoothstep

    return mix(mix(mix(hash(i + vec3(0,0,0)), hash(i + vec3(1,0,0)), f.x),
                 mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
             mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
                 mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    vec3 shift = vec3(100.0);
    for (int i = 0; i < 5; ++i) { // 5 "octaves"
        v += a * noise(p);
        p = p * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

void main()
{
    // NEW (and seamless):
    // Use the 3D model position as the base for all noise
    vec3 p = v_ModelPos * u_noiseScale;
    
    // Add time to the 3D position to make it animate
    vec3 time_offset = vec3(u_time * 0.1, u_time * 0.2, u_time * 0.15);

    // Calculate distortion using our new 3D 'p' and 'time_offset'
    vec3 q = vec3(fbm(p + time_offset + vec3(0.0, 0.0, 0.0)),
                  fbm(p + time_offset + vec3(5.2, 1.3, 0.0)),
                  fbm(p + time_offset + vec3(9.1, 3.7, 0.0)));
    
    // Calculate final noise, also based on 3D position + time
    float finalNoise = fbm(p + time_offset + q * u_distortionStrength);

    // Use the sun texture as the color ramp
    vec3 fireColor = texture(u_colorRamp, vec2(finalNoise, 0.5)).rgb;

    FragColor = vec4(fireColor, 1.0);
    
    // The sun is always bright, so we output its color
    // to the bright pass texture.
    BrightColor = vec4(fireColor, 1.0);
}
)glsl";

// --- ORBIT SHADERS ---
const char *orbitVertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    
    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;
    
    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)glsl";

const char *orbitFragmentShaderSource = R"glsl(
    #version 330 core
    
    uniform vec3 orbitColor;
    out vec4 FragColor;
    
    void main() {
        FragColor = vec4(orbitColor, 0.35);  // Low opacity for dotted appearance
    }
)glsl";

// --- POST-PROCESSING SHADERS ---

// A generic vertex shader for all post-processing passes
const char *postProcessVertexSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoords;
    out vec2 TexCoords;
    void main() {
        TexCoords = aTexCoords;
        gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
    }
)glsl";

// Gaussian Blur
const char *gaussianBlurFragmentSource = R"glsl(
#version 450 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D u_image;
uniform bool u_horizontal;

// Fixed 5-tap weights from your code
float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec2 tex_offset = 1.0 / textureSize(u_image, 0);
    vec3 result = texture(u_image, TexCoords).rgb * weights[0]; // Center sample
    
    if(u_horizontal)
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(u_image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
            result += texture(u_image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
        }
    }
    else // Vertical
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(u_image, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weights[i];
            result += texture(u_image, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weights[i];
        }
    }
    
    FragColor = vec4(result, 1.0);
}
)glsl";

// God Rays (Shortened)
const char *godRayFragmentSource = R"glsl(
#version 450 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D u_brightTexture; // texBrightMap
uniform vec2 u_sunScreenPos;       // Sun's (0-1) screen position

uniform float u_exposure = 0.8;
uniform float u_decay = 0.95;
uniform float u_density = 0.3; 
uniform float u_weight = 0.1;
const int NUM_SAMPLES = 100;

void main()
{
    vec2 delta = TexCoords - u_sunScreenPos;
    vec2 step = delta / float(NUM_SAMPLES) * u_density;

    vec3 color = vec3(0.0);
    float illuminationDecay = 1.0;

    for(int i=0; i < NUM_SAMPLES; i++)
    {
        vec2 sampleCoords = TexCoords - step * float(i);
        vec3 sampleColor = texture(u_brightTexture, sampleCoords).rgb;
        
        sampleColor *= illuminationDecay * u_weight;
        color += sampleColor;
        illuminationDecay *= u_decay;
    }
    
    FragColor = vec4(color * u_exposure, 1.0);
}
)glsl";

// Composite Shader
const char *compositeFragmentSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoords;

    uniform sampler2D texSceneColor; // Pass 1
    uniform sampler2D texBloom;      // Pass 2
    uniform sampler2D texGodRays;    // Pass 3

    void main()
    {
        vec3 sceneColor = texture(texSceneColor, TexCoords).rgb;
        vec3 bloomColor = texture(texBloom, TexCoords).rgb;
        vec3 godRayColor = texture(texGodRays, TexCoords).rgb;

        // Additive Blending
        vec3 finalColor = sceneColor + bloomColor + godRayColor;
        
        // Basic Tone Mapping
        finalColor = finalColor / (finalColor + vec3(1.0));
        finalColor = pow(finalColor, vec3(1.0/2.2)); // Gamma correction

        FragColor = vec4(finalColor, 1.0);
    }
)glsl";

// Heat Distortion (Declared but not used)
const char *heatDistortionFragmentSource = R"glsl(
#version 450 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D u_finalSceneTexture; // texComposite
uniform sampler2D u_noiseTexture;      // e.g., texNoise (earth clouds)
uniform float u_time;
uniform float u_distortionStrength = 0.01; 

void main()
{
    vec2 uv1 = TexCoords + vec2(u_time * 0.01, u_time * 0.02);
    vec2 uv2 = TexCoords - vec2(u_time * 0.015, u_time * 0.005);

    float noise1 = texture(u_noiseTexture, uv1).r;
    float noise2 = texture(u_noiseTexture, uv2).g; 
    
    float offset = (noise1 + noise2 - 1.0) * u_distortionStrength;
    vec2 distortedUVs = TexCoords + vec2(offset, offset);
    
    FragColor = texture(u_finalSceneTexture, distortedUVs);
}
)glsl";

// Final Screen Shader
const char *finalScreenFragmentSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoords;
    uniform sampler2D texFinal; // This shader will read from texComposite
    void main() {
        FragColor = texture(texFinal, TexCoords);
    }
)glsl";

// --- MARKER SHADER (Simple Colored Dot) ---
const char *markerVertexSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)glsl";

const char *markerFragmentSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    
    uniform vec3 markerColor;
    
    void main() {
        FragColor = vec4(markerColor, 1.0);
    }
)glsl";


// --- Main ---
int main() {
    // --- 1. Initialize GLFW and GLAD ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Interactive Solar System - Post-Processing", NULL, NULL);
    if (window == NULL) {
        cerr << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Failed to initialize GLAD" << endl;
        return -1;
    }

    // --- Initialize ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");


    // --- 2. Configure OpenGL State ---
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- 3. Build and Compile Shaders ---
    Shader litShader(litVertexShaderSource, litFragmentShaderSource);
    Shader skyboxShader(skyboxVertexShaderSource, skyboxFragmentShaderSource);
    Shader sunShader(sunVertexSource, sunFragmentSource);
    Shader orbitShader(orbitVertexShaderSource, orbitFragmentShaderSource);
    Shader gaussianBlurShader(postProcessVertexSource, gaussianBlurFragmentSource);
    Shader godRayShader(postProcessVertexSource, godRayFragmentSource);
    Shader compositeShader(postProcessVertexSource, compositeFragmentSource);
    Shader heatDistortionShader(postProcessVertexSource, heatDistortionFragmentSource); // Compiled but not used
    Shader finalScreenShader(postProcessVertexSource, finalScreenFragmentSource);
    Shader markerShader(markerVertexSource, markerFragmentSource);  // For location markers


    // --- 4. Load Textures ---
    unsigned int sunTex = loadTexture("sun.bmp", false);
    unsigned int mercuryTex = loadTexture("mercury.bmp", false); 
    unsigned int venusTex = loadTexture("venus.bmp", false);
    unsigned int venusAtmoTex = loadTexture("venus_atmosphere.bmp", true);
    unsigned int earthDayTex = loadTexture("earth_daymap.bmp", false);
    unsigned int earthCloudsTex = loadTexture("earth_clouds.bmp", true);
    unsigned int moonTex = loadTexture("moon.bmp", false);
    unsigned int marsTex = loadTexture("mars.bmp", false);
    unsigned int jupiterTex = loadTexture("jupiter.bmp", false);
    unsigned int saturnTex = loadTexture("saturn.bmp", false);
    unsigned int saturnRingTex = loadTexture("saturn_ring_alpha.bmp", true);
    unsigned int uranusTex = loadTexture("uranus.bmp", false);
    unsigned int neptuneTex = loadTexture("neptune.bmp", false);
    unsigned int skyTex = loadTexture("star_milky_way.jpg", false);
    unsigned int asteroidTex = loadTexture("moon.bmp", false); 
    
    // Assign moon textures (using moon texture for all moons for now)
    for (auto& moon : moons) {
        moon.texture = moonTex;
    }
    
    texNoise = earthCloudsTex; // Re-using clouds texture as a noise source

    // --- 5. Create Geometry ---
    Sphere sphere(50, 50);
    Sphere lowPolySphere(10, 10); 
    createRing(6.0f, 9.0f, 50);
    
    // --- 5b. Create Orbits ---
    for (int i = 0; i < 9; ++i) {
        createEllipticalOrbit(i);
    }

    // --- Setup Post-Processing ---
    setupScreenQuad();
    createFramebuffers(SCR_WIDTH, SCR_HEIGHT); // Create initial FBOs

    // --- 6. Initialize Asteroid Belt ---
    asteroidMatrices = new glm::mat4[ASTEROID_COUNT];
    srand(static_cast<unsigned int>(time(0)));
    for (int i = 0; i < ASTEROID_COUNT; ++i) {
        Asteroid a;
        a.orbitRadius = 40.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 5.0f));
        a.angle = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 360.0f));
        a.size = 0.02f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.05f));
        a.yOffset = -0.5f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 1.0f));
        asteroidBelt.push_back(a);
    }
    
    // --- 6b. Initialize Moons ---
    // Mars moons (1 moon)
    moons.push_back({4, 2.0f, 15.0f, 0.2f, 0});  // Phobos
    
    // Jupiter moons (3 moons)
    moons.push_back({5, 4.5f, 8.0f, 0.3f, 0});   // Io
    moons.push_back({5, 6.0f, 5.0f, 0.35f, 0});  // Europa
    moons.push_back({5, 8.0f, 3.0f, 0.25f, 0});  // Ganymede
    
    // Saturn moons (2 moons)
    moons.push_back({6, 5.5f, 10.0f, 0.3f, 0});  // Titan
    moons.push_back({6, 7.0f, 7.0f, 0.2f, 0});   // Enceladus
    
    // Earth Moon
    moons.push_back({3, 2.5f, 13.0f, 0.4f, 0});  // Moon
    
    // --- 6c. Initialize Planet Data ---
    initializePlanetData();

    // --- 7. Set up Shader Uniforms (that don't change) ---
    litShader.use();
    litShader.setInt("mainTexture", 0);
    litShader.setVec3("lightPos", glm::vec3(0.0f));
    litShader.setFloat("ambientStrength", 0.1f);
    
    sunShader.use();
    sunShader.setInt("u_colorRamp", 0);
    sunShader.setFloat("u_displacementStrength", 0.05f);
    sunShader.setFloat("u_noiseScale", 0.9f);
    sunShader.setFloat("u_distortionStrength", 0.05f);

    skyboxShader.use();
    skyboxShader.setInt("mainTexture", 0);
    GLint skyTimeLoc = glGetUniformLocation(skyboxShader.ID, "time");

    compositeShader.use();
    compositeShader.setInt("texSceneColor", 0);
    compositeShader.setInt("texBloom", 1);
    compositeShader.setInt("texGodRays", 2);

    // Set uniforms for shaders we aren't using (or are, in finalScreenShader's case)
    heatDistortionShader.use();
    heatDistortionShader.setInt("u_finalSceneTexture", 0);
    heatDistortionShader.setInt("u_noiseTexture", 1);
    heatDistortionShader.setFloat("u_distortionStrength", 0.01f);

    finalScreenShader.use();
    finalScreenShader.setInt("texFinal", 0); // This will read from whatever texture we bind to unit 0

    // --- 8. Render Loop ---
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        // --- Per-frame Time ---
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        g_simulationTime += deltaTime * timeScale; 
        float g_animationAngle = static_cast<float>(g_simulationTime * 20.0);

        // --- Input ---
        processInput(window);

        // --- Update all planet positions ---
        // Sun always stays at center
        planetPositions[0] = glm::vec3(0.0f, 0.0f, 0.0f); // Sun - ALWAYS at center
        
        // When a planet (not sun) is selected, keep sun at center and show the selected planet
        if (focusedPlanet == 0) {
            // Sun is focused - show all planets in their orbits around sun at center
            planetPositions[1] = glm::vec3(cos(glm::radians(g_animationAngle * 4.15f)) * 12.0f, 0.0f, sin(glm::radians(g_animationAngle * 4.15f)) * 12.0f); // Mercury
            planetPositions[2] = glm::vec3(cos(glm::radians(g_animationAngle * 1.62f)) * 16.0f, 0.0f, sin(glm::radians(g_animationAngle * 1.62f)) * 16.0f); // Venus
            planetPositions[3] = glm::vec3(cos(glm::radians(g_animationAngle * 1.0f)) * 22.0f, 0.0f, sin(glm::radians(g_animationAngle * 1.0f)) * 22.0f); // Earth
            planetPositions[4] = glm::vec3(cos(glm::radians(g_animationAngle * 0.53f)) * 30.0f, 0.0f, sin(glm::radians(g_animationAngle * 0.53f)) * 30.0f); // Mars
            planetPositions[5] = glm::vec3(cos(glm::radians(g_animationAngle * 0.08f)) * 50.0f, 0.0f, sin(glm::radians(g_animationAngle * 0.08f)) * 50.0f); // Jupiter
            planetPositions[6] = glm::vec3(cos(glm::radians(g_animationAngle * 0.03f)) * 70.0f, 0.0f, sin(glm::radians(g_animationAngle * 0.03f)) * 70.0f); // Saturn
            planetPositions[7] = glm::vec3(cos(glm::radians(g_animationAngle * 0.01f)) * 85.0f, 0.0f, sin(glm::radians(g_animationAngle * 0.01f)) * 85.0f); // Uranus
            planetPositions[8] = glm::vec3(cos(glm::radians(g_animationAngle * 0.006f)) * 100.0f, 0.0f, sin(glm::radians(g_animationAngle * 0.006f)) * 100.0f); // Neptune
        } else {
            // A planet is selected - Sun stays at center, selected planet orbits around it
            glm::vec3 orbitPositions[9];
            orbitPositions[1] = glm::vec3(cos(glm::radians(g_animationAngle * 4.15f)) * 12.0f, 0.0f, sin(glm::radians(g_animationAngle * 4.15f)) * 12.0f); // Mercury
            orbitPositions[2] = glm::vec3(cos(glm::radians(g_animationAngle * 1.62f)) * 16.0f, 0.0f, sin(glm::radians(g_animationAngle * 1.62f)) * 16.0f); // Venus
            orbitPositions[3] = glm::vec3(cos(glm::radians(g_animationAngle * 1.0f)) * 22.0f, 0.0f, sin(glm::radians(g_animationAngle * 1.0f)) * 22.0f); // Earth
            orbitPositions[4] = glm::vec3(cos(glm::radians(g_animationAngle * 0.53f)) * 30.0f, 0.0f, sin(glm::radians(g_animationAngle * 0.53f)) * 30.0f); // Mars
            orbitPositions[5] = glm::vec3(cos(glm::radians(g_animationAngle * 0.08f)) * 50.0f, 0.0f, sin(glm::radians(g_animationAngle * 0.08f)) * 50.0f); // Jupiter
            orbitPositions[6] = glm::vec3(cos(glm::radians(g_animationAngle * 0.03f)) * 70.0f, 0.0f, sin(glm::radians(g_animationAngle * 0.03f)) * 70.0f); // Saturn
            orbitPositions[7] = glm::vec3(cos(glm::radians(g_animationAngle * 0.01f)) * 85.0f, 0.0f, sin(glm::radians(g_animationAngle * 0.01f)) * 85.0f); // Uranus
            orbitPositions[8] = glm::vec3(cos(glm::radians(g_animationAngle * 0.006f)) * 100.0f, 0.0f, sin(glm::radians(g_animationAngle * 0.006f)) * 100.0f); // Neptune
            
            // For non-sun planets, show all but keep them positioned relative to Sun at origin
            for (int i = 1; i <= 8; i++) {
                planetPositions[i] = orbitPositions[i];
            }
        }
        
        // --- Update moon positions ---
        for (int i = 0; i < moons.size(); ++i) {
            const Moon& moon = moons[i];
            glm::vec3 parentPos = planetPositions[moon.parentPlanet];
            float angle = glm::radians(g_animationAngle * moon.orbitSpeed);
            planetPositions[9 + i] = parentPos + glm::vec3(cos(angle) * moon.orbitRadius, 0.0f, sin(angle) * moon.orbitRadius);
        }

        
        // --- View/Projection Matrices (Orbit Camera) ---
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
        
        // Compute camera target - either planet center or specific location on Earth
        glm::vec3 cameraTarget = planetPositions[focusedPlanet];
        
        // If a location on Earth is focused, position camera to view that location
        if (focusedPlanet == 3 && focusedLocationIndex >= 0 && focusedLocationIndex < earthLocations.size()) {
            const GeographicLocation& loc = earthLocations[focusedLocationIndex];
            glm::vec3 locPos = latLonToSpherePosition(loc.latitude, loc.longitude, 1.8f);
            
            // Apply Earth's rotation to marker position
            float earthRotation = glm::radians(g_animationAngle * g_daySpeed * 1.0f);
            glm::vec4 rotatedPos = glm::vec4(locPos, 1.0f);
            rotatedPos = glm::rotate(glm::mat4(1.0f), earthRotation, glm::vec3(0.0f, 1.0f, 0.0f)) * rotatedPos;
            
            // Position camera to look at the location from above Earth's surface
            cameraTarget = planetPositions[3] + glm::vec3(rotatedPos);
        }
        
        float camX = cameraTarget.x + cameraDistance * cos(glm::radians(cameraPitch)) * sin(glm::radians(cameraYaw));
        float camY = cameraTarget.y + cameraDistance * sin(glm::radians(cameraPitch));
        float camZ = cameraTarget.z + cameraDistance * cos(glm::radians(cameraPitch)) * cos(glm::radians(cameraYaw));
        glm::vec3 cameraPos = glm::vec3(camX, camY, camZ);
        
        // --- Smooth Camera Movement to Location ---
        if (focusedPlanet == 3 && isMovingToLocation && currentLocationIndex >= 0 && currentLocationIndex < earthLocations.size()) {
            const GeographicLocation& loc = earthLocations[currentLocationIndex];
            glm::vec3 locPos = latLonToSpherePosition(loc.latitude, loc.longitude, 1.8f);
            
            // Apply Earth's rotation to location position
            float earthRotation = glm::radians(g_animationAngle * g_daySpeed * 1.0f);
            glm::vec4 rotatedPos = glm::vec4(locPos, 1.0f);
            rotatedPos = glm::rotate(glm::mat4(1.0f), earthRotation, glm::vec3(0.0f, 1.0f, 0.0f)) * rotatedPos;
            
            glm::vec3 locationOnEarth = planetPositions[3] + glm::vec3(rotatedPos);
            
            // Calculate direction from Earth center to location (for camera positioning)
            glm::vec3 dirToLocation = glm::normalize(glm::vec3(rotatedPos));
            
            // Position camera above the location on Earth's surface
            glm::vec3 newTargetPos = locationOnEarth;
            glm::vec3 newCameraPos = locationOnEarth + dirToLocation * locationCameraDistance;
            
            // Smooth interpolation to new position
            targetCameraPos = newCameraPos;
            currentCameraPos = glm::mix(currentCameraPos, targetCameraPos, cameraLerpSpeed);
            cameraPos = currentCameraPos;
            cameraTarget = newTargetPos;
        }
        
        glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 model = glm::mat4(1.0f);
        // =================================================================
        // --- STEP 4: FBO PASS 1 (Scene + BrightMap) ---
        // =================================================================
        
        glBindFramebuffer(GL_FRAMEBUFFER, fboScene);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        
        unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, attachments);
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);


        // --- Draw Sky Sphere (Skymap) ---
        glDepthMask(GL_FALSE);
        skyboxShader.use();
        glUniform1f(skyTimeLoc, static_cast<float>(g_simulationTime));
        model = glm::mat4(1.0f);
        model = glm::translate(model, cameraPos); 
        model = glm::scale(model, glm::vec3(400.0f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        skyboxShader.setMat4("projection", projection);
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("model", model);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, skyTex);
        sphere.draw();
        glDepthMask(GL_TRUE);


        // --- Draw Sun (Emissive) ---
        sunShader.use();
        sunShader.setFloat("u_time", (float)g_simulationTime);
        model = glm::mat4(1.0f);
        model = glm::translate(model, planetPositions[0]); 
        model = glm::rotate(model, glm::radians(g_animationAngle * g_daySpeed * 0.1f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(8.0f));
        sunShader.setMat4("projection", projection);
        sunShader.setMat4("view", view);
        sunShader.setMat4("model", model);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sunTex);
        sphere.draw();


        // --- Draw Planets (Lit) ---
        litShader.use();
        litShader.setMat4("projection", projection);
        litShader.setMat4("view", view);
        litShader.setVec3("viewPos", cameraPos);
        litShader.setBool("hasTransparency", false);
        litShader.setFloat("opacity", 1.0f);

        auto drawBody = [&](GLuint tex, glm::vec3 position, float radius, float rotSpeed) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, position);
            model = glm::rotate(model, glm::radians(g_animationAngle * g_daySpeed * rotSpeed), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(radius));
            litShader.setMat4("model", model);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            sphere.draw();
        };

        drawBody(mercuryTex, planetPositions[1], 1.0f, 0.1f);
        
        drawBody(venusTex, planetPositions[2], 1.5f, 0.05f);
        model = glm::mat4(1.0f);
        model = glm::translate(model, planetPositions[2]);
        model = glm::rotate(model, glm::radians(g_animationAngle * g_daySpeed * 0.03f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.55f));
        litShader.setMat4("model", model);
        litShader.setBool("hasTransparency", true);
        litShader.setFloat("opacity", 0.9f);
        glBindTexture(GL_TEXTURE_2D, venusAtmoTex);
        sphere.draw();
        litShader.setBool("hasTransparency", false);
        litShader.setFloat("opacity", 1.0f);

        drawBody(earthDayTex, planetPositions[3], 1.6f, 1.0f);
        model = glm::mat4(1.0f);
        model = glm::translate(model, planetPositions[3]);
        model = glm::rotate(model, glm::radians(g_animationAngle * g_daySpeed * 1.2f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.62f));
        litShader.setMat4("model", model);
        litShader.setBool("hasTransparency", true);
        litShader.setFloat("opacity", 0.8f);
        glBindTexture(GL_TEXTURE_2D, earthCloudsTex);
        sphere.draw();
        litShader.setBool("hasTransparency", false);
        litShader.setFloat("opacity", 1.0f);
        
        // --- Draw Location Pointer on Earth for Selected Location Only ---
        if (focusedPlanet == 3 && showEarthLocation && currentLocationIndex >= 0 && currentLocationIndex < earthLocations.size()) {  // Earth
            markerShader.use();
            
            // Earth rotation angle
            float earthRotation = glm::radians(g_animationAngle * g_daySpeed * 1.0f);
            
            const GeographicLocation& loc = earthLocations[currentLocationIndex];
            
            // Convert lat/lon to 3D position on sphere surface
            glm::vec3 locPos = latLonToSpherePosition(loc.latitude, loc.longitude, 1.8f);
            
            // Apply Earth's rotation to the marker position
            glm::vec4 rotatedPos = glm::vec4(locPos, 1.0f);
            rotatedPos = glm::rotate(glm::mat4(1.0f), earthRotation, glm::vec3(0.0f, 1.0f, 0.0f)) * rotatedPos;
            
            glm::vec3 markerWorldPos = planetPositions[3] + glm::vec3(rotatedPos);
            
            // Create model matrix for marker pointer (larger than before)
            model = glm::mat4(1.0f);
            model = glm::translate(model, markerWorldPos);
            model = glm::scale(model, glm::vec3(0.4f));  // Larger pointer size
            
            markerShader.setMat4("model", model);
            markerShader.setMat4("view", view);
            markerShader.setMat4("projection", projection);
            markerShader.setVec3("markerColor", loc.color);  // Pass the location color
            
            // Draw marker sphere as pointer
            lowPolySphere.draw();
            
            // Switch back to lit shader for other objects
            litShader.use();
        }
        
        // --- Draw Location Pointer on Saturn for Selected Location Only ---
        if (focusedPlanet == 6 && showSaturnLocation && currentSaturnLocationIndex >= 0 && currentSaturnLocationIndex < saturnLocations.size()) {  // Saturn
            markerShader.use();
            
            // Saturn rotation angle (Saturn rotates slower)
            float saturnRotation = glm::radians(g_animationAngle * g_daySpeed * 0.45f);
            
            const GeographicLocation& loc = saturnLocations[currentSaturnLocationIndex];
            
            // Convert lat/lon to 3D position on sphere surface
            glm::vec3 locPos = latLonToSpherePosition(loc.latitude, loc.longitude, 4.7f);  // Saturn radius
            
            // Apply Saturn's rotation to the marker position (keep fixed on Saturn)
            glm::vec4 rotatedPos = glm::vec4(locPos, 1.0f);
            rotatedPos = glm::rotate(glm::mat4(1.0f), saturnRotation, glm::vec3(0.0f, 1.0f, 0.0f)) * rotatedPos;
            
            glm::vec3 markerWorldPos = planetPositions[6] + glm::vec3(rotatedPos);
            
            // Create model matrix for marker pointer
            model = glm::mat4(1.0f);
            model = glm::translate(model, markerWorldPos);
            model = glm::scale(model, glm::vec3(0.5f));  // Slightly larger for Saturn
            
            markerShader.setMat4("model", model);
            markerShader.setMat4("view", view);
            markerShader.setMat4("projection", projection);
            markerShader.setVec3("markerColor", loc.color);  // Pass the location color
            
            // Draw marker sphere as pointer
            lowPolySphere.draw();
            
            // Switch back to lit shader for other objects
            litShader.use();
        }
        
        drawBody(marsTex, planetPositions[4], 1.2f, 0.9f);
        
        // --- Draw Moons (same as Mars - simple sphere rendering) ---
        for (int i = 0; i < moons.size(); ++i) {
            const Moon& moon = moons[i];
            // Use moon texture for all moons
            drawBody(moonTex, planetPositions[9 + i], moon.size, 0.5f);
        }

        glBindTexture(GL_TEXTURE_2D, asteroidTex);
        float asteroidOrbitSpeed = g_animationAngle * 0.05f;
        for (int i = 0; i < ASTEROID_COUNT; i++) {
            model = glm::mat4(1.0f);
            model = glm::rotate(model, glm::radians(asteroidOrbitSpeed + asteroidBelt[i].angle), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::translate(model, glm::vec3(asteroidBelt[i].orbitRadius, asteroidBelt[i].yOffset, 0.0f));
            model = glm::scale(model, glm::vec3(asteroidBelt[i].size));
            litShader.setMat4("model", model);
            lowPolySphere.draw();
        }

        drawBody(jupiterTex, planetPositions[5], 5.0f, 2.2f);
        
        drawBody(saturnTex, planetPositions[6], 4.5f, 2.1f);
        model = glm::mat4(1.0f);
        model = glm::translate(model, planetPositions[6]);
        model = glm::rotate(model, glm::radians(15.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        litShader.setMat4("model", model);
        litShader.setBool("hasTransparency", true);
        litShader.setFloat("opacity", 1.0f);
        glBindTexture(GL_TEXTURE_2D, saturnRingTex);
        glBindVertexArray(ringVAO);
        glDrawElements(GL_TRIANGLES, ringIndexCount, GL_UNSIGNED_INT, 0);
        litShader.setBool("hasTransparency", false);

        drawBody(uranusTex, planetPositions[7], 3.5f, 1.3f);
        drawBody(neptuneTex, planetPositions[8], 3.3f, 1.4f);
        
        // --- Draw Outer Asteroid Belt (Kuiper Belt) ---
        glBindTexture(GL_TEXTURE_2D, asteroidTex);
        srand(12345); // Fixed seed for consistent outer belt
        for (int i = 0; i < ASTEROID_COUNT * 25; i++) {  // Same count as inner belt
            model = glm::mat4(1.0f);
            float outerOrbitSpeed = g_animationAngle * 0.005f;
            float outerRadius = 115.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 25.0f));
            float outerAngle = static_cast<float>(rand() % 360);
            model = glm::rotate(model, glm::radians(outerOrbitSpeed + outerAngle), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::translate(model, glm::vec3(outerRadius, -1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 2.0f)), 0.0f));
            float outerSize = 0.012f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.025f));
            model = glm::scale(model, glm::vec3(outerSize));
            litShader.setMat4("model", model);
            lowPolySphere.draw();
        }
        
     
        // --- Draw Orbits ---
        glLineWidth(1.2f);
        orbitShader.use();
        orbitShader.setMat4("projection", projection);
        orbitShader.setMat4("view", view);
        
        // Color array for orbits
        glm::vec3 orbitColors[9] = {
            glm::vec3(0.7f, 0.5f, 0.3f),   // Mercury - Orange-brown
            glm::vec3(0.9f, 0.7f, 0.2f),   // Venus - Yellow
            glm::vec3(0.2f, 0.6f, 0.9f),   // Earth - Cyan-blue
            glm::vec3(0.9f, 0.4f, 0.2f),   // Mars - Red-orange
            glm::vec3(0.8f, 0.7f, 0.5f),   // Jupiter - Tan
            glm::vec3(0.9f, 0.8f, 0.6f),   // Saturn - Light tan
            glm::vec3(0.5f, 0.8f, 0.9f),   // Uranus - Light blue
            glm::vec3(0.3f, 0.5f, 0.9f),   // Neptune - Deep blue
            glm::vec3(0.8f, 0.8f, 0.8f)    // Moon - Light gray
        };
        
        // Show all orbits as dotted lines with low opacity
        for (int i = 0; i < 9; ++i) {
            // Skip moon orbit when not focused on Earth
            if (i == 8 && focusedPlanet != 3) continue;
            
            glBindVertexArray(orbitVAO[i]);
            model = glm::mat4(1.0f);
            
            // For moon, position at Earth
            if (i == 8) {
                model = glm::translate(model, planetPositions[3]);
            }
            
            orbitShader.setMat4("model", model);
            orbitShader.setVec3("orbitColor", orbitColors[i] * 0.4f);  // Low opacity effect via color dimming
            
            glDrawElements(GL_LINES, orbitIndexCount[i], GL_UNSIGNED_INT, 0);
        }
        glLineWidth(1.0f);

        // =================================================================
        // --- RENDER MINIMAP TO FBO (Only when geographic location is selected) ---
        // =================================================================
        bool shouldShowMinimap = (focusedPlanet == 3 && showEarthLocation) || (focusedPlanet == 6 && showSaturnLocation);
        
        if (shouldShowMinimap) {
            glBindFramebuffer(GL_FRAMEBUFFER, fboMinimap);
            glViewport(0, 0, MINIMAP_WIDTH, MINIMAP_HEIGHT);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);

            // Minimap camera - use orthographic projection for better visibility of all planets
            float orthoSize = 120.0f;  // Size of the orthographic view
            glm::mat4 minimapProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 1000.0f);
            glm::vec3 minimapCameraPos = glm::vec3(0.0f, 150.0f, 0.0f);  // Higher up for better view
            glm::mat4 minimapView = glm::lookAt(minimapCameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));

            // Draw minimap sun
            sunShader.use();
            sunShader.setFloat("u_time", (float)g_simulationTime);
            model = glm::mat4(1.0f);
            model = glm::translate(model, planetPositions[0]);
            model = glm::rotate(model, glm::radians(g_animationAngle * g_daySpeed * 0.1f), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(2.0f));  // Smaller sun for minimap
            sunShader.setMat4("projection", minimapProjection);
            sunShader.setMat4("view", minimapView);
            sunShader.setMat4("model", model);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sunTex);
            sphere.draw();

            // Draw minimap planets
            litShader.use();
            litShader.setMat4("projection", minimapProjection);
            litShader.setMat4("view", minimapView);
            litShader.setVec3("viewPos", minimapCameraPos);
            litShader.setBool("hasTransparency", false);
            litShader.setFloat("opacity", 1.0f);

            auto drawMiniPlanet = [&](GLuint tex, glm::vec3 position, float radius) {
                model = glm::mat4(1.0f);
                model = glm::translate(model, position);
                model = glm::scale(model, glm::vec3(radius));
                litShader.setMat4("model", model);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex);
                sphere.draw();
            };

            // Draw all 8 planets with scaled radii for visibility
            drawMiniPlanet(mercuryTex, planetPositions[1], 0.3f);
            drawMiniPlanet(venusTex, planetPositions[2], 0.5f);
            drawMiniPlanet(earthDayTex, planetPositions[3], 0.5f);
            drawMiniPlanet(marsTex, planetPositions[4], 0.4f);
            drawMiniPlanet(jupiterTex, planetPositions[5], 1.5f);
            drawMiniPlanet(saturnTex, planetPositions[6], 1.3f);
            drawMiniPlanet(uranusTex, planetPositions[7], 0.8f);
            drawMiniPlanet(neptuneTex, planetPositions[8], 0.8f);
            
            // Draw orbit lines for reference
            glLineWidth(0.5f);
            orbitShader.use();
            orbitShader.setMat4("projection", minimapProjection);
            orbitShader.setMat4("view", minimapView);
            
            glm::vec3 orbitColor = glm::vec3(0.3f, 0.3f, 0.3f);  // Dark gray orbits
            
            for (int i = 0; i < 8; ++i) {  // Draw all 8 planet orbits
                glBindVertexArray(orbitVAO[i]);
                model = glm::mat4(1.0f);
                orbitShader.setMat4("model", model);
                orbitShader.setVec3("orbitColor", orbitColor);
                glDrawElements(GL_LINES, orbitIndexCount[i], GL_UNSIGNED_INT, 0);
            }
            glLineWidth(1.0f);
        }

        // Bind back to main FBO for post-processing
        glBindFramebuffer(GL_FRAMEBUFFER, fboScene);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

        // =================================================================
        // --- POST-PROCESSING PASSES ---
        // =================================================================
        
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(quadVAO);

        // =================================================================
        // --- STEP 5: FBO PASS 2 (Bloom) ---
        // =================================================================
        
        gaussianBlurShader.use();
        glActiveTexture(GL_TEXTURE0);
        gaussianBlurShader.setInt("u_image", 0);

        bool horizontal = true;
        bool first_iteration = true;
        int amount = 4; // Reduced from 10
        
        for (int i = 0; i < amount; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, fboBloom[horizontal]); 
            gaussianBlurShader.setBool("u_horizontal", horizontal);
            
            glBindTexture(GL_TEXTURE_2D, first_iteration ? texBrightMap : texBloom[!horizontal]);
            
            glDrawArrays(GL_TRIANGLES, 0, 6);
            
            horizontal = !horizontal;
            if (first_iteration) {
                first_iteration = false;
            }
        }

        // =================================================================
        // --- STEP 6: FBO PASS 3 (God Rays) ---
        // =================================================================
        
        glBindFramebuffer(GL_FRAMEBUFFER, fboGodRays);
        godRayShader.use();
        
        glm::vec4 sunClipSpace = projection * view * glm::vec4(planetPositions[0], 1.0);
        glm::vec3 sunNDC = glm::vec3(sunClipSpace) / sunClipSpace.w;
        glm::vec2 sunScreenPos = glm::vec2(sunNDC.x + 1.0, sunNDC.y + 1.0) * 0.5f;
        godRayShader.setVec2("u_sunScreenPos", sunScreenPos);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texBrightMap); 
        godRayShader.setInt("u_brightTexture", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        // =================================================================
        // --- STEP 7: FBO PASS 4 (Composite) ---
        // =================================================================
        
        glBindFramebuffer(GL_FRAMEBUFFER, fboComposite);
        compositeShader.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texSceneColor);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texBloom[0]); 
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, texGodRays);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);


        // =================================================================
        // --- STEP 8: FBO PASS 5 (Distortion) ---
        // =================================================================
        
        // --- THIS STEP IS REMOVED ---


        // =================================================================
        // --- STEP 9/10: Render to ImGui / FINAL RENDER TO SCREEN ---
        // =================================================================
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        finalScreenShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texComposite); // <-- MODIFIED: Bind composite texture
        glDrawArrays(GL_TRIANGLES, 0, 6); 


        // =================================================================
        // --- STEP 10: RENDER IMGUI UI ---
        // =================================================================
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (planetDatabase.count(focusedPlanet)) {
            ImGui::SetNextWindowPos(ImVec2(10, 10)); 
            ImGui::SetNextWindowSize(ImVec2(500, 350)); 
            ImGui::Begin("Planet Details", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse); 
            
            const PlanetInfo& info = planetDatabase[focusedPlanet];
            
            // Increase font size for title
            ImGui::GetFont()->Scale = 1.8f;
            ImGui::PushFont(ImGui::GetFont());
            ImGui::Text("FOCUS: %s", info.name.c_str());
            ImGui::PopFont();
            ImGui::GetFont()->Scale = 1.0f;
            
            ImGui::Separator();
            
            // Increase font size for content
            ImGui::GetFont()->Scale = 1.4f;
            ImGui::PushFont(ImGui::GetFont());
            ImGui::Text("Size:       %s", info.size.c_str());
            ImGui::Text("Rotation:   %s", info.rotation.c_str());
            ImGui::Text("Revolution: %s", info.revolution.c_str());
            ImGui::Text("Atmosphere: %s", info.atmosphere.c_str());
            ImGui::Text("Specialty:  %s", info.speciality.c_str());
            ImGui::Text("Moons:      %s", info.moons.c_str());
            ImGui::PopFont();
            ImGui::GetFont()->Scale = 1.0f;

            ImGui::End();
        }
        
        // --- Geographic Location Display (when Earth is focused) ---
        if (focusedPlanet == 3) {
            ImGui::SetNextWindowPos(ImVec2(550, 10));
            ImGui::SetNextWindowSize(ImVec2(420, 300));
            ImGui::Begin("Earth Locations", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Geographic Locations on Earth");
            ImGui::Separator();
            
            // Display all 4 locations as selectable buttons
            for (int i = 0; i < earthLocations.size(); ++i) {
                const GeographicLocation& loc = earthLocations[i];
                bool isSelected = (currentLocationIndex == i && showEarthLocation);
                
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
                }
                
                if (ImGui::Button(loc.name.c_str(), ImVec2(100, 30))) {
                    currentLocationIndex = i;
                    selectedLocationIndex = i;
                    showEarthLocation = true;
                }
                
                if (isSelected) {
                    ImGui::PopStyleColor();
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::Separator();
            
            if (showEarthLocation) {
                const GeographicLocation& loc = earthLocations[currentLocationIndex];
                
                // Location details
                ImGui::GetFont()->Scale = 1.2f;
                ImGui::PushFont(ImGui::GetFont());
                ImGui::TextColored(ImVec4(loc.color.x, loc.color.y, loc.color.z, 1.0f), "%s", loc.description.c_str());
                ImGui::Text("Latitude:  %.1f°", loc.latitude);
                ImGui::Text("Longitude: %.1f°", loc.longitude);
                ImGui::PopFont();
                ImGui::GetFont()->Scale = 1.0f;
            }
            
            ImGui::Separator();
            ImGui::TextWrapped("Controls:\n- Press: O=Ocean, M=Mountain, L=Land, D=Desert\n- Click on location markers on Earth\n- Or use buttons above");
            
            ImGui::End();
        }
        
        // --- Geographic Location Display (when Saturn is focused) ---
        if (focusedPlanet == 6) {
            ImGui::SetNextWindowPos(ImVec2(550, 10));
            ImGui::SetNextWindowSize(ImVec2(420, 250));
            ImGui::Begin("Saturn Locations", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            
            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.6f, 1.0f), "Geographic Locations on Saturn");
            ImGui::Separator();
            
            // Display all 2 locations as selectable buttons
            for (int i = 0; i < saturnLocations.size(); ++i) {
                const GeographicLocation& loc = saturnLocations[i];
                bool isSelected = (currentSaturnLocationIndex == i && showSaturnLocation);
                
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
                }
                
                if (ImGui::Button(loc.name.c_str(), ImVec2(120, 30))) {
                    currentSaturnLocationIndex = i;
                    selectedSaturnLocationIndex = i;
                    showSaturnLocation = true;
                }
                
                if (isSelected) {
                    ImGui::PopStyleColor();
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::Separator();
            
            if (showSaturnLocation) {
                const GeographicLocation& loc = saturnLocations[currentSaturnLocationIndex];
                
                // Location details
                ImGui::GetFont()->Scale = 1.2f;
                ImGui::PushFont(ImGui::GetFont());
                ImGui::TextColored(ImVec4(loc.color.x, loc.color.y, loc.color.z, 1.0f), "%s", loc.description.c_str());
                ImGui::Text("Latitude:  %.1f°", loc.latitude);
                ImGui::Text("Longitude: %.1f°", loc.longitude);
                ImGui::PopFont();
                ImGui::GetFont()->Scale = 1.0f;
            }
            
            ImGui::Separator();
            ImGui::TextWrapped("Controls:\n- Press: H=Diamond Mountain, C=Chloric Ocean\n- Or use buttons above");
            
            ImGui::End();
        }
        
        // --- Minimap Display (Bottom-Left) - Only show when geographic location is selected ---
        if ((focusedPlanet == 3 && showEarthLocation) || (focusedPlanet == 6 && showSaturnLocation)) {
            ImGui::SetNextWindowPos(ImVec2(10, SCR_HEIGHT - MINIMAP_HEIGHT - 20));
            ImGui::SetNextWindowSize(ImVec2(MINIMAP_WIDTH + 20, MINIMAP_HEIGHT + 40));
            ImGui::Begin("Solar System Minimap", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Solar System Map");
            ImGui::Separator();
            
            // Display the minimap texture
            ImGui::Image((void*)(intptr_t)texMinimap, ImVec2(MINIMAP_WIDTH, MINIMAP_HEIGHT), ImVec2(0, 1), ImVec2(1, 0));
            
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        // --- Swap Buffers and Poll Events ---
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- Cleanup ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteFramebuffers(1, &fboScene);
    glDeleteTextures(1, &texSceneColor);
    glDeleteTextures(1, &texBrightMap);
    glDeleteRenderbuffers(1, &rboDepth);
    glDeleteFramebuffers(2, fboBloom);
    glDeleteTextures(2, texBloom);
    glDeleteFramebuffers(1, &fboGodRays);
    glDeleteTextures(1, &texGodRays);
    glDeleteFramebuffers(1, &fboComposite);
    glDeleteTextures(1, &texComposite);
    glDeleteFramebuffers(1, &fboFinal);
    glDeleteTextures(1, &texFinal);

    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);

    delete[] asteroidMatrices;
    glfwTerminate();
    return 0;
}
//g++ src/solar1.cpp src/glad.c src/imgui.cpp src/imgui_draw.cpp src/imgui_widgets.cpp src/imgui_tables.cpp src/imgui_impl_glfw.cpp src/imgui_impl_opengl3.cpp -o Solar1.exe -Iinclude -Isrc -Llib -lglfw3 -lgdi32 -lopengl32