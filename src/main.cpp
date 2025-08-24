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
#define BUTTON_WIDTH 60
#define BUTTON_HEIGHT 48

struct Machine {
    int x = 0;
    int y = 0;

    virtual void update() = 0;
};

const char* game_title = "9X GAMBLER";
int screen_width = 1024;
int screen_height = 768;
float screen_scale;
Vector2 mouse;

Texture tex_background;
Texture tex_m3x1;

// --- Gameplay State -----------------------------------------
Machine* machines[9] = {};
int64_t money = 100;

struct Slot {
    bool spinning = false;

    void spin() {
        spinning = true;
    }

    void update() {
    }
};

struct M3x1 : Machine {
    Slot slot = {};

    virtual void update() {
        slot.update();

        DrawRectangle(x, y, MACHINE_WIDTH, MACHINE_HEIGHT, RED);

        Rectangle button = {
            .x = x + float(MACHINE_WIDTH - BUTTON_WIDTH) / 2,
            .y = y + float(MACHINE_HEIGHT - BUTTON_HEIGHT) - 8,
            .width = float(BUTTON_WIDTH),
            .height = float(BUTTON_HEIGHT),
        };

        Color color = { 0, 0, 255, 255 };

        if (slot.spinning) {
            button.y += 12;
            button.height -= 12;
            color = { 0, 0, 160, 180 };
        }
        else {
            if (CheckCollisionPointRec(mouse, button)) {
                color = Color { 32, 80, 255, 255 };

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    slot.spin();
                }
            }
        }

        DrawTexture(tex_m3x1, x, y - 9, WHITE);

        DrawRectangleRec(button, color);
        DrawText("SPIN", button.x + 6, button.y + 10, 20, WHITE);
    };
};



int main() {

    SetConfigFlags(FLAG_VSYNC_HINT  | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_MAXIMIZED);
    InitWindow(screen_width, screen_height, game_title);
    // SetTargetFPS(60);

    // --- Load Assets --------------------------------------------
    tex_background = LoadTexture("assets/background.png");
    tex_m3x1 = LoadTexture("assets/m3x1.png");

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

        mouse = GetScreenToWorld2D(GetMousePosition(), camera);

        // Render Game
        {
            DrawTexture(tex_background, 0, 0, WHITE);

            for (int i = 0; i < ARRAY_SIZE(machines); i++) {
                Machine* machine = machines[i];

                int x = TOP_PADDING + (i % 3) * (MACHINE_WIDTH + MACHINE_GAP_X);
                int y = RIGHT_PADDING + (i / 3) * (MACHINE_HEIGHT + MACHINE_GAP_Y);

                DrawRectangle(x, y, MACHINE_WIDTH, MACHINE_HEIGHT, Color{0, 0, 0, 90});

                if (machine) {
                    machine->x = x;
                    machine->y = x;
                    machine->update();
                }
            }

            char buf[64];
            snprintf(buf, 64, "%ld", money);
            DrawText(buf, 714, 154, 40, WHITE);
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
