#include "raylib.h"
#include <stdint.h>
#include <initializer_list>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <string>
#include <format>
#include <algorithm>

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
#define UPGRADE_COST_INCREASE_FACTOR 1.3
#define ROLL_COST_INCREASE_FACTOR 1.1
#define POLICE_TIME 60
#define START_MAX_UPGRADES 5

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef i64 Money;

struct Timer {
    const char* text;
    const char* tooltip;
    double time_left;
    Money cost = 0;

    virtual bool action() = 0;
    virtual ~Timer() {}
};

enum class ShopEntryType {
    Machine,
    Upgrade,
};

enum class UpgradeType {
    Speed,
    Auto_Click,
    Double_Stake,
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
    int upgrades = 0;
    Money stake = 1;

    virtual void update() = 0;
    virtual void draw() = 0;
    virtual ~Machine() {}

    virtual void upgrade(UpgradeType type) {
        upgrades++;
    }

    void shake();
};

struct ShopEntry {
    const char* name = "";
    const char* tagline = "";
    virtual Money cost() { return 0; }
    virtual const char* lock_reason() { return nullptr; }
    virtual void buy() { }
    virtual void draw_icon(int x, int y) {
        DrawRectangle(x, y, 130, 180, RED);
    };
};

template <typename T>
struct Weights {
    std::vector<T> array;

    Weights() {
    }

    Weights(std::initializer_list<std::pair<T, int>> list) {
        for (auto& item : list)
            add(item.first, item.second);
    }

    void add(T entry, int weight) {
        for (int i = 0; i < weight; i++)
            array.push_back(entry);
    }

    T generate() {
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
    SlotBuffer        buffer           = {};
    int               reels            = {};
    int               rows             = {};
    Weights<int>      weights          = {};
    std::vector<SlotTile> tiles        = {};
    float             speed            = 300;
    float             row_height       = 40;
    int               current_spin_distance = 0;
    double            last_tick        = 0;
    double            tick_rate        = 0.3;

    float offsets[MAX_SLOT_REELS]       = {};
    int   upper_buffer[MAX_SLOT_REELS]  = {};
    int   spin_iter[MAX_SLOT_REELS]     = {};
    bool stopped[MAX_SLOT_REELS]        = {};

    // CALLBACKS:
    void (*on_reel_stop)(Slot* slot, int reel) = nullptr;
    void (*on_stop)(Slot* slot) = nullptr;

    Rectangle get_reel_rect(int reel);
    void spin(Money stake, Vector2 pos);

    void update();
    void draw();

    virtual ~Slot() {}
};

struct SlotMachine : Machine {
    Slot slot = {};
    Texture texture = {};
    float auto_click_time = -1;
    double last_auto_click_time = 0;

    virtual void update() override;
    virtual void upgrade(UpgradeType type) override;
    virtual void calculate_ev();
    virtual Money calculate_win() = 0;
    virtual void on_reel_stop(int reel) {}
    virtual void on_stop();

    virtual void draw() override;
    virtual void draw_background();
    virtual void draw_spin_button();
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
Texture tex_mb5;
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

// --- Sounds -------------------------------------------------

Sound snd_upgrade;
Sound snd_win[2];
Sound snd_hat[48];
Sound snd_reelstop;
Music msc_police;
Music msc_anticipation;
int msc_anticipation_count = 0;

// --- Game state ---------------------------------------------

GameScreen  screen       = GameScreen::Machines;
Money       money        = 1500;
Money       roll_cost    = 50;
int         max_upgrades = START_MAX_UPGRADES;
std::vector<Timer*> timers;
Timer* police_timer = nullptr;

const Money spot_prices[9] = {
    100,    200,    2000,
    5000,   25000,  100000,
    20000,  500000, 1000000,
};

bool spot_unlocked[9] = {};
Machine* machines[9] = {};
double display_money = 0;

std::vector<ShopEntry*> shop_entries;

void gain_money(Money money, Vector2 pos);
bool button(ButtonState state);

Weights<ShopEntryType> shop_types_weights;
Weights<ShopEntry*> shop_machines_weights;
Weights<ShopEntry*> shop_upgrades_weights;

// machine selection
bool select_machine = false;
const char* select_machine_text = nullptr;
void (*select_machine_callback)(Machine* machine);
void* select_machine_callback_userdata;
UpgradeType current_upgarde_type;
bool has_illegal_machines = false;

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

void Slot::spin(Money stake, Vector2 pos) {
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

void play_tick_sound() {
    static int x = 0;
    x++;
    if (x >= 48) x = 0;
    PlaySound(snd_hat[x]);
}

void play_win_sound() {
    static int x = 0;
    x++;
    if (x >= 2) x = 0;
    PlaySound(snd_win[x]);
}


void Slot::update() {
    if (spinning) {
        spin_time += dt;

        if (game_time - last_tick > tick_rate) {
            play_tick_sound();
            last_tick = game_time;
        }

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
        PlaySound(snd_reelstop);
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

void SlotMachine::upgrade(UpgradeType type) {
    Machine::upgrade(type);

    switch (type) {
        case UpgradeType::Speed: {
            slot.speed *= 1.3;
            slot.reel_offset_time /= 1.3;
            slot.tick_rate /= 1.1;
            if (slot.tick_rate < 0.05)
                slot.tick_rate = 0.05;
            break;
        }
        case UpgradeType::Double_Stake: {
            stake *= 2;
            break;
        }
        case UpgradeType::Auto_Click: {
            if (auto_click_time < 0) auto_click_time = 5;
            else auto_click_time /= 2;
            break;
        }
    }
}

void SlotMachine::draw_background() {
    DrawTexture(texture, pos.x, pos.y - 9, WHITE);
}

void SlotMachine::draw_spin_button() {
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
        bool spin = false;
        if (auto_click_time >= 0 && game_time - last_auto_click_time > auto_click_time) {
            spin = true;
            last_auto_click_time = game_time;
        }

        if (CheckCollisionPointRec(mouse, button) && !select_machine) {
            color = Color { 32, 80, 255, 255 };
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) spin = true;
        }

        if (spin) slot.spin(stake, {button.x, button.y});
    }

    DrawRectangleRec(button, color);
    DrawText("SPIN", button.x + 6, button.y + 10, 20, WHITE);
}

void SlotMachine::draw() {
    if (slot.spinning) shake();

    draw_background();
    draw_slot();
    draw_spin_button();
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
        this->stake   = 10;

        slot.machine = this;
        slot.reels   = 1;
        slot.rows    = 1;
        slot.speed   = 1000;
        slot.spin_distance = 20;
        payouts = { 0, 3, 7, 15, 20 };
        slot.weights = { {0, 23}, {1,7}, {2,5}, {3,3}, {4,2} };
        texture = tex_m1x1;
        slot.tick_rate = 0.15;

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
        return payouts[slot.buffer.at(0,0)] * stake;
    }
};

// -- M3X1 ----------------------------------------------------

struct M3X1 : SlotMachine {
    bool anticipation = false;
    std::vector<float> payouts = {};

    M3X1() {
        this->stake = 10;

        slot.reels   = 3;
        slot.rows    = 1;
        slot.speed   = 800;
        slot.spin_distance = 20;
        slot.spin_distance_per_reel = 4;
        slot.reel_offset_time = 0.2;
        texture = tex_m3x1;
        payouts = { 20, 100, 200, 5000 };
        slot.weights = {{0,10}, {1,5}, {2,3}, {3,1}};
        slot.tick_rate = 0.15;

        slot.tiles = {
            { .id = 0, .texture = tex_tile_orange  },
            { .id = 1, .texture = tex_tile_cherry  },
            { .id = 2, .texture = tex_tile_7 },
            { .id = 3, .texture = tex_tile_777  },
        };

        calculate_ev();
        printf("Spawned M3X1 (RTP: %.2f%%, Win Chance: %.2f%%)\n", ev*100, win_percent*100);

        slot.buffer = SlotBuffer::generate(slot.reels, slot.rows, slot.weights);
    }

    virtual Money calculate_win() override {
        Money win = 0;
        if (slot.buffer.at(0,0) == slot.buffer.at(1,0) && slot.buffer.at(1,0) == slot.buffer.at(2,0)) {
            return payouts[slot.buffer.at(0,0)] * this->stake;
        }
        else {
            return 0;
        }
    }

    virtual void on_reel_stop(int reel) override {
        if (reel == 1 && slot.buffer.at(0,0) == slot.buffer.at(1,0)) {
            slot.current_spin_distance += 20;
            anticipation = true;
            if (msc_anticipation_count == 0) PlayMusicStream(msc_anticipation);
            msc_anticipation_count++;
        }
    }

    virtual void on_stop() override {
        SlotMachine::on_stop();
        last_auto_click_time = game_time;
        if (anticipation) {
            anticipation = false;
            msc_anticipation_count--;
            if (msc_anticipation_count == 0) StopMusicStream(msc_anticipation);
        }
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

struct MB5 : SlotMachine {
    std::vector<float> payouts = {};

    MB5() {
        this->stake = 10;

        slot.reels   = 4;
        slot.rows    = 3;
        slot.speed   = 800;
        slot.spin_distance = 25;
        slot.spin_distance_per_reel = 4;
        slot.reel_offset_time = 0.1;
        texture = tex_mb5;
        payouts = { 0, 20, 100, 800, 1200 };
        slot.weights = {{0,8}, {1,5}, {2,3}, {3,2}, {4,2}};
        slot.tick_rate = 0.15;

        slot.tiles = {
            { .id = 0, .texture = tex_tile_dot  },
            { .id = 1, .texture = tex_tile_orange  },
            { .id = 2, .texture = tex_tile_cherry  },
            { .id = 3, .texture = tex_tile_7 },
            { .id = 4, .texture = tex_tile_777  },
        };

        calculate_ev();
        printf("Spawned MB5 (RTP: %.2f%%, Win Chance: %.2f%%)\n", ev*100, win_percent*100);

        slot.buffer = SlotBuffer::generate(slot.reels, slot.rows, slot.weights);
    }

    virtual Money calculate_win() override {
        Money win = 0;
        for (SlotTile t : slot.tiles) {
            int count = 0;
            for (int reel = 0; reel < slot.reels; reel++)
                for (int row = 0; row < slot.rows; row++)
                    if (slot.buffer.at(reel, row) == t.id)
                            count++;
            if (count >= 5) {
                win += this->payouts[t.id] * stake;
            }
        }
        return win;
    }

    virtual void on_stop() override {
        SlotMachine::on_stop();
        last_auto_click_time = game_time;
    }

    virtual void draw_slot() override {
        slot.rect = { pos.x + 5, pos.y + 60, 184, 107 };
        slot.draw();
    }
};

// --- Timers -------------------------------------------------

struct Timer_Police : Timer {
    Timer_Police() {
        text = "POLICE";
    }

    virtual bool action() {
        for (int i = 0; i < 9; i++) {
            if (machines[i] && machines[i]->upgrades > max_upgrades) {
                delete machines[i];
                machines[i] = nullptr;
            }
        }
        police_timer = nullptr;
        has_illegal_machines = false;
        StopMusicStream(msc_police);
        return false;
    }
};

struct Timer_Tax : Timer {
    double t;

    Timer_Tax(const char* name, double t, Money cost) {
        this->text = name;
        this->t = t;
        this->time_left = t;
        this->cost = cost;
    }

    virtual bool action() override {
        gain_money(-this->cost, {400.0f,400.0f});
        this->time_left = t;
        return true;
    }
};

// --- Gameplay functions -------------------------------------

void gain_money(Money amount, Vector2 pos) {
    if (amount == 0) return;
    if (amount > 0) play_win_sound();

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
    if (select_machine) return;

    if (screen == _screen) return;

    screen = _screen;

    switch (screen) {
        case GameScreen::Shop: {
            break;
        }
        default: break;
    }
}

void buy_spot(int i) {
    assert(!spot_unlocked[i]);
    if (!spot_unlocked[i]) {
        PlaySound(snd_upgrade);
        gain_money(-spot_prices[i], mouse);
        spot_unlocked[i] = true;
    }
}

ShopEntry* roll_shop_entry() {

    switch (shop_types_weights.generate()) {
        case ShopEntryType::Machine: {
            return shop_machines_weights.generate();
        }
        case ShopEntryType::Upgrade: {
            return shop_upgrades_weights.generate();
        }
    }
}

void roll_shop() {
    shop_entries.clear();
    for (int i = 0; i < 3; i++) {
        shop_entries.push_back(roll_shop_entry());
    }
}

void check_illegal_machines() {
    bool ok = true;
    for (Machine* machine : machines)
        if (machine && machine->upgrades > max_upgrades)
            ok = false;

    if (!ok == has_illegal_machines)
        return;

    has_illegal_machines = !ok;

    if (has_illegal_machines) {
        police_timer = new Timer_Police();
        police_timer->time_left = POLICE_TIME;
        timers.push_back(police_timer);
        PlayMusicStream(msc_police);
    }
}

void apply_upgrade(Machine* machine, UpgradeType type) {
    PlaySound(snd_upgrade);

    select_machine = false;
    machine->upgrade(type);

    check_illegal_machines();
}

// --- Shop entries -------------------------------------------

struct ShopEntry_Machine : ShopEntry {
    std::string text;
    Money _cost;
    Machine* (*construct)();
    Texture tex;

    ShopEntry_Machine(const char* name, const char* tagline, Money cost, Machine* (*construct)(), Texture tex) {
        this->text = std::format("{} - Machine", name);
        this->tagline = tagline;
        this->name = this->text.c_str();
        this->_cost = cost;
        this->construct = construct;
        this->tex = tex;
    }

    virtual Money cost() override {
        return this->_cost;
    }

    virtual void draw_icon(int x, int y) override {
        DrawTextureEx(tex, {float(x),float(y)}, 0, 0.73, WHITE);
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

struct ShopEntry_Upgrade : ShopEntry {
    UpgradeType type;
    Money _cost;

    ShopEntry_Upgrade(UpgradeType type) {
        this->type = type;
        this->_cost = 200;

        switch (this->type) {
            case UpgradeType::Speed: {
                this->name = "SPEED UPGRADE";
                this->tagline = "Increase a Machine's Speed";

                break;
            }
            case UpgradeType::Auto_Click: {
                this->name = "AUTO SPIN UPGRADE";
                this->tagline = "Increase a Machine's Auto Spin Rate";
                break;
            }
            case UpgradeType::Double_Stake: {
                this->name = "STAKE DOUBLE UPGRADE";
                this->tagline = "Double a Machine's Stake (and thus wins!)";
                break;
            }
        }

        this->tagline = tagline;
    }

    virtual Money cost() override {
        return this->_cost;
    }

    virtual void buy() override {
        gain_money(-_cost, mouse);
        select_machine = true;
        select_machine_text = this->name;
        select_machine_callback = [](Machine* machine) {
            apply_upgrade(machine, current_upgarde_type);
        };
        current_upgarde_type = this->type;
        _cost *= UPGRADE_COST_INCREASE_FACTOR;
    }

    virtual const char* lock_reason() override {
        for (int i = 0; i < 9; i++)
            if (machines[i])
                return nullptr;
        return "Buy some machines first";
    }

    virtual void draw_icon(int x, int y) override {
        DrawRectangle(x, y, 130, 180, GOLD);
    };
};

// --- Send it ------------------------------------------------

int main() {
    SetConfigFlags(/*FLAG_VSYNC_HINT  | */ FLAG_WINDOW_RESIZABLE);
    InitWindow(screen_width, screen_height, GAME_NAME);
    InitAudioDevice();
    SetTargetFPS(60);

    // --- Load Assets --------------------------------------------

    tex_background = LoadTexture("assets/background.png");
    tex_m1x1 = LoadTexture("assets/m1x1.png");
    tex_m3x1 = LoadTexture("assets/m3x1.png");
    tex_mb5 = LoadTexture("assets/mb5.png");

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

    snd_upgrade = LoadSound("assets/upgrade.wav");
    snd_win[0] = LoadSound("assets/win1.wav");
    snd_win[1] = LoadSound("assets/win2.wav");

    for (int i = 0; i < 48; i++) {
        snd_hat[i] = LoadSound("assets/hat.wav");
        SetSoundPitch(snd_hat[i], GetRandomValue(0, 100) / 100.0f * 0.1 + 0.9f);
        SetSoundVolume(snd_hat[i], GetRandomValue(0, 100) / 100.0f * 0.3 + 0.3f);
    }
    snd_reelstop = LoadSound("assets/reelstop.wav");
    msc_police = LoadMusicStream("assets/police.wav");
    msc_police.looping = true;
    SetMusicVolume(msc_police, 0.3);

    msc_anticipation = LoadMusicStream("assets/anticipation.wav");
    msc_anticipation.looping = true;
    SetMusicVolume(msc_anticipation, 0.3);

    // --- Init gameplay ------------------------------------------

    ShopEntry* shop_entry_m1x1 = new ShopEntry_Machine(
        "1X1",
        "Baby's first slot machine. Low Volatility",
        500,
        []() -> Machine* { return new M1X1(); },
        tex_m1x1
    );

    ShopEntry* shop_entry_m3x1 = new ShopEntry_Machine(
        "3X1",
        "Match 3 to win. Medium Volatility",
        500,
        []() -> Machine* { return new M3X1(); },
        tex_m3x1
    );

    ShopEntry* shop_entry_mb5 = new ShopEntry_Machine(
        "BLOODY 5",
        "Get 5 of a kind to win. Medium Volatility",
        1000,
        []() -> Machine* { return new MB5(); },
        tex_mb5
    );

    // --- Init shop ----------------------------------------------

    ShopEntry* shop_entry_upgrade_speed = new ShopEntry_Upgrade(UpgradeType::Speed);
    ShopEntry* shop_entry_upgrade_auto_click = new ShopEntry_Upgrade(UpgradeType::Auto_Click);
    ShopEntry* shop_entry_upgrade_double_stake = new ShopEntry_Upgrade(UpgradeType::Double_Stake);

    shop_machines_weights.add(shop_entry_m1x1, 8);
    shop_machines_weights.add(shop_entry_m3x1, 4);
    shop_machines_weights.add(shop_entry_mb5, 3);

    shop_upgrades_weights.add(shop_entry_upgrade_auto_click, 1);
    shop_upgrades_weights.add(shop_entry_upgrade_double_stake, 1);
    shop_upgrades_weights.add(shop_entry_upgrade_speed, 1);

    shop_types_weights.add(ShopEntryType::Machine, 10);
    shop_types_weights.add(ShopEntryType::Upgrade, 3);

    roll_shop();

    // --- Init taxes ---------------------------------------------
    Timer_Tax* tax_car = new Timer_Tax("Car Payment", 199, 500);
    Timer_Tax* tax_rent = new Timer_Tax("Rent", 299, 1000);
    timers.push_back(tax_car);
    timers.push_back(tax_rent);

    display_money = money;

    run_start_time = GetTime();

    while (!WindowShouldClose()) {

        camera = {
        };

        if (police_timer) UpdateMusicStream(msc_police);
        if (msc_anticipation_count > 0) UpdateMusicStream(msc_anticipation);

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

        // --- Simulate timers ----------------------------------------

        for (int i = 0; i < timers.size(); i++) {
            Timer* timer = timers[i];
            timer->time_left -= dt;

            if (timer->time_left < 0) {
                if (!timer->action()) {
                    delete timer;
                    timers[i] = timers.back();
                    timers.pop_back();
                    i--;
                }
            }
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

                        Rectangle r = { (float)x-8, (float)y-8, MACHINE_WIDTH+16, MACHINE_HEIGHT+16 };
                        if (select_machine && CheckCollisionPointRec(mouse, r)) {
                            DrawRectangleLinesEx(r, 4, Color{0,255,0,255});
                            if (IsMouseButtonPressed(0)) {
                                select_machine_callback(machine);
                            }
                        }
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
                            snprintf(buf, _y, "Price: $%ld", spot_prices[i]);
                            DrawText(buf, x + 10, y + 90, 20,  enabled ? GREEN : RED);
                            _y += 30;

                            if (button({
                                .rect = Rectangle{float(x + 8), float(_y), MACHINE_WIDTH - 16, y + MACHINE_HEIGHT - _y - 8 },
                                .text = "BUY",
                                .enabled = enabled,
                            }) && !select_machine) {
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


                for (int i = 0; i < shop_entries.size(); i++) {
                    const int height = 200;
                    DrawRectangle(x_start, y, x_end - x_start, height, BLACK);

                    ShopEntry* entry = shop_entries[i];
                    if (!entry) {
                        y += height + 10;
                        continue;
                    }

                    Money cost = entry->cost();
                    bool can_afford = money >= cost;
                    const char* lock_reason = entry->lock_reason();



                    entry->draw_icon(x_start + 10, y + 10);
                    DrawText(entry->name, x_start + 150, y + 4, 20, WHITE);
                    DrawText(entry->tagline, x_start + 150, y + 24, 20, WHITE);

                    char buf[64] = {};
                    snprintf(buf, 64, "$%ld", cost);

                    int len = MeasureText(buf, 40);
                    DrawText(buf, x_end - 130 - 8 - len, y + 10 + 134, 40, can_afford ? WHITE : RED);

                    Rectangle button_rect = { x_end - 130, y + 6 + 134, 122, 47 };
                    if (button({
                        .rect         = button_rect,
                        .text         = "BUY",
                        .font_size    = 40,
                        .enabled      = can_afford && !entry->lock_reason(),
                    })) {
                        go_to_screen(GameScreen::Machines);
                        entry->buy();
                        shop_entries[i] = nullptr;
                    }

                    if (CheckCollisionPointRec(mouse, button_rect)) {
                        if (lock_reason)
                            tooltip = lock_reason;
                        else if (!can_afford)
                            tooltip = "Can't afford.";
                    }

                    y += height + 10;
                }

                char buf[256];
                snprintf(buf, sizeof(buf), "REROLL - $%ld", roll_cost);

                Rectangle button_rect = { x_start, y, 400, 50 };
                if (button({
                    .rect         = button_rect,
                    .text         = buf,
                    .background   = DARKGREEN,
                    .font_size    = 40,
                    .enabled      = money >= roll_cost,
                })) {
                    roll_cost *= ROLL_COST_INCREASE_FACTOR;
                    gain_money(-roll_cost, mouse);
                    roll_shop();
                }
            }
        }


        // --- Draw money ------------------------------------------

        char buf[64];
        display_money = Lerp(display_money, double(money), 10 * dt);
        snprintf(buf, 64, "%ld", i64(roundf(display_money)));
        int _y = 154;
        DrawText(buf, 714, _y, 40, WHITE);
        _y += 50;

        // --- Draw time spent solvent -----------------------------

        double time_solvent = game_time - run_start_time;
        snprintf(buf, 64, "%d:%.2d", int(time_solvent / 60), int(floor(time_solvent)) % 60);
        DrawText("Time spent Solvent", 700, _y, 20, WHITE);
        DrawText(buf, 900, _y, 40, WHITE);
        _y += 30;

        // --- Draw shop button -----------------------------

        if (button({
            .rect         = { 700, float(_y), 150, 45 },
            .text         = "SHOP",
            .background   = BLUE,
            .text_color   = WHITE,
            .font_size    = 40,
        })) {
            go_to_screen(screen == GameScreen::Shop ? GameScreen::Machines : GameScreen::Shop);
        }

        // --- Draw timers ----------------------------------

        std::sort(timers.begin(), timers.end(), [](const Timer* a, const Timer* b) { return a->time_left < b->time_left; });

        _y += 50;
        for (int i = 0; i < timers.size() && i < 5; i++) {
            Timer* timer = timers[i];

            DrawRectangle(652, _y, 1024 - 650 - 5, 52, BLACK); 

            snprintf(buf, sizeof(buf), "%s", timer->text);
            DrawText(buf, 660, _y, 20, WHITE);
            _y += 25;


            snprintf(buf, sizeof(buf), "%d:%.2d", int(timer->time_left / 60), int(floor(timer->time_left)) % 60);
            int len = MeasureText(buf, 30);
            DrawText(buf, 1024 - len - 10, _y, 30, WHITE);
            // DrawText(buf, 660, _y, 30, WHITE);

            if (timer->cost) {
                snprintf(buf, sizeof(buf), "$%ld", timer->cost);
                DrawText(buf, 660, _y, 30, WHITE);
            }


            _y += 34;
        }

        // --- Draw texts on screen -------------------------

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

        if (select_machine) {
            DrawRectangle(600, 0, 1024, 100, BLACK);
            DrawText("SELECT MACHINE", 610, 10, 40, WHITE);
            DrawText(select_machine_text, 610, 50, 20, WHITE);
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

        if (has_illegal_machines && police_timer) {
            double t = (game_time * 4) - int(game_time * 4);
            
            int py = 30;
            DrawRectangle(0, py, 1024, 100, Color{0,0,0,200});
            DrawText("ILLEGAL MACHINES", 100, py, 60, BLACK);
            DrawText("ILLEGAL MACHINES", 104, py, 60, t < 0.5 ? RED : BLUE);
            DrawText("ILLEGAL MACHINES", 108, py, 60, t > 0.5 ? RED : BLUE);

            char buf[64] = {};
            snprintf(buf, 64, "POLICE INCOMING IN %d:%.2d", int(police_timer->time_left / 60), int(floor(police_timer->time_left)) % 60);
            DrawText(buf, 100,  py + 60, 40, t < 0.5 ? RED : BLUE);
            DrawText(buf, 104,  py + 60, 40, t < 0.5 ? BLUE : RED);
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

