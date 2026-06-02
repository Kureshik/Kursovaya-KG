#include "Game.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace
{
constexpr float kPi = 3.1415926535f;
constexpr float kArenaSize = 12.0f;
constexpr float kEyeHeight = 0.72f;
constexpr float kPlayerRadius = 0.24f;
constexpr float kCrystalX = 10.4f;
constexpr float kCrystalY = 10.4f;

float Length(float x, float y)
{
    return std::sqrt(x * x + y * y);
}

float Distance(float x0, float y0, float x1, float y1)
{
    return Length(x0 - x1, y0 - y1);
}

float Random01(unsigned int& seed)
{
    seed = seed * 1664525u + 1013904223u;
    return static_cast<float>((seed >> 8) & 0x00ffffff) / static_cast<float>(0x00ffffff);
}
} // namespace

void NeonMazeGame::Init()
{
    maze = {
        "#################",
        "#S.........P...E#",
        "#.###.#.#####.#.#",
        "#.#...#.....#T#.#",
        "#.#.#######.#.#.#",
        "#.#.......#.#...#",
        "#.#######.#.###.#",
        "#P......#.#....P#",
        "#######.#.#####.#",
        "#.....#.#T....#.#",
        "#.###.#.#####.#.#",
        "#...#......T....#",
        "#################",
    };

    portals.clear();
    for (int y = 0; y < static_cast<int>(maze.size()); ++y)
    {
        for (int x = 0; x < static_cast<int>(maze[y].size()); ++x)
        {
            if (maze[y][x] == 'S')
            {
                spawnX = static_cast<float>(x) + 0.5f;
                spawnY = static_cast<float>(y) + 0.5f;
            }
            if (maze[y][x] == 'P')
            {
                portals.push_back({x, y});
            }
        }
    }

    stars.clear();
    unsigned int seed = 20260531u;
    for (int i = 0; i < 180; ++i)
    {
        stars.push_back({Random01(seed), Random01(seed), Random01(seed), 0.4f + Random01(seed) * 0.6f});
    }

    arenaLevel = {
        ".........#..",
        "...####..#..",
        "...#.....#..",
        "...#.....#..",
        "...#.##..#..",
        "...#..#..#..",
        "..##..#.....",
        "......#.....",
        "......#..###",
        "...#..#.....",
        "...#..####..",
        "...#........",
    };

    arenaWalls.clear();
    for (int y = 0; y < static_cast<int>(arenaLevel.size()); ++y)
    {
        for (int x = 0; x < static_cast<int>(arenaLevel[y].size()); ++x)
        {
            if (arenaLevel[y][x] == '#')
            {
                arenaWalls.push_back(
                    {static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + 1),
                     static_cast<float>(y + 1)});
            }
        }
    }

    mazeTexture.LoadTexture("textures/neon_tiles.png", GL_NEAREST);
    crystalTexture.LoadTexture("textures/neon_crystal.png", GL_NEAREST);
    portalTexture.LoadTexture("textures/portal_alpha.png", GL_LINEAR);

    neonShader.VshaderFileName = "shaders/v.vert";
    neonShader.FshaderFileName = "shaders/neon.frag";
    neonShader.LoadShaderFromFile();
    neonShader.Compile();

    neonTextureShader.VshaderFileName = "shaders/v.vert";
    neonTextureShader.FshaderFileName = "shaders/neon_texture.frag";
    neonTextureShader.LoadShaderFromFile();
    neonTextureShader.Compile();

    crystalModel.LoadModel("models/neon_crystal.obj_m");

    hudText.setSize(820, 210);
    centerText.setSize(700, 130);

    ResetGame();
    SetCursorVisible(false);
}

void NeonMazeGame::ResetGame()
{
    mode = Mode::TopDown;
    playerX = spawnX;
    playerY = spawnY;
    fpX = 1.4f;
    fpY = 1.4f;
    fpYaw = 0.35f;
    fpPitch = 0.0f;
    lives = 3;
    fragments = 0;
    activePortal = -1;
    portalDone = {false, false, false};
    portalCooldown = 0.0;
    damageCooldown = 0.0;
    transitionGlow = 1.0;
    lastMouseX = -1;
    lastMouseY = -1;
    sectionClear = false;
    SetMouseLock(false);
}

void NeonMazeGame::Update(double deltaTime)
{
    totalTime += deltaTime;
    portalCooldown = std::max(0.0, portalCooldown - deltaTime);
    damageCooldown = std::max(0.0, damageCooldown - deltaTime);
    transitionGlow = std::max(0.0, transitionGlow - deltaTime * 1.6);

    bool r = IsKeyDown('R');
    bool esc = IsKeyDown(VK_ESCAPE);

    if (r && !prevR)
    {
        ResetGame();
    }

    if (mode == Mode::FirstPerson && esc && !prevEsc && sectionClear)
    {
        ReturnToMaze();
    }

    prevR = r;
    prevEsc = esc;

    switch (mode)
    {
    case Mode::TopDown:
        UpdateTopDown(deltaTime);
        break;
    case Mode::FirstPerson:
        UpdateFirstPerson(deltaTime);
        break;
    default:
        break;
    }
}

void NeonMazeGame::UpdateTopDown(double deltaTime)
{
    float dx = 0.0f;
    float dy = 0.0f;

    if (IsKeyDown('A') || IsKeyDown(VK_LEFT))
        dx -= 1.0f;
    if (IsKeyDown('D') || IsKeyDown(VK_RIGHT))
        dx += 1.0f;
    if (IsKeyDown('W') || IsKeyDown(VK_UP))
        dy -= 1.0f;
    if (IsKeyDown('S') || IsKeyDown(VK_DOWN))
        dy += 1.0f;

    float len = Length(dx, dy);
    if (len > 0.0f)
    {
        dx /= len;
        dy /= len;
    }

    const float speed = 3.2f;
    float nx = playerX + dx * speed * static_cast<float>(deltaTime);
    float ny = playerY + dy * speed * static_cast<float>(deltaTime);

    if (CanStand(nx, playerY))
        playerX = nx;
    if (CanStand(playerX, ny))
        playerY = ny;

    int cellX = static_cast<int>(std::floor(playerX));
    int cellY = static_cast<int>(std::floor(playerY));
    char cell = CellAt(cellX, cellY);

    if (portalCooldown <= 0.0 && cell == 'P')
    {
        int portalIndex = PortalIndexAt(cellX, cellY);
        if (portalIndex >= 0 && portalIndex < static_cast<int>(portalDone.size()) && !portalDone[portalIndex])
        {
            EnterFirstPerson(SectionKind::Portal, portalIndex);
            return;
        }
    }

    if (portalCooldown <= 0.0 && cell == 'T')
    {
        EnterFirstPerson(SectionKind::Trap, -1);
        return;
    }

    if (cell == 'E' && fragments >= 3)
    {
        mode = Mode::Win;
        transitionGlow = 1.0;
    }
}

void NeonMazeGame::UpdateFirstPerson(double deltaTime)
{
    if (IsKeyDown(VK_LEFT))
        fpYaw += static_cast<float>(deltaTime) * 2.4f;
    if (IsKeyDown(VK_RIGHT))
        fpYaw -= static_cast<float>(deltaTime) * 2.4f;

    float forwardX = std::cos(fpYaw);
    float forwardY = std::sin(fpYaw);
    float rightX = forwardY;
    float rightY = -forwardX;

    float dx = 0.0f;
    float dy = 0.0f;

    if (IsKeyDown('W') || IsKeyDown(VK_UP))
    {
        dx += forwardX;
        dy += forwardY;
    }
    if (IsKeyDown('S') || IsKeyDown(VK_DOWN))
    {
        dx -= forwardX;
        dy -= forwardY;
    }
    if (IsKeyDown('A'))
    {
        dx -= rightX;
        dy -= rightY;
    }
    if (IsKeyDown('D'))
    {
        dx += rightX;
        dy += rightY;
    }

    float len = Length(dx, dy);
    if (len > 0.0f)
    {
        dx /= len;
        dy /= len;
    }

    const float speed = 3.0f;
    float nx = fpX + dx * speed * static_cast<float>(deltaTime);
    float ny = fpY + dy * speed * static_cast<float>(deltaTime);

    if (CanStandInArena(nx, fpY))
        fpX = nx;
    if (CanStandInArena(fpX, ny))
        fpY = ny;

    float laserX = 4.7f + std::sin(static_cast<float>(totalTime) * 1.65f) * 1.55f;
    float laserY = 6.0f + std::cos(static_cast<float>(totalTime) * 1.25f) * 2.2f;
    bool hitVertical = std::abs(fpX - laserX) < 0.16f && fpY > 1.3f && fpY < 10.8f;
    bool hitHorizontal = std::abs(fpY - laserY) < 0.16f && fpX > 2.0f && fpX < 10.9f;

    if ((hitVertical || hitHorizontal) && damageCooldown <= 0.0)
    {
        --lives;
        transitionGlow = 1.0;
        damageCooldown = 1.0;
        fpX = 1.4f;
        fpY = 1.4f;
        fpYaw = 0.35f;
        fpPitch = 0.0f;

        if (lives <= 0)
        {
            mode = Mode::GameOver;
            SetMouseLock(false);
            return;
        }
    }

    if (Distance(fpX, fpY, kCrystalX, kCrystalY) < 0.72f)
    {
        sectionClear = true;
        if (sectionKind == SectionKind::Portal && activePortal >= 0 &&
            activePortal < static_cast<int>(portalDone.size()) && !portalDone[activePortal])
        {
            portalDone[activePortal] = true;
            fragments = std::min(3, fragments + 1);
        }
        ReturnToMaze();
    }
}

void NeonMazeGame::EnterFirstPerson(SectionKind kind, int portalIndex)
{
    mode = Mode::FirstPerson;
    sectionKind = kind;
    activePortal = portalIndex;
    fpX = 1.4f;
    fpY = 1.4f;
    fpYaw = 0.35f;
    fpPitch = 0.0f;
    damageCooldown = 0.6;
    transitionGlow = 1.0;
    sectionClear = false;
    lastMouseX = -1;
    lastMouseY = -1;
    SetMouseLock(true);
}

void NeonMazeGame::ReturnToMaze()
{
    mode = Mode::TopDown;
    portalCooldown = 0.9;
    transitionGlow = 1.0;
    sectionClear = false;
    lastMouseX = -1;
    lastMouseY = -1;
    SetMouseLock(false);
}

void NeonMazeGame::OnMouseMove(OpenGL* sender, MouseEventArg arg)
{
    if (mode != Mode::FirstPerson)
    {
        lastMouseX = -1;
        lastMouseY = -1;
        return;
    }

    UpdateMouseClip();

    int centerX = sender->getWidth() / 2;
    int centerY = sender->getHeight() / 2;
    int dx = arg.x - centerX;
    int dy = arg.y - centerY;

    if (std::abs(dx) > 1 || std::abs(dy) > 1)
    {
        fpYaw -= static_cast<float>(dx) * 0.00095f;
        fpPitch -= static_cast<float>(dy) * 0.00085f;
        fpPitch = std::clamp(fpPitch, -0.45f, 0.45f);
    }

    CenterMouse(sender);
}

void NeonMazeGame::SetMouseLock(bool locked)
{
    if (cursorLocked == locked)
    {
        if (locked)
            UpdateMouseClip();
        return;
    }

    cursorLocked = locked;

    if (cursorLocked)
    {
        SetCursorVisible(false);
        UpdateMouseClip();
        CenterMouse(nullptr);
    }
    else
    {
        ClipCursor(nullptr);
        SetCursorVisible(false);
    }
}

void NeonMazeGame::SetCursorVisible(bool visible)
{
    if (cursorVisible == visible)
        return;

    if (visible)
    {
        while (ShowCursor(TRUE) < 0)
        {
        }
    }
    else
    {
        while (ShowCursor(FALSE) >= 0)
        {
        }
    }
    cursorVisible = visible;
}

void NeonMazeGame::UpdateMouseClip()
{
    if (!cursorLocked)
        return;

    extern OpenGL gl;
    HWND hwnd = gl.getHWND();
    if (hwnd == nullptr)
        return;

    RECT clientRect;
    if (!GetClientRect(hwnd, &clientRect))
        return;

    POINT topLeft = {clientRect.left, clientRect.top};
    POINT bottomRight = {clientRect.right, clientRect.bottom};
    ClientToScreen(hwnd, &topLeft);
    ClientToScreen(hwnd, &bottomRight);

    RECT clipRect = {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    ClipCursor(&clipRect);
}

void NeonMazeGame::CenterMouse(OpenGL* sender)
{
    extern OpenGL gl;
    HWND hwnd = sender != nullptr ? sender->getHWND() : gl.getHWND();
    if (hwnd == nullptr)
        return;

    RECT clientRect;
    if (!GetClientRect(hwnd, &clientRect))
        return;

    POINT center = {(clientRect.right - clientRect.left) / 2, (clientRect.bottom - clientRect.top) / 2};
    ClientToScreen(hwnd, &center);
    SetCursorPos(center.x, center.y);
}

bool NeonMazeGame::IsKeyDown(int key) const
{
    return OpenGL::isKeyPressed(key);
}

bool NeonMazeGame::IsWall(int x, int y) const
{
    return CellAt(x, y) == '#';
}

bool NeonMazeGame::CanStand(float x, float y) const
{
    constexpr float radius = 0.18f;
    return !IsWall(static_cast<int>(std::floor(x - radius)), static_cast<int>(std::floor(y - radius))) &&
           !IsWall(static_cast<int>(std::floor(x + radius)), static_cast<int>(std::floor(y - radius))) &&
           !IsWall(static_cast<int>(std::floor(x - radius)), static_cast<int>(std::floor(y + radius))) &&
           !IsWall(static_cast<int>(std::floor(x + radius)), static_cast<int>(std::floor(y + radius)));
}

bool NeonMazeGame::CanStandInArena(float x, float y) const
{
    if (x < 0.6f || x > kArenaSize - 0.6f || y < 0.6f || y > kArenaSize - 0.6f)
        return false;

    for (const Rect& r : arenaWalls)
    {
        if (x > r.x0 - kPlayerRadius && x < r.x1 + kPlayerRadius && y > r.y0 - kPlayerRadius &&
            y < r.y1 + kPlayerRadius)
        {
            return false;
        }
    }

    return true;
}

char NeonMazeGame::CellAt(int x, int y) const
{
    if (y < 0 || y >= static_cast<int>(maze.size()))
        return '#';
    if (x < 0 || x >= static_cast<int>(maze[y].size()))
        return '#';
    return maze[y][x];
}

int NeonMazeGame::PortalIndexAt(int x, int y) const
{
    for (int i = 0; i < static_cast<int>(portals.size()); ++i)
    {
        if (portals[i].x == x && portals[i].y == y)
            return i;
    }
    return -1;
}

void NeonMazeGame::Draw(int width, int height)
{
    glClearColor(0.004f, 0.003f, 0.012f, 1.0f);

    if (mode == Mode::FirstPerson)
        DrawFirstPerson(width, height);
    else
        DrawTopDown(width, height);

    DrawScreenFade(width, height);
}

void NeonMazeGame::DrawTopDown(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    Shader::DontUseShaders();

    glDisable(GL_TEXTURE_2D);
    glColor3f(0.004f, 0.003f, 0.012f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(static_cast<float>(width), 0);
    glVertex2f(static_cast<float>(width), static_cast<float>(height));
    glVertex2f(0, static_cast<float>(height));
    glEnd();

    DrawStars2D(width, height);

    int mapW = static_cast<int>(maze[0].size());
    int mapH = static_cast<int>(maze.size());
    float cell = std::min((static_cast<float>(width) - 48.0f) / static_cast<float>(mapW),
                          (static_cast<float>(height) - 120.0f) / static_cast<float>(mapH));
    cell = std::max(18.0f, cell);
    float originX = (static_cast<float>(width) - cell * static_cast<float>(mapW)) * 0.5f;
    float originY = 32.0f;

    glEnable(GL_TEXTURE_2D);
    mazeTexture.Bind();
    glColor4f(1, 1, 1, 1);

    for (int y = 0; y < mapH; ++y)
    {
        for (int x = 0; x < mapW; ++x)
        {
            float sx = originX + static_cast<float>(x) * cell;
            float sy = originY + static_cast<float>(mapH - y - 1) * cell;
            DrawMazeCell(sx, sy, cell, maze[y][x]);
        }
    }

    glDisable(GL_TEXTURE_2D);
    glLineWidth(2.0f);
    glColor4f(0.0f, 0.9f, 1.0f, 0.9f);
    for (int y = 0; y < mapH; ++y)
    {
        for (int x = 0; x < mapW; ++x)
        {
            if (maze[y][x] != '#')
                continue;
            float sx = originX + static_cast<float>(x) * cell;
            float sy = originY + static_cast<float>(mapH - y - 1) * cell;
            glBegin(GL_LINE_LOOP);
            glVertex2f(sx + 1, sy + 1);
            glVertex2f(sx + cell - 1, sy + 1);
            glVertex2f(sx + cell - 1, sy + cell - 1);
            glVertex2f(sx + 1, sy + cell - 1);
            glEnd();
        }
    }

    for (int i = 0; i < static_cast<int>(portals.size()); ++i)
    {
        float sx = originX + (static_cast<float>(portals[i].x) + 0.5f) * cell;
        float sy = originY + (static_cast<float>(mapH - portals[i].y - 1) + 0.5f) * cell;
        DrawPortal2D(sx, sy, cell, i);
    }

    float px = originX + playerX * cell;
    float py = originY + (static_cast<float>(mapH) - playerY) * cell;
    DrawPlayer2D(px, py, cell);

    glEnable(GL_DEPTH_TEST);
}

void NeonMazeGame::DrawFirstPerson(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(68.0, static_cast<double>(width) / static_cast<double>(std::max(1, height)), 0.05, 90.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float lookCos = std::cos(fpPitch);
    gluLookAt(fpX, fpY, kEyeHeight, fpX + std::cos(fpYaw) * lookCos, fpY + std::sin(fpYaw) * lookCos,
              kEyeHeight + std::sin(fpPitch), 0, 0, 1);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    Shader::DontUseShaders();

    DrawStars3D();

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat lightPos[] = {fpX, fpY, 1.4f, 1.0f};
    GLfloat lightAmbient[] = {0.03f, 0.05f, 0.07f, 1.0f};
    GLfloat lightDiffuse[] = {0.25f, 0.85f, 1.0f, 1.0f};
    GLfloat lightSpecular[] = {0.95f, 0.95f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);

    DrawArenaGeometry();
    DrawCrystalOrExit();
    DrawLaserField();
}

void NeonMazeGame::DrawStars2D(int width, int height)
{
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (const Star& s : stars)
    {
        float pulse = 0.6f + 0.4f * std::sin(static_cast<float>(totalTime) * (1.5f + s.z) + s.x * 12.0f);
        float b = s.brightness * pulse;
        glColor3f(0.35f * b, 0.75f * b, b);
        glVertex2f(s.x * static_cast<float>(width), s.y * static_cast<float>(height));
    }
    glEnd();
}

void NeonMazeGame::DrawStars3D()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glPointSize(2.0f);

    glBegin(GL_POINTS);
    for (const Star& s : stars)
    {
        float b = s.brightness;
        glColor3f(0.25f * b, 0.65f * b, b);
        glVertex3f((s.x - 0.5f) * 80.0f, (s.y - 0.5f) * 80.0f, 5.0f + s.z * 28.0f);
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void NeonMazeGame::DrawMazeCell(float x, float y, float size, char cell)
{
    int tile = 0;
    if (cell == '#')
        tile = 1;
    else if (cell == 'T')
        tile = 2;
    else if (cell == 'E')
        tile = fragments >= 3 ? 3 : 0;

    float u0 = tile * 0.25f;
    float u1 = u0 + 0.25f;
    DrawTexturedQuad(x, y, x + size, y + size, u0, 0, u1, 1);

    if (cell == 'T')
    {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float a = 0.25f + 0.2f * std::sin(static_cast<float>(totalTime) * 6.0f);
        glColor4f(1.0f, 0.02f, 0.18f, a);
        glBegin(GL_QUADS);
        glVertex2f(x + 3, y + 3);
        glVertex2f(x + size - 3, y + 3);
        glVertex2f(x + size - 3, y + size - 3);
        glVertex2f(x + 3, y + size - 3);
        glEnd();
        glDisable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        mazeTexture.Bind();
        glColor4f(1, 1, 1, 1);
    }
}

void NeonMazeGame::DrawPortal2D(float cx, float cy, float size, int portalIndex)
{
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    portalTexture.Bind();

    float alpha = portalDone[portalIndex] ? 0.22f : 0.85f;
    glColor4f(0.1f, 0.9f, 1.0f, alpha);
    glPushMatrix();
    glTranslatef(cx, cy, 0);
    glRotatef(static_cast<float>(totalTime) * 75.0f + portalIndex * 35.0f, 0, 0, 1);
    float r = size * 0.58f;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(-r, -r);
    glTexCoord2f(1, 0);
    glVertex2f(r, -r);
    glTexCoord2f(1, 1);
    glVertex2f(r, r);
    glTexCoord2f(0, 1);
    glVertex2f(-r, r);
    glEnd();
    glPopMatrix();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

void NeonMazeGame::DrawPlayer2D(float cx, float cy, float size)
{
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    float s = size * 0.34f;
    float pulse = 0.75f + 0.25f * std::sin(static_cast<float>(totalTime) * 7.0f);
    glColor4f(0.05f, 1.0f, 0.95f, pulse);
    glBegin(GL_TRIANGLES);
    glVertex2f(cx, cy + s);
    glVertex2f(cx - s * 0.85f, cy - s);
    glVertex2f(cx + s * 0.85f, cy - s);
    glEnd();

    glLineWidth(2.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx, cy + s);
    glVertex2f(cx - s * 0.85f, cy - s);
    glVertex2f(cx + s * 0.85f, cy - s);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

void NeonMazeGame::DrawArenaGeometry()
{
    Shader::DontUseShaders();
    glEnable(GL_TEXTURE_2D);
    mazeTexture.Bind();

    SetMaterial(0.04f, 0.11f, 0.15f, 64.0f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    for (int y = 0; y < static_cast<int>(kArenaSize); ++y)
    {
        for (int x = 0; x < static_cast<int>(kArenaSize); ++x)
        {
            glTexCoord2f(0.00f, 0.0f);
            glVertex3f(static_cast<float>(x), static_cast<float>(y), 0.0f);
            glTexCoord2f(0.25f, 0.0f);
            glVertex3f(static_cast<float>(x + 1), static_cast<float>(y), 0.0f);
            glTexCoord2f(0.25f, 1.0f);
            glVertex3f(static_cast<float>(x + 1), static_cast<float>(y + 1), 0.0f);
            glTexCoord2f(0.00f, 1.0f);
            glVertex3f(static_cast<float>(x), static_cast<float>(y + 1), 0.0f);
        }
    }
    glEnd();

    SetMaterial(0.02f, 0.22f, 0.30f, 96.0f);
    DrawBox(-0.25f, -0.25f, 0, 0, kArenaSize + 0.25f, 1.8f);
    DrawBox(kArenaSize, -0.25f, 0, kArenaSize + 0.25f, kArenaSize + 0.25f, 1.8f);
    DrawBox(0, -0.25f, 0, kArenaSize, 0, 1.8f);
    DrawBox(0, kArenaSize, 0, kArenaSize, kArenaSize + 0.25f, 1.8f);

    for (const Rect& r : arenaWalls)
        DrawBox(r.x0, r.y0, 0, r.x1, r.y1, 1.55f);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    UseNeonShader(0.0f, 0.95f, 1.0f, 1.4f);
    glLineWidth(2.5f);
    glNormal3f(0, 0, 1);
    glBegin(GL_LINES);
    for (int i = 0; i <= 12; ++i)
    {
        glVertex3f(static_cast<float>(i), 0, 0.025f);
        glVertex3f(static_cast<float>(i), kArenaSize, 0.025f);
        glVertex3f(0, static_cast<float>(i), 0.025f);
        glVertex3f(kArenaSize, static_cast<float>(i), 0.025f);
    }
    glEnd();
    Shader::DontUseShaders();
    glEnable(GL_LIGHTING);
}

void NeonMazeGame::DrawLaserField()
{
    Shader::DontUseShaders();
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    float laserX = 4.7f + std::sin(static_cast<float>(totalTime) * 1.65f) * 1.55f;
    float laserY = 6.0f + std::cos(static_cast<float>(totalTime) * 1.25f) * 2.2f;
    float glow = 0.55f + 0.35f * std::sin(static_cast<float>(totalTime) * 9.0f);

    glColor4f(1.0f, 0.05f, 0.2f, 0.50f + glow * 0.2f);
    glBegin(GL_QUADS);
    glVertex3f(laserX - 0.04f, 1.3f, 0.12f);
    glVertex3f(laserX + 0.04f, 1.3f, 0.12f);
    glVertex3f(laserX + 0.04f, 10.8f, 1.35f);
    glVertex3f(laserX - 0.04f, 10.8f, 1.35f);

    glVertex3f(2.0f, laserY - 0.04f, 0.20f);
    glVertex3f(10.9f, laserY - 0.04f, 1.20f);
    glVertex3f(10.9f, laserY + 0.04f, 1.20f);
    glVertex3f(2.0f, laserY + 0.04f, 0.20f);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void NeonMazeGame::DrawCrystalOrExit()
{
    if (sectionKind == SectionKind::Portal)
    {
        glEnable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        crystalTexture.Bind();
        UseNeonTextureShader(0.15f, 1.0f, 0.95f, 1.0f);
        SetMaterial(0.1f, 0.9f, 0.85f, 128.0f);

        glPushMatrix();
        glTranslatef(kCrystalX, kCrystalY, 0.9f);
        glRotatef(static_cast<float>(totalTime) * 80.0f, 0, 0, 1);
        glRotatef(18.0f, 1, 0, 0);
        glScalef(0.65f, 0.65f, 0.65f);
        crystalModel.Draw();
        glPopMatrix();
        Shader::DontUseShaders();
    }
    else
    {
        glDisable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        portalTexture.Bind();
        glColor4f(0.15f, 1.0f, 0.55f, 0.85f);
        glPushMatrix();
        glTranslatef(kCrystalX, kCrystalY, 0.05f);
        glRotatef(static_cast<float>(totalTime) * 110.0f, 0, 0, 1);
        float r = 0.8f;
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex3f(-r, -r, 0);
        glTexCoord2f(1, 0);
        glVertex3f(r, -r, 0);
        glTexCoord2f(1, 1);
        glVertex3f(r, r, 0);
        glTexCoord2f(0, 1);
        glVertex3f(-r, r, 0);
        glEnd();
        glPopMatrix();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }
}

void NeonMazeGame::DrawBox(float x0, float y0, float z0, float x1, float y1, float z1)
{
    constexpr float u0 = 0.25f;
    constexpr float u1 = 0.50f;

    glBegin(GL_QUADS);

    glNormal3f(0, -1, 0);
    glTexCoord2f(u0, 0);
    glVertex3f(x0, y0, z0);
    glTexCoord2f(u1, 0);
    glVertex3f(x1, y0, z0);
    glTexCoord2f(u1, 1);
    glVertex3f(x1, y0, z1);
    glTexCoord2f(u0, 1);
    glVertex3f(x0, y0, z1);

    glNormal3f(1, 0, 0);
    glTexCoord2f(u0, 0);
    glVertex3f(x1, y0, z0);
    glTexCoord2f(u1, 0);
    glVertex3f(x1, y1, z0);
    glTexCoord2f(u1, 1);
    glVertex3f(x1, y1, z1);
    glTexCoord2f(u0, 1);
    glVertex3f(x1, y0, z1);

    glNormal3f(0, 1, 0);
    glTexCoord2f(u0, 0);
    glVertex3f(x1, y1, z0);
    glTexCoord2f(u1, 0);
    glVertex3f(x0, y1, z0);
    glTexCoord2f(u1, 1);
    glVertex3f(x0, y1, z1);
    glTexCoord2f(u0, 1);
    glVertex3f(x1, y1, z1);

    glNormal3f(-1, 0, 0);
    glTexCoord2f(u0, 0);
    glVertex3f(x0, y1, z0);
    glTexCoord2f(u1, 0);
    glVertex3f(x0, y0, z0);
    glTexCoord2f(u1, 1);
    glVertex3f(x0, y0, z1);
    glTexCoord2f(u0, 1);
    glVertex3f(x0, y1, z1);

    glNormal3f(0, 0, 1);
    glTexCoord2f(u0, 0);
    glVertex3f(x0, y0, z1);
    glTexCoord2f(u1, 0);
    glVertex3f(x1, y0, z1);
    glTexCoord2f(u1, 1);
    glVertex3f(x1, y1, z1);
    glTexCoord2f(u0, 1);
    glVertex3f(x0, y1, z1);

    glEnd();
}

void NeonMazeGame::DrawTexturedQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1)
{
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0);
    glVertex2f(x0, y0);
    glTexCoord2f(u1, v0);
    glVertex2f(x1, y0);
    glTexCoord2f(u1, v1);
    glVertex2f(x1, y1);
    glTexCoord2f(u0, v1);
    glVertex2f(x0, y1);
    glEnd();
}

void NeonMazeGame::DrawScreenFade(int width, int height)
{
    if (transitionGlow <= 0.0)
        return;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    Shader::DontUseShaders();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    float a = static_cast<float>(transitionGlow) * 0.18f;
    glColor4f(0.0f, 0.8f, 1.0f, a);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(static_cast<float>(width), 0);
    glVertex2f(static_cast<float>(width), static_cast<float>(height));
    glVertex2f(0, static_cast<float>(height));
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void NeonMazeGame::DrawHud(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width - 1, 0, height - 1, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    Shader::DontUseShaders();
    glDisable(GL_DEPTH_TEST);

    std::wstringstream hud;
    hud << L"NEON MAZE 2D/3D\n";
    hud << L"Кристаллы: " << fragments << L"/3    Жизни: " << lives << L"\n";
    if (mode == Mode::TopDown)
    {
        hud << L"ЦЕЛЬ: зайди в 3 портала, забери кристаллы, потом иди к выходу.\n";
        hud << L"Красный квадрат - ловушка: перенесет в 3D-испытание. Красные лазеры отнимают жизнь.\n";
        hud << L"2D: WASD/стрелки - движение, R - рестарт.";
    }
    else if (mode == Mode::FirstPerson)
    {
        if (sectionKind == SectionKind::Portal)
            hud << L"3D-ЦЕЛЬ: доберись до вращающегося кристалла в конце арены.\n";
        else
            hud << L"3D-ЦЕЛЬ: доберись до зеленого кольца выхода.\n";
        hud << L"Управление: WASD - ходьба, мышь - взгляд. Не касайся красных лазеров.\n";
        hud << L"После сбора/выхода тебя вернет в лабиринт. R - рестарт.";
    }
    else
    {
        hud << L"R - начать заново.";
    }

    hudText.setPosition(10, height - 220);
    hudText.setText(hud.str().c_str(), 0, 115, 115);
    hudText.Draw();

    if (mode == Mode::Win || mode == Mode::GameOver)
    {
        std::wstring text = mode == Mode::Win ? L"ПОБЕДА\n3 кристалла собраны, выход открыт."
                                              : L"GAME OVER\nЛазерная сетка забрала все жизни.";
        centerText.setPosition((width - 700) / 2, (height - 130) / 2);
        centerText.setText(text.c_str(), mode == Mode::Win ? 0 : 120, mode == Mode::Win ? 120 : 20,
                           mode == Mode::Win ? 90 : 40);
        centerText.Draw();
    }

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void NeonMazeGame::UseNeonShader(float r, float g, float b, float strength)
{
    neonShader.UseShader();
    int location = glGetUniformLocationARB(neonShader.program, "time");
    if (location >= 0)
        glUniform1fARB(location, static_cast<float>(totalTime));
    location = glGetUniformLocationARB(neonShader.program, "baseColor");
    if (location >= 0)
        glUniform3fARB(location, r, g, b);
    location = glGetUniformLocationARB(neonShader.program, "glowStrength");
    if (location >= 0)
        glUniform1fARB(location, strength);
}

void NeonMazeGame::UseNeonTextureShader(float r, float g, float b, float alpha)
{
    neonTextureShader.UseShader();
    int location = glGetUniformLocationARB(neonTextureShader.program, "tex");
    if (location >= 0)
        glUniform1iARB(location, 0);
    location = glGetUniformLocationARB(neonTextureShader.program, "time");
    if (location >= 0)
        glUniform1fARB(location, static_cast<float>(totalTime));
    location = glGetUniformLocationARB(neonTextureShader.program, "tint");
    if (location >= 0)
        glUniform3fARB(location, r, g, b);
    location = glGetUniformLocationARB(neonTextureShader.program, "alpha");
    if (location >= 0)
        glUniform1fARB(location, alpha);
}

void NeonMazeGame::SetMaterial(float r, float g, float b, float shininess)
{
    GLfloat ambient[] = {r * 0.25f, g * 0.25f, b * 0.25f, 1.0f};
    GLfloat diffuse[] = {r, g, b, 1.0f};
    GLfloat specular[] = {0.8f, 0.95f, 1.0f, 1.0f};
    glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT, GL_SHININESS, shininess);
}
