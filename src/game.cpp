#include <glad/glad.h>
#include "GLFW/glfw3.h"
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
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                     &vertices[0], GL_STATIC_DRAW);

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
	unsigned int snowTexture, grassTexture, rockTexture, waterTexture, sandTexture;

    HeightmapTerrain(const std::string &heightmapPath,
                     const std::string &vertexShaderPath,
                     const std::string &fragmentShaderPath)
        : heightmapShader(vertexShaderPath.c_str(),
                          fragmentShaderPath.c_str()) {
        stbi_set_flip_vertically_on_load(true);
        unsigned char *data =
            stbi_load(FileSystem::getPath(heightmapPath).c_str(), &width,
                      &height, &nrChannels, 0);
        if (!data) {
            std::cerr << "Failed to load heightmap: " << heightmapPath
                      << std::endl;
            return;
        }

        // make sure bytePerPixel is set correctly after loading
        bytePerPixel = static_cast<unsigned>(nrChannels);

        // set up vertex data
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                unsigned char *pixelOffset =
                    data + (j + width * i) * bytePerPixel;
                unsigned char y = pixelOffset[0];

                // vertex - scale horizontally for bigger map
                vertices.push_back(
                    (-height / 2.0f + height * i / (float)height) *
                    horizontalScale);                                      // vx
                vertices.push_back(static_cast<int>(y) * yScale - yShift); // vy
                vertices.push_back((-width / 2.0f + width * j / (float)width) *
                                   horizontalScale); // vz
            }
        }
        std::cout << "Loaded " << vertices.size() / 3 << " vertices"
                  << std::endl;
        stbi_image_free(data);

        // build index list for triangle strips
        if (height > 1 && width > 1) {
            for (unsigned i = 0; i < static_cast<unsigned>(height - 1);
                 i += rez) {
                for (unsigned j = 0; j < static_cast<unsigned>(width);
                     j += rez) {
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
        std::cout << "Created " << numStrips * numTrisPerStrip
                  << " triangles total" << std::endl;

        // create GPU buffers for the mesh
        setupMesh(indices);
        
        waterTexture = loadTexture(FileSystem::getPath("resources/textures/terrain/water.jpg").c_str());
        sandTexture = loadTexture(FileSystem::getPath("resources/textures/terrain/sand.jpg").c_str());
        grassTexture = loadTexture(FileSystem::getPath("resources/textures/terrain/grass.jpg").c_str());
        rockTexture = loadTexture(FileSystem::getPath("resources/textures/terrain/rock.jpg").c_str());
        snowTexture = loadTexture(FileSystem::getPath("resources/textures/terrain/snow.jpg").c_str());
    }

    // Helper function for plane
    glm::vec3 get_random_start_location() {
        return glm::vec3(
            -width +
                ((float)rand() / RAND_MAX * (2 * width)),
            50.0f,
            -height +
                ((float)rand() / RAND_MAX * (2 * height)));
    }

    void Draw(glm::mat4 projection, glm::mat4 view, glm::mat4 model) {
        heightmapShader.use();
        heightmapShader.setMat4("projection", projection);
        heightmapShader.setMat4("view", view);
        heightmapShader.setMat4("model", model);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, waterTexture); // Put our water texture in unit 0

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, sandTexture); // Put our sand texture in unit 1

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, grassTexture); // Put our grass texture in unit 2

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, rockTexture); // Put our rock texture in unit 3

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, snowTexture); // Put our snow texture in unit 4

        glUniform1i(glGetUniformLocation(heightmapShader.ID, "uTextureWater"), 0);
        glUniform1i(glGetUniformLocation(heightmapShader.ID, "uTextureSand"), 1);
        glUniform1i(glGetUniformLocation(heightmapShader.ID, "uTextureGrass"), 2);
        glUniform1i(glGetUniformLocation(heightmapShader.ID, "uTextureRock"), 3);
        glUniform1i(glGetUniformLocation(heightmapShader.ID, "uTextureSnow"), 4);


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
        int iz =
            static_cast<int>(floor(glm::clamp(fz, 0.0f, (float)width - 1.0f)));

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

    const float REAL_ROLL_RATE = 0.5f;
    const float MAX_ROLL_SPEED = 10.0f; // maximum speed that the plane can roll
    const float INCREMENTAL_ROLL_SPEED = 2.4f;
    float ROLL_DAMPING = 1.5f;

    const float MIN_ROLL = -150.0f;
    const float MAX_ROLL = 150.0f;

    float Speed = 60.0f;          // units per second
    float TurnSpeed = 30.0f;      // yaw degrees per second
    float PitchSpeed = 60.0f;     // pitch degrees per second
    float RollSpeed = 0.0f;      // roll degrees per second at that moment
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
            }
            else {
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

    // build and compile shaders
    // -------------------------
    Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");
    Shader modelShader("shaders/model.vs", "shaders/model.fs");

    // load models
    // -----------
    Model ourModel(FileSystem::getPath("resources/objects/plane/plane.obj"));

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float skyboxVertices[] = {
        // positions
        -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
        1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

        -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
        -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

        1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

        -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

        -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
        1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

    // skybox VAO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          (void *)0);

    /* Load textures */
    vector<std::string> faces{
        FileSystem::getPath("resources/textures/skybox/right.jpg"),
        FileSystem::getPath("resources/textures/skybox/left.jpg"),
        FileSystem::getPath("resources/textures/skybox/top.jpg"),
        FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
        FileSystem::getPath("resources/textures/skybox/front.jpg"),
        FileSystem::getPath("resources/textures/skybox/back.jpg")};
    unsigned int cubemapTexture = loadCubemap(faces);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    /* Load Terrain */
    HeightmapTerrain terrain(
        "resources/terrain/heightmaps/iceland_heightmap.png", "shaders/terrain.vs",
        "shaders/terrain.fs");

    /* Create flyable plane */
    Plane player(glm::vec3(0.0f, 55.0f, 0.0f));
    player.Terrain = &terrain;

    // Camera follow settings
    bool followPlane = true;
    float camDistance = 12.0f;
    float camHeight = 4.0f;

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
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            player.SpeedUp(deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            player.SlowDown(deltaTime);

        // Auto-dampen roll when not actively rolling
        // if (glfwGetKey(window, GLFW_KEY_Q) != GLFW_PRESS &&
        //     glfwGetKey(window, GLFW_KEY_E) != GLFW_PRESS) {
        //     player.DampenRoll(deltaTime);
        // }

        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
            followPlane = !followPlane;
            captureMouse = !captureMouse;
            camera.Zoom = 50.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS &&
            camera.Zoom > 1.0f)
            camera.Zoom -= 1.0f;

        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS &&
            camera.Zoom < 100.0f)
            camera.Zoom += 1.0f;

        // debug
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
            std::cout << "Player Position: " << player.Position.x << ", " << player.Position.y << ", " << player.Position.z << std::endl;
            std::cout << "Player Rotation: " << player.Yaw << ", " << player.Pitch << ", " << player.Roll << std::endl;
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
            camera.Yaw = player.Yaw;
        }

        // render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // draw scene as normal
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 10000.0f);

        modelShader.use();
        modelShader.setMat4("view", view);
        modelShader.setMat4("projection", projection);

        // Render the player plane model using the plane's model matrix
        model = player.GetModelMatrix();
        model = glm::scale(model, glm::vec3(0.25f, 0.25f, 0.25f));
        modelShader.setMat4("model", model);
        ourModel.Draw(modelShader);

        // draw terrain (reset model for terrain)
        model = glm::mat4(1.0f);
        terrain.Draw(projection, view, model);

        // draw skybox as last
        glDepthFunc(
            GL_LEQUAL); // change depth function so depth test passes when
                        // values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(
            camera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // set depth function back to default

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse
        // moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);

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
            (lastY -
             ypos); // reversed since y-coordinates go from bottom to top

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
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width,
                         height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
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
