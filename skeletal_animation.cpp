#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/animator.h>
#include <learnopengl/model_animation.h>

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <deque>

// ------------- settings -------------
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// ------------- callbacks ------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

// ------------- timing ---------------
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ------------- game speed -----------
float gGameSpeed = 1.0f;
float gSpeedIncreaseRate = 0.02f;  // How fast speed increases
float gMaxSpeed = 3.0f;
float gGameTime = 0.0f;

// ------------- mouse lateral input ---
float g_mouseLastX = SCR_WIDTH * 0.5f;
bool  g_mouseFirstMove = true;
float g_mouseDeltaX = 0.0f;

// ------------- game state ------------
int  gHP = 1;
int  gCoinCount = 0;
bool gGameOver = false;

// ------------- player ---------------
enum class AnimState { Running, Jumping, Sliding };

struct Player {
    float scale = 1.0f;

    glm::vec3 pos{ 0.0f, 0.0f, 0.0f };
    glm::vec3 vel{ 0.0f, 0.0f, 0.0f };
    float yaw = 180.0f;
    float targetYaw = 180.0f;
    float forwardVel = 6.0f;
    float lateralSpeed = 50.0f;
    float jumpSpeed = 5.0f;
    float gravity = 12.0f;
    bool  onGround = true;
    AnimState state = AnimState::Running;

    float standHalfWidth = 0.35f;
    float standHalfDepth = 0.25f;
    float standHeight = 1.8f;
    float slideHeight = 1.0f;

    bool  sliding = false;
    float slideTimer = 0.0f;
    float slideDuration = 1.5f;

    float maxLateral = 4.0f;

    glm::vec3 slideRootStart{ 0.0f, 0.0f, 0.0f };
    glm::vec3 slideRootCurrent{ 0.0f, 0.0f, 0.0f };
    bool slideRootInitialized = false;

    float halfW()   const { return standHalfWidth * scale; }
    float halfD()   const { return standHalfDepth * scale; }
    float standH()  const { return standHeight * scale; }
    float slideH()  const { return slideHeight * scale; }

    glm::vec3 getForwardDir() const {
        float yawRad = glm::radians(yaw);
        return glm::vec3(sin(yawRad), 0.0f, cos(yawRad));
    }

    glm::vec3 getRightDir() const {
        glm::vec3 forward = getForwardDir();
        return glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void turnLeft() {
        if (!sliding && onGround) {
            yaw += 90.0f;
            if (yaw >= 360.0f) yaw -= 360.0f;
        }
    }

    void turnRight() {
        if (!sliding && onGround) {
            yaw -= 90.0f;
            if (yaw < 0.0f) yaw += 360.0f;
        }
    }

    glm::vec3 getSlideOffset() const {
        if (!slideRootInitialized) return glm::vec3(0.0f);

        glm::vec3 delta = slideRootCurrent - slideRootStart;

        float yawRad = glm::radians(yaw);
        float c = cos(yawRad);
        float s = sin(yawRad);

        glm::vec3 worldDelta;
        worldDelta.x = delta.x * c - delta.z * s;
        worldDelta.y = 0.0f;
        worldDelta.z = delta.x * s + delta.z * c;

        return worldDelta;
    }

    std::vector<glm::mat4> removeRootMotion(std::vector<glm::mat4> bones) const {
        if (!sliding || !slideRootInitialized || bones.empty()) return bones;

        glm::vec3 delta = slideRootCurrent - slideRootStart;
        delta.y = 0.0f;

        glm::mat4 cancelMat = glm::translate(glm::mat4(1.0f), -delta);

        for (size_t i = 0; i < bones.size(); ++i) {
            bones[i] = cancelMat * bones[i];
        }

        return bones;
    }

    void bakeSlideRootMotion() {
        if (!slideRootInitialized) return;

        glm::vec3 slideOffset = getSlideOffset();
        pos += slideOffset;

        std::cout << "Baked slide offset: (" << slideOffset.x << ", " << slideOffset.z << ")\n";
        std::cout << "New player.pos: (" << pos.x << ", " << pos.z << ")\n";
    }

    void startSlide() {
        if (onGround && !sliding) {
            sliding = true;
            slideTimer = slideDuration;
            state = AnimState::Sliding;
            slideRootInitialized = false;
        }
    }

    void updatePhysics(float dt, float mouseDeltaX) {
        glm::vec3 forward = getForwardDir();
        pos += forward * forwardVel * dt;

        const float MOUSE_SENS = 0.02f;
        float lateralMove = mouseDeltaX * MOUSE_SENS * lateralSpeed;
        glm::vec3 right = getRightDir();
        pos += right * lateralMove * dt;

        if (!sliding) {
            if (!onGround) vel.y -= gravity * dt;
            pos.y += vel.y * dt;
            if (pos.y <= 0.0f) {
                pos.y = 0.0f; vel.y = 0.0f;
                if (!onGround) { onGround = true; state = AnimState::Running; }
            }
        }
        else {
            pos.y = 0.0f; vel.y = 0.0f;
        }

        if (sliding) {
            slideTimer -= dt;
            if (slideTimer <= 0.0f) {
                sliding = false;
                state = AnimState::Running;
                slideRootInitialized = false;
            }
        }
    }

    void jump() {
        if (onGround && !sliding) {
            onGround = false;
            vel.y = jumpSpeed;
            state = AnimState::Jumping;
        }
    }

    void getAABB(glm::vec3& minOut, glm::vec3& maxOut) const {
        float h = sliding ? slideH() : standH();
        glm::vec3 actualPos = pos;
        if (sliding && slideRootInitialized) {
            glm::vec3 slideOffset = getSlideOffset();
            actualPos += slideOffset;
        }
        minOut = glm::vec3(actualPos.x - halfW(), 0.0f, actualPos.z - halfD());
        maxOut = glm::vec3(actualPos.x + halfW(), actualPos.y + h, actualPos.z + halfD());
    }

    void updateSlideRootMotion(const std::vector<glm::mat4>& boneMatrices) {
        if (!sliding || boneMatrices.empty()) return;

        glm::vec3 currentRoot = glm::vec3(boneMatrices[0][3]);

        if (!slideRootInitialized) {
            slideRootStart = currentRoot;
            slideRootInitialized = true;
        }

        slideRootCurrent = currentRoot;
    }

} player;

// ------------- camera ----------
glm::vec3 camPos(0.0f, 3.0f, 6.5f);
glm::vec3 camTarget(0.0f, 1.2f, -4.0f);
float camYaw = 180.0f;
float camLerpSpeed = 5.0f;
float camRotationSpeed = 8.0f;

glm::mat4 computeFixedChaseCamView() {
    float yawDiff = player.yaw - camYaw;

    if (yawDiff > 180.0f) yawDiff -= 360.0f;
    if (yawDiff < -180.0f) yawDiff += 360.0f;

    camYaw += yawDiff * camRotationSpeed * deltaTime;

    if (camYaw < 0.0f) camYaw += 360.0f;
    if (camYaw >= 360.0f) camYaw -= 360.0f;

    float camYawRad = glm::radians(camYaw);

    glm::vec3 playerForward = glm::vec3(sin(camYawRad), 0.0f, cos(camYawRad));
    glm::vec3 camBack = -playerForward;
    glm::vec3 desiredOffset = camBack * 6.5f + glm::vec3(0.0f, 3.0f, 0.0f);
    glm::vec3 desiredLookOffset = playerForward * 4.0f + glm::vec3(0.0f, 1.2f, 0.0f);

    if (player.sliding) {
        desiredOffset.y = 2.0f;
    }

    glm::vec3 actualPlayerPos = player.pos;
    if (player.sliding) {
        actualPlayerPos += player.getSlideOffset();
    }

    glm::vec3 targetPos = actualPlayerPos + desiredOffset;
    glm::vec3 targetLook = actualPlayerPos + desiredLookOffset;

    camPos = glm::mix(camPos, targetPos, camLerpSpeed * deltaTime);
    camTarget = glm::mix(camTarget, targetLook, camLerpSpeed * deltaTime);

    return glm::lookAt(camPos, camTarget, glm::vec3(0.0f, 1.0f, 0.0f));
}

// ------------- texture loading ------------------
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

unsigned int LoadTexture2D(const std::string& fullPath, bool flip = true) {
    if (flip) stbi_set_flip_vertically_on_load(true);
    int w, h, n;
    unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &n, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << fullPath << std::endl;
        return 0;
    }
    GLenum format = (n == 1) ? GL_RED : (n == 3) ? GL_RGB : GL_RGBA;
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return tex;
}

// ------------- static mesh ------------------
struct StaticMesh {
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int texture = 0;
    int indexCount = 0;

    void initUnitCube(const std::string& texPath) {
        struct V {
            float px, py, pz, nx, ny, nz, u, v, bid0, bid1, bid2, bid3, w0, w1, w2, w3;
        };
        const V v[] = {
            {+0.5f,-0.5f,-0.5f, 1,0,0, 0,0, 0,0,0,0, 1,0,0,0},
            {+0.5f,+0.5f,-0.5f, 1,0,0, 1,0, 0,0,0,0, 1,0,0,0},
            {+0.5f,+0.5f,+0.5f, 1,0,0, 1,1, 0,0,0,0, 1,0,0,0},
            {+0.5f,-0.5f,+0.5f, 1,0,0, 0,1, 0,0,0,0, 1,0,0,0},
            {-0.5f,-0.5f,+0.5f,-1,0,0, 0,0, 0,0,0,0, 1,0,0,0},
            {-0.5f,+0.5f,+0.5f,-1,0,0, 1,0, 0,0,0,0, 1,0,0,0},
            {-0.5f,+0.5f,-0.5f,-1,0,0, 1,1, 0,0,0,0, 1,0,0,0},
            {-0.5f,-0.5f,-0.5f,-1,0,0, 0,1, 0,0,0,0, 1,0,0,0},
            {-0.5f,+0.5f,-0.5f, 0,1,0, 0,0, 0,0,0,0, 1,0,0,0},
            {-0.5f,+0.5f,+0.5f, 0,1,0, 0,1, 0,0,0,0, 1,0,0,0},
            {+0.5f,+0.5f,+0.5f, 0,1,0, 1,1, 0,0,0,0, 1,0,0,0},
            {+0.5f,+0.5f,-0.5f, 0,1,0, 1,0, 0,0,0,0, 1,0,0,0},
            {-0.5f,-0.5f,+0.5f, 0,-1,0, 0,0, 0,0,0,0, 1,0,0,0},
            {-0.5f,-0.5f,-0.5f, 0,-1,0, 0,1, 0,0,0,0, 1,0,0,0},
            {+0.5f,-0.5f,-0.5f, 0,-1,0, 1,1, 0,0,0,0, 1,0,0,0},
            {+0.5f,-0.5f,+0.5f, 0,-1,0, 1,0, 0,0,0,0, 1,0,0,0},
            {-0.5f,-0.5f,+0.5f, 0,0,1, 0,0, 0,0,0,0, 1,0,0,0},
            {+0.5f,-0.5f,+0.5f, 0,0,1, 1,0, 0,0,0,0, 1,0,0,0},
            {+0.5f,+0.5f,+0.5f, 0,0,1, 1,1, 0,0,0,0, 1,0,0,0},
            {-0.5f,+0.5f,+0.5f, 0,0,1, 0,1, 0,0,0,0, 1,0,0,0},
            {+0.5f,-0.5f,-0.5f, 0,0,-1, 0,0, 0,0,0,0, 1,0,0,0},
            {-0.5f,-0.5f,-0.5f, 0,0,-1, 1,0, 0,0,0,0, 1,0,0,0},
            {-0.5f,+0.5f,-0.5f, 0,0,-1, 1,1, 0,0,0,0, 1,0,0,0},
            {+0.5f,+0.5f,-0.5f, 0,0,-1, 0,1, 0,0,0,0, 1,0,0,0},
        };
        const unsigned int idx[] = {
            0,1,2, 0,2,3, 4,5,6, 4,6,7,
            8,9,10, 8,10,11, 12,13,14, 12,14,15,
            16,17,18, 16,18,19, 20,21,22, 20,22,23
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

        GLsizei stride = sizeof(V);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(V, px));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(V, nx));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(V, u));
        glEnableVertexAttribArray(5);
        glVertexAttribIPointer(5, 4, GL_INT, stride, (void*)offsetof(V, bid0));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(V, w0));

        glBindVertexArray(0);
        texture = LoadTexture2D(texPath, true);
        indexCount = 36;
    }

    void draw(Shader& animShader, const glm::vec3& pos, const glm::vec3& size) {
        glm::mat4 I(1.0f);
        for (int i = 0; i < 100; ++i)
            animShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", I);

        glm::mat4 M(1.0f);
        M = glm::translate(M, pos);
        M = glm::scale(M, size);
        animShader.setMat4("model", M);

        animShader.setInt("texture_diffuse1", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

// ------------- floor plane ------------------
struct FloorTile {
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    static unsigned int sharedTexture;
    static bool textureLoaded;

    void init() {
        struct V {
            float px, py, pz, nx, ny, nz, u, v, bid0, bid1, bid2, bid3, w0, w1, w2, w3;
        };
        // Unit square floor
        const V verts[] = {
            {-0.5f, 0.0f,  0.5f, 0,1,0,  0.0f, 1.0f,  0,0,0,0, 1,0,0,0},
            { 0.5f, 0.0f,  0.5f, 0,1,0,  1.0f, 1.0f,  0,0,0,0, 1,0,0,0},
            { 0.5f, 0.0f, -0.5f, 0,1,0,  1.0f, 0.0f,  0,0,0,0, 1,0,0,0},
            {-0.5f, 0.0f, -0.5f, 0,1,0,  0.0f, 0.0f,  0,0,0,0, 1,0,0,0},
        };
        const unsigned int idx[] = { 0,1,2, 0,2,3 };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

        GLsizei stride = sizeof(V);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(V, px));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(V, nx));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(V, u));
        glEnableVertexAttribArray(5);
        glVertexAttribIPointer(5, 4, GL_INT, stride, (void*)offsetof(V, bid0));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(V, w0));
        glBindVertexArray(0);

        if (!textureLoaded) {
            sharedTexture = LoadTexture2D("C:/Users/User/source/repos/LearnOpenGL/resources/textures/darkwood.jpg", true);
            textureLoaded = true;
        }
    }

    void draw(Shader& animShader, const glm::vec3& pos, float size) {
        glm::mat4 I(1.0f);
        for (int i = 0; i < 100; ++i)
            animShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", I);

        glm::mat4 M(1.0f);
        M = glm::translate(M, pos);
        M = glm::scale(M, glm::vec3(size, 1.0f, size));
        animShader.setMat4("model", M);

        animShader.setInt("texture_diffuse1", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sharedTexture);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

unsigned int FloorTile::sharedTexture = 0;
bool FloorTile::textureLoaded = false;

// ------------- collision util ------------------
static inline bool AABBIntersect(const glm::vec3& amin, const glm::vec3& amax,
    const glm::vec3& bmin, const glm::vec3& bmax) {
    return (amin.x <= bmax.x && amax.x >= bmin.x) &&
        (amin.y <= bmax.y && amax.y >= bmin.y) &&
        (amin.z <= bmax.z && amax.z >= bmin.z);
}

// ------------- random ------------------
unsigned int gRandState = 1234567u;

static float frand01() {
    gRandState = 1664525u * gRandState + 1013904223u;
    return (gRandState >> 8) * (1.0f / 16777216.0f);
}

static int randInt(int min, int max) {
    return min + (int)(frand01() * (max - min + 1));
}

// ------------- Block-based generation ------------------
enum class BlockType {
    Normal,
    TurnLeft,
    TurnRight,
    TurnStraight
};

enum class ObsType { None, JumpWall, SlideGate };

struct Wall {
    glm::vec3 pos, size;
};

struct Obstacle {
    ObsType type = ObsType::None;
    glm::vec3 pos, size;
    bool hit = false;

    void getAABB(glm::vec3& mn, glm::vec3& mx) const {
        mn = pos - size * 0.5f;
        mx = pos + size * 0.5f;
    }
};

struct Block {
    int blockIndex;
    BlockType type;
    glm::vec3 centerPos;
    float yaw;

    Wall leftWall;
    Wall rightWall;
    Wall frontWall;
    bool hasFrontWall = false;

    Obstacle obstacle;
    bool hasObstacle = false;

    // Coins
    struct Coin {
        glm::vec3 pos;
        bool collected = false;
        float rotation = 0.0f;
    };
    std::vector<Coin> coins;

    // Block size
    static constexpr float SIZE = 5.0f;  // 5x5 units
    static constexpr float WALL_HEIGHT = 2.0f;
    static constexpr float WALL_THICKNESS = 0.5f;
};

std::deque<Block> gBlocks;
FloorTile gFloorTile;
StaticMesh gBox;

int gNextBlockIndex = 0;
glm::vec3 gNextBlockCenter(0.0f, 0.0f, 0.0f);
float gCurrentBuildYaw = 180.0f;

// Coin generation state
bool gCoinOnLeftSide = true;
int gCoinPatternStart = -1;
bool gCoinSwitchAtBlock4 = false;

glm::vec3 getDirectionFromYaw(float yaw) {
    float yawRad = glm::radians(yaw);
    return glm::vec3(sin(yawRad), 0.0f, cos(yawRad));
}

// Helper to get perpendicular right direction
glm::vec3 getRightFromYaw(float yaw) {
    glm::vec3 forward = getDirectionFromYaw(yaw);
    return glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
}

Block createBlock(int index, BlockType type, glm::vec3 center, float yaw) {
    Block block;
    block.blockIndex = index;
    block.type = type;
    block.centerPos = center;
    block.yaw = yaw;

    glm::vec3 forward = getDirectionFromYaw(yaw);
    glm::vec3 right = getRightFromYaw(yaw);

    float halfSize = Block::SIZE * 0.5f;

    if (type == BlockType::Normal || type == BlockType::TurnStraight) {
        block.leftWall.pos = center - right * (halfSize + Block::WALL_THICKNESS * 0.5f)
            + glm::vec3(0.0f, Block::WALL_HEIGHT * 0.5f, 0.0f);
        block.leftWall.size = glm::vec3(Block::WALL_THICKNESS, Block::WALL_HEIGHT, Block::SIZE);

        block.rightWall.pos = center + right * (halfSize + Block::WALL_THICKNESS * 0.5f)
            + glm::vec3(0.0f, Block::WALL_HEIGHT * 0.5f, 0.0f);
        block.rightWall.size = glm::vec3(Block::WALL_THICKNESS, Block::WALL_HEIGHT, Block::SIZE);

        block.hasFrontWall = false;
    }
    else if (type == BlockType::TurnLeft) {
        block.hasFrontWall = true;
        block.frontWall.pos = center + forward * (halfSize + Block::WALL_THICKNESS * 0.5f)
            + glm::vec3(0.0f, Block::WALL_HEIGHT * 0.5f, 0.0f);
        block.frontWall.size = glm::vec3(Block::SIZE + Block::WALL_THICKNESS * 2.0f, Block::WALL_HEIGHT, Block::WALL_THICKNESS);

        block.rightWall.pos = center + right * (halfSize + Block::WALL_THICKNESS * 0.5f)
            + glm::vec3(0.0f, Block::WALL_HEIGHT * 0.5f, 0.0f);
        block.rightWall.size = glm::vec3(Block::WALL_THICKNESS, Block::WALL_HEIGHT, Block::SIZE);

        // No left wall
        block.leftWall.size = glm::vec3(0.0f);
    }
    else if (type == BlockType::TurnRight) {
        block.hasFrontWall = true;
        block.frontWall.pos = center + forward * (halfSize + Block::WALL_THICKNESS * 0.5f)
            + glm::vec3(0.0f, Block::WALL_HEIGHT * 0.5f, 0.0f);
        block.frontWall.size = glm::vec3(Block::SIZE + Block::WALL_THICKNESS * 2.0f, Block::WALL_HEIGHT, Block::WALL_THICKNESS);

        block.leftWall.pos = center - right * (halfSize + Block::WALL_THICKNESS * 0.5f)
            + glm::vec3(0.0f, Block::WALL_HEIGHT * 0.5f, 0.0f);
        block.leftWall.size = glm::vec3(Block::WALL_THICKNESS, Block::WALL_HEIGHT, Block::SIZE);

        // No right wall
        block.rightWall.size = glm::vec3(0.0f);
    }

    // Rotate wall sizes based on yaw for proper collision
    if (std::abs(std::fmod(yaw, 180.0f) - 90.0f) < 1.0f || std::abs(std::fmod(yaw, 180.0f) + 90.0f) < 1.0f) {
        std::swap(block.leftWall.size.x, block.leftWall.size.z);
        std::swap(block.rightWall.size.x, block.rightWall.size.z);
        if (block.hasFrontWall) {
            std::swap(block.frontWall.size.x, block.frontWall.size.z);
        }
    }

    return block;
}

void generateNextBlock() {
    BlockType type = BlockType::Normal;
    bool shouldHaveObstacle = false;

    // Every 20th block (20, 40, 60...) is a turn block
    if (gNextBlockIndex > 0 && gNextBlockIndex % 20 == 0) {
        float r = frand01();
        if (r < 0.33f) {
            type = BlockType::TurnStraight;
        }
        else if (r < 0.66f) {
            type = BlockType::TurnLeft;
        }
        else {
            type = BlockType::TurnRight;
        }
    }
    // Every block where index % 4 == 1, starting from block 5 (5, 9, 13, 17...) has an obstacle
    // But exclude blocks where %20 == 1 (21, 41, 61...) which are right after turns
    else if (gNextBlockIndex >= 5 && gNextBlockIndex % 4 == 1 && gNextBlockIndex % 20 != 1) {
        shouldHaveObstacle = true;
    }

    Block block = createBlock(gNextBlockIndex, type, gNextBlockCenter, gCurrentBuildYaw);

    // Add obstacle if needed
    if (shouldHaveObstacle) {
        block.hasObstacle = true;
        ObsType obsType = (frand01() < 0.5f) ? ObsType::JumpWall : ObsType::SlideGate;
        block.obstacle.type = obsType;
        block.obstacle.pos = block.centerPos;

        // Determine obstacle orientation based on block yaw
        float blockYaw = gCurrentBuildYaw;
        bool facingZ = (std::abs(std::fmod(blockYaw + 360.0f, 180.0f)) < 1.0f); // Facing +Z or -Z

        if (obsType == ObsType::JumpWall) {
            if (facingZ) {
                block.obstacle.size = glm::vec3(Block::SIZE, 0.6f, 0.7f);
            }
            else {
                block.obstacle.size = glm::vec3(0.7f, 0.6f, Block::SIZE);
            }
            block.obstacle.pos.y = 0.3f;
        }
        else {
            if (facingZ) {
                block.obstacle.size = glm::vec3(Block::SIZE, 1.0f, 1.2f);
            }
            else {
                block.obstacle.size = glm::vec3(1.2f, 1.0f, Block::SIZE);
            }
            block.obstacle.pos.y = 1.5f;
        }
    }

    // Generate coins for blocks where index % 10 == 2,3,4,5,6
    int blockMod10 = gNextBlockIndex % 10;
    if (blockMod10 >= 2 && blockMod10 <= 6) {
        glm::vec3 forward = getDirectionFromYaw(gCurrentBuildYaw);
        glm::vec3 right = getRightFromYaw(gCurrentBuildYaw);

        // At block %10 == 2, start a new coin pattern
        if (blockMod10 == 2) {
            gCoinOnLeftSide = (frand01() < 0.5f);
            gCoinSwitchAtBlock4 = false;
        }

        // At block %10 == 4, 50% chance to switch sides
        if (blockMod10 == 4) {
            gCoinSwitchAtBlock4 = (frand01() < 0.5f);
        }

        float leftPos = -1.0f;   // Position 1
        float rightPos = 1.0f;   // Position 4

        // Determine current side based on pattern
        bool currentlyOnLeft = gCoinOnLeftSide;
        if (gCoinSwitchAtBlock4 && blockMod10 >= 5) {
            currentlyOnLeft = !gCoinOnLeftSide;  // Switched sides after block 4
        }

        float lateralOffset = currentlyOnLeft ? leftPos : rightPos;

        // Generate 5 coins per block
        for (int i = 0; i < 5; i++) {
            Block::Coin coin;

            // Position along forward direction: -2, -1, 0, +1, +2 within the block
            float forwardOffset = (i - 2) * 0.8f;

            bool isTransitionBlock = (blockMod10 == 4 && gCoinSwitchAtBlock4);
            if (isTransitionBlock) {
                float t = (i + 1) / 6.0f;
                if (gCoinOnLeftSide) {
                    //left to right
                    lateralOffset = glm::mix(leftPos, rightPos, t);
                }
                else {
                    //right to left
                    lateralOffset = glm::mix(rightPos, leftPos, t);
                }
            }

            coin.pos = block.centerPos + right * lateralOffset + forward * forwardOffset;
            coin.pos.y = 1.0f;

            // If this block has a jump obstacle
            if (block.hasObstacle && block.obstacle.type == ObsType::JumpWall) {
                if (i == 1) {
                    coin.pos.y = 1.4f;
                }
                else if (i == 2) {
                    coin.pos.y = 1.8f;
                }
                else if (i == 3) {
                    coin.pos.y = 1.4f;
                }
            }

            coin.collected = false;
            coin.rotation = 0.0f;

            block.coins.push_back(coin);
        }
    }

    gBlocks.push_back(block);

    // Calculate next block position
    glm::vec3 forward = getDirectionFromYaw(gCurrentBuildYaw);
    gNextBlockCenter += forward * Block::SIZE;

    // Update build direction if this was a turn block
    if (type == BlockType::TurnLeft) {
        gCurrentBuildYaw += 90.0f;
        if (gCurrentBuildYaw >= 360.0f) gCurrentBuildYaw -= 360.0f;
        // Adjust next center for the turn
        gNextBlockCenter = block.centerPos + getRightFromYaw(block.yaw) * (-Block::SIZE);
    }
    else if (type == BlockType::TurnRight) {
        gCurrentBuildYaw -= 90.0f;
        if (gCurrentBuildYaw < 0.0f) gCurrentBuildYaw += 360.0f;
        // Adjust next center for the turn
        gNextBlockCenter = block.centerPos + getRightFromYaw(block.yaw) * Block::SIZE;
    }

    gNextBlockIndex++;
}

void updateBlockGeneration() {
    // Calculate how far ahead we need to generate (50 blocks)
    const int BLOCKS_AHEAD = 50;

    // Find the block the player is currently in
    int playerBlockIndex = -1;
    for (const auto& block : gBlocks) {
        glm::vec3 toPlayer = player.pos - block.centerPos;
        float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
        if (distSq < Block::SIZE * Block::SIZE) {
            playerBlockIndex = block.blockIndex;
            break;
        }
    }

    // Generate blocks ahead
    while (gNextBlockIndex < playerBlockIndex + BLOCKS_AHEAD) {
        generateNextBlock();
    }

    // Remove blocks far behind (keep last 10)
    while (gBlocks.size() > 60 && !gBlocks.empty()) {
        if (gBlocks.front().blockIndex < playerBlockIndex - 10) {
            gBlocks.pop_front();
        }
        else {
            break;
        }
    }
}

void drawBlocks(Shader& animShader) {
    for (const auto& block : gBlocks) {
        // Draw floor
        gFloorTile.draw(animShader, block.centerPos, Block::SIZE);

        // Draw walls
        if (block.leftWall.size.x > 0.1f) {
            gBox.draw(animShader, block.leftWall.pos, block.leftWall.size);
        }
        if (block.rightWall.size.x > 0.1f) {
            gBox.draw(animShader, block.rightWall.pos, block.rightWall.size);
        }
        if (block.hasFrontWall) {
            gBox.draw(animShader, block.frontWall.pos, block.frontWall.size);
        }

        // Draw obstacle
        if (block.hasObstacle) {
            gBox.draw(animShader, block.obstacle.pos, block.obstacle.size);
        }
    }
}

Model* gCoinModel = nullptr;
Shader* gStaticShader = nullptr;
Model* gEnvironmentModel = nullptr;
unsigned int gSkyboxTexture = 0;

void drawCoins(Shader& animShader, const glm::mat4& projection, const glm::mat4& view) {
    if (!gCoinModel || !gStaticShader) return;

    gStaticShader->use();
    gStaticShader->setMat4("projection", projection);
    gStaticShader->setMat4("view", view);

    // Set lighting uniforms
    gStaticShader->setBool("useLighting", true);
    gStaticShader->setBool("useTexture", false);
    gStaticShader->setVec3("objectColor", glm::vec3(1.0f, 0.84f, 0.0f));
    gStaticShader->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
    gStaticShader->setVec3("lightPos", camPos + glm::vec3(0.0f, 5.0f, 0.0f));
    gStaticShader->setVec3("viewPos", camPos);

    for (auto& block : gBlocks) {
        for (auto& coin : block.coins) {
            if (coin.collected) continue;

            // coin rotation
            coin.rotation += 2.0f * deltaTime;

            glm::mat4 coinModelMat = glm::mat4(1.0f);
            coinModelMat = glm::translate(coinModelMat, coin.pos);
            coinModelMat = glm::rotate(coinModelMat, coin.rotation, glm::vec3(0, 1, 0));
            coinModelMat = glm::scale(coinModelMat, glm::vec3(1.0f));
            gStaticShader->setMat4("model", coinModelMat);
            gCoinModel->Draw(*gStaticShader);
        }
    }
    animShader.use();
}

void drawEnvironment(const glm::mat4& projection, const glm::mat4& view) {
    if (!gEnvironmentModel || !gStaticShader) return;

    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    gStaticShader->use();
    gStaticShader->setMat4("projection", projection);

    glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
    gStaticShader->setMat4("view", skyboxView);

    gStaticShader->setBool("useLighting", false);

    if (gSkyboxTexture != 0) {
        gStaticShader->setBool("useTexture", true);
        gStaticShader->setInt("texture_diffuse1", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gSkyboxTexture);
    }
    else {
        gStaticShader->setBool("useTexture", false);
    }
    gStaticShader->setVec3("objectColor", glm::vec3(0.5f, 0.7f, 1.0f));

    glm::mat4 envModelMat = glm::mat4(1.0f);
    envModelMat = glm::rotate(envModelMat, glm::radians(90.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
    envModelMat = glm::scale(envModelMat, glm::vec3(500.0f));  // scale very big

    gStaticShader->setMat4("model", envModelMat);
    gEnvironmentModel->Draw(*gStaticShader);

    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
}

void checkCoinCollisions() {
    glm::vec3 pmin, pmax;
    player.getAABB(pmin, pmax);

    for (auto& block : gBlocks) {
        for (auto& coin : block.coins) {
            if (coin.collected) continue;

            const float coinRadius = 0.5f;
            glm::vec3 cmin = coin.pos - glm::vec3(coinRadius);
            glm::vec3 cmax = coin.pos + glm::vec3(coinRadius);

            if (AABBIntersect(pmin, pmax, cmin, cmax)) {
                coin.collected = true;
                gCoinCount++;
            }
        }
    }
}

void checkBlockCollisions() {
    glm::vec3 pmin, pmax;
    player.getAABB(pmin, pmax);

    for (auto& block : gBlocks) {
        // Check wall collisions
        auto checkWall = [&](const Wall& w) {
            if (w.size.x < 0.1f && w.size.z < 0.1f) return;
            glm::vec3 wmin = w.pos - w.size * 0.5f;
            glm::vec3 wmax = w.pos + w.size * 0.5f;

            player.getAABB(pmin, pmax);

            if (AABBIntersect(pmin, pmax, wmin, wmax)) {
                // Calculate overlap on each axis
                float overlapX1 = wmax.x - pmin.x;
                float overlapX2 = pmax.x - wmin.x;
                float overlapZ1 = wmax.z - pmin.z;
                float overlapZ2 = pmax.z - wmin.z;

                float minOverlapX = (overlapX1 < overlapX2) ? overlapX1 : -overlapX2;
                float minOverlapZ = (overlapZ1 < overlapZ2) ? overlapZ1 : -overlapZ2;

                if (std::abs(minOverlapX) < std::abs(minOverlapZ)) {
                    player.pos.x += minOverlapX;
                }
                else {
                    player.pos.z += minOverlapZ;
                }
                player.getAABB(pmin, pmax);
            }
            };

        checkWall(block.leftWall);
        checkWall(block.rightWall);
        if (block.hasFrontWall) {
            checkWall(block.frontWall);
        }

        // Check obstacle collision
        if (block.hasObstacle && !block.obstacle.hit) {
            glm::vec3 omin, omax;
            block.obstacle.getAABB(omin, omax);

            bool shouldBlock = true;
            if (block.obstacle.type == ObsType::SlideGate) {
                float barBottom = omin.y;
                float playerTop = pmax.y;
                if (player.sliding && playerTop <= barBottom + 0.01f) {
                    shouldBlock = false;
                }
            }
            if (block.obstacle.type == ObsType::JumpWall) {
                float wallTop = omax.y;
                if (!player.onGround && player.pos.y > wallTop + 0.01f) {
                    shouldBlock = false;
                }
            }

            if (AABBIntersect(pmin, pmax, omin, omax)) {
                if (shouldBlock) {
                    block.obstacle.hit = true;
                    gHP -= 1;
                    if (gHP <= 0) {
                        gGameOver = true;
                        glfwSetWindowTitle(glfwGetCurrentContext(), "GAME OVER - Press R to Restart");
                    }

                    // Push player out
                    float dx1 = omax.x - pmin.x;
                    float dx2 = pmax.x - omin.x;
                    float dz1 = omax.z - pmin.z;
                    float dz2 = pmax.z - omin.z;
                    float pushX = (dx1 < dx2 ? -dx1 : dx2);
                    float pushZ = (dz1 < dz2 ? -dz1 : dz2);

                    if (std::abs(pushZ) <= std::abs(pushX)) {
                        player.pos.z += pushZ + (pushZ > 0 ? 0.001f : -0.001f);
                    }
                    else {
                        player.pos.x += pushX + (pushX > 0 ? 0.001f : -0.001f);
                    }

                    player.getAABB(pmin, pmax);
                }
            }
        }
    }
}

// ------------- reset game ------------------
static void ResetAll(GLFWwindow* window, Animator& animator, Animation& runAnim) {
    gHP = 1;
    gCoinCount = 0;
    gGameOver = false;
    gGameSpeed = 1.0f;  // Reset speed
    gGameTime = 0.0f;   // Reset game time
    glfwSetWindowTitle(window, "Temple Run - HP: 1 | Coins: 0 | Speed: 1.00x");

    player = Player();
    gBlocks.clear();

    gRandState = 1234567u;
    gNextBlockIndex = 0;
    gNextBlockCenter = glm::vec3(0.0f, 0.0f, 0.0f);
    gCurrentBuildYaw = 180.0f;

    // Generate initial blocks
    for (int i = 0; i < 50; ++i) {
        generateNextBlock();
    }

    animator.PlayAnimation(&runAnim);

    camPos = glm::vec3(0.0f, 3.0f, 6.5f);
    camTarget = glm::vec3(0.0f, 1.2f, -4.0f);
    camYaw = 180.0f;
}

// ------------- main ------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Temple Run - HP: 1 | Coins: 0", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return -1;
    }
    glEnable(GL_DEPTH_TEST);

    Shader animShader("anim_model.vs", "anim_model.fs");
    Shader staticShader("static_model.vs", "static_model.fs");
    gStaticShader = &staticShader;

    gBox.initUnitCube("C:/Users/User/Source/Repos/LearnOpenGL/resources/textures/green.jpg");
    gFloorTile.init();

    const std::string base = "C:/Users/User/source/repos/LearnOpenGL/resources/objects/player/";
    Model playerModel(base + "Idle.dae");
    Animation runAnim(base + "Running.dae", &playerModel);
    Animation jumpAnim(base + "Jump.dae", &playerModel);
    Animation slideAnim(base + "Running Slide.dae", &playerModel);
    Animator animator(&runAnim);

    // Load coin model
    Model coinModel("C:/Users/User/Source/Repos/LearnOpenGL/resources/objects/coin/Chinese Coin.fbx");
    std::cout << "[Coin] meshes=" << coinModel.meshes.size() << std::endl;
    gCoinModel = &coinModel;

    // Load skybox
    std::string envPath = "C:/Users/User/Source/Repos/LearnOpenGL/resources/objects/map/free-skybox-basic-sky/source/basic_skybox_3d.fbx";
    Model environmentModel(envPath);
    std::cout << "[Environment] meshes=" << environmentModel.meshes.size() << std::endl;
    gEnvironmentModel = &environmentModel;
    gSkyboxTexture = LoadTexture2D("C:/Users/User/Source/Repos/LearnOpenGL/resources/objects/map/free-skybox-basic-sky/textures/sky_water_landscape.jpg", false);

    // Generate initial blocks
    for (int i = 0; i < 50; ++i) {
        generateNextBlock();
    }

    bool prevSpace = false, prevS = false, prevR = false;
    bool prevA = false, prevD = false;
    Animation* activeAnim = &runAnim;

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float rawDeltaTime = now - lastFrame;
        lastFrame = now;

        // Apply speed multiplier to deltaTime
        deltaTime = rawDeltaTime * gGameSpeed;

        glfwPollEvents();

        bool rDown = (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS);

        if (gGameOver) {
            if (rDown && !prevR) {
                ResetAll(window, animator, runAnim);
                activeAnim = &runAnim;
            }
            prevR = rDown;

            glClearColor(0.15f, 0.02f, 0.02f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            animShader.use();
            glm::mat4 projection = glm::perspective(glm::radians(50.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 600.0f);
            glm::mat4 view = computeFixedChaseCamView();
            animShader.setMat4("projection", projection);
            animShader.setMat4("view", view);

            drawBlocks(animShader);
            drawCoins(animShader, projection, view);

            auto transforms = animator.GetFinalBoneMatrices();
            for (int i = 0; i < (int)transforms.size(); ++i)
                animShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, player.pos);
            model = glm::rotate(model, glm::radians(player.yaw), glm::vec3(0, 1, 0));
            model = glm::scale(model, glm::vec3(player.scale));
            animShader.setMat4("model", model);
            playerModel.Draw(animShader);

            // Draw skybox last
            drawEnvironment(projection, view);

            glfwSwapBuffers(window);
            continue;
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        bool spaceDown = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        if (spaceDown && !prevSpace) player.jump();
        prevSpace = spaceDown;

        bool sDown = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
        if (sDown && !prevS) player.startSlide();
        prevS = sDown;

        bool aDown = (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
        if (aDown && !prevA) player.turnLeft();
        prevA = aDown;

        bool dDown = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);
        if (dDown && !prevD) player.turnRight();
        prevD = dDown;

        float thisFrameMouseDeltaX = g_mouseDeltaX;
        g_mouseDeltaX = 0.0f;
        player.updatePhysics(deltaTime, thisFrameMouseDeltaX);

        // Update block generation
        updateBlockGeneration();

        // Check collisions
        checkBlockCollisions();
        checkCoinCollisions();

        // Increase game speed
        gGameTime += rawDeltaTime;
        gGameSpeed = 1.0f + (gGameTime * gSpeedIncreaseRate);
        if (gGameSpeed > gMaxSpeed) {
            gGameSpeed = gMaxSpeed;
        }

        // Update window title with speed
        static float titleUpdateTimer = 0.0f;
        titleUpdateTimer += rawDeltaTime;
        if (titleUpdateTimer > 0.5f) {
            titleUpdateTimer = 0.0f;
            char titleBuffer[256];
            snprintf(titleBuffer, sizeof(titleBuffer),
                "Temple Run - HP: %d | Coins: %d | Speed: %.2fx",
                gHP, gCoinCount, gGameSpeed);
            glfwSetWindowTitle(window, titleBuffer);
        }

        Animation* desired = &runAnim;
        if (player.state == AnimState::Jumping) desired = &jumpAnim;
        else if (player.state == AnimState::Sliding) desired = &slideAnim;

        if (desired != activeAnim) {
            animator.PlayAnimation(desired);
            activeAnim = desired;
        }
        animator.UpdateAnimation(deltaTime);

        if (player.sliding) {
            auto transforms = animator.GetFinalBoneMatrices();
            player.updateSlideRootMotion(transforms);
        }

        glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        animShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(50.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 600.0f);
        glm::mat4 view = computeFixedChaseCamView();
        animShader.setMat4("projection", projection);
        animShader.setMat4("view", view);

        drawBlocks(animShader);
        drawCoins(animShader, projection, view);

        auto transforms = animator.GetFinalBoneMatrices();
        transforms = player.removeRootMotion(transforms);

        for (int i = 0; i < (int)transforms.size(); ++i)
            animShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, player.pos);
        model = glm::rotate(model, glm::radians(player.yaw), glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(player.scale));
        animShader.setMat4("model", model);
        playerModel.Draw(animShader);

        // Draw skybox last
        drawEnvironment(projection, view);

        glfwSwapBuffers(window);
        prevR = rDown;
    }

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* /*window*/, double xpos, double ypos) {
    if (g_mouseFirstMove) {
        g_mouseLastX = (float)xpos;
        g_mouseFirstMove = false;
    }
    float dx = (float)xpos - g_mouseLastX;
    g_mouseLastX = (float)xpos;
    g_mouseDeltaX += dx;
}