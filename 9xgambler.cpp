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
double run_start_time = 0;

enum class GameScreen {
    Machines,
    Shop,
};

struct Machine {
    Vector2 pos;
    double ev = 0;
    double win_percent = 0;

    int shake_x = 0;
    int shake_y = 0;
    double shake_time = 0;

    virtual void update() = 0;
    virtual void draw() = 0;

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

        for (int reel = 0; reel < reels; reel++)
            for (int row = 0; row < reels; row++)
                buffer.buffer[reel][row] = weights.generate();

        return buffer;
    }

    int& at(int reel, int row) {
        return buffer[reel][row];
    }

    void advance(int reel, int new_tile) {
        for (int row = rows - 1; row >= 1; row--)
            buffer[reel][row] = buffer[reel][row-1];
         buffer[reel][0] = new_tile;
    }
};

struct SlotMachine;

struct Slot {
    SlotMachine*      machine          = nullptr;
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

    float offsets[MAX_SLOT_REELS]       = {};
    int   upper_buffer[MAX_SLOT_REELS]  = {};
    int   spin_iter[MAX_SLOT_REELS]     = {};

    // CALLBACKS:
    void (*on_reel_stop)(Slot* slot, int reel) = nullptr;
    void (*on_stop)(Slot* slot) = nullptr;

    Rectangle get_reel_rect(int reel);
    void spin(Vector2 pos);

    void update();
    void draw();

    virtual ~Slot() {}
};

struct SlotMachine : Machine {
    Slot slot = {};

    virtual void update();
    virtual void calculate_ev();
    virtual Money calculate_win() = 0;
    virtual void on_reel_stop(int reel) {}
    virtual void on_stop();

    virtual void draw();
    virtual void draw_background();
    virtual void draw_button();
    virtual void draw_slot();

    SlotMachine();
    virtual ~SlotMachine() {}
};


Texture tex_background;
Texture tex_m3x1;
Texture tex_m1x1;
Texture tex_tiles[TILE_COUNT];

// --- Renderer State -----------------------------------------

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

struct ButtonState {
    Rectangle   rect         = {};
    const char* text         = nullptr;
    Color       background   = BLUE;
    Color       text_color   = WHITE;
    int         font_size    = 20;
    bool        enabled      = true;  
};

std::vector<TextOnScreen> texts;

// --- Game state ---------------------------------------------

GameScreen screen = GameScreen::Machines;

const Money spot_prices[9] = {
    50,     500,    1000,
    5000,   25000,  100000,
    20000,  500000, 1000000,
};

bool spot_unlocked[9] = {};

Machine* machines[9] = {};
Money money = 200;
double display_money = 0;

void gain_money(Money money, Vector2 pos);
bool button(ButtonState state);
float Lerp(float a, float b, float t);
float Remap(float val, float old_min, float old_max, float new_min, float new_max);

// --- Utils --------------------------------------------------

float decimal_part(float f) {
    return f - floorf(f);
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float Remap(float val, float old_min, float old_max, float new_min, float new_max) {
    float t = (val - old_min) / (old_max-old_min);
    return lerp(new_min, new_max, t);
}

float color_clamp(float x) {
    if (x < 0) return 0;
    if (x > 255) return 255;
    return x;
}

// --- Machine methods ----------------------------------------

void Machine::shake() {
    if (game_time - shake_time > 0.02) {
        shake_time = game_time;
        shake_x = GetRandomValue(-1, 1);
        shake_y = GetRandomValue(-2, 2);
    }
    pos.x += shake_x;
    pos.y += shake_y;
}

// --- Slot methods -------------------------------------------

void Slot::spin(Vector2 pos) {
    if (!spinning) {
        for (int reel = 0; reel < reels; reel++) {
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
        for (int reel = 0; reel < reels; reel++) {
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
                if (spin_iter[reel] == current_spin_distance) {
                    spin_iter[reel] = 99999999; // set to high value in case current_spin_distance changes
                    if (this->on_reel_stop) this->on_reel_stop(this, reel); 
                }
            }
        }
        if (done) {
            spinning = false;
            if (this->on_stop) this->on_stop(this);
        }
    }
}

void Slot::draw() {
    float avail_space_x = rect.width - reels * 40;
    float avail_space_y = rect.height - rows * 40;
    float gap_x = avail_space_x / (reels + 1);
    float gap_y = avail_space_y / (rows + 1);

    row_height = 40 + gap_y;

    Vector2 scissor_pos = GetWorldToScreen2D({rect.x, rect.y}, camera);
    BeginScissorMode(scissor_pos.x, scissor_pos.y, rect.width * camera.zoom, rect.height * camera.zoom);

    for (int reel = 0; reel < reels; reel++) {
        for (int row = -1; row < rows; row++) {
            int tile = row >= 0 ? buffer.buffer[reel][row] : upper_buffer[reel];

            Vector2 pos = {
                .x = this->rect.x + gap_x * (reel + 1) + reel * 40,
                .y = this->rect.y + gap_y * (row + 1) + row * 40 + offsets[reel],
            };

            DrawTexture(tex_tiles[tile], pos.x, pos.y, WHITE);
        }
    }

    EndScissorMode();
}

// --- SlotMachine methods ------------------------------------

SlotMachine::SlotMachine() {
    slot.machine = this;

    slot.on_reel_stop = [](Slot* slot, int reel) {
        slot->machine->on_reel_stop(reel);
    };

    slot.on_stop = [](Slot* slot) {
        slot->machine->on_stop();
    };
}

void SlotMachine::on_stop() {
    Money win = calculate_win();
    gain_money(win, { slot.rect.x, slot.rect.y });
}

void SlotMachine::update() {
    slot.update();
}

void SlotMachine::draw_background() {
    DrawTexture(tex_m3x1, pos.x, pos.y - 9, WHITE);
}

void SlotMachine::draw_button() {
    Color color = { 0, 0, 255, 255 };

    Rectangle button = {
        .x = pos.x + float(MACHINE_WIDTH - BUTTON_WIDTH) / 2,
        .y = pos.y + float(MACHINE_HEIGHT - BUTTON_HEIGHT) - 8,
        .width = float(BUTTON_WIDTH),
        .height = float(BUTTON_HEIGHT),
    };

    if (slot.spinning) {
        button.y += 12;
        button.height -= 12;
        color = { 0, 0, 160, 80 };
    }
    else {
        if (CheckCollisionPointRec(mouse, button)) {
            color = Color { 32, 80, 255, 255 };

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                slot.spin({button.x, button.y});
            }
        }
    }

    DrawRectangleRec(button, color);
    DrawText("SPIN", button.x + 6, button.y + 10, 20, WHITE);
}

void SlotMachine::draw() {
    if (slot.spinning) shake();

    draw_background();
    draw_slot();
    draw_button();
}

void SlotMachine::draw_slot() {
    slot.rect = { pos.x + 10, pos.y + 60, 164, 86 };
    slot.draw();
}

void SlotMachine::calculate_ev() {
    SlotBuffer buffer;
    Money total = 0;
    int spins = 100000;
    int no_wins = 0;
    for (int i = 0; i < spins; i++) {
        slot.buffer = SlotBuffer::generate(slot.reels, slot.rows, slot.weights);
        Money win = calculate_win();
        if (!win) no_wins++;
        total += win;
    }
    this->ev = double(total) / spins;
    this->win_percent = double(spins - no_wins) / double(spins);
}

// -- M3X1 ----------------------------------------------------

struct M3X1 : SlotMachine {
    bool anticipation = false;
    std::vector<float> payouts = {};

    M3X1() {
        slot.stake   = 10;
        slot.reels   = 3;
        slot.rows    = 1;
        payouts      = { 0, 7, 20, 40 };
        slot.weights.add(1, 5);
        slot.weights.add(2, 3);
        slot.weights.add(3, 2);

        calculate_ev();
        printf("Spawned M3X1 (RTP: %.2f%%, Win Chance: %.2f%%)\n", ev*100, win_percent*100);

        slot.buffer = SlotBuffer::generate(slot.reels, slot.rows, slot.weights);
    }

    virtual Money calculate_win() override {
        Money win = 0;
        if (slot.buffer.at(0,0) == slot.buffer.at(1,0) && slot.buffer.at(1,0) == slot.buffer.at(2,0)) {
            return payouts[slot.buffer.at(0,0)] * slot.stake;
        }
        else {
            return 0;
        }
    }

    virtual void on_reel_stop(int reel) override {
        if (reel == 1 && slot.buffer.at(0,0) == slot.buffer.at(1,0)) {
            slot.current_spin_distance += 10;
            anticipation = true;
        }
    }

    virtual void on_stop() override {
        SlotMachine::on_stop();
        anticipation = false;
    }

    virtual void draw_slot() override {
        if (anticipation) {
            double t = game_time * 10;
            Color color = decimal_part(t) < 0.5 ? Color{54, 16, 112,255} : Color{117, 21, 143,255};
            DrawRectangleRec(slot.get_reel_rect(2), color);
        }
        SlotMachine::draw_slot();
    }
};

// --- M1X1 ---------------------------------------------------

struct M1X1 : SlotMachine {
    std::vector<float> payouts = {};
    Slot slot = {};

    M1X1() {
        slot.machine = this;
        slot.stake   = 10;
        slot.reels   = 1;
        slot.rows    = 1;
        slot.speed   = 400;
        slot.spin_distance = 13;
        payouts = { 0, 0, 1.5, 2.5, 6 };

        slot.weights.add(1, 4);
        slot.weights.add(2, 3);
        slot.weights.add(3, 2);
        slot.weights.add(4, 1);

        calculate_ev();
        printf("Spawned M1X1 (RTP: %.2f%%, Win Chance: %.2f%%)\n", ev*100, win_percent*100);

        slot.buffer = SlotBuffer::generate(slot.reels, slot.rows, slot.weights);
    }

    virtual Money calculate_win() override {
        return payouts[slot.buffer.at(0,0)] * slot.stake;
    }
};


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

bool button(ButtonState state) {
    bool hover = state.enabled && CheckCollisionPointRec(mouse, state.rect);
    if (hover) state.background.r = color_clamp(state.background.r * 1.5);
    if (hover) state.background.g = color_clamp(state.background.g * 1.5);
    if (hover) state.background.b = color_clamp(state.background.b * 1.5);

    if (!state.enabled) {
        state.background = GRAY;
        state.background.a = 128;
        state.text_color.a = 128;
    }

    DrawRectangleRec(state.rect, state.background);

    int w = MeasureText(state.text, state.font_size);
    DrawText(state.text, state.rect.x + state.rect.width / 2 - w/2.0f, 2 + state.rect.y + state.rect.height/2 - state.font_size/2.0f, state.font_size, state.text_color);

    bool click = hover && state.enabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    return click;
}

int main() {

    SetConfigFlags(/*FLAG_VSYNC_HINT  | */FLAG_WINDOW_RESIZABLE);
    InitWindow(screen_width, screen_height, game_title);
    SetTargetFPS(60);

    // --- Load Assets --------------------------------------------
    tex_background = LoadTexture("assets/background.png");
    tex_m1x1 = LoadTexture("assets/m1x1.png");
    tex_m3x1 = LoadTexture("assets/m3x1.png");

    tex_tiles[0] = LoadTexture("assets/tile0.png");
    tex_tiles[1] = LoadTexture("assets/tile1.png");
    tex_tiles[2] = LoadTexture("assets/tile2.png");
    tex_tiles[3] = LoadTexture("assets/tile3.png");
    tex_tiles[4] = LoadTexture("assets/tile4.png");
    tex_tiles[5] = LoadTexture("assets/tile5.png");

    // --- Init gameplay ------------------------------------------

    /*
    M1X1* m1x1 = new M1X1();
    machines[0] = m1x1;
    M3X1* m3x1 = new M3X1();
    machines[1] = m3x1;
    */

    display_money = money;

    run_start_time = GetTime();

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

        // --- Simulate machines --------------------------------------
        for (int i = 0; i < 9; i++) {
            if (machines[i]) machines[i]->update();
        }

        // --- Render game --------------------------------------------
        DrawTexture(tex_background, 0, 0, WHITE);

        switch (screen) {
            case GameScreen::Machines: {

                for (int i = 0; i < ARRAY_SIZE(machines); i++) {
                    Machine* machine = machines[i];

                    int x = TOP_PADDING + (i % 3) * (MACHINE_WIDTH + MACHINE_GAP_X);
                    int y = RIGHT_PADDING + (i / 3) * (MACHINE_HEIGHT + MACHINE_GAP_Y);


                    if (machine) {
                        machine->pos.x = x;
                        machine->pos.y = y;
                        machine->draw();
                    }
                    else {
                        DrawRectangle(x, y, MACHINE_WIDTH, MACHINE_HEIGHT, Color{0, 0, 0, 90});

                        if (spot_unlocked[i]) {
                            float _y = y + 5;
                            DrawText("NO MACHINE", x + 10, _y, 20, WHITE);
                            _y += 40;
                            DrawText("Open the SHOP", x + 10, _y, 20, WHITE);
                            _y += 20;
                            DrawText("to buy one", x + 10, _y, 20, WHITE);
                            _y += 20;
                        }
                        else {
                            bool enabled = spot_prices[i] <= money;
                            float _y = y + 5;
                            DrawText("SPOT", x + 10, _y, 40, RED);
                            _y += 40;
                            DrawText("LOCKED", x + 10, _y, 40, RED);
                            _y += 50;

                            char buf[64];
                            snprintf(buf, _y, "Price: $%lld", spot_prices[i]);
                            DrawText(buf, x + 10, y + 90, 20,  enabled ? GREEN : RED);
                            _y += 30;

                            if (button({
                                .rect = Rectangle{float(x + 8), float(_y), MACHINE_WIDTH - 16, y + MACHINE_HEIGHT - _y - 8 },
                                .text = "BUY",
                                .enabled = enabled,
                            })) {
                                gain_money(-spot_prices[i], mouse);
                                spot_unlocked[i] = true;
                            }
                        }
                    }
                }

                break;
            }
            case GameScreen::Shop: {
                break;
            }
        }

        // --- Draw Money ------------------------------------------
        char buf[64];
        display_money = Lerp(display_money, double(money), 10 * dt);
        snprintf(buf, 64, "%ld", i64(roundf(display_money)));
        int _y = 154;
        DrawText(buf, 714, _y, 40, WHITE);
        _y += 50;

        // --- Draw Time Spent Solvent -----------------------------
        double time_solvent = game_time - run_start_time;
        snprintf(buf, 64, "%d:%.2d", int(time_solvent / 60), int(floor(time_solvent)) % 60);
        DrawText("Time spent Solvent", 700, _y, 20, WHITE);
        DrawText(buf, 900, _y, 40, WHITE);
        _y += 30;

        // --- Draw Shop button -----------------------------
        if (button({
            .rect         = { 700, float(_y), 150, 45 },
            .text         = "SHOP",
            .background   = BLUE,
            .text_color   = WHITE,
            .font_size    = 40,
        })) {
            screen = screen == GameScreen::Shop ? GameScreen::Machines : GameScreen::Shop;
        }

        for (int i = 0; i < texts.size(); i++) {
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

