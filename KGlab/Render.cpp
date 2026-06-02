#include "Render.h"

#include "Game.h"
#include "MyOGL.h"

#include <GL/gl.h>

#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

extern OpenGL gl;

NeonMazeGame game;

void initRender()
{
    glClearColor(0.004f, 0.003f, 0.012f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    game.Init();
    gl.MouseMovieEvent.reaction(&game, &NeonMazeGame::OnMouseMove);
}

void Render(double deltaTime)
{
    int width = std::max(1, gl.getWidth());
    int height = std::max(1, gl.getHeight());

    game.Update(deltaTime);
    game.Draw(width, height);
    game.DrawHud(width, height);
}
