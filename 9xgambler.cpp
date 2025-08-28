#include "raylib.h"
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <string>
#include <format>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define VIEWPORT_WIDTH 1024
#define VIEWPORT_HEIGHT 768

#define GAME_NAME "9XGAMBLER"
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

struct ShopEntry {
    const char* name;
    virtual Money cost() { return 0; }
    virtual const char* lock_reason() { return nullptr; }
    virtual void buy() { }
    virtual bool should_remove() { return false; }
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

struct SlotTile {
    int     id;
    Texture texture;
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
    float             reel_offset_time = 0.1;
    int               spin_distance    = 10;
    int               spin_distance_per_reel = 3;
    Money             stake            = 1;
    SlotBuffer        buffer           = {};
    int               reels            = {};
    int               rows             = {};
    Weights<int>      weights          = {};
    std::vector<SlotTile> tiles        = {};
    float             speed            = 300;
    float             row_height       = 40;
    int               current_spin_distance = 0;

    float offsets[MAX_SLOT_REELS]       = {};
    int   upper_buffer[MAX_SLOT_REELS]  = {};
    int   spin_iter[MAX_SLOT_REELS]     = {};
    bool stopped[MAX_SLOT_REELS]        = {};

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
    Texture texture = {};

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

// --- Renderer State -----------------------------------------

int                       screen_width    = 1024;
int                       screen_height   = 768;
float                     screen_scale    = 1;
Camera2D                  camera          = {};
Vector2                   mouse           = {};
double                    game_time       = 0;
double                    dt              = 0;
double                    run_start_time  = 0;
std::vector<TextOnScreen> texts           = {};
const char*               tooltip         = nullptr;

// --- Textures -----------------------------------------------

Texture tex_background;
Texture tex_m3x1;
Texture tex_m1x1;

Texture tex_tile_0;
Texture tex_tile_dot;
Texture tex_tile_orange;
Texture tex_tile_cherry;
Texture tex_tile_7;
Texture tex_tile_777;

Texture tex_tile_9;
Texture tex_tile_10;
Texture tex_tile_j;
Texture tex_tile_q;
Texture tex_tile_k;


// --- Game state ---------------------------------------------


GameScreen  screen = GameScreen::Machines;
Money       money  = 1500;

const Money spot_prices[9] = {
    100,    200,    2000,
    5000,   25000,  100000,
    20000,  500000, 1000000,
};

bool spot_unlocked[9] = {};
Machine* machines[9] = {};
double display_money = 0;

std::vector<ShopEntry*> shop_entries;
int shop_page = 0;

void gain_money(Money money, Vector2 pos);
bool button(ButtonState state);

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
            stopped[reel] = false;
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

            if (stopped[reel]) {
                continue;
            }

            int required_distance = current_spin_distance + spin_distance_per_reel * reel;
            if (spin_iter[reel] >= required_distance) {
                if (!stopped[reel]) {
                    stopped[reel] = true;
                    spin_iter[reel] = 99999999; // set to high value in case current_spin_distance changes
                    offsets[reel] = 0;
                    if (this->on_reel_stop) this->on_reel_stop(this, reel); 
                }
            }
            else {
                offsets[reel] += speed * dt;
                done = false;

                while (offsets[reel] > row_height) {
                    buffer.advance(reel, upper_buffer[reel]);
                    upper_buffer[reel] = weights.generate();
                    offsets[reel] -= row_height;

                    spin_iter[reel]++;
                    if (spin_iter[reel] >= required_distance) {
                    }
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

            DrawTexture(tiles[tile].texture, pos.x, pos.y, WHITE);
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
    DrawTexture(texture, pos.x, pos.y - 9, WHITE);
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

// --- M1X1 ---------------------------------------------------

struct M1X1 : SlotMachine {
    std::vector<float> payouts = {};

    M1X1() {
        slot.machine = this;
        slot.stake   = 10;
        slot.reels   = 1;
        slot.rows    = 1;
        slot.speed   = 1000;
        slot.spin_distance = 20;
        payouts = { 0, 3, 7, 15, 20 };
        texture = tex_m1x1;

        slot.weights.add(0, 23);
        slot.weights.add(1, 7);
        slot.weights.add(2, 5);
        slot.weights.add(3, 3);
        slot.weights.add(4, 2);

        slot.tiles = {
            { .id = 0, .texture = tex_tile_dot },
            { .id = 1, .texture = tex_tile_orange },
            { .id = 2, .texture = tex_tile_cherry },
            { .id = 3, .texture = tex_tile_7 },
            { .id = 4, .texture = tex_tile_k },
            { .id = 4, .texture = tex_tile_k },
        };

        calculate_ev();
        printf("Spawned M1X1 (RTP: %.2f%%, Win Chance: %.2f%%)\n", ev*100, win_percent*100);

        slot.buffer = SlotBuffer::generate(slot.reels, slot.rows, slot.weights);
    }

    virtual Money calculate_win() override {
        return payouts[slot.buffer.at(0,0)] * slot.stake;
    }
};

// -- M3X1 ----------------------------------------------------

struct M3X1 : SlotMachine {
    bool anticipation = false;
    std::vector<float> payouts = {};

    M3X1() {
        slot.stake   = 10;
        slot.reels   = 3;
        slot.rows    = 1;
        slot.speed   = 800;
        slot.spin_distance = 20;
        slot.spin_distance_per_reel = 4;
        slot.reel_offset_time = 0.2;
        texture = tex_m3x1;
        payouts = { 20, 100, 200, 5000 };

        slot.tiles = {
            { .id = 0, .texture = tex_tile_orange  },
            { .id = 1, .texture = tex_tile_cherry  },
            { .id = 2, .texture = tex_tile_7 },
            { .id = 3, .texture = tex_tile_777  },
        };

        slot.weights.add(0, 10);
        slot.weights.add(1, 5);
        slot.weights.add(2, 3);
        slot.weights.add(3, 1);

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
            slot.current_spin_distance += 20;
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

// --- Gameplay functions -------------------------------------

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

void go_to_screen(GameScreen _screen) {
    if (screen == _screen) return;

    screen = _screen;

    switch (screen) {
        case GameScreen::Shop: {
            shop_page = 0;
            break;
        }
        default: break;
    }
}

void buy_spot(int i) {
    assert(!spot_unlocked[i]);
    if (!spot_unlocked[i]) {
        gain_money(-spot_prices[i], mouse);
        spot_unlocked[i] = true;
    }
}


struct ShopEntry_BuyNextSpot : ShopEntry {
    ShopEntry_BuyNextSpot() {
        this->name = "Buy next Machine Spot";
    }

    virtual bool should_remove() override {
        return next_spot_to_buy() == -1;
    }

    virtual Money cost() override {
        return spot_prices[next_spot_to_buy()];
    }

    virtual void buy() override {
        buy_spot(next_spot_to_buy());
    }

    int next_spot_to_buy() {
        for (int i = 0; i < 9; i++)
            if (!spot_unlocked[i])
                return i;
        return -1;
    }
};

struct ShopEntry_Machine : ShopEntry {
    std::string text;
    Money _cost;
    Machine* (*construct)();

    ShopEntry_Machine(const char* name, const char* tagline, Money cost, Machine* (*construct)()) {
        this->text = std::format("Buy {}", name);
        this->name = this->text.c_str();
        this->_cost = cost;
        this->construct = construct;
    }

    virtual Money cost() override {
        return this->_cost;
    }

    virtual const char* lock_reason() override {
        for (int i = 0; i < 9; i++)
            if (spot_unlocked[i] && !machines[i])
                return nullptr;
        return "No empty spots";
    }

    virtual void buy() override {
        gain_money(-_cost, mouse);
        Machine* machine = this->construct();

        for (int i = 0; i < 9; i++)
            if (spot_unlocked[i] && !machines[i]) {
                machines[i] = machine;
                break;
            }
    }
};

// --- Send it ------------------------------------------------

int main() {
    SetConfigFlags(/*FLAG_VSYNC_HINT  | */ FLAG_WINDOW_RESIZABLE);
    InitWindow(screen_width, screen_height, GAME_NAME);
    SetTargetFPS(60);

    // --- Load Assets --------------------------------------------

    tex_background = LoadTexture("assets/background.png");
    tex_m1x1 = LoadTexture("assets/m1x1.png");
    tex_m3x1 = LoadTexture("assets/m3x1.png");

    tex_tile_0        = LoadTexture("assets/tile_0.png");
    tex_tile_dot      = LoadTexture("assets/tile_dot.png");
    tex_tile_cherry   = LoadTexture("assets/tile_cherry.png");
    tex_tile_orange   = LoadTexture("assets/tile_orange.png");
    tex_tile_7        = LoadTexture("assets/tile_7.png");
    tex_tile_777      = LoadTexture("assets/tile_777.png");

    tex_tile_9     = LoadTexture("assets/tile_9.png");
    tex_tile_10    = LoadTexture("assets/tile_10.png");
    tex_tile_j     = LoadTexture("assets/tile_j.png");
    tex_tile_q     = LoadTexture("assets/tile_q.png");
    tex_tile_k     = LoadTexture("assets/tile_k.png");

    // --- Init gameplay ------------------------------------------

    shop_entries.push_back(new ShopEntry_BuyNextSpot());

    shop_entries.push_back(new ShopEntry_Machine(
        "1X1",
        "Baby's first slot machine",
        500,
        []() -> Machine* { return new M1X1(); }
    ));

    shop_entries.push_back(new ShopEntry_Machine(
        "3X1",
        "A more civilized slot machine",
        500,
        []() -> Machine* { return new M3X1(); }
    ));

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
                                buy_spot(i);
                            }
                        }
                    }
                }

                break;
            }
            case GameScreen::Shop: {
                float y = 8;
                float x_start = 8;
                float x_end = 620;

                DrawText("SHOP", y, 12, 40, WHITE);
                if (button({
                    .rect         = { 540, y, 80, 40 },
                    .text         = "Close",
                    .background   = RED,
                })) {
                    go_to_screen(GameScreen::Machines);
                }

                y = 55;
                DrawLineEx({x_start, y}, {x_end, y}, 4, WHITE);
                y += 20;

                int start = shop_page * 8;
                int end = 8;
                if (start < 0) start = 0;
                if (end > shop_entries.size()) end = shop_entries.size();


                for (int i = start; i < end; i++) {
                    ShopEntry* entry = shop_entries[i];
                    if (entry->should_remove()) {
                        delete entry;
                        shop_entries.erase(shop_entries.begin() + i);
                        i--;
                    }
                    else {
                        Money cost = entry->cost();
                        bool can_afford = money >= cost;
                        const char* lock_reason = entry->lock_reason();

                        DrawRectangle(x_start, y, x_end - x_start, 60, BLACK);
                        DrawText(entry->name, x_start + 4, y + 4, 20, WHITE);

                        char buf[64] = {};
                        snprintf(buf, 64, "$%lld", cost);

                        int len = MeasureText(buf, 40);
                        DrawText(buf, x_end - 130 - 8 - len, y + 10, 40, can_afford ? WHITE : RED);

                        Rectangle button_rect = { x_end - 130, y + 6, 122, 47 };
                        if (button({
                            .rect         = button_rect,
                            .text         = "BUY",
                            .font_size    = 40,
                            .enabled      = can_afford && !entry->lock_reason(),
                        })) {
                            entry->buy();
                        }

                        if (CheckCollisionPointRec(mouse, button_rect)) {
                            if (lock_reason)
                                tooltip = lock_reason;
                            else if (!can_afford)
                                tooltip = "Can't afford.";
                        }

                        y += 70;
                    }
                }

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
            go_to_screen(screen == GameScreen::Shop ? GameScreen::Machines : GameScreen::Shop);
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

        if (tooltip) {
            int len = MeasureText(tooltip, 20);
            Vector2 pos = mouse;
            pos.y += 30;
            if (pos.x + len > screen_width) pos.x = screen_width - len;
            if (pos.x < 0) pos.x = 0;
            if (pos.y + 20 > screen_height) pos.y = screen_height - 20;
            DrawRectangle(pos.x - 4, pos.y - 4, len + 8, 28, BLACK);
            DrawText(tooltip, pos.x, pos.y, 20, WHITE);
        }
        tooltip = nullptr;

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

