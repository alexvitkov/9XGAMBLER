#include "raylib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>


#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define VIEWPORT_WIDTH 1024
#define VIEWPORT_HEIGHT 768

#define MACHINE_WIDTH 185
#define MACHINE_HEIGHT 232
#define MACHINE_GAP_X 18
#define MACHINE_GAP_Y 18
#define TOP_PADDING 18
#define RIGHT_PADDING 18
#define BUTTON_SIZE 48

struct Machine {
    int x = 0;
    int y = 0;

    virtual void update() = 0;
};

struct M3x1 : Machine {
    virtual void update() {
        DrawRectangle(x, y, MACHINE_WIDTH, MACHINE_HEIGHT, RED);

        DrawRectangle(x + (MACHINE_WIDTH - BUTTON_SIZE) / 2, y + (MACHINE_HEIGHT - BUTTON_SIZE) - 8, BUTTON_SIZE, BUTTON_SIZE, BLUE);
    };
};


const char* game_title = "9X GAMBLER";
int screen_width = 1024;
int screen_height = 768;
float screen_scale;

Texture tex_background;

// --- Gameplay State -----------------------------------------
Machine* machines[9] = {};

int main() {

    SetConfigFlags(FLAG_VSYNC_HINT  | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_MAXIMIZED);
    InitWindow(screen_width, screen_height, game_title);
    // SetTargetFPS(60);

    // --- Load Assets --------------------------------------------
    tex_background = LoadTexture("assets/background.png");

    M3x1* m3x1 = new M3x1();
    machines[0] = m3x1;

    while (!WindowShouldClose()) {

        Camera2D camera = {
        };

        // --- Update viewport ----------------------------------------
        {
            screen_width = GetScreenWidth();
            screen_height = GetScreenHeight();

            float scale_x = float(screen_height) / float(VIEWPORT_HEIGHT);
            float scale_y = float(screen_width) / float(VIEWPORT_WIDTH);

            if (scale_x < scale_y) {
                screen_scale = scale_x;
                float free_space = screen_width - (VIEWPORT_WIDTH * screen_scale);
                camera.offset.x = free_space / 2;
            } else {
                screen_scale = scale_y;
                float free_space = screen_height - (VIEWPORT_HEIGHT * screen_scale);
                camera.offset.y = free_space / 2;
            }

            camera.zoom = screen_scale;

        }



        BeginDrawing();

        BeginMode2D(camera);
        ClearBackground(BLACK);

        // Render Game
        {
            DrawTexture(tex_background, 0, 0, WHITE);

            for (int i = 0; i < ARRAY_SIZE(machines); i++) {
                Machine* machine = machines[i];

                int x = TOP_PADDING + (i % 3) * (MACHINE_WIDTH + MACHINE_GAP_X);
                int y = RIGHT_PADDING + (i / 3) * (MACHINE_HEIGHT + MACHINE_GAP_Y);

                DrawRectangle(x, y, MACHINE_WIDTH, MACHINE_HEIGHT, Color{32,32,32,255});

                if (machine) {
                    machine->x = x;
                    machine->y = x;
                    machine->update();
                }
            }
        }


        // DrawText("Hello, Gambler!", 0, 0, 24, WHITE);
        EndMode2D();


        if (0) { // FPS Counter
            char buf[64];
            snprintf(buf, 64, "FPS: %d", GetFPS());
            DrawText(buf, 8, 8, 20, WHITE);
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
