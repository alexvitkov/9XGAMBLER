#include "raylib.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <string>
#include <format>

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
#define TILE_COUNT 6
#define MAX_SLOT_REELS 10
#define MAX_SLOT_ROWS  5

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef i64 Money;

const char* game_title = "9X GAMBLER";
int screen_width = 1024;
int screen_height = 768;
float screen_scale;
Vector2 mouse;
double game_time = 0;
double dt = 0;

struct Machine {
    Vector2 pos;

    int shake_x = 0;
    int shake_y = 0;
    double shake_time = 0;

    virtual void update() = 0;

    void shake();
};

template <typename T>
struct Weights {
    std::vector<T> array;

    void add(T entry, int weight) {
        for (int i = 0; i < weight; i++)
            array.push_back(entry);
    }

    int generate() {
        return array[GetRandomValue(0, array.size() - 1)];
    }
};

struct SlotBuffer {
    int reels = 0;
    int rows = 0;
    int buffer[MAX_SLOT_REELS][MAX_SLOT_ROWS] = {};

    static SlotBuffer generate(int reels, int rows, Weights<int>& weights) {
        SlotBuffer buffer = { reels, rows };
        buffer.reels = reels;
        buffer.rows = rows;

        for (i32 reel = 0; reel < reels; reel++)
            for (i32 row = 0; row < reels; row++)
                buffer.buffer[reel][row] = weights.generate();

        return buffer;
    }
};

struct Slot {
    Rectangle     rect     = {};
    bool          spinning = false;
    Money         stake    = 1;
    SlotBuffer    buffer   = {};
    int           reels    = {};
    int           rows     = {};
    Weights<int>  weights  = {};

    void spin(Vector2 pos);
    void update();
};

struct M3X1 : Machine {
    Slot slot = {};

    M3X1();
    virtual void update();
};

struct TextOnScreen {
    std::string text     = 0;
    Vector2     pos      = {};
    float       t        = 0;
    float       duration = 4;
    Color       color    = WHITE;
    float       size     = 20;
    Vector2     velocity = {0, -300};
    float       gravity  = 2000;
};

Texture tex_background;
Texture tex_m3x1;
Texture tex_tiles[TILE_COUNT];

// --- Renderer State -----------------------------------------

std::vector<TextOnScreen> texts;

// --- Gameplay State -----------------------------------------

Machine* machines[9] = {};
Money money = 100;
double display_money = 0;

void gain_money(Money money, Vector2 pos);
float Lerp(float a, float b, float t);
float Remap(float val, float old_min, float old_max, float new_min, float new_max);

// --- Gameplay Methods ---------------------------------------

void Machine::shake() {
    if (game_time - shake_time > 0.02) {
        shake_time = game_time;
        shake_x = GetRandomValue(-1, 1);
        shake_y = GetRandomValue(-2, 2);
    }
    pos.x += shake_x;
    pos.y += shake_y;
}

void Slot::spin(Vector2 pos) {
    if (!spinning) {
        spinning = true;
        gain_money(-stake, pos);
    }
}

void Slot::update() {
    DrawRectangleRec(rect, BLACK);

    float avail_space_x = rect.width - reels * 40;
    float avail_space_y = rect.height - rows * 40;
    float gap_x = avail_space_x / (reels + 1);
    float gap_y = avail_space_y / (rows + 1);

    for (i32 reel = 0; reel < reels; reel++) {
        for (i32 row = 0; row < rows; row++) {
            Vector2 pos = {
                .x = this->rect.x + gap_x * (reel + 1) + reel * 40,
                .y = this->rect.y + gap_y * (row + 1) + row * 40,
            };

            DrawTexture(tex_tiles[buffer.buffer[reel][row]], pos.x, pos.y, WHITE);
            
            printf("%d\n", reels);
        }
    }
}

M3X1::M3X1() {
    slot.stake = 10;
    slot.reels = 3;
    slot.rows = 1;
    slot.weights.add(1, 5);
    slot.weights.add(2, 4);
    slot.weights.add(3, 3);
    slot.buffer = SlotBuffer::generate(3, 1, slot.weights);
}

void M3X1::update() {

    if (slot.spinning) shake();

    Rectangle button = {
        .x = pos.x + float(MACHINE_WIDTH - BUTTON_WIDTH) / 2,
        .y = pos.y + float(MACHINE_HEIGHT - BUTTON_HEIGHT) - 8,
        .width = float(BUTTON_WIDTH),
        .height = float(BUTTON_HEIGHT),
    };

    Color color = { 0, 0, 255, 255 };
    slot.rect = { pos.x + 10, pos.y + 60, 164, 86 };

    if (slot.spinning) {
        button.y += 12;
        button.height -= 12;
        color = { 0, 0, 160, 180 };
    }
    else {
        if (CheckCollisionPointRec(mouse, button)) {
            color = Color { 32, 80, 255, 255 };

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                slot.spin({button.x, button.y});
            }
        }
    }

    DrawTexture(tex_m3x1, pos.x, pos.y - 9, WHITE);

    DrawRectangleRec(button, color);
    DrawText("SPIN", button.x + 6, button.y + 10, 20, WHITE);
    slot.update();
}

void gain_money(Money amount, Vector2 pos) {
    pos.y -= 10;
    money += amount;

    bool neg = amount < 0;
    if (neg) amount = -amount;

    TextOnScreen text {
        .text     = std::format("{}${}", neg ? "-" : "+", amount),
        .pos      = pos,
        .color    = neg ? RED : GREEN,
    };
    text.velocity.x = GetRandomValue(-100, 100);
    texts.push_back(text);
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float Remap(float val, float old_min, float old_max, float new_min, float new_max) {
    float t = (val - old_min) / (old_max-old_min);
    return lerp(new_min, new_max, t);
}

int main() {

    SetConfigFlags(/*FLAG_VSYNC_HINT  | */FLAG_WINDOW_RESIZABLE);
    InitWindow(screen_width, screen_height, game_title);
    //SetTargetFPS(60);

    // --- Load Assets --------------------------------------------
    tex_background = LoadTexture("assets/background.png");
    tex_m3x1 = LoadTexture("assets/m3x1.png");

    tex_tiles[0] = LoadTexture("assets/tile0.png");
    tex_tiles[1] = LoadTexture("assets/tile1.png");
    tex_tiles[2] = LoadTexture("assets/tile2.png");
    tex_tiles[3] = LoadTexture("assets/tile3.png");
    tex_tiles[4] = LoadTexture("assets/tile4.png");
    tex_tiles[5] = LoadTexture("assets/tile5.png");

    // --- Init gameplay ------------------------------------------
    M3X1* m3x1 = new M3X1();
    M3X1* m3x1_2 = new M3X1();
    machines[0] = m3x1;
    machines[1] = m3x1_2;
    display_money = money;


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
        game_time = GetTime();
        dt = GetFrameTime();

        // Render Game
        {
            DrawTexture(tex_background, 0, 0, WHITE);

            for (int i = 0; i < ARRAY_SIZE(machines); i++) {
                Machine* machine = machines[i];

                int x = TOP_PADDING + (i % 3) * (MACHINE_WIDTH + MACHINE_GAP_X);
                int y = RIGHT_PADDING + (i / 3) * (MACHINE_HEIGHT + MACHINE_GAP_Y);


                if (machine) {
                    machine->pos.x = x;
                    machine->pos.y = y;
                    machine->update();
                }
                else {
                    DrawRectangle(x, y, MACHINE_WIDTH, MACHINE_HEIGHT, Color{0, 0, 0, 90});
                }
            }

            char buf[64];
            display_money = Lerp(display_money, double(money), 10 * dt);
            snprintf(buf, 64, "%ld", i64(roundf(display_money)));
            DrawText(buf, 714, 154, 40, WHITE);
        }

        for (i32 i = 0; i < texts.size(); i++) {
            TextOnScreen& text = texts[i];
            float t = pow(text.t / text.duration, 2);

            text.pos.x += text.velocity.x * dt;
            text.pos.y += text.velocity.y * dt;
            text.velocity.y += text.gravity * dt;


            Color color = text.color;
            if (t > 0.8)
                color.a = Remap(t, 0.8, 1, 255, 0);

            DrawText(text.text.c_str(), text.pos.x, text.pos.y, text.size, {0,0,0,color.a});
            DrawText(text.text.c_str(), text.pos.x + 1, text.pos.y, text.size, color);

            text.t += GetFrameTime();

            if (text.t > text.duration) {
                texts[i] = texts.back();
                texts.pop_back();
                i--;
            }
        }

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

