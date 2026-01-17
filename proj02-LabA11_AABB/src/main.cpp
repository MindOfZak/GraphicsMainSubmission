// Print text with std:cout
#include <iostream>
// loads OpenGL
#include <glad/glad.h>
// creates a window, handles input (keyboard/mouse), and OpenGL context
#include <GLFW/glfw3.h>
// Enables some extra GLM features
#define GLM_ENABLE_EXPERIMENTAL
// glm::vec3, glm::mat4 etc (vectors and matrices)
#include <glm/glm.hpp>
// Common transforms: translate/rotate/scale
#include <glm/gtc/matrix_transform.hpp>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// My shaders file loader
#include "shader.h"

// My Mesh class (loads obj models, draws them, contains picking/collision helpers)
#include "Mesh.h"
//#include "Node.h" // currently unused

static Shader shader;


// Root model matrix (identity = no movement)
glm::mat4 matModelRoot = glm::mat4(1.0);

// Camera "view" matrix (identity to start, later becomes lookAt matrix)
glm::mat4 matView = glm::mat4(1.0);

// Projection matrix (how 3D gets projected to the screen)
// Starts as orthographic, but later you replace it with perspective
glm::mat4 matProj = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, -2.0f, 2.0f);

// Floor model transform (starts as "scaled" because mat4(5.0f) creates a matrix
// with 5s down the diagonal; you later overwrite floorModel properly anyway)
glm::mat4 floorModel(5.0f);

// Lighting + camera positions
glm::vec3 lightPos = glm::vec3(5.0f, 5.0f, 10.0f);

// Default camera position
glm::vec3 viewPos_default = glm::vec3(0.0f, 2.0f, 6.0f);

// Current camera position (starts as default)
glm::vec3 viewPos = viewPos_default;

// My Attempt at unreal engine CAMERA movement
// - Hold Right click mouse button to look around
// - Use WASD to fly around while holding Right click mouse button

glm::vec3 gCamPos = viewPos_default;       // camera position
glm::vec3 gCamFront = glm::vec3(0, 0, -1);   // direction camera looks
glm::vec3 gCamUp = glm::vec3(0, 1, 0);    // camera up direction
glm::vec3 gCamRight = glm::vec3(1, 0, 0);    // camera right direction

bool   gRMBDown = false; // is right mouse held down?
bool   gFirstMouse = true;  // used to prevent camera "jump" on first mouse move
double gLastX = 0.0, gLastY = 0.0; // previous mouse position

float gYaw = -90.0f; // yaw angle (rotation around Y). -90 means face -Z direction.
float gPitch = 0.0f;   // pitch angle (look up/down)
float gMouseSensitivity = 0.10f;
float gCamMoveSpeed = 4.0f;

// Scroll wheel zoom
float gScrollDelta = 0.0f;   // how much the scroll wheel moved this frame
float scrollZoomStep = 0.5f; // how much each scroll "tick" moves the camera


// My Meshes List
std::vector<std::shared_ptr<Mesh>> meshList;

// Mesh transform list: same index as meshList.
// meshMatList[i] is the model matrix for meshList[i]
std::vector<glm::mat4> meshMatList;

// Shader program IDs that I have and can use
GLuint flatShader;
GLuint blinnShader;
GLuint phongShader;
GLuint texblinnShader;


// VAO/VBO/EBO = OpenGL objects that store geometry
GLuint floorVAO = 0, floorVBO = 0, floorEBO = 0;
GLuint floorShader = 0;
GLsizei floorIndexCount = 0;

// Render toggles / settings
bool wireframeMode = false;
bool useMipMaps = false; // starts OFF 

// Picking + movement state
int pickedMeshIndex = -1; // -1 means nothing selected
float moveSpeed = 2.0f;   // how fast selected mesh moves (units/sec)
float lastTime = 0.0f;    // for delta time calculation
float rotateSpeed = 90.0f;// degrees/sec rotation speed

//  SHADER INITIALISATION
GLuint initShader(std::string pathVert, std::string pathFrag)
{
    // Load vertex+fragment shader code from files
    shader.read_source(pathVert.c_str(), pathFrag.c_str());

    // Compile them + link into a program
    shader.compile();

    // Tell OpenGL "use this program for drawing"
    glUseProgram(shader.program);

    // Return the OpenGL program ID
    return shader.program;
}

//  CREATE FLOOR GRID GEOMETRY
// This builds a flat grid made of triangles.
// vertsPerSide = resolution (81 means 81x81 vertices)
// spacing = distance between vertices
// yLevel = the Y height of the floor
static void CreateFloorGrid(int vertsPerSide, float spacing, float yLevel)
{
    std::vector<glm::vec3> positions;     // vertex positions
    std::vector<unsigned int> indices;    // triangle index list

    positions.reserve(vertsPerSide * vertsPerSide);

    // Half-size so we can centre the grid at (0,0,0)
    float half = (vertsPerSide - 1) * spacing * 0.5f;

    // Create a flat "sheet" of vertices on the XZ plane
    for (int z = 0; z < vertsPerSide; z++)
    {
        for (int x = 0; x < vertsPerSide; x++)
        {
            float px = (x * spacing) - half;
            float pz = (z * spacing) - half;
            positions.emplace_back(px, yLevel, pz);
        }
    }

    // Create triangle indices (2 triangles per square)
    for (int z = 0; z < vertsPerSide - 1; z++)
    {
        for (int x = 0; x < vertsPerSide - 1; x++)
        {
            // i0 i1
            // i2 i3
            int i0 = z * vertsPerSide + x;
            int i1 = i0 + 1;
            int i2 = i0 + vertsPerSide;
            int i3 = i2 + 1;

            // Triangle 1: i0, i2, i1
            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);

            // Triangle 2: i1, i2, i3
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
        }
    }

    // Store how many indices we will draw later
    floorIndexCount = (GLsizei)indices.size();

    // Create OpenGL objects for the floor geometry
    glGenVertexArrays(1, &floorVAO);
    glGenBuffers(1, &floorVBO);
    glGenBuffers(1, &floorEBO);

    // Bind VAO first (it "remembers" attribute bindings)
    glBindVertexArray(floorVAO);

    // Upload vertex positions into VBO
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER,
        positions.size() * sizeof(glm::vec3),
        positions.data(),
        GL_STATIC_DRAW);

    // Upload indices into EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW);

    // Tell OpenGL: attribute 0 = position, 3 floats (x,y,z)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(glm::vec3),
        (void*)0);

    // Unbind VAO to avoid accidentally changing it later
    glBindVertexArray(0);

    // Floor transform: move floor down and scale it
    floorModel =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 1.0f));
}

//  DRAW FLOOR
static void DrawFloor(const glm::mat4& view, const glm::mat4& proj, bool wireframe)
{
    // Use the floor shader program
    glUseProgram(floorShader);

    // Get uniform locations (variables in your shader)
    GLint modelLoc = glGetUniformLocation(floorShader, "model");
    GLint viewLoc = glGetUniformLocation(floorShader, "view");
    GLint projLoc = glGetUniformLocation(floorShader, "proj");
    GLint colLoc = glGetUniformLocation(floorShader, "floorColor");

    // Send matrices to the shader
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &floorModel[0][0]);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &proj[0][0]);

    // Send floor color to the shader
    glUniform3f(colLoc, 0.08f, 0.08f, 0.20f);

    // Draw indexed triangles
    glBindVertexArray(floorVAO);
    glDrawElements(GL_TRIANGLES, floorIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

//  SEND LIGHT/CAMERA POS TO CURRENT SHADER
void setLightPosition(glm::vec3 lightPos)
{
    GLuint lightpos_loc = glGetUniformLocation(shader.program, "lightPos");
    glUniform3fv(lightpos_loc, 1, glm::value_ptr(lightPos));
}

void setViewPosition(glm::vec3 eyePos)
{
    GLuint viewpos_loc = glGetUniformLocation(shader.program, "viewPos");
    glUniform3fv(viewpos_loc, 1, glm::value_ptr(eyePos));
}

// This is used for selecting objects with the mouse.
// It turns a 2D mouse position into a 3D ray direction.
glm::vec3 screenPosToRay(int mouseX, int mouseY, int w, int h,
    const glm::mat4& proj, const glm::mat4& view);

// GLFW callback function declarations:
void mouse_button_callback(GLFWwindow* win, int button, int action, int mods);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

//  CAMERA VECTOR UPDATE
// Converts yaw/pitch angles into a proper forward/right/up direction.
static void UpdateCameraVectors()
{
    glm::vec3 front;

    // Turn yaw/pitch into an actual direction vector
    front.x = cos(glm::radians(gPitch)) * cos(glm::radians(gYaw));
    front.y = sin(glm::radians(gPitch));
    front.z = cos(glm::radians(gPitch)) * sin(glm::radians(gYaw));

    // Normalize = keep length 1 (prevents weird speed issues)
    gCamFront = glm::normalize(front);

    // Right = perpendicular to front and world-up
    gCamRight = glm::normalize(glm::cross(gCamFront, glm::vec3(0, 1, 0)));

    // Up = perpendicular to right and front
    gCamUp = glm::normalize(glm::cross(gCamRight, gCamFront));
}

//  SCREEN POS TO WORLD RAY
// Step-by-step:
// 1) Convert mouse pixel coords into Normalized Device Coords (-1 to +1)
// 2) Convert that into eye-space using inverse projection
// 3) Convert that into world-space using inverse view
glm::vec3 screenPosToRay(int mouseX, int mouseY, int w, int h,
    const glm::mat4& proj, const glm::mat4& view)
{
    // Convert mouse pixel coords to NDC (-1..1)
    float x = (2.0f * mouseX) / w - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / h;

    // In clip space, z=-1 means "forward"
    glm::vec4 ray_clip(x, y, -1.0f, 1.0f);

    // Back into eye space (undo projection)
    glm::vec4 ray_eye = glm::inverse(proj) * ray_clip;

    // Set ray pointing forward (z=-1) and direction vector (w=0)
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

    // Convert into world direction (undo view matrix)
    glm::vec3 ray_world = glm::normalize(glm::vec3(glm::inverse(view) * ray_eye));
    return ray_world;
}

//  MOUSE BUTTON CALLBACK
void mouse_button_callback(GLFWwindow* win, int button, int action, int mods)
{
    // LEFT CLICK = try to pick/select a mesh
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Get mouse position in window
        double mx, my;
        glfwGetCursorPos(win, &mx, &my);

        // Get window size
        int w, h;
        glfwGetWindowSize(win, &w, &h);

        // Ray origin = camera position.
        // glm::inverse(matView)[3] is basically "where the camera is"
        glm::vec3 rayOrig = glm::vec3(glm::inverse(matView)[3]);

        // Ray direction computed from mouse screen position
        glm::vec3 rayDir = screenPosToRay((int)mx, (int)my, w, h, matProj, matView);

        // Your Ray struct (likely defined in your spatial system)
        Ray ray{ rayOrig, rayDir };

        pickedMeshIndex = -1; // assume nothing picked until proven
        float bestT = 1e30f;  // bestT = closest hit distance along the ray (smaller = closer)

        // Check every mesh in the scene
        for (int i = 0; i < (int)meshList.size(); i++)
        {
            HitInfo hit;

            // Ask the mesh's spatial structure "do I hit you?"
            if (meshList[i]->pSpatial->Raycast(ray, hit))
            {
                // Keep the closest object hit
                if (hit.t < bestT)
                {
                    bestT = hit.t;
                    pickedMeshIndex = i;
                }
            }
        }

        // Update "picked" state on all meshes
        for (int i = 0; i < (int)meshList.size(); i++)
        {
            meshList[i]->setPicked(i == pickedMeshIndex);
        }

        // Debug output
        if (pickedMeshIndex >= 0)
            std::cout << "Picked mesh index: " << pickedMeshIndex << std::endl;
        else
            std::cout << "No objects picked" << std::endl;
    }

    // RIGHT CLICK = Unreal-style free look mode
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            gRMBDown = true;
            gFirstMouse = true;

            // Hide mouse cursor + lock it (so moving mouse rotates camera)
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (action == GLFW_RELEASE)
        {
            gRMBDown = false;
            gFirstMouse = true;

            // Release cursor
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

// Scroll wheel callback: store scroll movement for use in the main loop
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)window;
    (void)xoffset;
    gScrollDelta += (float)yoffset;
}

// Mouse move callback: ONLY rotates camera while RMB is held
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!gRMBDown) return;

    // Prevent jump on first frame RMB is pressed
    if (gFirstMouse)
    {
        gLastX = xpos;
        gLastY = ypos;
        gFirstMouse = false;
    }

    // Mouse delta from last frame
    float xoffset = (float)(xpos - gLastX);
    float yoffset = (float)(gLastY - ypos); // inverted so moving up looks up

    gLastX = xpos;
    gLastY = ypos;

    // Apply sensitivity
    xoffset *= gMouseSensitivity;
    yoffset *= gMouseSensitivity;

    // Update angles
    gYaw += xoffset;
    gPitch += yoffset;

    // Clamp pitch so you can’t flip upside down
    if (gPitch > 89.0f)  gPitch = 89.0f;
    if (gPitch < -89.0f) gPitch = -89.0f;

    // Recalculate camera direction vectors from yaw/pitch
    UpdateCameraVectors();
}

// My main part of main.cpp where the magic happens
int main()
{
    GLFWwindow* window;

    // 1) Start GLFW
    if (!glfwInit())
    {
        std::cout << "glfw failed" << std::endl;
        return -1;
    }

    // 2) Create a window + OpenGL context
    window = glfwCreateWindow(1920, 1080, "Hello OpenGL 11", NULL, NULL);
    glfwMakeContextCurrent(window);

    // 3) Register input callbacks
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // 4) Load OpenGL functions using GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Couldn't load opengl" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Build floor geometry
    CreateFloorGrid(81, 0.25f, 0.0f);

    // Compile shaders (each call returns a program ID)
    // NOTE: you call setLightPosition/setViewPosition right after each initShader,
    // so those uniforms go onto the shader you just created.
    phongShader = initShader("shaders/blinn.vert", "shaders/phong.frag");
    setLightPosition(lightPos);
    setViewPosition(viewPos);

    blinnShader = initShader("shaders/blinn.vert", "shaders/blinn.frag");
    setLightPosition(lightPos);
    setViewPosition(viewPos);

    texblinnShader = initShader("shaders/texblinn.vert", "shaders/texblinn.frag");
    setLightPosition(lightPos);
    setViewPosition(viewPos);

    floorShader = initShader("shaders/floor.vert", "shaders/floor.frag");

    // -------------------------
    // Camera setup:
    // -------------------------
    // Put camera at default position
    gCamPos = viewPos_default;

    // Make camera face the origin (0,0,0)
    gCamFront = glm::normalize(glm::vec3(0, 0, 0) - gCamPos);

    // Convert that direction into yaw/pitch so RMB look starts aligned
    gYaw = glm::degrees(atan2(gCamFront.z, gCamFront.x));
    gPitch = glm::degrees(asin(gCamFront.y));
    UpdateCameraVectors();

    // Build view matrix from camera values
    matView = glm::lookAt(gCamPos, gCamPos + gCamFront, gCamUp);
    viewPos = gCamPos;

    // Use perspective projection (FOV 60 degrees)
    // NOTE: aspect ratio is set to 1.0f here (square).
    // If your window is 1920x1080, you probably want aspect = (float)fbw/fbh.
    matProj = glm::perspective(glm::radians(60.0f), 1.0f, 0.5f, 100.0f);

    // =========================
    //  CREATE YOUR MESH OBJECTS
    // =========================
    glm::mat4 mat = glm::mat4(1.0);

    // --- Teapot ---
    std::shared_ptr<Mesh> teapot = std::make_shared<Mesh>();
    teapot->init("models/teapot.obj", blinnShader);
    meshList.push_back(teapot);

    // Teapot transform: move it left
    mat = glm::translate(glm::vec3(-2.0f, 1.0f, 0.0f));
    meshMatList.push_back(mat);

    // initSpatial probably builds collision/picking data for this mesh
    teapot->initSpatial(false, matModelRoot * mat);

    // --- Bunny (commented out) ---
    // ... bunny code removed right now

    // --- Cyber Truck ---
    std::shared_ptr<Mesh> cyber_truck = std::make_shared<Mesh>();
    cyber_truck->init("models/MyModels/Cyber_truck.obj", blinnShader);
    meshList.push_back(cyber_truck);

    // Truck transform: move it slightly up
    glm::mat4 truckMat = glm::translate(glm::vec3(0.0f, 0.5f, 0.0f));

    // BUG / QUIRK:
    // This scale call does NOTHING because you didn't multiply it into truckMat.
    // You probably meant:
    // truckMat = truckMat * glm::scale(glm::mat4(1.0f), glm::vec3(0.0005f));
    glm::scale(glm::vec3(0.0005f, 0.0005f, 0.0005f));

    meshMatList.push_back(truckMat);
    cyber_truck->initSpatial(false, matModelRoot * truckMat);

    // --- Ferrari ---
    std::shared_ptr<Mesh> Ferrari = std::make_shared<Mesh>();
    Ferrari->init("models/MyModels/Ferrari/ferrari 288 gto.obj", texblinnShader);
    meshList.push_back(Ferrari);

    glm::mat4 FerrariMat =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.25f));

    meshMatList.push_back(FerrariMat);
    Ferrari->initSpatial(false, matModelRoot * FerrariMat);

    // Background colour (grey)
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

    // Enable depth testing so closer objects draw in front of farther ones
    glEnable(GL_DEPTH_TEST);

    // =========================
    //  GAME LOOP (RUNS EVERY FRAME)
    // =========================
    while (!glfwWindowShouldClose(window))
    {
        // Process input callbacks
        glfwPollEvents();

        // Delta time = time since last frame (so movement is frame-rate independent)
        float now = (float)glfwGetTime();
        float deltaTime = now - lastTime;
        lastTime = now;

        // -------------------------
        // Scroll wheel zoom
        // -------------------------
        if (gScrollDelta != 0.0f)
        {
            float step = gScrollDelta * scrollZoomStep;

            // Move camera forward/backward depending on scroll
            glm::vec3 nextCamPos = gCamPos + (gCamFront * step);

            // Create a small AABB box around where the camera WOULD be
            // so we can check collision
            AABB mybox{ nextCamPos - 0.2f, nextCamPos + 0.2f };
            bool bCollide = false;
            std::vector<int> out;

            // Check camera box against each mesh's spatial system
            for (std::shared_ptr<Mesh> pMesh : meshList)
            {
                out.clear();
                pMesh->pSpatial->QueryAABB(mybox, out);
                if (!out.empty())
                {
                    bCollide = true;
                    break;
                }
            }

            // Only apply the zoom move if no collision
            if (!bCollide)
            {
                gCamPos = nextCamPos;
                viewPos = gCamPos;
                matView = glm::lookAt(gCamPos, gCamPos + gCamFront, gCamUp);
            }

            // Reset scroll input so it doesn’t keep applying
            gScrollDelta = 0.0f;
        }

        // -------------------------
        // RMB fly camera (UE style)
        // -------------------------
        if (gRMBDown)
        {
            float v = gCamMoveSpeed * deltaTime;

            glm::vec3 move(0.0f);

            // WASD for camera movement
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move += gCamFront * v;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move -= gCamFront * v;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += gCamRight * v;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= gCamRight * v;

            // If actually moving, collision-check before applying
            if (glm::length(move) > 0.0f)
            {
                glm::vec3 nextCamPos = gCamPos + move;

                AABB mybox{ nextCamPos - 0.2f, nextCamPos + 0.2f };
                bool bCollide = false;
                std::vector<int> out;

                for (std::shared_ptr<Mesh> pMesh : meshList)
                {
                    out.clear();
                    pMesh->pSpatial->QueryAABB(mybox, out);
                    if (!out.empty())
                    {
                        bCollide = true;
                        break;
                    }
                }

                if (!bCollide)
                {
                    gCamPos = nextCamPos;
                }
            }

            // Even if you didn’t move, mouse-look changes yaw/pitch,
            // so you still rebuild view matrix every frame while RMB is down.
            viewPos = gCamPos;
            matView = glm::lookAt(gCamPos, gCamPos + gCamFront, gCamUp);
        }

        // -------------------------
        // Move selected mesh with WASD
        // (ONLY when RMB is NOT down)
        // -------------------------
        if (pickedMeshIndex >= 0 && !gRMBDown)
        {
            glm::vec3 delta(0.0f);

            // Here W/S moves along Z, A/D along X (world-ish movement)
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) delta.z -= moveSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) delta.z += moveSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) delta.x -= moveSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) delta.x += moveSpeed * deltaTime;

            if (glm::length(delta) > 0.0f)
            {
                // Apply translation into that mesh's model matrix
                meshMatList[pickedMeshIndex] =
                    glm::translate(meshMatList[pickedMeshIndex], delta);

                // Update collision/picking data to match its new position
                meshList[pickedMeshIndex]->initSpatial(false, matModelRoot * meshMatList[pickedMeshIndex]);
            }
        }

        // -------------------------
        // Rotate selected mesh
        // U/O rotate around Y (yaw)
        // Y/H rotate around X (pitch)
        // -------------------------
        if (pickedMeshIndex >= 0)
        {
            float rotDeltaDeg = rotateSpeed * deltaTime;
            float rotRad = glm::radians(rotDeltaDeg);

            bool rotated = false;

            if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
            {
                meshMatList[pickedMeshIndex] =
                    glm::rotate(glm::mat4(1.0f), rotRad, glm::vec3(0, 1, 0)) *
                    meshMatList[pickedMeshIndex];
                rotated = true;
            }
            if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
            {
                meshMatList[pickedMeshIndex] =
                    glm::rotate(glm::mat4(1.0f), -rotRad, glm::vec3(0, 1, 0)) *
                    meshMatList[pickedMeshIndex];
                rotated = true;
            }

            if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS)
            {
                meshMatList[pickedMeshIndex] =
                    glm::rotate(glm::mat4(1.0f), rotRad, glm::vec3(1, 0, 0)) * meshMatList[pickedMeshIndex];
                rotated = true;
            }
            if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
            {
                meshMatList[pickedMeshIndex] =
                    glm::rotate(glm::mat4(1.0f), -rotRad, glm::vec3(1, 0, 0)) * meshMatList[pickedMeshIndex];
                rotated = true;
            }

            if (rotated)
            {
                meshList[pickedMeshIndex]->initSpatial(false, matModelRoot * meshMatList[pickedMeshIndex]);
            }
        }

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        DrawFloor(matView, matProj, wireframeMode);

        for (int i = 0; i < (int)meshList.size(); i++)
        {
            meshList[i]->draw(matModelRoot * meshMatList[i], matView, matProj);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    glm::mat4 mat = glm::mat4(1.0);

    float angleStep = 5.0f;
    float transStep = 1.0f;

    if (action == GLFW_PRESS)
    {
        if (GLFW_KEY_M == key)
        {
            useMipMaps = !useMipMaps;
            std::cout << "Mipmaps: " << (useMipMaps ? "ON" : "OFF") << std::endl;

            for (auto& m : meshList)
                m->setUseMipmaps(useMipMaps);

            return;
        }

        if (GLFW_KEY_R == key)
        {
            // Reset camera + model root
            matModelRoot = glm::mat4(1.0f);

            gCamPos = viewPos_default;
            gCamFront = glm::normalize(glm::vec3(0, 0, 0) - gCamPos);

            gYaw = glm::degrees(atan2(gCamFront.z, gCamFront.x));
            gPitch = glm::degrees(asin(gCamFront.y));
            UpdateCameraVectors();

            viewPos = gCamPos;
            matView = glm::lookAt(gCamPos, gCamPos + gCamFront, gCamUp);

            return;
        }

        // Keep your other camera keys if you still want them
        // (They won't use WASD anymore so they won't conflict with UE controls.)

        glm::mat4 nextMatView = matView;
        glm::vec3 nextViewPos = viewPos;

        // camera control (CTRL + arrows etc)
        if (mods & GLFW_MOD_CONTROL) {
            if (GLFW_KEY_LEFT == key) {
                mat = glm::rotate(glm::radians(-angleStep), glm::vec3(0.0, 1.0, 0.0));
                nextMatView = mat * matView;
            }
            else if (GLFW_KEY_RIGHT == key) {
                mat = glm::rotate(glm::radians(angleStep), glm::vec3(0.0, 1.0, 0.0));
                nextMatView = mat * matView;
            }
            else if (GLFW_KEY_UP == key) {
                mat = glm::rotate(glm::radians(-angleStep), glm::vec3(1.0, 0.0, 0.0));
                nextMatView = mat * matView;
            }
            else if (GLFW_KEY_DOWN == key) {
                mat = glm::rotate(glm::radians(angleStep), glm::vec3(1.0, 0.0, 0.0));
                nextMatView = mat * matView;
            }
            else if ((GLFW_KEY_KP_ADD == key) ||
                (GLFW_KEY_EQUAL == key) && (mods & GLFW_MOD_SHIFT)) {
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, transStep));
                nextMatView = mat * matView;
            }
            else if ((GLFW_KEY_KP_SUBTRACT == key) || (GLFW_KEY_MINUS == key)) {
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -transStep));
                nextMatView = mat * matView;
            }
        }

        // translation along world axis (arrows etc)
        if (GLFW_KEY_LEFT == key) {
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(transStep, 0.0f, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.x -= transStep;
        }
        else if (GLFW_KEY_RIGHT == key) {
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(-transStep, 0.0f, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.x += transStep;
        }
        else if (GLFW_KEY_UP == key) {
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -transStep, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.y += transStep;
        }
        else if (GLFW_KEY_DOWN == key) {
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, transStep, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.y -= transStep;
        }

        // Wireframe toggle
        else if (GLFW_KEY_G == key)
        {
            wireframeMode = !wireframeMode;

            if (wireframeMode)
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            else
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        // collision check for these key-moves
        AABB mybox{ nextViewPos - 0.2f, nextViewPos + 0.2f };
        std::vector<int> out;

        bool bCollide = false;
        for (std::shared_ptr<Mesh> pMesh : meshList)
        {
            pMesh->pSpatial->QueryAABB(mybox, out);
            if (!out.empty())
                bCollide = true;
        }

        if (!bCollide)
        {
            matView = nextMatView;
            viewPos = nextViewPos;

            // keep UE camera position synced if you still use these keys
            gCamPos = viewPos;
        }
    }
}
