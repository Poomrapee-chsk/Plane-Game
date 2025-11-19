#include <glad/glad.h>
#include "GLFW/glfw3.h"
#include "glm/common.hpp"
#include "glm/trigonometric.hpp"
#include <cmath>
#include <cstdlib>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/camera.h>
#include <learnopengl/filesystem.h>
#include <learnopengl/model.h>
#include <learnopengl/shader_m.h>

#include <iostream>
#include <vector>

void renderCube();
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);
unsigned int loadCubemap(vector<std::string> faces);

// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;
bool captureMouse = false; // start in follow mode

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// camera toggle
float lastToggleTime = 0.0f;
const float toggleCooldown = 0.3f;

glm::vec3 ForwardFromEuler(float yaw, float pitch) {
  // Convert degrees to radians
  float yawRad = glm::radians(yaw);
  float pitchRad = glm::radians(pitch);

  float cy = cos(yawRad);
  float sy = sin(yawRad);
  float cp = cos(pitchRad);
  float sp = sin(pitchRad);

  return glm::vec3(-sy * cp, sp, -cy * cp);
}

class HeightmapTerrain {
private:
  int width, height, nrChannels;
  std::vector<float> vertices;
  float yScale = 1.2f, yShift = 36.0f;
  float horizontalScale = 8.0f;
  unsigned bytePerPixel;
  int rez = 1;
  unsigned int terrainVAO, terrainVBO, terrainIBO;
  std::vector<unsigned> indices;
  int numStrips;
  int numTrisPerStrip;
  Shader heightmapShader;

  void setupMesh(std::vector<unsigned> &indices) {
    // first, configure the cube's VAO (and terrainVBO + terrainIBO)
    glGenVertexArrays(1, &terrainVAO);
    glBindVertexArray(terrainVAO);

    glGenBuffers(1, &terrainVBO);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0],
                 GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &terrainIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned),
                 &indices[0], GL_STATIC_DRAW);
  }

public:
  unsigned int snowTexture, grassTexture, rockTexture, waterTexture,
      sandTexture;

  HeightmapTerrain(const std::string &heightmapPath,
                   const std::string &vertexShaderPath,
                   const std::string &fragmentShaderPath)
      : heightmapShader(vertexShaderPath.c_str(), fragmentShaderPath.c_str()) {
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(FileSystem::getPath(heightmapPath).c_str(),
                                    &width, &height, &nrChannels, 0);
    if (!data) {
      std::cerr << "Failed to load heightmap: " << heightmapPath << std::endl;
      return;
    }

    // make sure bytePerPixel is set correctly after loading
    bytePerPixel = static_cast<unsigned>(nrChannels);

    // set up vertex data
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++) {
        unsigned char *pixelOffset = data + (j + width * i) * bytePerPixel;
        unsigned char y = pixelOffset[0];

        // vertex - scale horizontally for bigger map
        vertices.push_back((-height / 2.0f + height * i / (float)height) *
                           horizontalScale);                       // vx
        vertices.push_back(static_cast<int>(y) * yScale - yShift); // vy
        vertices.push_back((-width / 2.0f + width * j / (float)width) *
                           horizontalScale); // vz
      }
    }
    std::cout << "Loaded " << vertices.size() / 3 << " vertices" << std::endl;
    stbi_image_free(data);

    // build index list for triangle strips
    if (height > 1 && width > 1) {
      for (unsigned i = 0; i < static_cast<unsigned>(height - 1); i += rez) {
        for (unsigned j = 0; j < static_cast<unsigned>(width); j += rez) {
          for (unsigned k = 0; k < 2; k++) {
            indices.push_back(j + width * (i + k * rez));
          }
        }
      }
    }
    std::cout << "Loaded " << indices.size() << " indices" << std::endl;

    numStrips = (height > 1) ? (height - 1) / rez : 0;
    numTrisPerStrip = (width > 1) ? (width / rez) * 2 - 2 : 0;
    std::cout << "Created lattice of " << numStrips << " strips with "
              << numTrisPerStrip << " triangles each" << std::endl;
    std::cout << "Created " << numStrips * numTrisPerStrip << " triangles total"
              << std::endl;

    // create GPU buffers for the mesh
    setupMesh(indices);

    waterTexture = loadTexture(
        FileSystem::getPath("resources/textures/terrain/water.jpg").c_str());
    sandTexture = loadTexture(
        FileSystem::getPath("resources/textures/terrain/sand.jpg").c_str());
    grassTexture = loadTexture(
        FileSystem::getPath("resources/textures/terrain/grass.jpg").c_str());
    rockTexture = loadTexture(
        FileSystem::getPath("resources/textures/terrain/rock.jpg").c_str());
    snowTexture = loadTexture(
        FileSystem::getPath("resources/textures/terrain/snow.jpg").c_str());
  }

  // Helper function for plane
  glm::vec3 get_random_start_location() {
    return glm::vec3(-width + ((float)rand() / RAND_MAX * (2 * width)), 50.0f,
                     -height + ((float)rand() / RAND_MAX * (2 * height)));
  }

  void DrawDepth(Shader &depthShader, glm::mat4 model) {
    depthShader.use();
    depthShader.setMat4("model", model);

    glBindVertexArray(terrainVAO);

    for (unsigned strip = 0; strip < numStrips; strip++) {
      glDrawElements(GL_TRIANGLE_STRIP,   // primitive type
                     numTrisPerStrip + 2, // number of indices to render
                     GL_UNSIGNED_INT,     // index data type
                     (void *)(sizeof(unsigned) * (numTrisPerStrip + 2) *
                              strip)); // offset to starting index
    }
  }

  void Draw(glm::mat4 projection, glm::mat4 view, glm::mat4 model,
            glm::mat4 lightSpaceMatrix, glm::vec3 lightDir,
            glm::vec3 lightColor, glm::vec3 viewPos, unsigned int depthMap,
            glm::vec3 fogColor, float fogDensity) {
    heightmapShader.use();
    heightmapShader.setMat4("projection", projection);
    heightmapShader.setMat4("view", view);
    heightmapShader.setMat4("model", model);
    heightmapShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    heightmapShader.setVec3("lightDir", lightDir);
    heightmapShader.setVec3("lightColor", lightColor);
    heightmapShader.setVec3("viewPos", viewPos);
    heightmapShader.setVec3("fogColor", fogColor);
    heightmapShader.setFloat("fogDensity", fogDensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,
                  waterTexture); // Put our water texture in unit 0

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sandTexture); // Put our sand texture in unit 1

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,
                  grassTexture); // Put our grass texture in unit 2

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, rockTexture); // Put our rock texture in unit 3

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, snowTexture); // Put our snow texture in unit 4

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, depthMap); // Shadow map

    glUniform1i(glGetUniformLocation(heightmapShader.ID, "uTextureWater"), 0);
    glUniform1i(glGetUniformLocation(heightmapShader.ID, "uTextureSand"), 1);
    glUniform1i(glGetUniformLocation(heightmapShader.ID, "uTextureGrass"), 2);
    glUniform1i(glGetUniformLocation(heightmapShader.ID, "uTextureRock"), 3);
    glUniform1i(glGetUniformLocation(heightmapShader.ID, "uTextureSnow"), 4);
    glUniform1i(glGetUniformLocation(heightmapShader.ID, "shadowMap"), 5);

    glBindVertexArray(terrainVAO);

    for (unsigned strip = 0; strip < numStrips; strip++) {
      glDrawElements(GL_TRIANGLE_STRIP,   // primitive type
                     numTrisPerStrip + 2, // number of indices to render
                     GL_UNSIGNED_INT,     // index data type
                     (void *)(sizeof(unsigned) * (numTrisPerStrip + 2) *
                              strip)); // offset to starting index
    }
  }

  // --- Terrain sampling helpers for collision & queries ---
  // Get the Y value for a vertex at integer grid coordinates (ix, iz).
  float GetVertexY(int ix, int iz) const {
    if (ix < 0)
      ix = 0;
    if (iz < 0)
      iz = 0;
    if (ix >= height)
      ix = height - 1;
    if (iz >= width)
      iz = width - 1;
    size_t idx = (static_cast<size_t>(ix) * static_cast<size_t>(width) +
                  static_cast<size_t>(iz)) *
                     3 +
                 1;
    return vertices[idx];
  }

  // Bilinear sample of the terrain height at world coordinates (worldX,
  // worldZ). This matches the coordinate mapping used when generating the
  // vertices. Also divide by the horizontal scale to get the height in world
  // units.
  //   vx = -height/2 + i  -> i = worldX / horizontalScale + height/2
  //   vz = -width/2  + j  -> j = worldZ / horizontalScale + width/2
  float GetHeightAt(float worldX, float worldZ) const {
    // convert world coords to grid coordinates (account for horizontal
    // scale)
    float fx = (worldX / horizontalScale) + (float)height * 0.5f;
    float fz = (worldZ / horizontalScale) + (float)width * 0.5f;

    // clamp inside valid sampling range
    if (fx < 0.0f)
      fx = 0.0f;
    if (fz < 0.0f)
      fz = 0.0f;
    if (fx > (float)height - 1.001f)
      fx = (float)height - 1.001f;
    if (fz > (float)width - 1.001f)
      fz = (float)width - 1.001f;

    int ix = static_cast<int>(floor(fx));
    int iz = static_cast<int>(floor(fz));
    float sx = fx - ix;
    float sz = fz - iz;

    float h00 = GetVertexY(ix, iz);
    float h10 = GetVertexY(ix + 1, iz);
    float h01 = GetVertexY(ix, iz + 1);
    float h11 = GetVertexY(ix + 1, iz + 1);

    float hx0 = h00 + (h10 - h00) * sx;
    float hx1 = h01 + (h11 - h01) * sx;
    return hx0 + (hx1 - hx0) * sz;
  }

  // Estimate normal at world position by central differences on the
  // heightfield.
  glm::vec3 GetNormalAt(float worldX, float worldZ) const {
    // convert to grid space (account for horizontal scale)
    float fx = (worldX / horizontalScale) + (float)height * 0.5f;
    float fz = (worldZ / horizontalScale) + (float)width * 0.5f;

    int ix =
        static_cast<int>(floor(glm::clamp(fx, 0.0f, (float)height - 1.0f)));
    int iz = static_cast<int>(floor(glm::clamp(fz, 0.0f, (float)width - 1.0f)));

    float hL = GetVertexY(ix - 1, iz);
    float hR = GetVertexY(ix + 1, iz);
    float hD = GetVertexY(ix, iz - 1);
    float hU = GetVertexY(ix, iz + 1);

    // grid spacing is 1.0 in world units based on vertex generation
    float dx = (hR - hL) * 0.5f;
    float dz = (hU - hD) * 0.5f;

    glm::vec3 n = glm::normalize(glm::vec3(-dx, 1.0f, -dz));
    return n;
  }
};

/// --- Simple Arcade Plane: easy keyboard-flyable plane, no complex physics ---
struct Plane {
  // State
  const glm::vec3 startingPos{0.0f, 55.0f, 0.0f};
  glm::vec3 Position{startingPos};
  float Yaw = 0.0f;   // degrees (left/right heading)
  float Pitch = 0.0f; // degrees (nose up/down)
  float Roll = 0.0f;  // degrees (banking left/right)

  // Movement parameters
  const float MAX_SPEED = 150.0f;
  const float MIN_SPEED = 20.0f;
  const float ACCELERATION = 10.0f;

  const float REAL_ROLL_RATE = 0.5f;  // how much roll affects turning
  const float MAX_ROLL_SPEED = 10.0f; // maximum speed that the plane can roll
  const float INCREMENTAL_ROLL_SPEED = 4.8f; // how fast the plane rolls
  float ROLL_DAMPING = 1.5f;

  const float MIN_ROLL = -150.0f;
  const float MAX_ROLL = 150.0f;

  float Speed = 60.0f;          // units per second
  float TurnSpeed = 30.0f;      // yaw degrees per second
  float PitchSpeed = 60.0f;     // pitch degrees per second
  float RollSpeed = 0.0f;       // roll degrees per second at that moment
  float GroundClearance = 2.5f; // min height above terrain

  HeightmapTerrain *Terrain = nullptr;

  Plane() = default;
  Plane(const glm::vec3 &pos) : Position(pos) {}

  // Simple control methods (call with deltaTime each frame)
  void TurnLeft(float dt) { Yaw += TurnSpeed * dt; }
  void TurnRight(float dt) { Yaw -= TurnSpeed * dt; }
  void PitchUp(float dt) {
    Pitch -= PitchSpeed * dt;
    Pitch = glm::clamp(Pitch, -85.0f, 85.0f);
  }
  void PitchDown(float dt) {
    Pitch += PitchSpeed * dt;
    Pitch = glm::clamp(Pitch, -85.0f, 85.0f);
  }
  void RollLeft(float dt) {
    RollSpeed += INCREMENTAL_ROLL_SPEED * dt;
    RollSpeed = glm::clamp(RollSpeed, -MAX_ROLL_SPEED, MAX_ROLL_SPEED);
  }
  void RollRight(float dt) {
    RollSpeed -= INCREMENTAL_ROLL_SPEED * dt;
    RollSpeed = glm::clamp(RollSpeed, -MAX_ROLL_SPEED, MAX_ROLL_SPEED);
  }
  void SpeedUp(float dt) {
    Speed += ACCELERATION * dt;
    Speed = glm::clamp(Speed, MIN_SPEED, MAX_SPEED);
  }
  void SlowDown(float dt) {
    Speed -= ACCELERATION * dt;
    Speed = glm::clamp(Speed, MIN_SPEED, MAX_SPEED);
  }

  void DampenRoll(float dt) {
    // auto-level: move roll toward zero
    if (Roll > 0.1f)
      RollRight(dt);
    else if (Roll < -0.1f)
      RollLeft(dt);
    else
      Roll = 0.0f;
  }

  // Returns model matrix for rendering
  glm::mat4 GetModelMatrix() const {
      glm::mat4 m(1.0f);
      m = glm::translate(m, Position);
      m = glm::rotate(m, glm::radians(Yaw), glm::vec3(0, 1, 0));
      m = glm::rotate(m, glm::radians(Pitch), glm::vec3(1, 0, 0));
      m = glm::rotate(m, glm::radians(Roll), glm::vec3(0, 0, 1));
      return m;
  }

  void Reset() {
    Position = glm::vec3(Terrain->get_random_start_location());
    Pitch = 0.0f;
    Roll = 0.0f;
    Yaw = 0.0f;
    Speed = 20.0f;
    TurnSpeed = 10.0f;
    PitchSpeed = 60.0f;
    RollSpeed = 0.0f;
  }

  // Update: move forward and handle ground collision
  void Update(float dt) {
    if (dt <= 0.0f || dt > 0.1f)
      return;

    // Roll influences yaw (banking turns)
    // More roll = more turn, scaled by speed for realistic feel
    float rollTurnRate = glm::sin(glm::radians(Roll)) * 0.8 * Speed * dt;
    Yaw += rollTurnRate;

    // Compute forward direction from yaw/pitch
    glm::vec3 forward = ForwardFromEuler(Yaw, Pitch);

    // Move forward at current speed
    Position += forward * Speed * dt;

    // Convert rollspeed to actual roll
    if (std::abs(RollSpeed) > 0.0001f) {
      if (RollSpeed > 0.0f) {
        Roll += REAL_ROLL_RATE * RollSpeed;
        RollSpeed = std::max(RollSpeed - ROLL_DAMPING * dt, 0.0f);
      } else {
        Roll += REAL_ROLL_RATE * RollSpeed;
        RollSpeed = std::min(RollSpeed + ROLL_DAMPING * dt, 0.0f);
      }
    }

    // Terrain collision: clamp to ground + clearance
    if (Terrain) {
      float groundY = Terrain->GetHeightAt(Position.x, Position.z);
      float minY = groundY + GroundClearance;
      if (Position.y < minY) {
        // When hitting the ground, reset
        Reset();
      }
    }
  }
};

int main() {
  // glfw: initialize and configure
  // ------------------------------
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  // glfw window creation
  // --------------------
  GLFWwindow *window =
      glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
  if (window == NULL) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetScrollCallback(window, scroll_callback);

  // tell GLFW to capture our mouse
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  // glad: load all OpenGL function pointers
  // ---------------------------------------
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
  }

  // configure global opengl state
  // -----------------------------
  glEnable(GL_DEPTH_TEST);

  // shadow map setup
  // -----------------
  const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
  unsigned int depthMapFBO;
  glGenFramebuffers(1, &depthMapFBO);
  unsigned int depthMap;
  glGenTextures(1, &depthMap);
  glBindTexture(GL_TEXTURE_2D, depthMap);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH,
               SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depthMap, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // build and compile shaders
  // -------------------------
  Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");
  Shader modelShader("shaders/model.vs", "shaders/model.fs");
  Shader depthShader("shaders/depth.vs", "shaders/depth.fs");

  // load models
  // -----------
  Model ourModel(FileSystem::getPath("resources/objects/plane/plane.obj"));

  // build and compile shaders
  // -------------------------
  Shader equirectangularToCubemapShader(
      "shaders/cubemap.vs", "shaders/equirectangular_to_cubemap.fs");
  Shader backgroundShader("shaders/background.vs", "shaders/background.fs");

  backgroundShader.use();
  backgroundShader.setInt("environmentMap", 0);
  // pbr: setup framebuffer
  // ----------------------
  unsigned int captureFBO;
  unsigned int captureRBO;
  glGenFramebuffers(1, &captureFBO);
  glGenRenderbuffers(1, &captureRBO);

  glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
  glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, captureRBO);

  /* Load Cubemap */
  stbi_set_flip_vertically_on_load(true);
  int width, height, nrComponents;
  float *data = stbi_loadf(
      FileSystem::getPath("resources/textures/hdr/industrial_sunset.hdr")
          .c_str(),
      &width, &height, &nrComponents, 0);
  unsigned int hdrTexture;

  if (data) {
    glGenTextures(1, &hdrTexture);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB,
                 GL_FLOAT, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
  } else {
    std::cout << "Failed to load HDR image." << std::endl;
  }

  // pbr: setup cubemap to render to and attach to framebuffer
  // ---------------------------------------------------------
  unsigned int envCubemap;
  glGenTextures(1, &envCubemap);
  glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
  for (unsigned int i = 0; i < 6; ++i) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0,
                 GL_RGB, GL_FLOAT, nullptr);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // pbr: set up projection and view matrices for capturing data onto the 6
  // cubemap face directions
  // ----------------------------------------------------------------------------------------------
  glm::mat4 captureProjection =
      glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  glm::mat4 captureViews[] = {
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, -1.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f))};

  // pbr: convert HDR equirectangular environment map to cubemap equivalent
  // ----------------------------------------------------------------------
  equirectangularToCubemapShader.use();
  equirectangularToCubemapShader.setInt("equirectangularMap", 0);
  equirectangularToCubemapShader.setMat4("projection", captureProjection);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, hdrTexture);

  glViewport(
      0, 0, 512,
      512); // don't forget to configure the viewport to the capture dimensions.
  glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
  for (unsigned int i = 0; i < 6; ++i) {
    equirectangularToCubemapShader.setMat4("view", captureViews[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderCube();
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  /* Load Terrain */
  HeightmapTerrain terrain("resources/terrain/heightmaps/iceland_heightmap.png",
                           "shaders/terrain.vs", "shaders/terrain.fs");

  /* Create flyable plane */
  Plane player(glm::vec3(0.0f, 55.0f, 0.0f));
  player.Terrain = &terrain;

  // Light settings
  glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
  glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

  // Fog settings
  glm::vec3 fogColor = glm::vec3(0.5f, 0.5f, 0.5f);
  float fogDensity = 0.005f;

  // Camera follow settings
  bool followPlane = true;
  float camDistance = 15.0f;
  float camHeight = 2.4f;

  // render loop
  // -----------
  while (!glfwWindowShouldClose(window)) {
    // per-frame time logic
    // --------------------
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // input
    // -----
    processInput(window);

    // --- Plane keyboard controls ---
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
      player.PitchUp(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
      player.PitchDown(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
      player.TurnLeft(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
      player.TurnRight(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
      player.RollLeft(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
      player.RollRight(deltaTime);
    // Plane controls only active in follow mode
    if (followPlane) {
      if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        player.SpeedUp(deltaTime);
      if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        player.SlowDown(deltaTime);
    }

    // Auto-dampen roll when not actively rolling
    // if (glfwGetKey(window, GLFW_KEY_Q) != GLFW_PRESS &&
    //     glfwGetKey(window, GLFW_KEY_E) != GLFW_PRESS) {
    //     player.DampenRoll(deltaTime);
    // }

    // Toggle camera mode with debounce
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
      if (currentFrame - lastToggleTime > toggleCooldown) {
        followPlane = !followPlane;
        captureMouse = !captureMouse;
        camera.Zoom = 50.0f;
        lastToggleTime = currentFrame;
        firstMouse = true; // Reset mouse to avoid jumps
        std::cout << (followPlane ? "Follow Plane Mode" : "Free Camera Mode")
                  << std::endl;
      }
    }

    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS && camera.Zoom > 1.0f)
      camera.Zoom -= 1.0f;

    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS &&
        camera.Zoom < 100.0f)
      camera.Zoom += 1.0f;

    // debug
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
      std::cout << "Player Position: " << player.Position.x << ", "
                << player.Position.y << ", " << player.Position.z << std::endl;
      std::cout << "Yaw: " << player.Yaw << ", Pitch: " << player.Pitch
                << ", Roll: " << player.Roll << std::endl;
      std::cout << "Player Roll Speed: " << player.RollSpeed << std::endl;
    }

    // Update plane physics
    player.Update(deltaTime);

    // Camera: follow behind the plane
    if (followPlane) {
      glm::vec3 forward = ForwardFromEuler(player.Yaw, player.Pitch);
      camera.Position = player.Position - forward * camDistance +
                        glm::vec3(0.0f, camHeight, 0.0f);
      camera.Front = glm::normalize(player.Position - camera.Position);
    } else {
      // set camera to look at plane only
      camera.Front = glm::normalize(player.Position - camera.Position);
    }

    // render
    // ------

    // render depth of scene to texture (from light's perspective)
    // --------------------------------------------------------------
    glm::mat4 lightProjection =
        glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, 1.0f, 100.0f);
    glm::mat4 lightView = glm::lookAt(lightDir * 50.0f, glm::vec3(0.0f),
                                      glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    depthShader.use();
    depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    // draw shadow regards to model
    glm::mat4 model = player.GetModelMatrix();
    model = glm::scale(model, glm::vec3(0.25f));
    depthShader.setMat4("model", model);

    // render terrain
    glm::mat4 depthModel = glm::mat4(1.0f);
    terrain.DrawDepth(depthShader, depthModel);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // reset viewport
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Update projection matrix with current zoom level
    glm::mat4 projection =
        glm::perspective(glm::radians(camera.Zoom),
                         (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);

    // Render plane
    glm::mat4 view = camera.GetViewMatrix();

    modelShader.use();
    modelShader.setMat4("view", view);
    modelShader.setMat4("projection", projection);
    modelShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    modelShader.setVec3("lightDir", lightDir);
    modelShader.setVec3("lightColor", lightColor);
    modelShader.setVec3("viewPos", camera.Position);
    modelShader.setVec3("fogColor", fogColor);
    modelShader.setFloat("fogDensity", fogDensity);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    modelShader.setInt("shadowMap", 1);
    modelShader.setMat4("model", model);
    ourModel.Draw(modelShader);

    // Apply plane rotation to environment in follow mode
    if (followPlane) {
        glm::mat4 rotationMatrix = glm::mat4(1.0f);
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(-player.Roll), glm::vec3(0.0f, 0.0f, 1.0f));
        view = rotationMatrix * view;
    }

    // draw terrain
    glm::mat4 terrainModel = glm::mat4(1.0f);
    terrain.Draw(projection, view, terrainModel, lightSpaceMatrix, lightDir,
                 lightColor, camera.Position, depthMap, fogColor, fogDensity);

    /* Render Skybox Equirectangular Map*/
    glDepthFunc(GL_LEQUAL); // change depth function so depth test passes when
                            // values are equal to depth buffer's content
    backgroundShader.use();
    backgroundShader.setMat4("projection", projection);
    backgroundShader.setMat4("view", view);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    renderCube();
    glDepthFunc(GL_LESS); // set depth function back to default

    // glfw: swap buffers and poll IO events (keys pressed/released, mouse
    // moved etc.)
    // -------------------------------------------------------------------------------
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  // Camera controls disabled when plane is active (use C to toggle camera
  // mode if needed)
}

// glfw: whenever the window size changed (by OS or user resize) this callback
// function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  // make sure the viewport matches the new window dimensions; note that width
  // and height will be significantly larger than specified on retina
  // displays.
  glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn) {
  if (captureMouse) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) {
      lastX = xpos;
      lastY = ypos;
      firstMouse = false;
    }

    float xoffset = (xpos - lastX);

    float yoffset =
        (lastY - ypos); // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
  }
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const *path) {
  unsigned int textureID;
  glGenTextures(1, &textureID);

  int width, height, nrComponents;
  unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
  if (data) {
    GLenum format;
    if (nrComponents == 1)
      format = GL_RED;
    else if (nrComponents == 3)
      format = GL_RGB;
    else if (nrComponents == 4)
      format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
  } else {
    std::cout << "Texture failed to load at path: " << path << std::endl;
    stbi_image_free(data);
  }

  return textureID;
}

// loads a cubemap texture from 6 individual texture faces
// order:
// +X (right)
// -X (left)
// +Y (top)
// -Y (bottom)
// +Z (front)
// -Z (back)
// -------------------------------------------------------
unsigned int loadCubemap(vector<std::string> faces) {
  unsigned int textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

  int width, height, nrChannels;
  for (unsigned int i = 0; i < faces.size(); i++) {
    unsigned char *data =
        stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
    if (data) {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height,
                   0, GL_RGB, GL_UNSIGNED_BYTE, data);
      stbi_image_free(data);
    } else {
      std::cout << "Cubemap texture failed to load at path: " << faces[i]
                << std::endl;
      stbi_image_free(data);
    }
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  return textureID;
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube() {
  // initialize (if necessary)
  if (cubeVAO == 0) {
    float vertices[] = {
        // back face
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
        1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,  // bottom-right
        1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,  // top-left
        // front face
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
        1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right
        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
        -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // top-left
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
        // left face
        -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
        -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // top-left
        -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
        -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
        -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
        -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
                                                            // right face
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right
        1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,    // top-right
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left
        1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,    // bottom-left
        // bottom face
        -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
        1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,  // top-left
        1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
        1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
        -1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
        -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
        // top face
        -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
        1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // top-right
        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
        -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
        -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f   // bottom-left
    };
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    // fill buffer
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // link vertex attributes
    glBindVertexArray(cubeVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void *)(6 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
  }
  // render Cube
  glBindVertexArray(cubeVAO);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);
}
