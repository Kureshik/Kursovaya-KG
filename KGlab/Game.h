#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "GUItextRectangle.h"
#include "MyOGL.h"
#include "MyShaders.h"
#include "ObjLoader.h"
#include "Texture.h"

#include <array>
#include <string>
#include <vector>

class NeonMazeGame
{
  public:
    enum class Mode
    {
        TopDown,
        FirstPerson,
        Win,
        GameOver
    };

    void Init();
    void Update(double deltaTime);
    void Draw(int width, int height);
    void DrawHud(int width, int height);
    void OnMouseMove(OpenGL* sender, MouseEventArg arg);

  private:
    struct Point
    {
        int x = 0;
        int y = 0;
    };

    struct Star
    {
        float x = 0;
        float y = 0;
        float z = 0;
        float brightness = 1;
    };

    struct Rect
    {
        float x0 = 0;
        float y0 = 0;
        float x1 = 0;
        float y1 = 0;
    };

    enum class SectionKind
    {
        Portal,
        Trap
    };

    Mode mode = Mode::TopDown;
    SectionKind sectionKind = SectionKind::Portal;

    std::vector<std::string> maze;
    std::vector<std::string> arenaLevel;
    std::vector<Point> portals;
    std::vector<Star> stars;
    std::vector<Rect> arenaWalls;
    std::array<bool, 3> portalDone = {false, false, false};

    Texture mazeTexture;
    Texture crystalTexture;
    Texture portalTexture;
    ObjModel crystalModel;
    Shader neonShader;
    Shader neonTextureShader;
    GuiTextRectangle hudText;
    GuiTextRectangle centerText;

    double totalTime = 0;
    double portalCooldown = 0;
    double damageCooldown = 0;
    double transitionGlow = 0;

    float playerX = 1.5f;
    float playerY = 1.5f;
    float spawnX = 1.5f;
    float spawnY = 1.5f;
    float fpX = 1.4f;
    float fpY = 1.4f;
    float fpYaw = 0.0f;
    float fpPitch = 0.0f;

    int lives = 3;
    int fragments = 0;
    int activePortal = -1;
    int lastMouseX = -1;
    int lastMouseY = -1;

    bool cursorVisible = true;
    bool cursorLocked = false;
    bool prevR = false;
    bool prevEsc = false;
    bool sectionClear = false;

    void ResetGame();
    void EnterFirstPerson(SectionKind kind, int portalIndex);
    void ReturnToMaze();
    void UpdateTopDown(double deltaTime);
    void UpdateFirstPerson(double deltaTime);
    void SetMouseLock(bool locked);
    void SetCursorVisible(bool visible);
    void UpdateMouseClip();
    void CenterMouse(OpenGL* sender);

    bool IsKeyDown(int key) const;
    bool IsWall(int x, int y) const;
    bool CanStand(float x, float y) const;
    bool CanStandInArena(float x, float y) const;
    char CellAt(int x, int y) const;
    int PortalIndexAt(int x, int y) const;

    void DrawTopDown(int width, int height);
    void DrawFirstPerson(int width, int height);
    void DrawStars2D(int width, int height);
    void DrawStars3D();
    void DrawMazeCell(float x, float y, float size, char cell);
    void DrawPortal2D(float cx, float cy, float size, int portalIndex);
    void DrawPlayer2D(float cx, float cy, float size);
    void DrawArenaGeometry();
    void DrawLaserField();
    void DrawCrystalOrExit();
    void DrawBox(float x0, float y0, float z0, float x1, float y1, float z1);
    void DrawTexturedQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1);
    void DrawScreenFade(int width, int height);

    void UseNeonShader(float r, float g, float b, float strength);
    void UseNeonTextureShader(float r, float g, float b, float alpha);
    void SetMaterial(float r, float g, float b, float shininess);
};
