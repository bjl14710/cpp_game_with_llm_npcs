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

// Draws a vertical textured cylinder (caps + wall) centered at `center`.
void drawTexturedCylinder(const Vec3& center, float radius, float halfHeight,
                          int segments, GLuint texture, const sf::Color& tint) {
    glBindTexture(GL_TEXTURE_2D, texture);
    setColor(tint);

    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, 1.0f, 0.0f);
    vertexTextured(Vec3{center.x, center.y + halfHeight, center.z}, 0.5f, 0.5f);
    for (int i = 0; i <= segments; ++i) {
        const float angle = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(segments);
        vertexTextured(Vec3{center.x + std::cos(angle) * radius, center.y + halfHeight,
                            center.z + std::sin(angle) * radius},
                       0.5f + std::cos(angle) * 0.5f, 0.5f + std::sin(angle) * 0.5f);
    }
    glEnd();

    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        const float angle = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(segments);
        const float nx = std::cos(angle);
        const float nz = std::sin(angle);
        const float u = static_cast<float>(i) / static_cast<float>(segments) * 2.0f;
        glNormal3f(nx, 0.0f, nz);
        vertexTextured(Vec3{center.x + nx * radius, center.y - halfHeight, center.z + nz * radius}, u, 0.0f);
        vertexTextured(Vec3{center.x + nx * radius, center.y + halfHeight, center.z + nz * radius}, u, 1.0f);
    }
    glEnd();
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

// Clothing palettes for NPC figures: {body, accent} pairs cycled by index.
const std::array<std::array<sf::Color, 2>, 10> kNpcPalettes = {{
    {{sf::Color(214, 178, 148), sf::Color(120, 70, 40)}},    // baker: flour apron
    {{sf::Color(40, 70, 140), sf::Color(20, 35, 80)}},       // cop: police blue
    {{sf::Color(240, 200, 60), sf::Color(40, 40, 40)}},      // taxi: cab yellow
    {{sf::Color(90, 60, 40), sf::Color(50, 130, 90)}},       // barista: espresso + green
    {{sf::Color(130, 60, 130), sf::Color(230, 220, 200)}},   // librarian: plum cardigan
    {{sf::Color(60, 120, 170), sf::Color(190, 150, 100)}},   // musician: denim + guitar wood
    {{sf::Color(200, 50, 50), sf::Color(250, 245, 235)}},    // hot dog: red + white stripes
    {{sf::Color(160, 90, 40), sf::Color(110, 110, 120)}},    // hardware: work tan + steel
    {{sf::Color(250, 130, 160), sf::Color(245, 245, 250)}},  // tourist: bright pink
    {{sf::Color(110, 110, 100), sf::Color(60, 60, 70)}},     // teacher: tweed grey
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

    const GLfloat ambient[] = {0.32f, 0.32f, 0.34f, 1.0f};
    const GLfloat diffuse[] = {0.9f, 0.9f, 0.85f, 1.0f};
    const GLfloat specular[] = {0.15f, 0.15f, 0.15f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);

    texAsphalt_ = makeCheckerTexture(sf::Color(70, 72, 76), sf::Color(62, 64, 68), 16);
    texPavement_ = makeCheckerTexture(sf::Color(176, 170, 160), sf::Color(158, 152, 142), 8);
    texGrass_ = makeCheckerTexture(sf::Color(64, 130, 62), sf::Color(54, 112, 52), 12);
    texBrick_ = makeBrickTexture(sf::Color(150, 92, 70), sf::Color(190, 180, 170));
    texGlass_ = makeCheckerTexture(sf::Color(120, 160, 200), sf::Color(80, 110, 150), 6);
    texStripe_ = makeStripeTexture(sf::Color(235, 235, 235), sf::Color(200, 60, 60));
    texCloth_ = makeCheckerTexture(sf::Color(255, 255, 255), sf::Color(235, 235, 235), 4);
}

void Renderer3D::beginFrame(const sf::RenderWindow& window, const CameraPose& camera) {
    const sf::Vector2u size = window.getSize();
    aspect_ = size.y == 0 ? 1.0f : static_cast<float>(size.x) / static_cast<float>(size.y);

    glViewport(0, 0, static_cast<GLint>(size.x), static_cast<GLint>(size.y));
    glClearColor(0.56f, 0.72f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
}

void Renderer3D::drawGroundPatch(float minX, float minZ, float maxX, float maxZ, float y,
                                 GLuint texture, const sf::Color& tint, float tile) const {
    glBindTexture(GL_TEXTURE_2D, texture);
    setColor(tint);
    glBegin(GL_QUADS);
    drawTexturedQuad(Vec3{minX, y, minZ}, Vec3{maxX, y, minZ}, Vec3{maxX, y, maxZ}, Vec3{minX, y, maxZ}, tile, tile);
    glEnd();
}

void Renderer3D::drawBuilding(const Building& b) const {
    const Vec3 center{(b.minX + b.maxX) * 0.5f, b.height * 0.5f, (b.minZ + b.maxZ) * 0.5f};
    const float halfX = (b.maxX - b.minX) * 0.5f;
    const float halfZ = (b.maxZ - b.minZ) * 0.5f;
    const float halfY = b.height * 0.5f;

    GLuint texture = texBrick_;
    sf::Color tint = sf::Color::White;
    switch (b.facadeKind) {
        case 0: tint = sf::Color(235, 200, 170); break;                       // bakery: warm brick
        case 1: tint = sf::Color(180, 190, 215); break;                       // police: blue-grey
        case 2: tint = sf::Color(205, 165, 130); break;                       // coffee: roast brown
        case 3: tint = sf::Color(225, 210, 175); break;                       // library: sandstone
        case 4: tint = sf::Color(195, 195, 195); break;                       // hardware: grey
        case 5: texture = texStripe_; break;                                  // hot-dog cart awning
        case 6: texture = texGlass_; tint = sf::Color(245, 210, 70); break;   // taxi: cab yellow
        case 7: texture = texPavement_; tint = sf::Color(205, 215, 225); break;  // fountain stone
        case 8: texture = texPavement_; tint = sf::Color(150, 105, 65); break;   // wooden bench
        case 9: tint = sf::Color(185, 160, 150); break;                       // apartments
        case 10: texture = texGlass_; break;                                  // office tower
        default: break;
    }

    const float tile = std::max(1.0f, halfY / 3.0f);
    drawTexturedBox(center, halfX, halfY, halfZ, texture, tint, tile);

    if (b.id == "fountain") {
        // Water surface just below the rim.
        glDisable(GL_LIGHTING);
        drawGroundPatch(b.minX + 0.7f, b.minZ + 0.7f, b.maxX - 0.7f, b.maxZ - 0.7f,
                        b.height - 0.25f, texGlass_, sf::Color(90, 160, 220), 2.f);
        glEnable(GL_LIGHTING);
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

    // Legs, torso, head, and a short visor/brim hinting at the facing side.
    drawTexturedCylinder(Vec3{0.f, 0.45f, 0.f}, 0.26f, 0.45f, 14, texCloth_, accent);
    drawTexturedCylinder(Vec3{0.f, 1.18f, 0.f}, 0.32f, 0.32f, 14, texCloth_, body);
    drawTexturedBox(Vec3{0.f, 1.68f, 0.f}, 0.17f, 0.18f, 0.17f, texCloth_, skin, 1.f);
    drawTexturedBox(Vec3{0.f, 1.82f, 0.07f}, 0.18f, 0.04f, 0.24f, texCloth_, accent, 1.f);
    drawFace(npc.face);

    // Arms hang at the sides by default. The NPC's right arm (+X local) can be
    // raised or waved in response to a "raise your hand" / "wave" instruction;
    // the left arm always hangs.
    drawArm(-0.40f, body, skin, 0.f, 0.f);  // left, always down
    float raise = 0.f;   // degrees about local +X: 0 = down, ~195 = up & forward
    float swing = 0.f;   // degrees about local +Z for a side-to-side wave
    if (npc.pose == NpcPose::RaiseHand) {
        raise = 195.f;
    } else if (npc.pose == NpcPose::Wave) {
        raise = 195.f;
        swing = 22.f * std::sin(npc.gesturePhase * 9.f);
    }
    drawArm(0.40f, body, skin, raise, swing);  // right, posed

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
    // raised high when surprised.
    float browTilt = 0.f;  // degrees about local +Z, applied mirrored per side
    if (face == NpcFace::Angry) browTilt = -18.f;
    if (face == NpcFace::Sad) browTilt = 14.f;
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
    const float shoulderY = 1.42f;
    const float armHalf = 0.27f;  // half-length of the upper arm
    glPushMatrix();
    glTranslatef(xOffset, shoulderY, 0.f);
    if (swingDeg != 0.f) glRotatef(swingDeg, 0.f, 0.f, 1.f);
    if (raiseDeg != 0.f) glRotatef(raiseDeg, 1.f, 0.f, 0.f);
    // Arm and hand extend downward (-Y) from the shoulder pivot.
    drawTexturedBox(Vec3{0.f, -armHalf, 0.f}, 0.07f, armHalf, 0.07f, texCloth_, sleeve, 1.f);
    drawTexturedBox(Vec3{0.f, -2.f * armHalf - 0.06f, 0.f}, 0.07f, 0.07f, 0.07f, texCloth_, skin, 1.f);
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
