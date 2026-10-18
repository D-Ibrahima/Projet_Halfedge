#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "halfedge.hpp"
#include "lecture.hpp"

/
void afficherStats(const HalfEdgeMesh& m, const std::string& path) {
    const Stats s = m.stats();
    std::cout << "\n=== " << path << " ===\n"
              << "V=" << s.V << "  E=" << s.E << "  F=" << s.F
              << "  V-E+F=" << s.euler << "\n"
              << "Demi-aretes de bord (sans twin) : " << s.sansTwin
              << (s.sansTwin == 0 ? "  -> maillage ferme\n" : "  -> maillage ouvert ou irregulier\n");
    if (m.aretesDupliquees())
        std::cout << "Attention : " << m.aretesDupliquees()
                  << " aretes orientees dupliquees (non manifold ou faces mal orientees)\n";

    // Demo parcours : sommet 0 et face 0
    const HE_Vertex* v0 = &m.sommets()[0];
    std::cout << "Sommet 0 : " << m.sortantes(v0).size() << " demi-aretes sortantes\n";
    std::cout << "Face 0   : " << m.sommetsDeFace(&m.faces()[0]).size() << " sommets\n";
}

// ===============================================================
// OpenGL
// ===============================================================
struct GPUMesh {
    GLuint vao = 0, vbo = 0;
    GLsizei count = 0;   // nombre de sommets a dessiner
};


void envoyerAuGPU(const HalfEdgeMesh& m, GPUMesh& g) {
    Eigen::Vector3d mn = m.sommets()[0].Coord, mx = mn;
    for (const HE_Vertex& v : m.sommets()) {
        mn = mn.cwiseMin(v.Coord);
        mx = mx.cwiseMax(v.Coord);
    }
    const Eigen::Vector3d c = (mn + mx) / 2;
    double r = (mx - mn).maxCoeff() / 2;
    if (r <= 0) r = 1;

    std::vector<float> buf;
    for (const HE_Face& f : m.faces()) {
        std::vector<Eigen::Vector3d> p;
        const HalfEdge* h = f.he;
        do {
            p.push_back((h->target->Coord - c) / r);
            h = h->next;
        } while (h != f.he);

        Eigen::Vector3d n = Eigen::Vector3d::Zero();
        for (size_t i = 0; i < p.size(); ++i) {
            const auto& a = p[i];
            const auto& b = p[(i + 1) % p.size()];
            n += Eigen::Vector3d((a.y() - b.y()) * (a.z() + b.z()),
                                 (a.z() - b.z()) * (a.x() + b.x()),
                                 (a.x() - b.x()) * (a.y() + b.y()));
        }
        if (n.norm() > 0) n.normalize();

        auto push = [&](const Eigen::Vector3d& q) {
            for (int k = 0; k < 3; ++k) buf.push_back(float(q[k]));
            for (int k = 0; k < 3; ++k) buf.push_back(float(n[k]));
        };
        for (size_t i = 1; i + 1 < p.size(); ++i) {
            push(p[0]);
            push(p[i]);
            push(p[i + 1]);
        }
    }

    if (!g.vao) glGenVertexArrays(1, &g.vao);
    if (!g.vbo) glGenBuffers(1, &g.vbo);
    glBindVertexArray(g.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(buf.size() * sizeof(float)), buf.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    g.count = GLsizei(buf.size() / 6);
}

const char* VERT = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat3 uRot;
out vec3 vN;
void main() {
    vN = uRot * aNormal;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

const char* FRAG = R"GLSL(
#version 330 core
in vec3 vN;
uniform int uWire;
out vec4 color;
void main() {
    if (uWire == 1) { color = vec4(0.08, 0.08, 0.10, 1.0); return; }
    float d = abs(normalize(vN).z);               // lumiere = camera, double face
    color = vec4(vec3(0.35, 0.55, 0.85) * (0.30 + 0.70 * d), 1.0);
}
)GLSL";

GLuint compiler(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "Erreur shader :\n" << log << "\n";
    }
    return s;
}

GLuint creerProgramme() {
    GLuint p = glCreateProgram();
    GLuint v = compiler(GL_VERTEX_SHADER, VERT), f = compiler(GL_FRAGMENT_SHADER, FRAG);
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

Eigen::Matrix4f perspective(float fovy, float aspect, float n, float f) {
    const float t = std::tan(fovy / 2);
    Eigen::Matrix4f P = Eigen::Matrix4f::Zero();
    P(0, 0) = 1 / (aspect * t);
    P(1, 1) = 1 / t;
    P(2, 2) = -(f + n) / (f - n);
    P(2, 3) = -2 * f * n / (f - n);
    P(3, 2) = -1;
    return P;
}

/
// ---------------------------------------------------------------
static float  gYaw = 0.6f, gPitch = 0.4f, gDist = 3.0f;
static bool   gDrag = false, gWire = true;
static double gLastX = 0, gLastY = 0;
static std::string gPending;   // fichier depose dans la fenetre

void onMouseButton(GLFWwindow* w, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        gDrag = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &gLastX, &gLastY);
    }
}
void onCursor(GLFWwindow*, double x, double y) {
    if (gDrag) {
        gYaw   += float(x - gLastX) * 0.01f;
        gPitch += float(y - gLastY) * 0.01f;
        gPitch = std::max(-1.5f, std::min(1.5f, gPitch));
    }
    gLastX = x;
    gLastY = y;
}
void onScroll(GLFWwindow*, double, double dy) {
    gDist *= (dy > 0) ? 0.9f : 1.1f;
    gDist = std::max(1.2f, std::min(20.0f, gDist));
}
void onKey(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
    if (key == GLFW_KEY_W) gWire = !gWire;
}
void onDrop(GLFWwindow*, int count, const char** paths) {
    if (count > 0) gPending = paths[0];
}










int main(int argc, char** argv) {
    if (!glfwInit()) { std::cerr << "GLFW : echec d'initialisation\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(1000, 800, "Half-edge viewer", nullptr, nullptr);
    if (!window) { std::cerr << "Impossible de creer la fenetre OpenGL 3.3\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onCursor);
    glfwSetScrollCallback(window, onScroll);
    glfwSetKeyCallback(window, onKey);
    glfwSetDropCallback(window, onDrop);

    const GLuint prog = creerProgramme();
    const GLint uMVP = glGetUniformLocation(prog, "uMVP");
    const GLint uRot = glGetUniformLocation(prog, "uRot");
    const GLint uWire = glGetUniformLocation(prog, "uWire");

    GPUMesh gpu;
    HalfEdgeMesh mesh;

    auto charger = [&](const std::string& path) {
        MaillageBrut brut;
        if (!lireMaillage(path, brut)) {
            std::cerr << "Impossible de charger " << path << "\n";
            return;
        }
        if (brut.facesIgnorees) std::cout << brut.facesIgnorees << " faces ignorees (points, segments, degenerees)\n";
        mesh.construire(brut.points, brut.polygones);
        afficherStats(mesh, path);
        envoyerAuGPU(mesh, gpu);
        glfwSetWindowTitle(window, ("Half-edge viewer - " + path).c_str());
    };

    if (argc > 1) charger(argv[1]);
    else std::cout << "Glisse un fichier 3D dans la fenetre (ou : ./halfedge fichier.obj)\n";

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (!gPending.empty()) {
            charger(gPending);
            gPending.clear();
        }

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        if (w == 0 || h == 0) continue;   // fenetre reduite
        glViewport(0, 0, w, h);
        glClearColor(0.95f, 0.95f, 0.97f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (gpu.count > 0) {
            const Eigen::Matrix3f R =
                (Eigen::AngleAxisf(gPitch, Eigen::Vector3f::UnitX()) *
                 Eigen::AngleAxisf(gYaw, Eigen::Vector3f::UnitY())).toRotationMatrix();
            Eigen::Matrix4f V = Eigen::Matrix4f::Identity();
            V.block<3, 3>(0, 0) = R;
            V(2, 3) = -gDist;
            const Eigen::Matrix4f MVP = perspective(0.8f, float(w) / float(h), 0.1f, 100.0f) * V;

            glUseProgram(prog);
            glUniformMatrix4fv(uMVP, 1, GL_FALSE, MVP.data());
            glUniformMatrix3fv(uRot, 1, GL_FALSE, R.data());
            glBindVertexArray(gpu.vao);

            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
            glUniform1i(uWire, 0);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawArrays(GL_TRIANGLES, 0, gpu.count);
            glDisable(GL_POLYGON_OFFSET_FILL);

            if (gWire) {
                glUniform1i(uWire, 1);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glDrawArrays(GL_TRIANGLES, 0, gpu.count);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
        }
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}