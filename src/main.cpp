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
Camera2D camera;
Vector2 mouse;
double game_time = 0;
double dt = 0;

struct Machine {
    Vector2 pos;
    double ev = 0;
    double win_percent = 0;

    int shake_x = 0;
    int shake_y = 0;
    double shake_time = 0;

    virtual void update() = 0;
    virtual void calculate_ev() = 0;

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

    int& at(int reel, int row) {
        return buffer[reel][row];
    }

    void advance(int reel, int new_tile) {
        for (i32 row = rows - 1; row >= 1; row--)
            buffer[reel][row] = buffer[reel][row-1];
         buffer[reel][0] = new_tile;
    }
};

struct Slot {
    Machine*          machine          = nullptr;
    Rectangle         rect             = {};
    bool              spinning         = false;
    float             spin_time        = 0;
    float             reel_offset_time = 0.3;
    int               spin_distance    = 10;
    Money             stake            = 1;
    SlotBuffer        buffer           = {};
    int               reels            = {};
    int               rows             = {};
    Weights<int>      weights          = {};
    float             speed            = 300;
    float             row_height       = 40;
    int               current_spin_distance = 0;

    Money (*win_algo)(Slot* slot) = 0;
    void (*stop_callback)(Slot* slot, int reel) = 0;

    float offsets[MAX_SLOT_REELS]       = {};
    i32   upper_buffer[MAX_SLOT_REELS]  = {};
    i32   spin_iter[MAX_SLOT_REELS]     = {};

    Rectangle get_reel_rect(int reel);
    void spin(Vector2 pos);
    void update();
};

struct M3X1 : Machine {
    Slot slot = {};
    bool anticipation = false;
    double payouts[TILE_COUNT] = { 0, 7, 20, 30, 40, 50 };

    M3X1();
    virtual void update();
    virtual void calculate_ev();
};

struct TextOnScreen {
    std::string text     = 0;
    Vector2     pos      = {};
    float       t        = 0;
    float       duration = 4;
    Color       color    = WHITE;
    float       size     = 40;
    Vector2     velocity = {0, -100};
    float       gravity  = 1000;
};

Texture tex_background;
Texture tex_m3x1;
Texture tex_tiles[TILE_COUNT];

// --- Renderer State -----------------------------------------

std::vector<TextOnScreen> texts;

// --- Gameplay State -----------------------------------------

Machine* machines[9] = {};
Money money = 500;
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
        for (i32 reel = 0; reel < reels; reel++) {
            upper_buffer[reel] = weights.generate();
            spin_iter[reel] = 0;
        }

        current_spin_distance = spin_distance;
        spinning = true;
        spin_time = 0;
        gain_money(-stake, pos);
    }
}

Rectangle Slot::get_reel_rect(int reel) {
    float avail_space_x = rect.width - reels * 40;
    float gap_x = avail_space_x / (reels + 1);

    return Rectangle {
        .x = this->rect.x + gap_x * (reel + 1) + reel * 40,
        .y = this->rect.y,
        .width = 40,
        .height = this->rect.height,
    };
}

void Slot::update() {
    if (spinning) {
        spin_time += dt;

        bool done = true;
        for (i32 reel = 0; reel < reels; reel++) {
            if (spin_time < reel_offset_time * reel) continue;
            if (spin_iter[reel] >= current_spin_distance)
                continue;

            done = false;
            offsets[reel] += speed * dt;

            while (offsets[reel] > row_height) {
                buffer.advance(reel, upper_buffer[reel]);
                upper_buffer[reel] = weights.generate();
                offsets[reel] -= row_height;

                spin_iter[reel]++;
                if (spin_iter[reel] == current_spin_distance && this->stop_callback) {
                    spin_iter[reel] = 99999999; // set to high value in case current_spin_distance changes
                    this->stop_callback(this, reel); 
                }
            }
        }
        if (done) {
            spinning = false;
            gain_money(win_algo(this), { rect.x, rect.y });
        }
    }

    float avail_space_x = rect.width - reels * 40;
    float avail_space_y = rect.height - rows * 40;
    float gap_x = avail_space_x / (reels + 1);
    float gap_y = avail_space_y / (rows + 1);

    row_height = 40 + gap_y;

    Vector2 scissor_pos = GetWorldToScreen2D({rect.x, rect.y}, camera);
    BeginScissorMode(scissor_pos.x, scissor_pos.y, rect.width * camera.zoom, rect.height * camera.zoom);

    for (i32 reel = 0; reel < reels; reel++) {
        for (i32 row = -1; row < rows; row++) {
            i32 tile = row >= 0 ? buffer.buffer[reel][row] : upper_buffer[reel];

            Vector2 pos = {
                .x = this->rect.x + gap_x * (reel + 1) + reel * 40,
                .y = this->rect.y + gap_y * (row + 1) + row * 40 + offsets[reel],
            };

            DrawTexture(tex_tiles[tile], pos.x, pos.y, WHITE);
        }
    }

    EndScissorMode();
}

M3X1::M3X1() {
    slot.machine = this;
    slot.stake   = 10;
    slot.reels   = 3;
    slot.rows    = 1;

    slot.weights.add(1, 5);
    slot.weights.add(2, 3);
    slot.weights.add(3, 2);
    slot.buffer  = SlotBuffer::generate(slot.reels, slot.rows, slot.weights);

    slot.win_algo = [](Slot* slot) -> Money {
        M3X1* m3x1 = (M3X1*)slot->machine;

        Money win = 0;
        if (slot->buffer.at(0,0) == slot->buffer.at(1,0) && slot->buffer.at(1,0) == slot->buffer.at(2,0)) {
            return m3x1->payouts[slot->buffer.at(0,0)] * slot->stake;
        }
        else {
            return 0;
        }
    };
    slot.stop_callback = [](Slot* slot, int reel) {
        M3X1* m3x1 = (M3X1*)slot->machine;

        if (reel == 1 && slot->buffer.at(0,0) == slot->buffer.at(1,0)) {
            slot->current_spin_distance += 10;
            m3x1->anticipation = true;
        }

        if (reel == slot->reels - 1) {
            m3x1->anticipation = false;
        }
    };

    calculate_ev();
    printf("Spawned machien with %.2f%% RTP and %.2f%% win change.\n", ev*100, win_percent*100);
}

void M3X1::calculate_ev() {
    SlotBuffer buffer;
    Money total = 0;
    int spins = 100000;
    int no_wins = 0;
    for (i32 i = 0; i < spins; i++) {
        slot.buffer = SlotBuffer::generate(slot.reels, slot.rows, slot.weights);
        Money win = slot.win_algo(&slot) / slot.stake;
        if (!win) no_wins++;
        total += win;
    }
    this->ev = double(total) / spins;
    this->win_percent = double(spins - no_wins) / double(spins);
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
    //slot.rect = { pos.x + 10, pos.y + 60, 220, 130 };

    if (slot.spinning) {
        button.y += 12;
        button.height -= 12;
        color = { 0, 0, 160, 180 };

        // DrawRectangleRec(rect, BLACK);
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

    if (anticipation) {
        double t = game_time * 10;
        Color color = (t - floor(t) < 0.5) ? Color{54, 16, 112,255} : Color{117, 21, 143,255};
        DrawRectangleRec(slot.get_reel_rect(2), color);
    }
    slot.update();
}

void gain_money(Money amount, Vector2 pos) {
    if (amount == 0) return;

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
    machines[0] = m3x1;
    display_money = money;


    while (!WindowShouldClose()) {

        camera = {
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
        ClearBackground({22,0,50,255});

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


        if (1) { // FPS Counter
            char buf[64];
            snprintf(buf, 64, "FPS: %d", GetFPS());
            DrawText(buf, 8, 8, 20, WHITE);
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

