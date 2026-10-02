typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;

#define REG_DISPCNT (*(volatile u16 *)0x04000000)
#define REG_DISPSTAT (*(volatile u16 *)0x04000004)
#define REG_KEYINPUT (*(volatile u16 *)0x04000130)
#define BG_PALETTE ((volatile u16 *)0x05000000)
#define SCREEN0 ((volatile u16 *)0x06000000)
#define KEY_A 0x0001
#define KEY_B 0x0002
#define KEY_SELECT 0x0004
#define KEY_START 0x0008
#define KEY_RIGHT 0x0010
#define KEY_LEFT 0x0020
#define KEY_UP 0x0040
#define KEY_DOWN 0x0080
#define KEY_R 0x0100
#define KEY_L 0x0200
#define SCREEN_W 240
#define SCREEN_H 160
#define MAX_ENEMIES 16
#define MAX_SHOTS 12
#define MAX_HOSTILE 8

extern u8 __bss_start__;
extern u8 __bss_end__;

__attribute__((used, naked, noreturn, section(".text.startup")))
void _start(void) {
    __asm__ volatile(
        "ldr sp, =0x03007F00\n"
        "ldr r0, =__bss_start__\n"
        "ldr r1, =__bss_end__\n"
        "mov r2, #0\n"
        "1: cmp r0, r1\n"
        "strlo r2, [r0], #4\n"
        "blo 1b\n"
        "bl game_main\n"
        "2: b 2b\n"
    );
}

typedef struct { s32 x, y; u16 live; u16 type; } Enemy;
typedef struct { s32 x, y; s32 vx, vy; u16 live; } Shot;

static Enemy enemies[MAX_ENEMIES];
static Shot shots[MAX_SHOTS];
static Shot hostile[MAX_HOSTILE];
static const u16 visor_colors[5] = { 0x5fda, 0x7f15, 0x5d9f, 0x3fff, 0x7c1f };
static volatile u16 *screen;
static u16 frame_count;
static u16 keys_previous;
static u16 color_choice;
static u16 ear_choice;
static u16 face_choice;
static s32 player_x;
static s32 player_y;
static s32 aim_x;
static s32 aim_y;
static u16 wave;
static u16 wave_size;
static u16 spawned;
static u16 wave_kills;
static u16 score;
static u16 total_kills;
static u16 health;
static u16 fire_cooldown;
static u16 hit_cooldown;
static u16 spawn_clock;
static u16 intermission;
static u16 menu_open;
static u16 game_started;
static u16 game_over;
static u16 enemy_tick;
static u16 hostile_clock;
static u16 hostile_live;
static char score_text[5];
static const u16 score_places[4] = { 1000, 100, 10, 1 };
static const char wave_text[10][3] = { "01", "02", "03", "04", "05", "06", "07", "08", "09", "10" };
static void pixel(s32 x, s32 y, u16 color);
static void rect(s32 x, s32 y, s32 w, s32 h, u16 color);

static const u8 font[43][5] = {
    {2,5,7,5,5}, {6,5,6,5,6}, {3,4,4,4,3}, {6,5,5,5,6}, {7,4,6,4,7},
    {7,4,6,4,4}, {3,4,5,5,3}, {5,5,7,5,5}, {7,2,2,2,7}, {1,1,1,5,2},
    {5,5,6,5,5}, {4,4,4,4,7}, {5,7,7,5,5}, {5,7,7,7,5}, {2,5,5,5,2},
    {6,5,6,4,4}, {2,5,5,3,1}, {6,5,6,5,5}, {3,4,2,1,6}, {7,2,2,2,2},
    {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5}, {5,5,2,2,2},
    {7,1,2,4,7},
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,3,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,2,2,2}, {7,5,7,5,7}, {7,5,7,1,7},
    {0,2,0,2,0}, {0,0,0,0,2}, {0,0,7,0,0}, {1,1,2,4,4}, {2,2,2,0,2},
    {5,2,2,0,2}, {7,1,2,0,2}
};

static u16 rgb(u16 r, u16 g, u16 b) { return (u16)(r | (g << 5) | (b << 10)); }

static void set_palette(void) {
    BG_PALETTE[0] = rgb(2, 4, 5);
    BG_PALETTE[1] = rgb(3, 7, 8);
    BG_PALETTE[2] = rgb(5, 12, 12);
    BG_PALETTE[3] = rgb(10, 15, 13);
    BG_PALETTE[4] = rgb(24, 29, 24);
    BG_PALETTE[5] = rgb(7, 11, 10);
    BG_PALETTE[6] = rgb(31, 14, 11);
    BG_PALETTE[7] = rgb(31, 26, 13);
    BG_PALETTE[8] = rgb(11, 21, 15);
    BG_PALETTE[9] = rgb(14, 24, 19);
    BG_PALETTE[10] = rgb(19, 29, 23);
    BG_PALETTE[11] = rgb(16, 20, 18);
    BG_PALETTE[12] = rgb(28, 11, 9);
    BG_PALETTE[13] = rgb(20, 27, 20);
    BG_PALETTE[14] = rgb(7, 16, 13);
    BG_PALETTE[15] = rgb(31, 31, 28);
    BG_PALETTE[16] = visor_colors[color_choice];
    BG_PALETTE[17] = rgb(21, 27, 23);
    BG_PALETTE[18] = rgb(7, 13, 12);
}

static void wait_vblank(void) {
    while (REG_DISPSTAT & 1) { }
    while (!(REG_DISPSTAT & 1)) { }
}

static void start_frame(void) {
    screen = SCREEN0;
    volatile u32 *words = (volatile u32 *)screen;
    u16 background = BG_PALETTE[1];
    u32 fill = (u32)background | ((u32)background << 16);
    for (u32 i = 0; i < 9600; i++) words[i] = fill;
    for (s32 x = 0; x < SCREEN_W; x += 20)
        for (s32 y = 0; y < SCREEN_H; y++) pixel(x, y, 2);
    for (s32 y = 0; y < SCREEN_H; y += 20)
        rect(0, y, SCREEN_W, 1, 2);
    for (s32 i = 0; i < 12; i++) {
        s32 x = (i * 73 + 19) % SCREEN_W;
        s32 y = (i * 41 + 17) % SCREEN_H;
        pixel(x, y, 3);
    }
}

static void finish_frame(void) {
    wait_vblank();
    REG_DISPCNT = 0x0403;
}

static void pixel(s32 x, s32 y, u16 color) {
    if ((u32)x < SCREEN_W && (u32)y < SCREEN_H)
        screen[(u32)y * SCREEN_W + (u32)x] = BG_PALETTE[color];
}

static void rect(s32 x, s32 y, s32 w, s32 h, u16 color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;
    u16 fill_color = BG_PALETTE[color];
    for (s32 row = y; row < y + h; row++) {
        volatile u16 *dest = &screen[row * SCREEN_W + x];
        for (s32 col = 0; col < w; col++) *dest++ = fill_color;
    }
}

static void line(s32 x0, s32 y0, s32 x1, s32 y1, u16 color) {
    s32 dx = x1 - x0; if (dx < 0) dx = -dx;
    s32 sx = x0 < x1 ? 1 : -1;
    s32 dy = y1 - y0; if (dy < 0) dy = -dy;
    s32 sy = y0 < y1 ? 1 : -1;
    s32 err = dx - dy;
    for (;;) {
        pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        s32 twice = err << 1;
        if (twice > -dy) { err -= dy; x0 += sx; }
        if (twice < dx) { err += dx; y0 += sy; }
    }
}

static void circle(s32 cx, s32 cy, s32 radius, u16 color) {
    for (s32 y = -radius; y <= radius; y++)
        for (s32 x = -radius; x <= radius; x++)
            if (x * x + y * y <= radius * radius) pixel(cx + x, cy + y, color);
}

static u16 glyph_index(char c) {
    if (c >= 'A' && c <= 'Z') return (u16)(c - 'A');
    if (c >= '0' && c <= '9') return (u16)(26 + c - '0');
    if (c == ':') return 36;
    if (c == '.') return 37;
    if (c == '-') return 38;
    if (c == '/') return 39;
    if (c == '!') return 40;
    if (c == '%') return 41;
    if (c == '?') return 42;
    return 0xffff;
}

static s32 text_width(const char *text, s32 scale) {
    s32 length = 0;
    while (text[length]) length++;
    return length ? length * 4 * scale - scale : 0;
}

static void text(s32 x, s32 y, const char *value, u16 color, s32 scale) {
    for (s32 i = 0; value[i]; i++) {
        u16 index = glyph_index(value[i]);
        if (index < 43)
            for (s32 row = 0; row < 5; row++)
                for (s32 col = 0; col < 3; col++)
                    if (font[index][row] & (4 >> col)) rect(x + (i * 4 + col) * scale, y + row * scale, scale, scale, color);
    }
}

static s32 text_center(const char *value, s32 scale) {
    return (SCREEN_W - text_width(value, scale)) >> 1;
}

static void draw_player(s32 x, s32 y, u16 blink) {
    if (blink && (frame_count & 4)) return;
    if (ear_choice == 0) {
        line(x - 6, y - 4, x - 10, y - 10, 16);
        line(x - 10, y - 10, x - 7, y - 4, 16);
        line(x + 6, y - 4, x + 10, y - 10, 16);
        line(x + 10, y - 10, x + 7, y - 4, 16);
    } else if (ear_choice == 1) {
        line(x - 4, y - 6, x - 5, y - 13, 16);
        line(x + 4, y - 6, x + 5, y - 13, 16);
        circle(x - 5, y - 14, 1, 16);
        circle(x + 5, y - 14, 1, 16);
    } else {
        line(x - 6, y - 4, x - 8, y - 12, 16);
        line(x - 8, y - 12, x - 3, y - 6, 16);
        line(x + 6, y - 4, x + 8, y - 12, 16);
        line(x + 8, y - 12, x + 3, y - 6, 16);
    }
    rect(x - 8, y - 5, 16, 12, 17);
    rect(x - 6, y - 4, 12, 8, 16);
    rect(x - 5, y - 2, 10, 5, 18);
    if (face_choice == 0) {
        rect(x - 4, y - 1, 2, 1, 16);
        rect(x + 2, y - 1, 2, 1, 16);
        pixel(x, y + 2, 16);
    } else if (face_choice == 1) {
        pixel(x - 4, y - 1, 16);
        pixel(x - 3, y, 16);
        pixel(x + 3, y, 16);
        pixel(x + 4, y - 1, 16);
        rect(x - 2, y + 2, 4, 1, 16);
    } else {
        rect(x - 4, y - 1, 2, 2, 16);
        rect(x + 2, y - 1, 2, 2, 16);
        rect(x - 2, y + 2, 4, 1, 16);
    }
    rect(x - 5, y + 7, 10, 2, 17);
    pixel(x, y + 10, 16);
}

static void draw_enemy(const Enemy *enemy) {
    u16 shell = enemy->type == 0 ? 6 : enemy->type == 1 ? 7 : 12;
    circle(enemy->x, enemy->y, enemy->type == 2 ? 6 : 5, shell);
    rect(enemy->x - 3, enemy->y - 2, 2, 2, 15);
    rect(enemy->x + 2, enemy->y - 2, 2, 2, 15);
    rect(enemy->x - 2, enemy->y + 2, 4, 1, 11);
    pixel(enemy->x, enemy->y, shell);
}

static void draw_hud(void) {
    rect(0, 0, SCREEN_W, 14, 5);
    rect(0, 14, SCREEN_W, 1, 17);
    text(5, 5, "WAVE", 3, 1);
    text(25, 5, wave_text[wave < 10 ? wave - 1 : 9], 15, 1);
    text(69, 5, "SCORE", 3, 1);
    text(95, 5, score_text, 15, 1);
    text(163, 5, "CORE", 3, 1);
    rect(184, 6, 45, 4, 18);
    rect(184, 6, (45 * health) / 100, 4, health < 30 ? 6 : 16);
}

static void update_score_text(void) {
    u16 remaining = score;
    for (s32 i = 0; i < 4; i++) {
        u16 digit = 0;
        while (remaining >= score_places[i]) { remaining -= score_places[i]; digit++; }
        score_text[i] = (char)('0' + digit);
    }
    score_text[4] = 0;
}

static void spawn_enemy(void) {
    for (s32 i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].live) continue;
        u16 edge = (u16)((spawned + wave) & 3);
        enemies[i].x = edge == 0 ? 5 : edge == 1 ? 234 : (s32)(26 + ((spawned * 47 + wave * 19) % 188));
        enemies[i].y = edge == 2 ? 19 : edge == 3 ? 154 : (s32)(24 + ((spawned * 31 + wave * 13) % 126));
        enemies[i].type = (u16)((spawned + wave) % 3);
        enemies[i].live = 1;
        spawned++;
        return;
    }
}

static s32 abs_s32(s32 v) { return v < 0 ? -v : v; }

static void fire_shot(void) {
    s32 target = -1;
    s32 best = 0x7fffffff;
    for (s32 i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].live) continue;
        s32 dx = enemies[i].x - player_x;
        s32 dy = enemies[i].y - player_y;
        s32 distance = dx * dx + dy * dy;
        if (distance < best) { best = distance; target = i; }
    }
    s32 dx = aim_x;
    s32 dy = aim_y;
    if (target >= 0) {
        dx = enemies[target].x - player_x;
        dy = enemies[target].y - player_y;
    }
    s32 vx = 0, vy = 0;
    if (abs_s32(dx) >= abs_s32(dy)) vx = dx < 0 ? -1 : 1;
    else vy = dy < 0 ? -1 : 1;
    for (s32 i = 0; i < MAX_SHOTS; i++) {
        if (shots[i].live) continue;
        shots[i].x = player_x + vx * 9;
        shots[i].y = player_y + vy * 9;
        shots[i].vx = vx * 5;
        shots[i].vy = vy * 5;
        shots[i].live = 1;
        return;
    }
}

static void fire_hostile(s32 x, s32 y) {
    s32 dx = player_x - x;
    s32 dy = player_y - y;
    s32 vx = 0, vy = 0;
    if (abs_s32(dx) >= abs_s32(dy)) vx = dx < 0 ? -1 : 1;
    else vy = dy < 0 ? -1 : 1;
    for (s32 i = 0; i < MAX_HOSTILE; i++) {
        if (hostile[i].live) continue;
        hostile[i].x = x;
        hostile[i].y = y;
        hostile[i].vx = vx * 2;
        hostile[i].vy = vy * 2;
        hostile[i].live = 1;
        hostile_live++;
        return;
    }
}

static void reset_game(void) {
    for (s32 i = 0; i < MAX_ENEMIES; i++) enemies[i].live = 0;
    for (s32 i = 0; i < MAX_SHOTS; i++) shots[i].live = 0;
    for (s32 i = 0; i < MAX_HOSTILE; i++) hostile[i].live = 0;
    player_x = 120; player_y = 83;
    aim_x = 1; aim_y = 0;
    wave = 1; wave_size = 5; spawned = 0; wave_kills = 0;
    score = 0; total_kills = 0; health = 100;
    fire_cooldown = 0; hit_cooldown = 0; spawn_clock = 24;
    intermission = 0; hostile_clock = 90; hostile_live = 0;
    game_over = 0; game_started = 1; menu_open = 0;
}

static void update_game(u16 keys, u16 pressed) {
    if (menu_open) {
        if (pressed & KEY_LEFT) color_choice = (u16)((color_choice + 4) % 5);
        if (pressed & KEY_RIGHT) color_choice = (u16)((color_choice + 1) % 5);
        if (pressed & KEY_L) ear_choice = (u16)((ear_choice + 2) % 3);
        if (pressed & KEY_R) ear_choice = (u16)((ear_choice + 1) % 3);
        if (pressed & KEY_A) face_choice = (u16)((face_choice + 1) % 3);
        if ((pressed & KEY_SELECT) || ((pressed & KEY_START) && game_started)) menu_open = 0;
        if ((pressed & KEY_START) && !game_started) reset_game();
        set_palette();
        return;
    }
    if (game_over) {
        if (pressed & KEY_START) reset_game();
        if (pressed & KEY_SELECT) menu_open = 1;
        return;
    }
    if (pressed & KEY_SELECT) { menu_open = 1; return; }
    if (!game_started) {
        if (pressed & KEY_LEFT) color_choice = (u16)((color_choice + 4) % 5);
        if (pressed & KEY_RIGHT) color_choice = (u16)((color_choice + 1) % 5);
        if (pressed & KEY_L) ear_choice = (u16)((ear_choice + 2) % 3);
        if (pressed & KEY_R) ear_choice = (u16)((ear_choice + 1) % 3);
        if (pressed & KEY_A) face_choice = (u16)((face_choice + 1) % 3);
        if (pressed & KEY_START) reset_game();
        set_palette();
        return;
    }

    if (keys & KEY_LEFT) { player_x -= 2; aim_x = -1; aim_y = 0; }
    if (keys & KEY_RIGHT) { player_x += 2; aim_x = 1; aim_y = 0; }
    if (keys & KEY_UP) { player_y -= 2; aim_x = 0; aim_y = -1; }
    if (keys & KEY_DOWN) { player_y += 2; aim_x = 0; aim_y = 1; }
    if (player_x < 12) player_x = 12;
    if (player_x > SCREEN_W - 12) player_x = SCREEN_W - 12;
    if (player_y < 25) player_y = 25;
    if (player_y > SCREEN_H - 12) player_y = SCREEN_H - 12;

    if (fire_cooldown) fire_cooldown--;
    if ((keys & (KEY_A | KEY_B)) && fire_cooldown == 0) {
        fire_shot();
        fire_cooldown = 9;
    }
    if (spawned < wave_size) {
        if (spawn_clock) spawn_clock--;
        else {
            spawn_enemy();
            spawn_clock = wave > 6 ? 20 : 31;
        }
    }
    if (intermission) intermission--;
    enemy_tick++;
    for (s32 i = 0; i < MAX_ENEMIES; i++) {
        Enemy *enemy = &enemies[i];
        if (!enemy->live) continue;
        s32 dx = player_x - enemy->x;
        s32 dy = player_y - enemy->y;
        if ((enemy_tick & 1) == 0) {
            if (abs_s32(dx) >= abs_s32(dy)) enemy->x += dx > 0 ? 1 : dx < 0 ? -1 : 0;
            else enemy->y += dy > 0 ? 1 : dy < 0 ? -1 : 0;
        }
        if (enemy->type == 2 && wave >= 2) {
            if (hostile_clock) hostile_clock--;
            else { fire_hostile(enemy->x, enemy->y); hostile_clock = 112; }
        }
        dx = enemy->x - player_x; dy = enemy->y - player_y;
        if (dx * dx + dy * dy < 110 && hit_cooldown == 0) {
            health = health > 17 ? (u16)(health - 17) : 0;
            hit_cooldown = 40;
        }
    }
    if (hit_cooldown) hit_cooldown--;
    for (s32 i = 0; i < MAX_SHOTS; i++) {
        if (!shots[i].live) continue;
        shots[i].x += shots[i].vx; shots[i].y += shots[i].vy;
        if (shots[i].x < 0 || shots[i].x >= SCREEN_W || shots[i].y < 15 || shots[i].y >= SCREEN_H) {
            shots[i].live = 0; continue;
        }
        for (s32 j = 0; j < MAX_ENEMIES; j++) {
            if (!enemies[j].live) continue;
            s32 dx = shots[i].x - enemies[j].x;
            s32 dy = shots[i].y - enemies[j].y;
            if (dx * dx + dy * dy < 35) {
                enemies[j].live = 0; shots[i].live = 0;
                wave_kills++; total_kills++;
                score = (u16)(score + 100 + wave * 10);
                if (score > 9999) score = 9999;
                update_score_text();
                break;
            }
        }
    }
    for (s32 i = 0; i < MAX_HOSTILE; i++) {
        if (!hostile[i].live) continue;
        hostile[i].x += hostile[i].vx; hostile[i].y += hostile[i].vy;
        if (hostile[i].x < 0 || hostile[i].x >= SCREEN_W || hostile[i].y < 15 || hostile[i].y >= SCREEN_H) {
            hostile[i].live = 0; hostile_live--; continue;
        }
        s32 dx = hostile[i].x - player_x, dy = hostile[i].y - player_y;
        if (dx * dx + dy * dy < 90 && hit_cooldown == 0) {
            health = health > 10 ? (u16)(health - 10) : 0;
            hit_cooldown = 40;
            hostile[i].live = 0; hostile_live--;
        }
    }
    if (health == 0) { game_over = 1; return; }
    if (wave_kills >= wave_size && spawned >= wave_size) {
        s32 remaining = 0;
        for (s32 i = 0; i < MAX_ENEMIES; i++) remaining += enemies[i].live;
        if (remaining == 0 && intermission == 0) {
            intermission = 80;
            wave++;
            wave_size = (u16)(wave_size + 2);
            if (wave_size > 14) wave_size = 14;
            spawned = 0; wave_kills = 0; spawn_clock = 80;
            health = health < 90 ? (u16)(health + 10) : 100;
        }
    }
}

static void draw_customizer(void) {
    rect(25, 24, 190, 115, 5);
    rect(26, 25, 188, 1, 17);
    text(40, 34, "PROTOGEN LAB", 16, 2);
    text(40, 49, "LEFT RIGHT: COLOR", 15, 1);
    text(40, 59, "L R: EAR MODULES", 15, 1);
    text(40, 69, "A: VISOR DISPLAY", 15, 1);
    text(40, 84, "VISOR LIGHT", 3, 1);
    for (s32 i = 0; i < 5; i++) rect(40 + i * 17, 94, 10, 4, visor_colors[i]);
    rect(39 + color_choice * 17, 92, 12, 8, 15);
    rect(40 + color_choice * 17, 93, 10, 6, visor_colors[color_choice]);
    text(40, 111, "START: DEPLOY", 3, 1);
    text(40, 125, "SELECT: CLOSE", 3, 1);
    draw_player(184, 78, 0);
}

static void draw_title(void) {
    rect(0, 0, SCREEN_W, SCREEN_H, 5);
    for (s32 y = 8; y < SCREEN_H; y += 8) rect(0, y, SCREEN_W, 1, 2);
    text(text_center("NEON FRONTIER", 3), 30, "NEON FRONTIER", 16, 3);
    text(text_center("PROTOGEN DEFENSE", 1), 53, "PROTOGEN DEFENSE", 15, 1);
    draw_player(120, 90, 0);
    text(34, 112, "LEFT RIGHT: COLOR", 3, 1);
    text(34, 122, "L R: EARS   A: FACE", 3, 1);
    text(text_center("START: DEPLOY", 1), 141, "START: DEPLOY", 16, 1);
}

static void draw_game(void) {
    start_frame();
    if (!game_started) { draw_title(); finish_frame(); return; }
    if (game_started) draw_hud();
    for (s32 i = 0; i < MAX_SHOTS; i++)
        if (shots[i].live) { circle(shots[i].x, shots[i].y, 2, 16); pixel(shots[i].x, shots[i].y, 15); }
    for (s32 i = 0; i < MAX_HOSTILE; i++)
        if (hostile[i].live) { circle(hostile[i].x, hostile[i].y, 2, 6); pixel(hostile[i].x, hostile[i].y, 7); }
    for (s32 i = 0; i < MAX_ENEMIES; i++) if (enemies[i].live) draw_enemy(&enemies[i]);
    draw_player(player_x, player_y, hit_cooldown);
    if (intermission && !game_over) text(text_center("AREA CLEAR", 2), 70, "AREA CLEAR", 16, 2);
    if (menu_open) draw_customizer();
    if (game_over) {
        rect(25, 49, 190, 57, 5);
        text(text_center("SIGNAL LOST", 2), 60, "SIGNAL LOST", 6, 2);
        text(text_center("START: RETRY", 1), 88, "START: RETRY", 15, 1);
        text(text_center("SELECT: CUSTOMIZE", 1), 99, "SELECT: CUSTOMIZE", 3, 1);
    }
    finish_frame();
}

void game_main(void) {
    REG_DISPCNT = 0x0403;
    set_palette();
    player_x = 120; player_y = 90;
    aim_x = 1; aim_y = 0;
    for (;;) {
        u16 keys = (u16)(~REG_KEYINPUT) & 0x03ff;
        u16 pressed = (u16)(keys & ~keys_previous);
        keys_previous = keys;
        if (!game_started && (pressed & KEY_START)) reset_game();
        else update_game(keys, pressed);
        draw_game();
        frame_count++;
    }
}