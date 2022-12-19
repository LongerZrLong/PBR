////////////////////////////////////////////////////////////////////////
//
//   Harvard University
//   CS175 : Computer Graphics
//   Professor Steven Gortler
//
////////////////////////////////////////////////////////////////////////

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <fstream>

#define GLEW_STATIC

#include "GL/glew.h"
#include "GL/glfw3.h"

#include "stb_image.h"
#include "tiny_obj_loader.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "cvec.h"
#include "geometrymaker.h"
#include "glsupport.h"
#include "matrix4.h"
#include "ppm.h"
#include "quat.h"
#include "rigtform.h"
#include "arcball.h"

#include "common.h"
#include "scenegraph.h"
#include "drawer.h"
#include "picker.h"
#include "sgutils.h"
#include "geometry.h"
#include "model.h"

using namespace std; // for string, vector, iostream, and other standard C++ stuff

// G L O B A L S ///////////////////////////////////////////////////

static const string USER_OBJ_PATH = "./resource/cerberus/mesh.obj";
static const string USER_PBR_TEX_DIR = "./resource/cerberus";
static const string USER_PBR_TEX_IMG_TYPE = "tga";

static const string ENV_HDR_DIR = "./resource/hdr";
static const string ENV_HDRs[] = {"Arches.hdr", "Canyon.hdr", "CharlesRiver.hdr", "Loft.hdr", "MIT.hdr", "Ruins.hdr"};
static int g_curEnvIdx = 4;   // default to MIT.hdr
static int g_prevEnvIdx = -1;
static string g_items;  // used for ui display

static const float g_frustMinFov = 60.0; // A minimal of 60 degree field of view
static float g_frustFovY = g_frustMinFov; // FOV in y direction (updated by updateFrustFovY)

static const float g_frustNear = 0.1;  // near plane
static const float g_frustFar = 1000.0;  // far plane
static const float g_groundY = -2.0;    // y coordinate of the ground
static const float g_groundSize = 10.0; // half the ground length

static GLFWwindow *g_window;

static int g_windowWidth = 1024;
static int g_windowHeight = 768;
static double g_wScale = 1;
static double g_hScale = 1;

static bool g_mouseClickDown = false; // is the mouse button pressed
static bool g_mouseLClickButton, g_mouseRClickButton, g_mouseMClickButton;
static bool g_spaceDown = false; // space state, for middle mouse emulation
static double g_mouseClickX, g_mouseClickY; // coordinates for mouse click event

bool g_isPicking = false;

const bool g_Gl2Compatible = false;

static shared_ptr<Material>
        g_arcballMat,
        g_pickingMat,
        g_lightMat;

shared_ptr<Material> g_overridingMaterial;

// material for display purpose
static shared_ptr<Material> g_skyboxMat;
static shared_ptr<Material> g_pbrMat;

// for precompute purpose
static shared_ptr<Material> g_equirect2cubemap;
static shared_ptr<Material> g_irradiance;
static shared_ptr<Material> g_prefilter;
static shared_ptr<Material> g_brdf;

// for storing precomputed results
static shared_ptr<CubeMapTexture> g_envCubemap;
static shared_ptr<CubeMapTexture> g_irradianceMap;
static shared_ptr<CubeMapTexture> g_prefilterMap;
static shared_ptr<ImageTexture> g_brdfLUT;


// --------- Geometry

// Macro used to obtain relative offset of a field within a struct
typedef SgGeometryShapeNode MyShapeNode;
static shared_ptr<Geometry> g_plane, g_cube, g_sphere, g_quad;

// --------- Scene
static shared_ptr<SgRootNode> g_world;
static shared_ptr<SgRbtNode> g_skyNode;
static shared_ptr<SgRbtNode> g_currentPickedRbtNode = g_skyNode; // used later when you do picking

const int MAX_LIGHT = 4;
static vector<shared_ptr<SgRbtNode>> g_lightNodes;
Cvec3 g_lightPositions[] = {
//        {-2.0, 5.0, 4.0},
//        {2.0, 5.0, -5.0}
};
Cvec3 g_lightColors[] = {
//        {150.0, 150.0, 150.0},
//        {150.0, 150.0, 150.0}
};

// --------- IBL
static const int g_captureWidth = 1024;
static const int g_captureHeight = 1024;

static const int g_irradianceCaptureWidth = 32;
static const int g_irradianceCaptureHeight = 32;

static const int g_prefilterCaptureWidth = 128;
static const int g_prefilterCaptureHeight = 128;

static const int MAX_MIP_LEVELS = 5;
static const int g_brdfLUTWidth = 512;
static const int g_brdfLUTHeight = 512;


// --------- key frames
typedef std::vector<shared_ptr<SgRbtNode>> SgRbtNodes;
SgRbtNodes g_rbtNodes = {};
list<vector<RigTForm> > g_key_frames = {};
list<vector<RigTForm> >::iterator g_current_key_frame = g_key_frames.end();

string g_key_frame_file_name = "key_frame.dat";


// Global variables for animation timing

// Frames to render per second
static int g_framesPerSecond = 60;
// 2 seconds between keyframes
static int g_msBetweenKeyFrames = 2000;

static double g_lastFrameClock;

// Is the animation playing?
static bool g_playingAnimation = false;
// Time since last key frame
static int g_animateTime = 0;


int view_state = 0;
int sky_mode = 0;

double g_arcballScreenRadius = 0.20 * min(g_windowWidth, g_windowHeight);
double g_arcballScale = 1;

///////////////// END OF G L O B A L S
/////////////////////////////////////////////////////
static void initPlane() {
    int ibLen, vbLen;
    getPlaneVbIbLen(vbLen, ibLen);

    // Temporary storage for cube Geometry
    vector<VertexPNX> vtx(vbLen);
    vector<unsigned short> idx(ibLen);

    makePlane(g_groundSize * 2, vtx.begin(), idx.begin());
    g_plane.reset(new SimpleIndexedGeometryPNX(&vtx[0], &idx[0], vbLen, ibLen));
}

static void initCube() {
    int ibLen, vbLen;
    getCubeVbIbLen(vbLen, ibLen);

    // Temporary storage for cube Geometry
    vector<VertexPNX> vtx(vbLen);
    vector<unsigned short> idx(ibLen);

    makeCube(1, vtx.begin(), idx.begin());
    g_cube.reset(new SimpleIndexedGeometryPNX(&vtx[0], &idx[0], vbLen, ibLen));
}

static void initSphere() {
    int ibLen, vbLen;
    getSphereVbIbLen(20, 10, vbLen, ibLen);

    // Temporary storage for sphere Geometry
    vector<VertexPNX> vtx(vbLen);
    vector<unsigned short> idx(ibLen);
    makeSphere(1, 20, 10, vtx.begin(), idx.begin());
    g_sphere.reset(new SimpleIndexedGeometryPNX(&vtx[0], &idx[0], vtx.size(), idx.size()));
}

static void initQuad() {
    // Temporary storage for quad Geometry
    VertexPX vtx[4] = {
            VertexPX(-1, -1, 0, 0, 0),
            VertexPX(1, -1, 0, 1, 0),
            VertexPX(1, 1, 0, 1, 1),
            VertexPX(-1, 1, 0, 0, 1)
    };

    unsigned short idx[6] = {0, 1, 2, 2, 3, 0};

    g_quad.reset(new SimpleIndexedGeometryPX(vtx, idx, 4, 6));
}

// update g_frustFovY from g_frustMinFov, g_windowWidth, and g_windowHeight
static void updateFrustFovY() {
    if (g_windowWidth >= g_windowHeight)
        g_frustFovY = g_frustMinFov;
    else {
        const double RAD_PER_DEG = 0.5 * CS175_PI / 180;
        g_frustFovY = atan2(sin(g_frustMinFov * RAD_PER_DEG) * g_windowHeight /
                            g_windowWidth,
                            cos(g_frustMinFov * RAD_PER_DEG)) /
                      RAD_PER_DEG;
    }
}

static Matrix4 makeProjectionMatrix() {
    return Matrix4::makeProjection(
            g_frustFovY, g_windowWidth / static_cast<double>(g_windowHeight),
            g_frustNear, g_frustFar);
}

static void drawStuff(bool picking) {
    // short hand for current shader state
    Uniforms uniforms;

    // PerFrame context: projection matrix, view matrix, lights, camera position

    // projection matrix
    const Matrix4 projMat = makeProjectionMatrix();
    sendProjectionMatrix(uniforms, projMat);

    // view matrix
    // use the skyRbt as the eyeRbt
    shared_ptr<SgRbtNode> eyeNode = g_skyNode;

    RigTForm eyeRbt = getPathAccumRbt(g_world, eyeNode);
    RigTForm invEyeRbt = inv(eyeRbt);
    Matrix4 viewMat = rigTFormToMatrix(invEyeRbt);
    sendViewMatrix(uniforms, viewMat);

    // camera position
    uniforms.put("uCameraPos", eyeRbt.getTranslation());

    // lights
    vector<Cvec3> lightPositions;
    vector<Cvec3> lightColors;
    int i = 0;
    for (; i < g_lightNodes.size(); i++) {
        Cvec3 lightPos = getPathAccumRbt(g_world, g_lightNodes[i]).getTranslation();
        lightPositions.push_back(lightPos);
        lightColors.push_back(g_lightColors[i]);
    }
    // fill the rest with dummy data
    for (; i < MAX_LIGHT; i++) {
        lightPositions.push_back({0., 0., 0.});
        lightColors.push_back({0., 0., 0.});
    }

    uniforms.put("uNumLights", (int)g_lightNodes.size());
    uniforms.put("uLightPositions", lightPositions.data(), MAX_LIGHT);
    uniforms.put("uLightColors", lightColors.data(), MAX_LIGHT);

    if (!picking) {
        Drawer drawer(RigTForm(), uniforms);
        g_world->accept(drawer);

        if (g_currentPickedRbtNode && *g_currentPickedRbtNode != *g_skyNode) {
            RigTForm objectRbt = getPathAccumRbt(g_world, g_currentPickedRbtNode);
            if (g_mouseMClickButton ||
                (g_mouseLClickButton && g_mouseRClickButton) ||
                (g_mouseLClickButton && !g_mouseRClickButton &&
                 g_spaceDown)) {}
            else {
                g_arcballScale = getScreenToEyeScale((invEyeRbt * objectRbt).getTranslation()[2],
                                                     g_frustFovY, g_windowHeight);
            }
            Matrix4 modelMat = rigTFormToMatrix(objectRbt) * Matrix4::makeScale(Cvec3(1, 1, 1) * g_arcballScale * g_arcballScreenRadius);
            Matrix4 normalMat = normalMatrix(viewMat * modelMat);

            sendModelMatrix(uniforms, modelMat);

            g_arcballMat->draw(*g_sphere, uniforms);
        }
    } else {
        Picker picker(RigTForm(), uniforms);
        g_overridingMaterial = g_pickingMat;
        g_world->accept(picker);
        g_overridingMaterial.reset();
        glFlush();
        g_currentPickedRbtNode = picker.getRbtNodeAtXY(g_mouseClickX * g_wScale,
                                                       g_mouseClickY * g_hScale);
        if (!g_currentPickedRbtNode)
            g_currentPickedRbtNode = g_skyNode;   // set to NULL
    }
}

static void pick() {
    // We need to set the clear color to black, for pick rendering.
    // so let's save the clear color
    GLdouble clearColor[4];
    glGetDoublev(GL_COLOR_CLEAR_VALUE, clearColor);

    glClearColor(0, 0, 0, 0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawStuff(true);

    // uncomment the line below to see the scene rendered with id as color
    // glfwSwapBuffers(g_window);

    //Now set back the clear color
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    checkGlErrors();
}

static void drawUI() {
    // ImGui render
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Setting");

    if (ImGui::Combo("Environment", &g_curEnvIdx, g_items.data()))
    {
        switch (g_curEnvIdx)
        {
            case 0: g_curEnvIdx = 0; break;
            case 1: g_curEnvIdx = 1; break;
            case 2: g_curEnvIdx = 2; break;
            case 3: g_curEnvIdx = 3; break;
            case 4: g_curEnvIdx = 4; break;
            case 5: g_curEnvIdx = 5; break;
        }
    }

    ImGui::Text("Avg: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static void display() {
    // clear framebuffer color&depth
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawStuff(false);

    drawUI();

    glfwSwapBuffers(g_window); // show the back buffer (where we rendered stuff)

    checkGlErrors();
}

static void reshape(GLFWwindow *window, const int w, const int h) {
    int width, height;
    glfwGetFramebufferSize(g_window, &width, &height);
    glViewport(0, 0, width, height);

    g_windowWidth = w;
    g_windowHeight = h;
    g_arcballScreenRadius = 0.25 * min(g_windowWidth, g_windowHeight);
    cerr << "Size of window is now " << g_windowWidth << "x" << g_windowHeight << endl;
    updateFrustFovY();
}

static Cvec3 point_on_arcball(Cvec2 point, Cvec2 center, double radius) {
    double x = point[0] - center[0], y = point[1] - center[1];
    double x2y2 = x * x + y * y;
    double r2 = radius * radius;
    double z2 = r2 - x2y2;
    if (z2 >= 0) {
        return Cvec3(x / radius, y / radius, sqrt(z2) / radius);
    } else {
        return Cvec3(x / sqrt(x2y2), y / sqrt(x2y2), 0);
    }
}

RigTForm doMtoOwrtA(RigTForm M, RigTForm O, RigTForm A) {
    return A * M * inv(A) * O;
}

static void motion(GLFWwindow *window, double x, double y) {
    if (view_state != 0)return;
    const double dx = x - g_mouseClickX;
    const double dy = g_windowHeight - y - 1 - g_mouseClickY;
    if (!g_mouseClickDown) {
        g_mouseClickX = x;
        g_mouseClickY = g_windowHeight - y - 1;
        return;
    }
    if (!g_currentPickedRbtNode)g_currentPickedRbtNode = g_skyNode;
    RigTForm skyRbt = getPathAccumRbt(g_world, g_skyNode);
    shared_ptr<SgRbtNode> eyeNode = g_skyNode;

    RigTForm C_e = getPathAccumRbt(g_world, eyeNode);
    RigTForm C_s = getPathAccumRbt(g_world, g_currentPickedRbtNode, 1);
    RigTForm C_l = getPathAccumRbt(g_world, g_currentPickedRbtNode);
    RigTForm A = transFact(C_l) * linFact(C_e);
    RigTForm As = inv(C_s) * A;

    RigTForm L = g_currentPickedRbtNode->getRbt();

    RigTForm m, mx, my;
    if (g_mouseLClickButton && !g_mouseRClickButton &&
        !g_spaceDown) { // left button down?
        if (*g_currentPickedRbtNode == *g_skyNode) {

            my = RigTForm(Quat::makeYRotation(-dx));
            mx = RigTForm(Quat::makeXRotation(dy));

            if (sky_mode == 1) {//ego
                As = transFact(C_l);
                L = doMtoOwrtA(my, L, As);

                As = L;
                L = doMtoOwrtA(mx, L, As);
            }

            if (sky_mode == 0) {//orbit
                As = C_s;
                L = doMtoOwrtA(my, L, As);

                As = linFact(L);
                L = doMtoOwrtA(mx, L, As);
            }
        } else {
            Cvec2 obj_center = getScreenSpaceCoord(
                    Cvec3(inv(skyRbt) * C_l * Cvec4(0, 0, 0, 1)),
                    makeProjectionMatrix(),
                    g_frustNear, g_frustFovY,
                    g_windowWidth, g_windowHeight);
            Cvec3 v1 = point_on_arcball(Cvec2(g_mouseClickX, g_mouseClickY), obj_center, g_arcballScreenRadius),
                    v2 = point_on_arcball(Cvec2(g_mouseClickX + dx, g_mouseClickY + dy), obj_center,
                                          g_arcballScreenRadius);
            m = RigTForm(Quat(0, v2) * Quat(0, -v1));

            L = doMtoOwrtA(m, L, As);
        }
    } else if (g_mouseRClickButton &&
               !g_mouseLClickButton) { // right button down?
        if (*g_currentPickedRbtNode == *g_skyNode) {
            m = RigTForm(Cvec3(-dx, -dy, 0) * 0.01);
            L = doMtoOwrtA(m, L, As);
        } else {
            m = RigTForm(Cvec3(dx, dy, 0) * g_arcballScale);
            L = doMtoOwrtA(m, L, As);
        }
    } else if (g_mouseMClickButton ||
               (g_mouseLClickButton && g_mouseRClickButton) ||
               (g_mouseLClickButton && !g_mouseRClickButton &&
                g_spaceDown)) { // middle or (left and right, or left + space)
        // button down?
        if (*g_currentPickedRbtNode == *g_skyNode) {
            m = RigTForm(Cvec3(0, 0, dy) * 0.01);
            L = doMtoOwrtA(m, L, As);
        } else {
            m = RigTForm(Cvec3(0, 0, -dy) * g_arcballScale);
            L = doMtoOwrtA(m, L, As);
        }
    }
    g_currentPickedRbtNode->setRbt(L);

    g_mouseClickX = x;
    g_mouseClickY = g_windowHeight - y - 1;
}

static void mouse(GLFWwindow *window, int button, int state, int mods) {
    // when editing imgui window, block the mouse input
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }

    double x, y;
    glfwGetCursorPos(window, &x, &y);

    g_mouseClickX = x;
    // conversion from window-coordinate-system to OpenGL window-coordinate-system
    g_mouseClickY = g_windowHeight - y - 1;

    g_mouseLClickButton |= (button == GLFW_MOUSE_BUTTON_LEFT && state == GLFW_PRESS);
    g_mouseRClickButton |= (button == GLFW_MOUSE_BUTTON_RIGHT && state == GLFW_PRESS);
    g_mouseMClickButton |= (button == GLFW_MOUSE_BUTTON_MIDDLE && state == GLFW_PRESS);

    g_mouseLClickButton &= !(button == GLFW_MOUSE_BUTTON_LEFT && state == GLFW_RELEASE);
    g_mouseRClickButton &= !(button == GLFW_MOUSE_BUTTON_RIGHT && state == GLFW_RELEASE);
    g_mouseMClickButton &= !(button == GLFW_MOUSE_BUTTON_MIDDLE && state == GLFW_RELEASE);

    g_mouseClickDown = g_mouseLClickButton || g_mouseRClickButton || g_mouseMClickButton;
}

void apply_key_frame() {
    if (g_current_key_frame == g_key_frames.end() || g_key_frames.empty()) return;
    int n_node = g_rbtNodes.size();
    for (int i = 0; i < n_node; ++i) {
        g_rbtNodes[i]->setRbt((*g_current_key_frame)[i]);
    }
}

void new_key_frame() {
    int n_node = g_rbtNodes.size();
    vector<RigTForm> key_frame;
    for (int i = 0; i < n_node; ++i) {
        key_frame.push_back(g_rbtNodes[i]->getRbt());
    }

    if (g_current_key_frame != g_key_frames.end() && !g_key_frames.empty()) {
        ++g_current_key_frame;
    }
    g_key_frames.insert(g_current_key_frame, key_frame);
    --g_current_key_frame;
}

void update_key_frame() {
    if (g_current_key_frame == g_key_frames.end() || g_key_frames.empty()) new_key_frame();
    int n_node = g_rbtNodes.size();
    for (int i = 0; i < n_node; ++i) {
        (*g_current_key_frame)[i] = g_rbtNodes[i]->getRbt();
    }
}

void previous_key_frame() {
    if (g_current_key_frame == g_key_frames.begin() || g_key_frames.empty()) return;
    --g_current_key_frame;
    apply_key_frame();
}

void next_key_frame() {
    if (g_current_key_frame == g_key_frames.end() || g_key_frames.empty()) return;
    ++g_current_key_frame;
    apply_key_frame();
}

void delete_key_frame() {
    if (g_current_key_frame == g_key_frames.end() || g_key_frames.empty()) return;
    g_key_frames.erase(g_current_key_frame);
    if (g_key_frames.empty()) {
        g_current_key_frame = g_key_frames.end();
    } else if (g_current_key_frame != g_key_frames.begin()) {
        --g_current_key_frame;
    }
    apply_key_frame();
}

ostream &operator<<(ostream &os, const RigTForm rtf) {
    Cvec3 t = rtf.getTranslation();
    Quat q = rtf.getRotation();
    os << t[0] << ' '
       << t[1] << ' '
       << t[2] << ' '
       << q[0] << ' '
       << q[1] << ' '
       << q[2] << ' '
       << q[3] << endl;
    return os;
}

void read_key_frame() {
    ifstream file;
    file.open(g_key_frame_file_name, ios::in | ios::binary);
    int n_node = g_rbtNodes.size();
    double t0, t1, t2, q0, q1, q2, q3;
    g_key_frames.clear();
    while (file >> t0 >> t1 >> t2 >> q0 >> q1 >> q2 >> q3) {
        vector<RigTForm> frame = {};
        for (int i = 1; i < n_node; ++i) {
            frame.push_back(RigTForm(Cvec3(t0, t1, t2),
                                     Quat(q0, q1, q2, q3)));
            file >> t0 >> t1 >> t2 >> q0 >> q1 >> q2 >> q3;
        }
        frame.push_back(RigTForm(Cvec3(t0, t1, t2),
                                 Quat(q0, q1, q2, q3)));
        g_key_frames.push_back(frame);
    }
    file.close();
}

void write_key_frame() {
    ofstream file;
    file.open(g_key_frame_file_name, ios::out | ios::binary);
    int n_node = g_rbtNodes.size();
    for (auto kf: g_key_frames) {
        for (auto rtf: kf) {
            file << rtf;
        }
    }
    file.close();
}

static void keyboard(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                exit(0);
            case GLFW_KEY_H:
                cout << " ============== H E L P ==============\n\n"
                     << "h\t\thelp menu\n"
                     << "s\t\tsave screenshot\n"
                     << "p\t\tpick object (pick background to use world camera)"
                     << "use mouse to manipulate world camera or picked object\n"
                     << endl;
                break;
            case GLFW_KEY_S:
                glFlush();
                writePpmScreenshot(g_windowWidth, g_windowHeight, "out.ppm");
                break;
            case GLFW_KEY_SPACE:
                g_spaceDown = true;
                break;
            case GLFW_KEY_M:
                sky_mode = 1 - sky_mode;
                break;
            case GLFW_KEY_P:
                g_isPicking = true;
                break;
            case GLFW_KEY_C:
                apply_key_frame();
                break;
            case GLFW_KEY_U:
                update_key_frame();
                break;
            case GLFW_KEY_PERIOD: // >
                if (!(mods & GLFW_MOD_SHIFT)) break;
                next_key_frame();
                break;
            case GLFW_KEY_COMMA: // <
                if (!(mods & GLFW_MOD_SHIFT)) break;
                previous_key_frame();
                break;
            case GLFW_KEY_D:
                delete_key_frame();
                break;
            case GLFW_KEY_N:
                new_key_frame();
                break;
            case GLFW_KEY_I:
                read_key_frame();
                break;
            case GLFW_KEY_W:
                write_key_frame();
                break;
            case GLFW_KEY_Y:
                if (!g_playingAnimation && g_key_frames.size() >= 4) {
                    g_playingAnimation = true;
                } else {
                    g_playingAnimation = false;
                    apply_key_frame();
                }
                break;
            case GLFW_KEY_MINUS: // -
                g_msBetweenKeyFrames = int(g_msBetweenKeyFrames * 1.5);
                break;
            case GLFW_KEY_EQUAL: // +
                if (!(mods & GLFW_MOD_SHIFT)) break;
                g_msBetweenKeyFrames = max(1, int(g_msBetweenKeyFrames / 1.5));
                break;
        }
    } else {
        switch (key) {
            case GLFW_KEY_SPACE:
                g_spaceDown = false;
                break;
        }
    }
}

void error_callback(int error, const char *description) {
    fprintf(stderr, "Error: %s\n", description);
}

bool interpolate(double t) {
    int max_frame = g_key_frames.size() - 3;
    if (t >= max_frame)return true;
    double alpha = t - floor(t);
    int cnt = -1;
    vector<RigTForm> F1, F2, F_1, F_2, d, e;
    for (auto i = g_key_frames.begin(); i != g_key_frames.end(); ++i, ++cnt) {
        if (cnt == floor(t) - 1) {
            F_1 = *i;
            F1 = *(++i);
            F2 = *(++i);
            F_2 = *(++i);
            break;
        }
    }
    int n_node = g_rbtNodes.size();
    for (int i = 0; i < n_node; ++i) {
        g_rbtNodes[i]->setRbt(slerp(alpha, F1[i], F2[i], F_1[i], F_2[i]));
    }
    return false;
}

void animationUpdate() {
    if (g_playingAnimation) {
        bool endReached = interpolate((float) g_animateTime / g_msBetweenKeyFrames);
        if (!endReached)
            g_animateTime += 1000. / g_framesPerSecond;
        else {
            // finish and clean up
            g_playingAnimation = false;
            g_animateTime = 0;
            g_current_key_frame = g_key_frames.end();
            --g_current_key_frame;
            --g_current_key_frame;
        }
    }
}

static void initGlfwState() {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);

    g_window = glfwCreateWindow(g_windowWidth, g_windowHeight,
                                "PBR", NULL, NULL);
    if (!g_window) {
        fprintf(stderr, "Failed to create GLFW window or OpenGL context\n");
        exit(1);
    }
    glfwMakeContextCurrent(g_window);

    glfwSwapInterval(1);

    glfwSetErrorCallback(error_callback);
    glfwSetMouseButtonCallback(g_window, mouse);
    glfwSetCursorPosCallback(g_window, motion);
    glfwSetWindowSizeCallback(g_window, reshape);
    glfwSetKeyCallback(g_window, keyboard);

    int screen_width, screen_height;
    glfwGetWindowSize(g_window, &screen_width, &screen_height);
    int pixel_width, pixel_height;
    glfwGetFramebufferSize(g_window, &pixel_width, &pixel_height);

    g_wScale = pixel_width / screen_width;
    g_hScale = pixel_height / screen_height;
}

static void initGLState() {
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

static void initMaterials() {
    // Create some prototype materials
    Material solid("./shaders/basic-gl3.vshader", "./shaders/solid-gl3.fshader");
    Material pbr("./shaders/pbr.vshader", "./shaders/pbr.fshader");

    // copy solid prototype, and set to wireframed rendering
    g_arcballMat.reset(new Material(solid));
    g_arcballMat->getUniforms().put("uColor", Cvec3f(1.0f, 1.0f, 1.0f));
    g_arcballMat->getRenderStates().polygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // copy solid prototype, and set to color white
    g_lightMat.reset(new Material(solid));
    g_lightMat->getUniforms().put("uColor", Cvec3f(1, 1, 1));

    // precompute purpose
    g_equirect2cubemap.reset(new Material("./shaders/cubemap.vshader", "./shaders/equirect2cubemap.fshader"));
    g_irradiance.reset(new Material("./shaders/cubemap.vshader", "./shaders/irradiance_conv.fshader"));
    g_prefilter.reset(new Material("./shaders/cubemap.vshader", "./shaders/prefilter.fshader"));
    g_brdf.reset(new Material("./shaders/brdf.vshader", "./shaders/brdf.fshader"));

    // pick shader
    g_pickingMat.reset(new Material("./shaders/basic-gl3.vshader", "./shaders/pick-gl3.fshader"));

    // skybox material
    g_skyboxMat.reset(new Material("./shaders/skybox.vshader", "./shaders/skybox.fshader"));

    // user pbr materials
    g_pbrMat = loadPBRTextures(pbr, USER_PBR_TEX_DIR.c_str(), USER_PBR_TEX_IMG_TYPE.c_str());
}

static void initGeometry() {
    initPlane();
    initCube();
    initSphere();
    initQuad();
}

static void initScene() {
    // root node
    g_world.reset(new SgRootNode());
    g_world->addChild(shared_ptr<MyShapeNode>(new MyShapeNode(g_cube, g_skyboxMat)));

    // sky node
    g_skyNode.reset(new SgRbtNode(RigTForm(Cvec3(0.0, 0.0, 10.0))));
    g_world->addChild(g_skyNode);

    // light nodes
    for (int i = 0; i < sizeof(g_lightPositions) / sizeof(g_lightPositions[0]); i++) {
        auto ptr = make_shared<SgRbtNode>(RigTForm(g_lightPositions[i]));
        ptr->addChild(shared_ptr<MyShapeNode>(
                new MyShapeNode(g_sphere, g_lightMat,
                                Cvec3(0, 0, 0),
                                Cvec3(0, 0, 0),
                                Cvec3(0.15, 0.15, 0.15))));
        g_lightNodes.push_back(ptr);
    }

    for (int i = 0; i < g_lightNodes.size(); i++) {
        g_world->addChild(g_lightNodes[i]);
    }

    // custom pbr model
    auto node = make_shared<SgRbtNode>(RigTForm(Cvec3(0,0,0), Quat::makeYRotation(-45)));
    auto geoPtr = loadObj(USER_OBJ_PATH.c_str());
    node->addChild(shared_ptr<MyShapeNode>(new MyShapeNode(geoPtr, g_pbrMat)));
    g_world->addChild(node);

    dumpSgRbtNodes(g_world, g_rbtNodes);
}

static void initIBL() {
    Uniforms *uniformsPtr;  // for convenience

    // pbr: setup framebuffer
    // ----------------------
    unsigned int captureFBO;
    unsigned int captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, g_captureWidth, g_captureHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // pbr: load the HDR environment map
    // ---------------------------------
    string curEnvHdrPath = ENV_HDR_DIR;
    curEnvHdrPath += "/";
    curEnvHdrPath += ENV_HDRs[g_curEnvIdx];

    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    float *data = stbi_loadf(curEnvHdrPath.c_str(), &width, &height, &nrComponents, 0);

    auto hdrTexture = make_shared<ImageTexture>();
    if (data)
    {
        hdrTexture->bind();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data); // note how we specify the texture's data value to be float

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load HDR image." << std::endl;
    }

    // pbr: setup cubemap to render to and attach to framebuffer
    // ---------------------------------------------------------
    g_envCubemap = std::make_shared<CubeMapTexture>();
    g_envCubemap->bind();
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, g_captureWidth, g_captureHeight, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // pbr: set up projection and view matrices for capturing data onto the 6 cubemap face directions
    // ----------------------------------------------------------------------------------------------
    const Matrix4 captureProjection = Matrix4::makeProjection(90.0f, 1.0f, 0.1f, 10.0f);
    const Matrix4 captureViews[] =
            {
                    Matrix4::lookAt(Cvec3(0.0f, 0.0f, 0.0f), Cvec3( 1.0f,  0.0f,  0.0f), Cvec3(0.0f, -1.0f,  0.0f)),
                    Matrix4::lookAt(Cvec3(0.0f, 0.0f, 0.0f), Cvec3(-1.0f,  0.0f,  0.0f), Cvec3(0.0f, -1.0f,  0.0f)),
                    Matrix4::lookAt(Cvec3(0.0f, 0.0f, 0.0f), Cvec3( 0.0f,  1.0f,  0.0f), Cvec3(0.0f,  0.0f,  1.0f)),
                    Matrix4::lookAt(Cvec3(0.0f, 0.0f, 0.0f), Cvec3( 0.0f, -1.0f,  0.0f), Cvec3(0.0f,  0.0f, -1.0f)),
                    Matrix4::lookAt(Cvec3(0.0f, 0.0f, 0.0f), Cvec3( 0.0f,  0.0f,  1.0f), Cvec3(0.0f, -1.0f,  0.0f)),
                    Matrix4::lookAt(Cvec3(0.0f, 0.0f, 0.0f), Cvec3( 0.0f,  0.0f, -1.0f), Cvec3(0.0f, -1.0f,  0.0f))
            };

    // pbr: convert HDR equirectangular environment map to cubemap equivalent
    // ----------------------------------------------------------------------
    uniformsPtr = &g_equirect2cubemap->getUniforms();
    uniformsPtr->put("uEquirectangularMap", hdrTexture);
    sendProjectionMatrix(*uniformsPtr, captureProjection);

    glViewport(0, 0, g_captureWidth, g_captureHeight); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (int i = 0; i < 6; i++)
    {
        sendViewMatrix(*uniformsPtr, captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, g_envCubemap->getGlTexture(), 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        g_equirect2cubemap->draw(*g_cube, *uniformsPtr);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

     // pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
    // --------------------------------------------------------------------------------
    g_irradianceMap = make_shared<CubeMapTexture>();
    g_irradianceMap->bind();
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, g_irradianceCaptureWidth, g_irradianceCaptureHeight, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, g_irradianceCaptureWidth, g_irradianceCaptureHeight);

    // pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
    // -----------------------------------------------------------------------------
    uniformsPtr = &g_irradiance->getUniforms();
    uniformsPtr->put("uEnvironmentMap", g_envCubemap);
    sendProjectionMatrix(*uniformsPtr, captureProjection);

    glViewport(0, 0, g_irradianceCaptureWidth, g_irradianceCaptureHeight); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        sendViewMatrix(*uniformsPtr, captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, g_irradianceMap->getGlTexture(), 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        g_irradiance->draw(*g_cube, *uniformsPtr);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // pbr: create a pre-filter cubemap, and re-scale capture FBO to pre-filter scale.
    // --------------------------------------------------------------------------------
    g_prefilterMap = make_shared<CubeMapTexture>();
    g_prefilterMap->bind();
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, g_prefilterCaptureWidth, g_prefilterCaptureHeight, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minification filter to mip_linear
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // pbr: run a quasi monte-carlo simulation on the environment lighting to create a prefilter (cube)map.
    // ----------------------------------------------------------------------------------------------------
    uniformsPtr = &g_prefilter->getUniforms();
    uniformsPtr->put("uEnvironmentMap", g_envCubemap);
    sendProjectionMatrix(*uniformsPtr, captureProjection);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int mip = 0; mip < MAX_MIP_LEVELS; ++mip)
    {
        // reisze framebuffer according to mip-level size.
        unsigned int mipWidth  = 128 * std::pow(0.5, mip);
        unsigned int mipHeight = 128 * std::pow(0.5, mip);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(MAX_MIP_LEVELS - 1);
        uniformsPtr->put("uRoughness", roughness);
        for (unsigned int i = 0; i < 6; ++i)
        {
            sendViewMatrix(*uniformsPtr, captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, g_prefilterMap->getGlTexture(), mip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            g_prefilter->draw(*g_cube, *uniformsPtr);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // pbr: generate a 2D LUT from the BRDF equations used.
    // ----------------------------------------------------
    g_brdfLUT = make_shared<ImageTexture>();
    g_brdfLUT->bind();

    // pre-allocate enough memory for the LUT texture.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, g_brdfLUTWidth, g_brdfLUTHeight, 0, GL_RG, GL_FLOAT, 0);

    // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, g_brdfLUTWidth, g_brdfLUTHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_brdfLUT->getGlTexture(), 0);

    glViewport(0, 0, g_brdfLUTWidth, g_brdfLUTHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    g_brdf->draw(*g_quad, Uniforms());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // set skybox to the cube map converted from hdr
    g_skyboxMat->getUniforms().put("uSkyBox", g_envCubemap);

    // set irradiance map in
    g_pbrMat->getUniforms().put("uIrradianceMap", g_irradianceMap);

    // set prefilter map
    g_pbrMat->getUniforms().put("uPrefilterMap", g_prefilterMap);

    // set brdfLUT
    g_pbrMat->getUniforms().put("uBrdfLUT", g_brdfLUT);

    // convert viewport back to screen
    glfwGetFramebufferSize(g_window, &width, &height);
    glViewport(0, 0, width, height);
}

void initImGui() {
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void initUI() {
    // construct items in the menu
    for (auto str : ENV_HDRs) {
        g_items += str.substr(0, str.find('.'));
        g_items.push_back('\0');
    }
}

void glfwLoop() {
    g_lastFrameClock = glfwGetTime();
    while (!glfwWindowShouldClose(g_window)) {
        // if env hdr changes, reinitialize
        if (g_curEnvIdx != g_prevEnvIdx) {
            initIBL();
            g_prevEnvIdx = g_curEnvIdx;
        }

        if (g_playingAnimation) {
            double thisTime = glfwGetTime();
            if (thisTime - g_lastFrameClock >= 1. / g_framesPerSecond) {
                animationUpdate();
                display();
                g_lastFrameClock = thisTime;
            }
            glfwPollEvents();
        } else {
            if (g_isPicking) {
                pick();
                if (g_mouseLClickButton && !g_mouseRClickButton)g_isPicking = false;
            } else display();
            glfwWaitEvents();
        }
    }
    printf("end loop\n");

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(g_window);
    glfwTerminate();
}

int main(int argc, char *argv[]) {
    try {
        initGlfwState();

        glewInit(); // load the OpenGL extensions
#ifndef __MAC__
        if (!GLEW_VERSION_3_0)
              throw runtime_error("Error: card/driver does not support OpenGL "
                                  "Shading Language v1.3");
#endif

        initGLState();
        initMaterials();
        initGeometry();
        initScene();
        initImGui();
        initUI();

        glfwLoop();
        return 0;
    } catch (const runtime_error &e) {
        cout << "Exception caught: " << e.what() << endl;
        return -1;
    }
}
