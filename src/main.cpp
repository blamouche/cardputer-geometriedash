// =============================================================================
//  Geometry Dash  -  M5Stack Cardputer
// -----------------------------------------------------------------------------
//  An auto-runner: the cube moves forward on its own, you only control the jump.
//  Land cleanly, dodge the spikes, ride the blocks. One touch and you restart.
//
//  Controls
//    - Any key / SPACE / front button : jump  (hold to keep jumping)
//    - On the menu or game-over screen, any key starts / restarts
//
//  The build is a single translation unit on purpose - easy to flash and hack.
// =============================================================================

#include <M5Cardputer.h>
#include <Preferences.h>
#include <vector>
#include <cmath>

// --- screen / world geometry ------------------------------------------------
static constexpr int   SCREEN_W   = 240;
static constexpr int   SCREEN_H   = 135;
static constexpr int   GROUND_Y   = 112;          // y of the ground surface
static constexpr int   CUBE_SIZE  = 14;
static constexpr int   CUBE_X     = 46;           // cube stays at a fixed x

// --- physics ----------------------------------------------------------------
static constexpr float GRAVITY    = 0.62f;
static constexpr float JUMP_VEL   = -7.6f;
static constexpr float ROT_SPEED  = 8.0f;         // degrees per frame, in air
static constexpr float SPEED_MIN  = 2.6f;
static constexpr float SPEED_MAX  = 6.2f;

// --- obstacle sizing --------------------------------------------------------
static constexpr int   SPIKE_W    = 14;
static constexpr int   SPIKE_H    = 14;
static constexpr int   BLOCK_W    = 16;

// RGB565 helper so the palette stays readable.
static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// --- palette ----------------------------------------------------------------
static constexpr uint16_t COL_SKY_TOP  = rgb( 32,  20,  72);
static constexpr uint16_t COL_SKY_BOT  = rgb( 96,  40, 130);
static constexpr uint16_t COL_GRID     = rgb( 70,  46, 120);
static constexpr uint16_t COL_GROUND   = rgb( 24,  16,  44);
static constexpr uint16_t COL_GROUND_E = rgb(150, 230, 255);
static constexpr uint16_t COL_GROUND_G = rgb( 52,  38,  92);
static constexpr uint16_t COL_CUBE     = rgb(255, 206,  56);
static constexpr uint16_t COL_CUBE_HI  = rgb(255, 240, 170);
static constexpr uint16_t COL_CUBE_LN  = rgb(120,  70,   0);
static constexpr uint16_t COL_SPIKE    = rgb(255,  86,  86);
static constexpr uint16_t COL_SPIKE_LN = rgb(120,  20,  20);
static constexpr uint16_t COL_BLOCK    = rgb( 90, 200, 255);
static constexpr uint16_t COL_BLOCK_LN = rgb( 20,  80, 130);
static constexpr uint16_t COL_TEXT     = rgb(255, 255, 255);
static constexpr uint16_t COL_TEXT_DIM = rgb(170, 160, 200);

// ---------------------------------------------------------------------------

enum GameState { STATE_MENU, STATE_PLAYING, STATE_DEAD };

enum ObstacleType { OB_SPIKE, OB_BLOCK };

struct Obstacle {
    float x;
    int   type;
    int   w;
    int   h;        // height (blocks); spikes use SPIKE_H
};

struct Particle {
    float x, y, vx, vy;
    int   life;
};

// ---------------------------------------------------------------------------

M5Canvas   canvas(&M5Cardputer.Display);
Preferences prefs;

GameState  gameState = STATE_MENU;

// cube state
float  cubeY      = GROUND_Y - CUBE_SIZE;
float  cubeVY     = 0.0f;
float  cubeAngle  = 0.0f;
bool   onGround   = true;

// world state
float  scrollSpeed = SPEED_MIN;
double distance    = 0.0;          // travelled distance == score
int    bestScore   = 0;
int    attempts    = 0;
float  bgScroll    = 0.0f;

std::vector<Obstacle> obstacles;
std::vector<Particle> particles;

uint32_t deadAt   = 0;             // millis() the run ended (input lockout)
uint32_t lastFrame = 0;

// input edge tracking
bool prevKeyDown = false;

// ---------------------------------------------------------------------------
//  Input helpers
// ---------------------------------------------------------------------------

static bool jumpHeld() {
    return M5Cardputer.Keyboard.isPressed() || M5Cardputer.BtnA.isPressed();
}

// true on the frame a key/button transitions from up to down
static bool jumpPressedEdge() {
    bool down = jumpHeld();
    bool edge = down && !prevKeyDown;
    return edge;
}

// ---------------------------------------------------------------------------
//  World setup / spawning
// ---------------------------------------------------------------------------

static void resetRun() {
    cubeY       = GROUND_Y - CUBE_SIZE;
    cubeVY      = 0.0f;
    cubeAngle   = 0.0f;
    onGround    = true;
    scrollSpeed = SPEED_MIN;
    distance    = 0.0;
    obstacles.clear();
    particles.clear();
    // first obstacle gets a generous run-up
    float x = SCREEN_W + 40;
    for (int i = 0; i < 4; ++i) {
        Obstacle o;
        o.x = x;
        o.type = OB_SPIKE;
        o.w = SPIKE_W;
        o.h = SPIKE_H;
        obstacles.push_back(o);
        x += 90 + (esp_random() % 70);
    }
}

// rightmost edge currently occupied by an obstacle
static float lastObstacleEdge() {
    float edge = 0;
    for (auto& o : obstacles) {
        float e = o.x + o.w;
        if (e > edge) edge = e;
    }
    return edge;
}

// add one obstacle "pattern" just off the right edge of the screen
static void spawnPattern() {
    float gap   = 64 + (esp_random() % 70) + scrollSpeed * 6.0f;
    float baseX = lastObstacleEdge() + gap;
    int   roll  = esp_random() % 100;

    if (roll < 34) {
        // a row of 1-3 spikes (still clearable with a single jump)
        int n = 1 + (esp_random() % 3);
        for (int i = 0; i < n; ++i) {
            Obstacle o;
            o.x = baseX + i * SPIKE_W;
            o.type = OB_SPIKE;
            o.w = SPIKE_W;
            o.h = SPIKE_H;
            obstacles.push_back(o);
        }
    } else if (roll < 64) {
        // a single block, low or tall
        Obstacle o;
        o.x = baseX;
        o.type = OB_BLOCK;
        o.w = BLOCK_W;
        o.h = (esp_random() % 2) ? 16 : 30;
        obstacles.push_back(o);
    } else if (roll < 84) {
        // a wider platform you can ride across
        Obstacle o;
        o.x = baseX;
        o.type = OB_BLOCK;
        o.w = BLOCK_W * 2;
        o.h = 16;
        obstacles.push_back(o);
    } else {
        // block followed closely by a spike - jump on, then off
        Obstacle b;
        b.x = baseX;
        b.type = OB_BLOCK;
        b.w = BLOCK_W;
        b.h = 16;
        obstacles.push_back(b);
        Obstacle s;
        s.x = baseX + BLOCK_W + 26;
        s.type = OB_SPIKE;
        s.w = SPIKE_W;
        s.h = SPIKE_H;
        obstacles.push_back(s);
    }
}

// ---------------------------------------------------------------------------
//  Collision
// ---------------------------------------------------------------------------

static bool rangesOverlap(float a0, float a1, float b0, float b1) {
    return a0 < b1 && b0 < a1;
}

static void spawnDeathBurst() {
    float cx = CUBE_X + CUBE_SIZE * 0.5f;
    float cy = cubeY + CUBE_SIZE * 0.5f;
    for (int i = 0; i < 26; ++i) {
        Particle p;
        p.x = cx;
        p.y = cy;
        float ang = (esp_random() % 628) / 100.0f;
        float spd = 1.0f + (esp_random() % 100) / 28.0f;
        p.vx = cosf(ang) * spd;
        p.vy = sinf(ang) * spd - 1.0f;
        p.life = 22 + (esp_random() % 16);
        particles.push_back(p);
    }
}

static void killCube() {
    gameState = STATE_DEAD;
    deadAt    = millis();
    attempts++;
    int score = (int)(distance / 10.0);
    if (score > bestScore) {
        bestScore = score;
        prefs.putInt("best", bestScore);
    }
    spawnDeathBurst();
    // descending "you died" sound
    M5Cardputer.Speaker.tone(440, 70);
    delay(60);
    M5Cardputer.Speaker.tone(330, 70);
    delay(60);
    M5Cardputer.Speaker.tone(220, 130);
}

// ---------------------------------------------------------------------------
//  Per-frame update
// ---------------------------------------------------------------------------

static void updatePlaying() {
    // --- jump input (hold to auto-jump, just like the real game) ------------
    if (jumpHeld() && onGround) {
        cubeVY   = JUMP_VEL;
        onGround = false;
        M5Cardputer.Speaker.tone(720, 25);
    }

    float prevBottom = cubeY + CUBE_SIZE;

    // --- gravity ------------------------------------------------------------
    cubeVY += GRAVITY;
    cubeY  += cubeVY;

    // --- resolve vertical collisions: ground + tops of blocks ---------------
    onGround = false;
    float cubeL = CUBE_X;
    float cubeR = CUBE_X + CUBE_SIZE;

    if (cubeY + CUBE_SIZE >= GROUND_Y) {
        cubeY    = GROUND_Y - CUBE_SIZE;
        cubeVY   = 0.0f;
        onGround = true;
    }

    for (auto& o : obstacles) {
        if (o.type != OB_BLOCK) continue;
        float bTop = GROUND_Y - o.h;
        if (!rangesOverlap(cubeL, cubeR, o.x, o.x + o.w)) continue;
        // landing on top: was above the block, now at/below its surface
        if (cubeVY >= 0 && prevBottom <= bTop + 2 && cubeY + CUBE_SIZE >= bTop) {
            cubeY    = bTop - CUBE_SIZE;
            cubeVY   = 0.0f;
            onGround = true;
        }
    }

    // --- lethal collisions --------------------------------------------------
    // cube hitbox, slightly inset to feel fair
    float hx0 = CUBE_X + 2;
    float hx1 = CUBE_X + CUBE_SIZE - 2;
    float hy0 = cubeY + 2;
    float hy1 = cubeY + CUBE_SIZE - 2;

    for (auto& o : obstacles) {
        if (o.type == OB_SPIKE) {
            // forgiving box around the lower-centre of the spike
            float sx0 = o.x + 3;
            float sx1 = o.x + o.w - 3;
            float sy0 = GROUND_Y - o.h + 4;
            float sy1 = GROUND_Y;
            if (rangesOverlap(hx0, hx1, sx0, sx1) &&
                rangesOverlap(hy0, hy1, sy0, sy1)) {
                killCube();
                return;
            }
        } else { // OB_BLOCK - dying only when hitting it from the side
            float bTop = GROUND_Y - o.h;
            if (rangesOverlap(hx0, hx1, o.x, o.x + o.w) &&
                rangesOverlap(hy0, hy1, bTop + 3, (float)GROUND_Y)) {
                killCube();
                return;
            }
        }
    }

    // --- rotation: spin while airborne, snap to a clean face on landing -----
    if (!onGround) {
        cubeAngle += ROT_SPEED;
        if (cubeAngle >= 360.0f) cubeAngle -= 360.0f;
    } else {
        float snapped = roundf(cubeAngle / 90.0f) * 90.0f;
        cubeAngle = snapped;
    }

    // --- scroll the world ---------------------------------------------------
    for (auto& o : obstacles) o.x -= scrollSpeed;
    bgScroll -= scrollSpeed;
    if (bgScroll <= -24.0f) bgScroll += 24.0f;

    // drop obstacles that left the screen
    while (!obstacles.empty() && obstacles.front().x + obstacles.front().w < -4) {
        obstacles.erase(obstacles.begin());
    }
    // keep the track populated
    while (lastObstacleEdge() < SCREEN_W + 40) {
        spawnPattern();
    }

    // --- score & difficulty -------------------------------------------------
    distance += scrollSpeed;
    scrollSpeed = SPEED_MIN + (float)(distance / 2600.0);
    if (scrollSpeed > SPEED_MAX) scrollSpeed = SPEED_MAX;
}

static void updateParticles() {
    for (auto& p : particles) {
        p.vy += 0.35f;
        p.x  += p.vx;
        p.y  += p.vy;
        p.life--;
    }
    while (!particles.empty() && particles.front().life <= 0) {
        particles.erase(particles.begin());
    }
}

// ---------------------------------------------------------------------------
//  Rendering
// ---------------------------------------------------------------------------

static void drawBackground() {
    // vertical sky gradient
    for (int y = 0; y < GROUND_Y; ++y) {
        float t = (float)y / GROUND_Y;
        uint8_t r = 32 + (uint8_t)((96 - 32) * t);
        uint8_t g = 20 + (uint8_t)((40 - 20) * t);
        uint8_t b = 72 + (uint8_t)((130 - 72) * t);
        canvas.drawFastHLine(0, y, SCREEN_W, rgb(r, g, b));
    }
    // scrolling grid lines
    for (float x = bgScroll; x < SCREEN_W; x += 24.0f) {
        canvas.drawFastVLine((int)x, 0, GROUND_Y, COL_GRID);
    }
    for (int y = 16; y < GROUND_Y; y += 24) {
        canvas.drawFastHLine(0, y, SCREEN_W, COL_GRID);
    }
    // ground slab
    canvas.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, COL_GROUND);
    canvas.drawFastHLine(0, GROUND_Y, SCREEN_W, COL_GROUND_E);
    for (float x = bgScroll; x < SCREEN_W; x += 24.0f) {
        canvas.drawFastVLine((int)x, GROUND_Y + 1, SCREEN_H - GROUND_Y - 1,
                             COL_GROUND_G);
    }
}

static void drawObstacles() {
    for (auto& o : obstacles) {
        if (o.type == OB_SPIKE) {
            int x = (int)o.x;
            canvas.fillTriangle(x, GROUND_Y,
                                x + o.w, GROUND_Y,
                                x + o.w / 2, GROUND_Y - o.h, COL_SPIKE);
            canvas.drawTriangle(x, GROUND_Y,
                                x + o.w, GROUND_Y,
                                x + o.w / 2, GROUND_Y - o.h, COL_SPIKE_LN);
        } else {
            int x    = (int)o.x;
            int bTop = GROUND_Y - o.h;
            canvas.fillRect(x, bTop, o.w, o.h, COL_BLOCK);
            canvas.drawRect(x, bTop, o.w, o.h, COL_BLOCK_LN);
            canvas.drawFastHLine(x + 1, bTop + 1, o.w - 2, COL_TEXT);
        }
    }
}

// draw the cube as a rotated square (two triangles) plus an inner face detail
static void drawCube() {
    float r   = CUBE_SIZE * 0.5f;
    float ccx = CUBE_X + r;
    float ccy = cubeY + r;
    float a   = cubeAngle * DEG_TO_RAD;
    float s   = sinf(a);
    float c   = cosf(a);

    const float corners[4][2] = {
        {-r, -r}, {r, -r}, {r, r}, {-r, r}
    };
    float px[4], py[4];
    for (int i = 0; i < 4; ++i) {
        px[i] = ccx + corners[i][0] * c - corners[i][1] * s;
        py[i] = ccy + corners[i][0] * s + corners[i][1] * c;
    }
    canvas.fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], COL_CUBE);
    canvas.fillTriangle(px[0], py[0], px[2], py[2], px[3], py[3], COL_CUBE);

    // inner face: a smaller rotated square
    float ir = r * 0.45f;
    const float icorn[4][2] = {
        {-ir, -ir}, {ir, -ir}, {ir, ir}, {-ir, ir}
    };
    float ix[4], iy[4];
    for (int i = 0; i < 4; ++i) {
        ix[i] = ccx + icorn[i][0] * c - icorn[i][1] * s;
        iy[i] = ccy + icorn[i][0] * s + icorn[i][1] * c;
    }
    canvas.fillTriangle(ix[0], iy[0], ix[1], iy[1], ix[2], iy[2], COL_CUBE_HI);
    canvas.fillTriangle(ix[0], iy[0], ix[2], iy[2], ix[3], iy[3], COL_CUBE_HI);

    // outline
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) & 3;
        canvas.drawLine(px[i], py[i], px[j], py[j], COL_CUBE_LN);
    }
}

static void drawParticles() {
    for (auto& p : particles) {
        uint16_t col = (p.life > 10) ? COL_CUBE : COL_CUBE_LN;
        canvas.fillRect((int)p.x - 1, (int)p.y - 1, 3, 3, col);
    }
}

static void drawHud() {
    canvas.setTextColor(COL_TEXT);
    canvas.setTextSize(1);
    canvas.setCursor(4, 4);
    canvas.printf("%d", (int)(distance / 10.0));
    canvas.setTextColor(COL_TEXT_DIM);
    canvas.setCursor(SCREEN_W - 60, 4);
    canvas.printf("BEST %d", bestScore);
}

static void drawCenteredText(const char* txt, int y, int size, uint16_t col) {
    canvas.setTextSize(size);
    canvas.setTextColor(col);
    int w = canvas.textWidth(txt);
    canvas.setCursor((SCREEN_W - w) / 2, y);
    canvas.print(txt);
}

static void drawMenu() {
    drawBackground();
    drawObstacles();
    drawCube();
    drawCenteredText("GEOMETRY DASH", 34, 2, COL_CUBE);
    drawCenteredText("Cardputer Edition", 54, 1, COL_TEXT_DIM);
    if ((millis() / 450) % 2) {
        drawCenteredText("press any key to start", 78, 1, COL_TEXT);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "best %d", bestScore);
    drawCenteredText(buf, 92, 1, COL_TEXT_DIM);
}

static void drawDead() {
    drawBackground();
    drawObstacles();
    drawParticles();
    // a panel on top of the frozen scene
    canvas.fillRect(30, 30, SCREEN_W - 60, SCREEN_H - 60, COL_GROUND);
    canvas.drawRect(30, 30, SCREEN_W - 60, SCREEN_H - 60, COL_SPIKE);

    drawCenteredText("GAME OVER", 42, 2, COL_SPIKE);
    char buf[40];
    snprintf(buf, sizeof(buf), "score  %d", (int)(distance / 10.0));
    drawCenteredText(buf, 64, 1, COL_TEXT);
    snprintf(buf, sizeof(buf), "best %d   attempt %d", bestScore, attempts);
    drawCenteredText(buf, 78, 1, COL_TEXT_DIM);
    if (millis() - deadAt > 700 && (millis() / 450) % 2) {
        drawCenteredText("press any key to retry", 94, 1, COL_TEXT);
    }
}

// ---------------------------------------------------------------------------
//  Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setColorDepth(16);
    canvas.setColorDepth(16);
    canvas.createSprite(SCREEN_W, SCREEN_H);
    canvas.setTextWrap(false);
    canvas.setFont(&fonts::Font0);

    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(150);

    prefs.begin("geodash", false);
    bestScore = prefs.getInt("best", 0);

    resetRun();
    lastFrame = millis();
}

void loop() {
    M5Cardputer.update();

    // --- state transitions on the input edge --------------------------------
    bool edge = jumpPressedEdge();

    switch (gameState) {
        case STATE_MENU:
            if (edge) {
                resetRun();
                gameState = STATE_PLAYING;
            }
            break;

        case STATE_PLAYING:
            updatePlaying();
            break;

        case STATE_DEAD:
            updateParticles();
            // lock input briefly so a held key doesn't skip the screen
            if (edge && millis() - deadAt > 500) {
                resetRun();
                gameState = STATE_PLAYING;
            }
            break;
    }

    // --- render -------------------------------------------------------------
    switch (gameState) {
        case STATE_MENU:
            drawMenu();
            break;
        case STATE_PLAYING:
            drawBackground();
            drawObstacles();
            drawCube();
            drawHud();
            break;
        case STATE_DEAD:
            drawDead();
            break;
    }
    canvas.pushSprite(0, 0);

    prevKeyDown = jumpHeld();

    // --- frame pacing: aim for ~60 FPS --------------------------------------
    uint32_t now = millis();
    uint32_t dt  = now - lastFrame;
    if (dt < 16) delay(16 - dt);
    lastFrame = millis();
}
