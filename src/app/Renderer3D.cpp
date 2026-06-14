#include "Renderer3D.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace llm_npc {

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Uploads a square RGBA pixel buffer as a repeating, linearly filtered texture.
GLuint createTexture(const std::vector<std::uint8_t>& pixels, int size) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return texture;
}

// Procedural checkerboard texture of `cells` x `cells` squares in two colors.
GLuint makeCheckerTexture(const sf::Color& a, const sf::Color& b, int cells) {
    constexpr int size = 64;
    std::vector<std::uint8_t> pixels(size * size * 4, 255);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int cellX = (x * cells) / size;
            const int cellY = (y * cells) / size;
            const sf::Color color = ((cellX + cellY) % 2 == 0) ? a : b;
            const int i = (y * size + x) * 4;
            pixels[i + 0] = color.r;
            pixels[i + 1] = color.g;
            pixels[i + 2] = color.b;
        }
    }
    return createTexture(pixels, size);
}

// Procedural vertical-stripe texture: alternating 8-pixel columns.
GLuint makeStripeTexture(const sf::Color& base, const sf::Color& stripe) {
    constexpr int size = 64;
    std::vector<std::uint8_t> pixels(size * size * 4, 255);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const sf::Color color = ((x / 8) % 2 == 0) ? stripe : base;
            const int i = (y * size + x) * 4;
            pixels[i + 0] = color.r;
            pixels[i + 1] = color.g;
            pixels[i + 2] = color.b;
        }
    }
    return createTexture(pixels, size);
}

// Procedural brick texture: running-bond courses with mortar lines.
GLuint makeBrickTexture(const sf::Color& brick, const sf::Color& mortar) {
    constexpr int size = 64;
    constexpr int courseH = 8;
    constexpr int brickW = 16;
    std::vector<std::uint8_t> pixels(size * size * 4, 255);
    for (int y = 0; y < size; ++y) {
        const int course = y / courseH;
        const int offset = (course % 2 == 0) ? 0 : brickW / 2;
        for (int x = 0; x < size; ++x) {
            const bool mortarRow = (y % courseH) == 0;
            const bool mortarCol = ((x + offset) % brickW) == 0;
            const sf::Color color = (mortarRow || mortarCol) ? mortar : brick;
            const int i = (y * size + x) * 4;
            pixels[i + 0] = color.r;
            pixels[i + 1] = color.g;
            pixels[i + 2] = color.b;
        }
    }
    return createTexture(pixels, size);
}

// Procedural windowed-facade texture: a grid of glass panes with dark mullion
// frames, a scatter of panes lit warm. Tiled across a tall building it reads as
// a curtain wall of windows.
GLuint makeWindowTexture() {
    constexpr int size = 128;
    constexpr int panes = 4;          // panes per tile edge
    constexpr int cell = size / panes;
    std::vector<std::uint8_t> pixels(size * size * 4, 255);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int px = x / cell, py = y / cell;
            const bool frame = (x % cell) < 3 || (y % cell) < 3;
            // Cheap hash so a few panes glow as if lit inside.
            const unsigned h = static_cast<unsigned>(px * 73856093 ^ py * 19349663);
            const bool lit = (h % 5u) == 0u;
            sf::Color c;
            if (frame) {
                c = sf::Color(54, 58, 66);                 // mullion / spandrel
            } else if (lit) {
                c = sf::Color(250, 226, 150);              // a lit room
            } else {
                const int j = static_cast<int>(h % 24u);   // subtle pane variation
                c = sf::Color(120 + j, 150 + j, 178 + j);  // cool daylit glass
            }
            const int i = (y * size + x) * 4;
            pixels[i + 0] = c.r;
            pixels[i + 1] = c.g;
            pixels[i + 2] = c.b;
        }
    }
    return createTexture(pixels, size);
}

// Sets the GL color from an opaque SFML color.
void setColor(const sf::Color& color) {
    glColor3f(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
}

// Emits one textured vertex.
void vertexTextured(const Vec3& v, float u, float t) {
    glTexCoord2f(u, t);
    glVertex3f(v.x, v.y, v.z);
}

// Emits a textured quad a-b-c-d with a face normal; must be inside
// glBegin(GL_QUADS).
void drawTexturedQuad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, float tileU, float tileV) {
    const Vec3 n = normalize(cross(b - a, c - a));
    glNormal3f(n.x, n.y, n.z);
    vertexTextured(a, 0.0f, 0.0f);
    vertexTextured(b, tileU, 0.0f);
    vertexTextured(c, tileU, tileV);
    vertexTextured(d, 0.0f, tileV);
}

// Draws an axis-aligned textured box (top + four sides; the bottom is never
// visible from a first-person camera on the ground).
void drawTexturedBox(const Vec3& center, float halfX, float halfY, float halfZ,
                     GLuint texture, const sf::Color& tint, float tileScale) {
    const Vec3 lbf{center.x - halfX, center.y - halfY, center.z - halfZ};
    const Vec3 rbf{center.x + halfX, center.y - halfY, center.z - halfZ};
    const Vec3 rbb{center.x + halfX, center.y - halfY, center.z + halfZ};
    const Vec3 lbb{center.x - halfX, center.y - halfY, center.z + halfZ};
    const Vec3 ltf{center.x - halfX, center.y + halfY, center.z - halfZ};
    const Vec3 rtf{center.x + halfX, center.y + halfY, center.z - halfZ};
    const Vec3 rtb{center.x + halfX, center.y + halfY, center.z + halfZ};
    const Vec3 ltb{center.x - halfX, center.y + halfY, center.z + halfZ};

    glBindTexture(GL_TEXTURE_2D, texture);
    setColor(tint);
    glBegin(GL_QUADS);
    drawTexturedQuad(ltf, rtf, rtb, ltb, tileScale, tileScale);
    drawTexturedQuad(lbf, rbf, rtf, ltf, tileScale, tileScale);
    drawTexturedQuad(rbf, rbb, rtb, rtf, tileScale, tileScale);
    drawTexturedQuad(rbb, lbb, ltb, rtb, tileScale, tileScale);
    drawTexturedQuad(lbb, lbf, ltf, ltb, tileScale, tileScale);
    glEnd();
}

// Draws a vertical frustum (cone slice) with caps and correctly slanted side
// normals, so a torso can flare from waist to shoulders and a limb can taper.
void drawTaperedCylinder(const Vec3& center, float rBottom, float rTop, float halfHeight,
                         int segments, GLuint texture, const sf::Color& tint) {
    glBindTexture(GL_TEXTURE_2D, texture);
    setColor(tint);
    const float yb = center.y - halfHeight;
    const float yt = center.y + halfHeight;

    // Top cap.
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, 1.0f, 0.0f);
    vertexTextured(Vec3{center.x, yt, center.z}, 0.5f, 0.5f);
    for (int i = 0; i <= segments; ++i) {
        const float a = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(segments);
        vertexTextured(Vec3{center.x + std::cos(a) * rTop, yt, center.z + std::sin(a) * rTop},
                       0.5f + std::cos(a) * 0.5f, 0.5f + std::sin(a) * 0.5f);
    }
    glEnd();

    // Bottom cap.
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, -1.0f, 0.0f);
    vertexTextured(Vec3{center.x, yb, center.z}, 0.5f, 0.5f);
    for (int i = segments; i >= 0; --i) {
        const float a = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(segments);
        vertexTextured(Vec3{center.x + std::cos(a) * rBottom, yb, center.z + std::sin(a) * rBottom},
                       0.5f + std::cos(a) * 0.5f, 0.5f + std::sin(a) * 0.5f);
    }
    glEnd();

    // Slanted wall: normal tilts by the radius change over the height.
    const float ny = (rBottom - rTop);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        const float a = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(segments);
        const float c = std::cos(a), s = std::sin(a);
        const Vec3 n = normalize(Vec3{c * 2.0f * halfHeight, ny, s * 2.0f * halfHeight});
        const float u = static_cast<float>(i) / static_cast<float>(segments) * 2.0f;
        glNormal3f(n.x, n.y, n.z);
        vertexTextured(Vec3{center.x + c * rBottom, yb, center.z + s * rBottom}, u, 0.0f);
        vertexTextured(Vec3{center.x + c * rTop, yt, center.z + s * rTop}, u, 1.0f);
    }
    glEnd();
}

// Straight cylinder: a frustum with equal radii.
void drawTexturedCylinder(const Vec3& center, float radius, float halfHeight,
                          int segments, GLuint texture, const sf::Color& tint) {
    drawTaperedCylinder(center, radius, radius, halfHeight, segments, texture, tint);
}

// Smooth UV sphere with per-vertex normals (heads, shoulders, tree canopies).
void drawSphere(const Vec3& center, float radius, int slices, int stacks,
                GLuint texture, const sf::Color& tint) {
    glBindTexture(GL_TEXTURE_2D, texture);
    setColor(tint);
    for (int st = 0; st < stacks; ++st) {
        const float p0 = kPi * (static_cast<float>(st) / stacks - 0.5f);
        const float p1 = kPi * (static_cast<float>(st + 1) / stacks - 0.5f);
        glBegin(GL_QUAD_STRIP);
        for (int sl = 0; sl <= slices; ++sl) {
            const float a = 2.0f * kPi * static_cast<float>(sl) / slices;
            const float ca = std::cos(a), sa = std::sin(a);
            for (float p : {p1, p0}) {
                const float cp = std::cos(p), sp = std::sin(p);
                const Vec3 n{cp * ca, sp, cp * sa};
                glNormal3f(n.x, n.y, n.z);
                vertexTextured(Vec3{center.x + n.x * radius, center.y + n.y * radius,
                                    center.z + n.z * radius},
                               static_cast<float>(sl) / slices, (p == p0 ? st : st + 1) / 1.0f);
            }
        }
        glEnd();
    }
}

// A soft round contact shadow on the ground, centered at the local origin.
// Caller has already translated to the prop's feet. Sits above all ground
// patches and writes no depth so it never z-fights.
void drawBlobShadow(float radius) {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.0f, 0.0f, 0.0f, 0.34f);
    glVertex3f(0.0f, 0.08f, 0.0f);
    glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
    const int seg = 18;
    for (int i = 0; i <= seg; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / seg;
        glVertex3f(std::cos(a) * radius, 0.08f, std::sin(a) * radius);
    }
    glEnd();
    glDepthMask(GL_TRUE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
}

// Builds an OpenGL frustum for a vertical field of view + aspect ratio.
void setupPerspective(float fovDeg, float aspect, float nearPlane, float farPlane) {
    const float top = nearPlane * std::tan(degToRad(fovDeg) * 0.5f);
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
}

// Loads a right-handed look-at matrix into the current matrix stack.
void setupLookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    const std::array<float, 16> matrix = {
        s.x, u.x, -f.x, 0.0f,
        s.y, u.y, -f.y, 0.0f,
        s.z, u.z, -f.z, 0.0f,
        -dot(s, eye), -dot(u, eye), dot(f, eye), 1.0f,
    };
    glLoadMatrixf(matrix.data());
}

// Unit forward vector for yaw/pitch in degrees.
Vec3 forwardFromAngles(float yawDeg, float pitchDeg) {
    const float yaw = degToRad(yawDeg);
    const float pitch = degToRad(pitchDeg);
    const float cp = std::cos(pitch);
    return normalize(Vec3{std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp});
}

// Clothing palettes for NPC figures: {body, accent} pairs indexed by the NPC's
// position in the roster, which loadAllPersonas sorts by filename. Keep this
// list in that alphabetical order so each citizen keeps sensible colors.
const std::array<std::array<sf::Color, 2>, 11> kNpcPalettes = {{
    {{sf::Color(214, 178, 148), sf::Color(120, 70, 40)}},    // baker: flour apron
    {{sf::Color(90, 60, 40), sf::Color(50, 130, 90)}},       // barista: espresso + green
    {{sf::Color(40, 70, 140), sf::Color(20, 35, 80)}},       // cop: police blue
    {{sf::Color(70, 150, 80), sf::Color(240, 235, 225)}},    // grocer: green apron
    {{sf::Color(160, 90, 40), sf::Color(110, 110, 120)}},    // hardware: work tan + steel
    {{sf::Color(200, 50, 50), sf::Color(250, 245, 235)}},    // hotdog: red + white stripes
    {{sf::Color(130, 60, 130), sf::Color(230, 220, 200)}},   // librarian: plum cardigan
    {{sf::Color(60, 120, 170), sf::Color(190, 150, 100)}},   // musician: denim + guitar wood
    {{sf::Color(240, 200, 60), sf::Color(40, 40, 40)}},      // taxi: cab yellow
    {{sf::Color(110, 110, 100), sf::Color(60, 60, 70)}},     // teacher: tweed grey
    {{sf::Color(250, 130, 160), sf::Color(245, 245, 250)}},  // tourist: bright pink
}};

}  // namespace

void Renderer3D::init() {
    glClearDepth(1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_NORMALIZE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_SMOOTH);

    // Warm, slightly-overcast key light with a cool sky-fill ambient.
    const GLfloat ambient[] = {0.34f, 0.36f, 0.40f, 1.0f};
    const GLfloat diffuse[] = {1.0f, 0.96f, 0.88f, 1.0f};
    const GLfloat specular[] = {0.30f, 0.30f, 0.28f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);

    // A gentle specular highlight so curved surfaces read as rounded.
    const GLfloat matSpecular[] = {0.18f, 0.18f, 0.18f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 14.0f);

    // Distance fog dissolves far buildings into the horizon haze (sky bottom).
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    const GLfloat fogColor[] = {0.80f, 0.85f, 0.90f, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_START, 70.0f);
    glFogf(GL_FOG_END, 220.0f);

    // A dim positional "fill" light pinned to the camera each frame so roofed
    // interiors are readable without washing out the daylit exterior.
    glEnable(GL_LIGHT1);
    const GLfloat fillAmbient[] = {0.06f, 0.06f, 0.06f, 1.0f};
    const GLfloat fillDiffuse[] = {0.42f, 0.40f, 0.36f, 1.0f};
    glLightfv(GL_LIGHT1, GL_AMBIENT, fillAmbient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fillDiffuse);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.03f);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.0f);

    texAsphalt_ = makeCheckerTexture(sf::Color(70, 72, 76), sf::Color(62, 64, 68), 16);
    texPavement_ = makeCheckerTexture(sf::Color(176, 170, 160), sf::Color(158, 152, 142), 8);
    texGrass_ = makeCheckerTexture(sf::Color(64, 130, 62), sf::Color(54, 112, 52), 12);
    texBrick_ = makeBrickTexture(sf::Color(150, 92, 70), sf::Color(190, 180, 170));
    texGlass_ = makeCheckerTexture(sf::Color(120, 160, 200), sf::Color(80, 110, 150), 6);
    texStripe_ = makeStripeTexture(sf::Color(235, 235, 235), sf::Color(200, 60, 60));
    texCloth_ = makeCheckerTexture(sf::Color(255, 255, 255), sf::Color(235, 235, 235), 4);
    texWood_ = makeStripeTexture(sf::Color(124, 84, 48), sf::Color(102, 66, 36));
    texLeaf_ = makeCheckerTexture(sf::Color(46, 98, 44), sf::Color(58, 116, 54), 10);
    texShelf_ = makeStripeTexture(sf::Color(178, 62, 52), sf::Color(70, 96, 160));
    texWindow_ = makeWindowTexture();
}

void Renderer3D::beginFrame(const sf::RenderWindow& window, const CameraPose& camera) {
    const sf::Vector2u size = window.getSize();
    aspect_ = size.y == 0 ? 1.0f : static_cast<float>(size.x) / static_cast<float>(size.y);

    glViewport(0, 0, static_cast<GLint>(size.x), static_cast<GLint>(size.y));
    glClearColor(0.80f, 0.85f, 0.90f, 1.0f);  // horizon haze; matches the fog
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawSky();  // gradient background painted before the 3D pass

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setupPerspective(kFovDeg, aspect_, kNearPlane, kFarPlane);

    eye_ = camera.position + Vec3{0.f, kEyeHeight, 0.f};
    forward_ = forwardFromAngles(camera.yawDeg, camera.pitchDeg);
    right_ = normalize(cross(forward_, Vec3{0.f, 1.f, 0.f}));
    up_ = cross(right_, forward_);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    setupLookAt(eye_, eye_ + forward_, Vec3{0.f, 1.f, 0.f});

    const GLfloat lightPos[] = {60.0f, 120.0f, 40.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // Fill light rides with the camera (positional, set in eye space).
    const GLfloat fillPos[] = {eye_.x, eye_.y, eye_.z, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, fillPos);
}

void Renderer3D::drawSky() const {
    // Full-screen vertical gradient in an ortho pass, with depth/lighting/fog
    // off so it sits behind everything and the 3D pass paints over it.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);

    glBegin(GL_QUADS);
    setColor(sf::Color(204, 217, 230));  // horizon haze (== fog color)
    glVertex2f(0.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    setColor(sf::Color(82, 140, 216));   // zenith blue
    glVertex2f(1.0f, 1.0f);
    glVertex2f(0.0f, 1.0f);
    glEnd();

    glEnable(GL_FOG);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Renderer3D::drawGroundPatch(float minX, float minZ, float maxX, float maxZ, float y,
                                 GLuint texture, const sf::Color& tint, float tile) const {
    glBindTexture(GL_TEXTURE_2D, texture);
    setColor(tint);
    glBegin(GL_QUADS);
    drawTexturedQuad(Vec3{minX, y, minZ}, Vec3{maxX, y, minZ}, Vec3{maxX, y, maxZ}, Vec3{minX, y, maxZ}, tile, tile);
    glEnd();
}

void Renderer3D::drawBox(const AABB& box, float y0, float y1, GLuint texture,
                         const sf::Color& tint, float tile) const {
    const Vec3 center{(box.minX + box.maxX) * 0.5f, (y0 + y1) * 0.5f, (box.minZ + box.maxZ) * 0.5f};
    drawTexturedBox(center, (box.maxX - box.minX) * 0.5f, (y1 - y0) * 0.5f,
                    (box.maxZ - box.minZ) * 0.5f, texture, tint, tile);
}

void Renderer3D::drawBuilding(const Building& b) const {
    if (b.enterable) {
        drawHollowBuilding(b);
        return;
    }
    if (b.facadeKind == 14) {  // holding-cell bars
        drawCellBars(b);
        return;
    }

    const Vec3 center{(b.minX + b.maxX) * 0.5f, b.height * 0.5f, (b.minZ + b.maxZ) * 0.5f};
    const float halfX = (b.maxX - b.minX) * 0.5f;
    const float halfZ = (b.maxZ - b.minZ) * 0.5f;
    const float halfY = b.height * 0.5f;

    GLuint texture = texBrick_;
    sf::Color tint = sf::Color::White;
    float tile = std::max(1.0f, halfY / 3.0f);
    const bool tower = (b.facadeKind == 9 || b.facadeKind == 10);
    switch (b.facadeKind) {
        case 5: texture = texStripe_; break;                                  // hot-dog cart awning
        case 6: texture = texGlass_; tint = sf::Color(245, 210, 70); break;   // taxi: cab yellow
        case 7: texture = texPavement_; tint = sf::Color(205, 215, 225); break;  // fountain stone
        case 8: texture = texPavement_; tint = sf::Color(150, 105, 65); break;   // wooden bench
        case 9:  texture = texWindow_; tint = sf::Color(214, 200, 188);        // apartments
                 tile = std::max(2.0f, b.height / 7.0f); break;
        case 10: texture = texWindow_; tint = sf::Color(196, 210, 224);        // office tower
                 tile = std::max(2.0f, b.height / 7.0f); break;
        case 13: tint = sf::Color(150, 150, 158); break;                      // cell wall: concrete
        default: break;
    }

    drawTexturedBox(center, halfX, halfY, halfZ, texture, tint, tile);

    // Tall buildings get a capping cornice and a base plinth so they don't read
    // as bare extruded boxes.
    if (tower) {
        drawBox(AABB{b.minX - 0.4f, b.minZ - 0.4f, b.maxX + 0.4f, b.maxZ + 0.4f},
                b.height - 0.6f, b.height + 0.25f, texBrick_, sf::Color(96, 92, 90), 3.f);
        drawBox(AABB{b.minX - 0.25f, b.minZ - 0.25f, b.maxX + 0.25f, b.maxZ + 0.25f},
                0.f, 1.1f, texPavement_, sf::Color(120, 116, 112), 3.f);
    }

    if (b.id == "fountain") {
        // Water surface just below the rim.
        glDisable(GL_LIGHTING);
        drawGroundPatch(b.minX + 0.7f, b.minZ + 0.7f, b.maxX - 0.7f, b.maxZ - 0.7f,
                        b.height - 0.25f, texGlass_, sf::Color(90, 160, 220), 2.f);
        glEnable(GL_LIGHTING);
    }
}

void Renderer3D::drawHollowBuilding(const Building& b) const {
    sf::Color wallTint(200, 190, 175);
    sf::Color signTint(80, 80, 90);
    switch (b.facadeKind) {
        case 0:  wallTint = sf::Color(235, 200, 170); signTint = sf::Color(150, 90, 60); break;  // bakery
        case 1:  wallTint = sf::Color(180, 190, 215); signTint = sf::Color(40, 70, 140); break;  // police
        case 2:  wallTint = sf::Color(205, 165, 130); signTint = sf::Color(90, 60, 40); break;   // coffee
        case 3:  wallTint = sf::Color(225, 210, 175); signTint = sf::Color(130, 60, 130); break; // library
        case 4:  wallTint = sf::Color(195, 195, 195); signTint = sf::Color(160, 90, 40); break;  // hardware
        case 11: wallTint = sf::Color(205, 180, 150); signTint = sf::Color(60, 120, 170); break; // guitar
        case 12: wallTint = sf::Color(190, 215, 185); signTint = sf::Color(70, 150, 80); break;  // grocery
        default: break;
    }

    const float t = kWallThickness;
    const float cx = (b.minX + b.maxX) * 0.5f;
    const float cz = (b.minZ + b.maxZ) * 0.5f;
    const float half = kDoorWidth * 0.5f;
    const float wallTile = std::max(1.0f, b.height / 3.0f);

    for (const AABB& w : buildingWallSegments(b)) {
        drawBox(w, 0.f, b.height, texBrick_, wallTint, wallTile);
    }

    // Lintel above the doorway and an exterior sign band, oriented to the door.
    AABB lintel{}, sign{};
    switch (b.doorSide) {
        case DoorSide::NegZ: lintel = {cx - half, b.minZ, cx + half, b.minZ + t};
                             sign = {b.minX, b.minZ - 0.3f, b.maxX, b.minZ + 0.1f}; break;
        case DoorSide::PosZ: lintel = {cx - half, b.maxZ - t, cx + half, b.maxZ};
                             sign = {b.minX, b.maxZ - 0.1f, b.maxX, b.maxZ + 0.3f}; break;
        case DoorSide::NegX: lintel = {b.minX, cz - half, b.minX + t, cz + half};
                             sign = {b.minX - 0.3f, b.minZ, b.minX + 0.1f, b.maxZ}; break;
        case DoorSide::PosX: lintel = {b.maxX - t, cz - half, b.maxX, cz + half};
                             sign = {b.maxX - 0.1f, b.minZ, b.maxX + 0.3f, b.maxZ}; break;
    }
    drawBox(lintel, kDoorHeight, b.height, texBrick_, wallTint, 1.f);
    drawBox(sign, b.height - 1.0f, b.height - 0.2f, texStripe_, signTint, 3.f);

    // Flat roof and a wood interior floor inset past the walls.
    drawBox(AABB{b.minX, b.minZ, b.maxX, b.maxZ}, b.height, b.height + 0.4f,
            texBrick_, sf::Color(110, 105, 100), 4.f);
    glDisable(GL_LIGHTING);
    drawGroundPatch(b.minX + t, b.minZ + t, b.maxX - t, b.maxZ - t, 0.06f,
                    texWood_, sf::Color(235, 225, 215), 4.f);
    glEnable(GL_LIGHTING);

    // Eave cornice and base plinth, slightly oversized, to break the box edge.
    drawBox(AABB{b.minX - 0.3f, b.minZ - 0.3f, b.maxX + 0.3f, b.maxZ + 0.3f},
            b.height - 0.4f, b.height + 0.2f, texBrick_, sf::Color(96, 92, 90), 2.f);
    drawBox(AABB{b.minX - 0.2f, b.minZ - 0.2f, b.maxX + 0.2f, b.maxZ + 0.2f},
            0.f, 0.5f, texPavement_, sf::Color(120, 116, 112), 2.f);

    // Storefront glass flanking the doorway on the door wall (sits just proud).
    const sf::Color glass(150, 185, 210);
    const float gy = 0.55f;
    if (b.doorSide == DoorSide::NegZ || b.doorSide == DoorSide::PosZ) {
        const float zc = (b.doorSide == DoorSide::NegZ) ? b.minZ : b.maxZ;
        drawBox(AABB{b.minX + 0.4f, zc - 0.05f, cx - half - 0.2f, zc + 0.05f},
                gy, kDoorHeight, texGlass_, glass, 2.f);
        drawBox(AABB{cx + half + 0.2f, zc - 0.05f, b.maxX - 0.4f, zc + 0.05f},
                gy, kDoorHeight, texGlass_, glass, 2.f);
    } else {
        const float xc = (b.doorSide == DoorSide::NegX) ? b.minX : b.maxX;
        drawBox(AABB{xc - 0.05f, b.minZ + 0.4f, xc + 0.05f, cz - half - 0.2f},
                gy, kDoorHeight, texGlass_, glass, 2.f);
        drawBox(AABB{xc - 0.05f, cz + half + 0.2f, xc + 0.05f, b.maxZ - 0.4f},
                gy, kDoorHeight, texGlass_, glass, 2.f);
    }

    drawShopFixtures(b);

    // Swinging door panel, hinged at one end of the gap; opens as you approach.
    const Vec3 dc = buildingDoorCenter(b);
    const float openFrac = clampf((5.0f - distanceXZ(eye_, dc)) / 3.0f, 0.f, 1.f);
    const bool alongX = (b.doorSide == DoorSide::NegZ || b.doorSide == DoorSide::PosZ);
    glPushMatrix();
    glTranslatef(alongX ? cx - half : dc.x, 0.f, alongX ? dc.z : cz - half);
    glRotatef((alongX ? 0.f : 90.f) + 100.f * openFrac, 0.f, 1.f, 0.f);
    drawTexturedBox(Vec3{kDoorWidth * 0.5f, kDoorHeight * 0.5f, 0.f}, kDoorWidth * 0.5f,
                    kDoorHeight * 0.5f, 0.08f, texWood_, sf::Color(120, 80, 45), 1.f);
    glPopMatrix();
}

void Renderer3D::drawShopFixtures(const Building& b) const {
    const float t = kWallThickness;
    const float ix0 = b.minX + t, iz0 = b.minZ + t, ix1 = b.maxX - t, iz1 = b.maxZ - t;
    const sf::Color wood(120, 80, 45);

    switch (b.facadeKind) {
        case 3:    // library bookshelves
        case 4:    // hardware shelving
        case 12: { // grocery shelving
            for (int i = 0; i < 3; ++i) {
                const float z = iz0 + 1.0f + static_cast<float>(i) * (iz1 - iz0 - 2.0f) / 3.0f;
                drawBox(AABB{ix0 + 1.0f, z, ix1 - 1.0f, z + 0.7f}, 0.f, 2.6f,
                        texShelf_, sf::Color::White, 2.f);
            }
            break;
        }
        case 11: { // guitar shop: counter + guitars on the back wall
            drawBox(AABB{ix0 + 1.0f, iz1 - 1.2f, ix1 - 1.0f, iz1 - 0.4f}, 0.f, 1.1f,
                    texWood_, wood, 2.f);
            for (int i = 0; i < 4; ++i) {
                const float x = ix0 + 1.5f + static_cast<float>(i) * 1.6f;
                drawBox(AABB{x, iz0 + 0.2f, x + 0.5f, iz0 + 0.6f}, 1.2f, 2.6f,
                        texWood_, sf::Color(150, 90, 50), 1.f);
            }
            break;
        }
        default: { // bakery / coffee / police: a service counter at the back
            drawBox(AABB{ix0 + 1.0f, iz1 - 1.4f, ix1 - 1.0f, iz1 - 0.6f}, 0.f, 1.1f,
                    texWood_, wood, 2.f);
            break;
        }
    }
}

void Renderer3D::drawCellBars(const Building& b) const {
    const sf::Color steel(120, 124, 132);
    const float cz = (b.minZ + b.maxZ) * 0.5f;
    const int n = 10;
    for (int i = 0; i < n; ++i) {
        const float x = b.minX + (b.maxX - b.minX) * (static_cast<float>(i) / (n - 1));
        drawBox(AABB{x - 0.06f, cz - 0.06f, x + 0.06f, cz + 0.06f}, 0.f, b.height,
                texPavement_, steel, 1.f);
    }
    drawBox(AABB{b.minX, cz - 0.06f, b.maxX, cz + 0.06f}, b.height - 0.15f, b.height,
            texPavement_, steel, 1.f);
}

void Renderer3D::drawStreetProps() const {
    // Lamp posts at the four plaza corners with a glowing head.
    const float lampX[] = {-30.f, 30.f, -30.f, 30.f};
    const float lampZ[] = {-30.f, -30.f, 30.f, 30.f};
    for (int i = 0; i < 4; ++i) {
        drawTexturedCylinder(Vec3{lampX[i], 2.0f, lampZ[i]}, 0.12f, 2.0f, 8,
                             texPavement_, sf::Color(70, 70, 76));
        glDisable(GL_LIGHTING);
        drawTexturedBox(Vec3{lampX[i], 4.1f, lampZ[i]}, 0.25f, 0.25f, 0.25f,
                        texGlass_, sf::Color(255, 240, 180), 1.f);
        glEnable(GL_LIGHTING);
    }
    // Trees: a cluster in the park (NE) and a pair flanking the guitar shop.
    const float treeX[] = {50.f, 72.f, 58.f, -22.f, 22.f};
    const float treeZ[] = {52.f, 66.f, 78.f, 20.f, 20.f};
    for (int i = 0; i < 5; ++i) {
        drawTexturedCylinder(Vec3{treeX[i], 1.4f, treeZ[i]}, 0.3f, 1.4f, 8,
                             texWood_, sf::Color(110, 75, 45));
        drawTexturedBox(Vec3{treeX[i], 3.4f, treeZ[i]}, 1.5f, 1.5f, 1.5f,
                        texLeaf_, sf::Color::White, 2.f);
    }
}

void Renderer3D::drawCity(const City& city) {
    const float half = city.halfSize();

    // Asphalt base over the whole walkable square.
    drawGroundPatch(-half, -half, half, half, 0.f, texAsphalt_, sf::Color::White, half / 4.f);

    // Sidewalk pads under the nine blocks (centers at -64/0/64, +-26 with curb
    // overhang past the 24-unit block so buildings sit on pavement).
    for (int bx = -1; bx <= 1; ++bx) {
        for (int bz = -1; bz <= 1; ++bz) {
            const float cx = static_cast<float>(bx) * 64.f;
            const float cz = static_cast<float>(bz) * 64.f;
            const bool park = bx == 1 && bz == 1;
            drawGroundPatch(cx - 26.f, cz - 26.f, cx + 26.f, cz + 26.f, 0.04f,
                            park ? texGrass_ : texPavement_,
                            sf::Color::White, park ? 6.f : 10.f);
        }
    }

    for (const auto& building : city.buildings()) {
        drawBuilding(building);
    }

    drawStreetProps();
}

void Renderer3D::drawNpc(const NpcVisual& npc) {
    const auto& palette = kNpcPalettes[static_cast<std::size_t>(npc.palette) % kNpcPalettes.size()];
    const sf::Color body = palette[0];
    const sf::Color accent = palette[1];
    const sf::Color skin(235, 200, 175);

    glPushMatrix();
    glTranslatef(npc.position.x, npc.position.y, npc.position.z);
    // +facing about Y maps the figure's local +Z (visor/front) onto
    // flatForward(facing); negating it mirrored every NPC in X.
    glRotatef(npc.facingDeg, 0.0f, 1.0f, 0.0f);

    // A soft contact shadow grounds the figure.
    drawBlobShadow(0.5f);

    // Rounded, smooth-shaded figure: two tapered legs, hips, a torso that
    // flares to the shoulders, a neck, a spherical head, and a hair cap.
    drawTaperedCylinder(Vec3{-0.13f, 0.45f, 0.f}, 0.11f, 0.16f, 0.45f, 12, texCloth_, accent);
    drawTaperedCylinder(Vec3{0.13f, 0.45f, 0.f}, 0.11f, 0.16f, 0.45f, 12, texCloth_, accent);
    drawSphere(Vec3{0.f, 0.92f, 0.f}, 0.27f, 14, 10, texCloth_, body);
    drawTaperedCylinder(Vec3{0.f, 1.2f, 0.f}, 0.24f, 0.33f, 0.30f, 16, texCloth_, body);
    drawSphere(Vec3{-0.30f, 1.46f, 0.f}, 0.13f, 10, 8, texCloth_, body);
    drawSphere(Vec3{0.30f, 1.46f, 0.f}, 0.13f, 10, 8, texCloth_, body);
    drawTexturedCylinder(Vec3{0.f, 1.55f, 0.f}, 0.09f, 0.07f, 10, texCloth_, skin);
    drawSphere(Vec3{0.f, 1.70f, 0.f}, 0.18f, 16, 12, texCloth_, skin);
    drawFace(npc.face);
    // Hair cap: a flattened sphere skull-cap on the crown, tilted back so it
    // never covers the face.
    glPushMatrix();
    glTranslatef(0.f, 1.78f, -0.02f);
    glScalef(1.0f, 0.55f, 1.0f);
    drawSphere(Vec3{0.f, 0.f, 0.f}, 0.18f, 14, 8, texCloth_, accent);
    glPopMatrix();

    // Arms hang at the sides by default. The NPC's right arm (+X local) can be
    // raised or waved in response to a "raise your hand" / "wave" instruction;
    // the left arm always hangs.
    drawArm(-0.34f, body, skin, 0.f, 0.f);  // left, always down
    float raise = 0.f;   // degrees about local +X: 0 = down, ~195 = up & forward
    float swing = 0.f;   // degrees about local +Z for a side-to-side wave
    if (npc.pose == NpcPose::RaiseHand) {
        raise = 195.f;
    } else if (npc.pose == NpcPose::Wave) {
        raise = 195.f;
        swing = 22.f * std::sin(npc.gesturePhase * 9.f);
    }
    drawArm(0.34f, body, skin, raise, swing);  // right, posed

    glPopMatrix();
}

// Draws the facial features on the head's front (+Z) face in the NPC's local
// frame. Everything is tiny textured boxes sitting just proud of the skin so
// they read at a distance: dark eyes and brows, a mouth whose corners shift
// with the expression, blush patches when embarrassed, wide eyes and an open
// mouth when surprised, slanted brows when angry or sad.
void Renderer3D::drawFace(NpcFace face) const {
    const sf::Color dark(45, 38, 38);
    const sf::Color lip(150, 75, 70);
    const sf::Color blush(235, 140, 150);
    const float zFront = 0.176f;  // head half-depth 0.17 + a hair

    // Eyes: taller when surprised.
    const float eyeHalfY = face == NpcFace::Surprised ? 0.040f : 0.022f;
    drawTexturedBox(Vec3{-0.07f, 1.71f, zFront}, 0.024f, eyeHalfY, 0.006f, texCloth_, dark, 1.f);
    drawTexturedBox(Vec3{0.07f, 1.71f, zFront}, 0.024f, eyeHalfY, 0.006f, texCloth_, dark, 1.f);

    // Brows: flat by default, inner ends pulled down when angry, up when sad,
    // raised high when surprised. With the mirrored per-side rotation below,
    // a positive tilt lowers the inner (nose-side) ends: +theta about Z lifts
    // a box's +X end, and each side's inner end alternates between +X and -X.
    float browTilt = 0.f;  // degrees about local +Z, applied mirrored per side
    if (face == NpcFace::Angry) browTilt = 18.f;
    if (face == NpcFace::Sad) browTilt = -14.f;
    const float browY = face == NpcFace::Surprised ? 1.80f : 1.77f;
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(0.07f * static_cast<float>(side), browY, 0.f);
        glRotatef(browTilt * static_cast<float>(side), 0.f, 0.f, 1.f);
        drawTexturedBox(Vec3{0.f, 0.f, zFront}, 0.038f, 0.008f, 0.006f, texCloth_, dark, 1.f);
        glPopMatrix();
    }

    // Mouth.
    switch (face) {
        case NpcFace::Surprised:
            // Small open "o".
            drawTexturedBox(Vec3{0.f, 1.615f, zFront}, 0.022f, 0.030f, 0.006f, texCloth_, dark, 1.f);
            break;
        case NpcFace::Happy:
            drawTexturedBox(Vec3{0.f, 1.615f, zFront}, 0.045f, 0.008f, 0.006f, texCloth_, lip, 1.f);
            drawTexturedBox(Vec3{-0.048f, 1.628f, zFront}, 0.010f, 0.010f, 0.006f, texCloth_, lip, 1.f);
            drawTexturedBox(Vec3{0.048f, 1.628f, zFront}, 0.010f, 0.010f, 0.006f, texCloth_, lip, 1.f);
            break;
        case NpcFace::Sad:
        case NpcFace::Angry:
            drawTexturedBox(Vec3{0.f, 1.612f, zFront}, 0.040f, 0.008f, 0.006f, texCloth_, lip, 1.f);
            drawTexturedBox(Vec3{-0.044f, 1.600f, zFront}, 0.010f, 0.010f, 0.006f, texCloth_, lip, 1.f);
            drawTexturedBox(Vec3{0.044f, 1.600f, zFront}, 0.010f, 0.010f, 0.006f, texCloth_, lip, 1.f);
            break;
        case NpcFace::Embarrassed:
            // Tight little mouth plus blush patches on the cheeks.
            drawTexturedBox(Vec3{0.f, 1.612f, zFront}, 0.022f, 0.008f, 0.006f, texCloth_, lip, 1.f);
            drawTexturedBox(Vec3{-0.115f, 1.66f, zFront}, 0.030f, 0.016f, 0.005f, texCloth_, blush, 1.f);
            drawTexturedBox(Vec3{0.115f, 1.66f, zFront}, 0.030f, 0.016f, 0.005f, texCloth_, blush, 1.f);
            break;
        case NpcFace::Neutral:
            drawTexturedBox(Vec3{0.f, 1.612f, zFront}, 0.038f, 0.008f, 0.006f, texCloth_, lip, 1.f);
            break;
    }
}

// Draws one arm in the NPC's local frame, pivoting at the shoulder. `xOffset`
// places it left (-) or right (+); `raiseDeg` rotates it up about local +X
// (0 hangs down, ~180 points straight up); `swingDeg` adds a side-to-side
// rotation about local +Z for waving. Upper arm is sleeve-colored cloth with a
// small skin "hand" at the end.
void Renderer3D::drawArm(float xOffset, const sf::Color& sleeve, const sf::Color& skin,
                         float raiseDeg, float swingDeg) const {
    const float shoulderY = 1.46f;
    const float armHalf = 0.27f;  // half-length of the upper arm
    glPushMatrix();
    glTranslatef(xOffset, shoulderY, 0.f);
    if (swingDeg != 0.f) glRotatef(swingDeg, 0.f, 0.f, 1.f);
    if (raiseDeg != 0.f) glRotatef(raiseDeg, 1.f, 0.f, 0.f);
    // A tapered sleeve and a rounded hand extend downward from the shoulder.
    drawTaperedCylinder(Vec3{0.f, -armHalf, 0.f}, 0.07f, 0.10f, armHalf, 10, texCloth_, sleeve);
    drawSphere(Vec3{0.f, -2.f * armHalf - 0.02f, 0.f}, 0.085f, 8, 6, texCloth_, skin);
    glPopMatrix();
}

bool Renderer3D::worldToScreen(const Vec3& world, const sf::RenderWindow& window, sf::Vector2f& out) const {
    const Vec3 rel = world - eye_;
    const float zCam = dot(forward_, rel);
    if (zCam <= kNearPlane) return false;

    const float xCam = dot(right_, rel);
    const float yCam = dot(up_, rel);
    const float tanHalf = std::tan(degToRad(kFovDeg) * 0.5f);

    const float ndcX = xCam / (zCam * tanHalf * aspect_);
    const float ndcY = yCam / (zCam * tanHalf);

    const sf::Vector2u size = window.getSize();
    out.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(size.x);
    out.y = (0.5f - ndcY * 0.5f) * static_cast<float>(size.y);
    return true;
}

}  // namespace llm_npc
