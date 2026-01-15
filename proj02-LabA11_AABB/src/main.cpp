#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp> 

#include "shader.h"
#include "Mesh.h"
//#include "Node.h"


static Shader shader;

glm::mat4 matModelRoot = glm::mat4(1.0);
glm::mat4 matView = glm::mat4(1.0);
glm::mat4 matProj = glm::ortho(-2.0f,2.0f,-2.0f,2.0f, -2.0f,2.0f);
glm::mat4 floorModel(1.0f);

glm::vec3 lightPos = glm::vec3(5.0f, 5.0f, 10.0f);
glm::vec3 viewPos_default = glm::vec3(0.0f, 2.0f, 6.0f);
glm::vec3 viewPos = viewPos_default;

// We are using mesh list instead of scene graph to demo our picking and collision detection
std::vector< std::shared_ptr <Mesh> > meshList;
std::vector< glm::mat4 > meshMatList;



GLuint flatShader;
GLuint blinnShader;
GLuint phongShader;
GLuint texblinnShader;

//Grid floor data
GLuint floorVAO = 0, floorVBO = 0, floorEBO = 0;
GLuint floorShader = 0;
GLsizei floorIndexCount = 0;



bool wireframeMode = false;

// Initialize shader
GLuint initShader(std::string pathVert, std::string pathFrag) 
{
    shader.read_source( pathVert.c_str(), pathFrag.c_str());

    shader.compile();
    glUseProgram(shader.program);

    return shader.program;
}

static void CreateFloorGrid(int vertsPerSide, float spacing, float yLevel)
{
    // vertsPerSide e.g. 41, spacing e.g. 0.25, yLevel e.g. 0.0
    std::vector<glm::vec3> positions;
    std::vector<unsigned int> indices;

    positions.reserve(vertsPerSide * vertsPerSide);

    // Center the grid around (0, yLevel, 0)
    float half = (vertsPerSide - 1) * spacing * 0.5f;

    for (int z = 0; z < vertsPerSide; z++)
    {
        for (int x = 0; x < vertsPerSide; x++)
        {
            float px = (x * spacing) - half;
            float pz = (z * spacing) - half;
            positions.emplace_back(px, yLevel, pz);
        }
    }

    // Two triangles per quad
    for (int z = 0; z < vertsPerSide - 1; z++)
    {
        for (int x = 0; x < vertsPerSide - 1; x++)
        {
            int i0 = z * vertsPerSide + x;
            int i1 = i0 + 1;
            int i2 = i0 + vertsPerSide;
            int i3 = i2 + 1;

            // Triangle 1: i0, i2, i1
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            // Triangle 2: i1, i2, i3
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    floorIndexCount = (GLsizei)indices.size();

    // Upload to GPU
    glGenVertexArrays(1, &floorVAO);
    glGenBuffers(1, &floorVBO);
    glGenBuffers(1, &floorEBO);

    glBindVertexArray(floorVAO);

    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // layout(location = 0) vec3 aPos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glBindVertexArray(0);

    floorModel = glm::mat4(1.0f);
}

static void DrawFloor(const glm::mat4& view, const glm::mat4& proj, bool wireframe)
{
    glUseProgram(floorShader);

    GLint modelLoc = glGetUniformLocation(floorShader, "model");
    GLint viewLoc = glGetUniformLocation(floorShader, "view");
    GLint projLoc = glGetUniformLocation(floorShader, "proj");
    GLint colLoc = glGetUniformLocation(floorShader, "floorColor");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &floorModel[0][0]);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &proj[0][0]);

    // Neutral Unreal-ish grey. Change these if you want.
    glUniform3f(colLoc, 0.18f, 0.18f, 0.20f);

    glBindVertexArray(floorVAO);
    glDrawElements(GL_TRIANGLES, floorIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}


void setLightPosition(glm::vec3 lightPos)
{
    GLuint lightpos_loc = glGetUniformLocation(shader.program, "lightPos" );
    glUniform3fv(lightpos_loc, 1, glm::value_ptr(lightPos));
}

void setViewPosition(glm::vec3 eyePos)
{
    GLuint viewpos_loc = glGetUniformLocation(shader.program, "viewPos" );
    glUniform3fv(viewpos_loc, 1, glm::value_ptr(eyePos));
}


glm::vec3 screenPosToRay(int mouseX, int mouseY, int w, int h,
                         const glm::mat4 &proj, const glm::mat4 &view);

void mouse_button_callback(GLFWwindow *win, int button, int action, int mods);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);


glm::vec3 screenPosToRay(int mouseX, int mouseY, int w, int h,
                         const glm::mat4 &proj, const glm::mat4 &view)
{
    float x = (2.0f * mouseX) / w - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / h;

    // clicked point on the near plane in NDC space
    // if we assume the eye space is scaled so that z_{near} = -1.0
    // then this NDC coordinate is the same as its clip coordinate
    glm::vec4 ray_clip(x, y, -1.0f, 1.0f);

    // set one point with (x, y) and z = -1.0 in eye/camera space
    // the camera is located at (0, 0, 0)
    glm::vec4 ray_eye = glm::inverse(proj) * ray_clip;

    // ray direction in scaled eye space: z_{near} = -1.0
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

    // convert the vector to the word space
    glm::vec3 ray_world = glm::normalize(glm::vec3(glm::inverse(view) * ray_eye));
    return ray_world;
}

void mouse_button_callback(GLFWwindow *win, int button, int action, int mods)
{
    
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double mx, my;
        glfwGetCursorPos(win, &mx, &my);

        std::cout << "Mouse click at: (" << mx <<", " << my << ")" << std::endl;

        int w, h;
        glfwGetWindowSize(win, &w, &h);

        glm::vec3 rayOrig = viewPos;
        glm::vec3 rayDir = screenPosToRay((int)mx, (int)my, w, h, matProj, matView);

        Ray ray{rayOrig, rayDir};

        
        for (std::shared_ptr<Mesh> pMesh : meshList)
        {
            HitInfo hit;
            if (pMesh->pSpatial->Raycast(ray, hit)) {
                std::cout << "Picked triangle index: " << hit.triIndex << ", t=" << hit.t << std::endl;
                pMesh->setPicked(true);
            } else {
                pMesh->setPicked(false);
                std::cout << "No objects picked" << std::endl;
            }
        }
    }
}


int main()
{
    GLFWwindow *window;

    // GLFW init
    if (!glfwInit())
    {
        std::cout << "glfw failed" << std::endl;
        return -1;
    }

    // create a GLFW window
    window = glfwCreateWindow(1920, 1080, "Hello OpenGL 11", NULL, NULL);
    glfwMakeContextCurrent(window);

    // register the key event callback function
    glfwSetKeyCallback(window, key_callback);
    
    // register the mouse button event callback function
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // loading glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Couldn't load opengl" << std::endl;
        glfwTerminate();
        return -1;
    }

    CreateFloorGrid(41, 0.25f, 0.0f);

    phongShader = initShader( "shaders/blinn.vert", "shaders/phong.frag");
    setLightPosition(lightPos);
    setViewPosition(viewPos);
    blinnShader = initShader( "shaders/blinn.vert", "shaders/blinn.frag");
    setLightPosition(lightPos);
    setViewPosition(viewPos);
    // added for LabA07
    texblinnShader = initShader("shaders/texblinn.vert", "shaders/texblinn.frag");
    setLightPosition(lightPos);
    setViewPosition(viewPos);
    floorShader = initShader("shaders/floor.vert", "shaders/floor.frag");

    // set the eye at (0, 0, 5), looking at the centre of the world
    // try to change the eye position
    //viewPos = glm::vec3(0.0f, 2.0f, 5.0f);
    matView = glm::lookAt(viewPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)); 

    // set the Y field of view angle to 60 degrees, width/height ratio to 1.0, and a near plane of 3.5, far plane of 6.5
    // try to play with the FoV
    //matProj = glm::perspective(glm::radians(60.0f), 1.0f, 2.0f, 8.0f);
    // setting to a close near plane and a farway far plane to test collision detection
    matProj = glm::perspective(glm::radians(60.0f), 1.0f, 0.5f, 20.0f);

    //----------------------------------------------------
    // Meshes
    std::shared_ptr<Mesh> cube = std::make_shared<Mesh>();
    cube->init("models/cube.obj", blinnShader);

    glm::mat4 mat = glm::mat4(1.0);

    
    std::shared_ptr<Mesh> teapot = std::make_shared<Mesh>();
    teapot->init("models/teapot.obj", texblinnShader);
    meshList.push_back(teapot);
    mat = glm::translate(glm::vec3(-2.0f, 1.0f, 0.0f));
    meshMatList.push_back(mat); // TRS
    teapot->initSpatial(true, mat);
    
    std::shared_ptr<Mesh> bunny = std::make_shared<Mesh>();
    bunny->init("models/bunny_normal.obj", texblinnShader);
    meshList.push_back(bunny);
    mat = glm::translate(glm::vec3(1.5f, 1.5f, 0.0f)) *
          glm::scale(glm::vec3(0.005f, 0.005f, 0.005f));
    meshMatList.push_back( mat ); // TRS
    bunny->initSpatial(true, mat);
  

  

    // setting the background colour, you can change the value
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    
    glEnable(GL_DEPTH_TEST);
    //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    // setting the event loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        


        for (int i = 0; i < meshList.size(); i++ ) {
            std::shared_ptr<Mesh> pMesh = meshList[i];
            pMesh->draw(matModelRoot * meshMatList[i], matView, matProj);
            DrawFloor(matView, matProj, wireframeMode);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();

    return 0;
}


void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    glm::mat4 mat = glm::mat4(1.0);

    float angleStep = 5.0f;
    float transStep = 1.0f;

    if (action == GLFW_PRESS)
    {
        /*
        // we don't allow objects to move for picking and collision detection
        if (mods & GLFW_MOD_CONTROL) {
            // translation in world space
            if (GLFW_KEY_LEFT == key) {
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(transStep, 0.0f, 0.0f));
                matModelRoot = mat * matModelRoot;
            } else if (GLFW_KEY_RIGHT == key) {
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(-transStep, 0.0f, 0.0f));
            } else if (GLFW_KEY_UP == key) {
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, transStep, 0.0f));
                matModelRoot = mat * matModelRoot;
            } else if (GLFW_KEY_DOWN == key) {
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -transStep, 0.0f));
            }
            matModelRoot = mat * matModelRoot;
        }
        */

        if (GLFW_KEY_R == key)
        {
            // std::cout << "R pressed" << std::endl;
            //  reset
            viewPos = viewPos_default;
            matView = glm::lookAt(viewPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
            matModelRoot = glm::mat4(1.0f);

            return;
        } 

        glm::mat4 nextMatView = matView;
        glm::vec3 nextViewPos = viewPos;

        // camera control
        if (mods & GLFW_MOD_CONTROL) {
            if (GLFW_KEY_LEFT == key) {
                // pan left, rotate around Y, CCW
                mat = glm::rotate(glm::radians(-angleStep), glm::vec3(0.0, 1.0, 0.0));
                nextMatView = mat * matView;
            }
            else if (GLFW_KEY_RIGHT == key) {
                // pan right, rotate around Y, CW
                mat = glm::rotate(glm::radians(angleStep), glm::vec3(0.0, 1.0, 0.0));
                nextMatView = mat * matView;
            }
            else if (GLFW_KEY_UP == key) {
                // tilt up, rotate around X, CCW
                mat = glm::rotate(glm::radians(-angleStep), glm::vec3(1.0, 0.0, 0.0));
                nextMatView = mat * matView;
            }
            else if (GLFW_KEY_DOWN == key) {
                // tilt down, rotate around X, CW
                mat = glm::rotate(glm::radians(angleStep), glm::vec3(1.0, 0.0, 0.0));
                nextMatView = mat * matView;
            }
            else if ((GLFW_KEY_KP_ADD == key) ||
                (GLFW_KEY_EQUAL == key) && (mods & GLFW_MOD_SHIFT)) {
                // zoom in, move along -Z
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, transStep));
                nextMatView = mat * matView;
            }
            else if ((GLFW_KEY_KP_SUBTRACT == key) || (GLFW_KEY_MINUS == key)) {
                // zoom out, move along -Z
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -transStep));
                nextMatView = mat * matView;
            }
        }

        // translation along camera axis (first person view)
        if (GLFW_KEY_A == key) {
            //  move left along -X
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(transStep, 0.0f, 0.0f));
            nextMatView = mat * matView;
        } else if (GLFW_KEY_D == key) {
            // move right along X
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(-transStep, 0.0f, 0.0f));
            nextMatView = mat * matView;
        } else if (GLFW_KEY_W == key) {
            // move forward along -Z
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, transStep));
            nextMatView = mat * matView;
        } else if (GLFW_KEY_S == key) {
            // move backward along Z
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -transStep)); 
            nextMatView = mat * matView;
        }


        // translation along world axis
        if (GLFW_KEY_LEFT == key) {
            //  move left along -X
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(transStep, 0.0f, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.x -= transStep;
        } else if (GLFW_KEY_RIGHT == key) {
            // move right along X
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(-transStep, 0.0f, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.x += transStep;
        } else if (GLFW_KEY_UP == key) {
            // move up along Y
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -transStep, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.y += transStep;
        } else if (GLFW_KEY_DOWN == key) {
            // move down along -Y
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, transStep, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.y -= transStep;
        } else  if ((GLFW_KEY_KP_SUBTRACT == key) || (GLFW_KEY_MINUS == key))  {
            // move backward along +Z
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -transStep));
            nextMatView = matView * mat;
            nextViewPos.z += transStep;
        } else if ((GLFW_KEY_KP_ADD == key) ||
            (GLFW_KEY_EQUAL == key) && (mods & GLFW_MOD_SHIFT))  {
            // move forward along -Z
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, transStep));
            nextMatView = matView * mat;
            nextViewPos.z -= transStep;
        }

        // *Wireframe toggle using the G Key* //
        else
            if (GLFW_KEY_G == key)
            {
                wireframeMode = !wireframeMode;

                if (wireframeMode)
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);   // Wireframe
                else
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);   // Normal fill
            }
    


        // check collision detection
        AABB mybox{ nextViewPos - 0.2f, nextViewPos + 0.2f };
        std::vector<int> out;

        bool bCollide = false;
        for (std::shared_ptr<Mesh> pMesh : meshList)
        {
            pMesh->pSpatial->QueryAABB(mybox, out);
            
            if (out.empty()) {
                std::cout << "No collision" << std::endl;
            } else {
                bCollide = true;
                std::cout << "Collision detected: " << out.size() << std::endl;
            }
        }

        if (!bCollide) {
            matView = nextMatView;
            viewPos = nextViewPos;
        }
    }
}