#include "GamesEngineeringBase.h"
#include <windows.h>   // Sleep, VK_*
#include <cmath>
using namespace GamesEngineeringBase;

// 屏幕与世界
const int SCR_W = 800;
const int SCR_H = 600;
const int WORLD_W = 3000;
const int WORLD_H = 3000;

// 玩家
const int P_SIZE = 20;
const int P_SPEED = 4;

// 敌人对象池（定长、无 STL）
const int MAX_ENEMY = 300;
int   eX[MAX_ENEMY];
int   eY[MAX_ENEMY];
bool  eAlive[MAX_ENEMY];
int   eSpeed[MAX_ENEMY];  // 不同敌人可有不同速度（像素/帧）

// 简单 LCG 随机数
static unsigned seed_ = 1234567u;
inline int rnd() { seed_ = seed_ * 1103515245u + 12345u; return (seed_ >> 16) & 0x7FFF; }

// 画屏幕坐标小方块
inline void fill3(Window& win, int sx, int sy, unsigned char r, unsigned char g, unsigned char b) {
    // 3x3 小方块（边界裁剪）
    for (int y = sy - 1; y <= sy + 1; ++y) {
        if (y < 0 || y >= SCR_H) continue;
        for (int x = sx - 1; x <= sx + 1; ++x) {
            if (x < 0 || x >= SCR_W) continue;
            win.draw(x, y, r, g, b);
        }
    }
}

// 在相机四周“屏幕外”随机刷出一个敌人
void spawnEnemyOffscreen(int camX, int camY) {
    // 找空位
    int idx = -1;
    for (int i = 0; i < MAX_ENEMY; ++i) { if (!eAlive[i]) { idx = i; break; } }
    if (idx < 0) return; // 满了

    // 选择一侧：0=左 1=右 2=上 3=下
    int side = rnd() % 4;
    const int margin = 40; // 距离屏幕边缘的“外侧”距离

    int sx = 0, sy = 0; // 屏幕坐标（先放在屏幕外）
    if (side == 0) { // 左
        sx = -margin;
        sy = rnd() % (SCR_H + 2 * margin) - margin;
    }
    else if (side == 1) { // 右
        sx = SCR_W + margin;
        sy = rnd() % (SCR_H + 2 * margin) - margin;
    }
    else if (side == 2) { // 上
        sy = -margin;
        sx = rnd() % (SCR_W + 2 * margin) - margin;
    }
    else { // 下
        sy = SCR_H + margin;
        sx = rnd() % (SCR_W + 2 * margin) - margin;
    }

    // 转回“世界坐标”
    int wx = camX + sx;
    int wy = camY + sy;

    // 夹到世界内（避免刷到世界外侧太远）
    if (wx < 0) wx = 0;
    if (wy < 0) wy = 0;
    if (wx >= WORLD_W) wx = WORLD_W - 1;
    if (wy >= WORLD_H) wy = WORLD_H - 1;

    eX[idx] = wx;
    eY[idx] = wy;
    eSpeed[idx] = 2 + (rnd() % 3); // 2~4 随机速度
    eAlive[idx] = true;
}

int main() {
    Window win;
    win.create(SCR_W, SCR_H, "Enemies: offscreen spawn + chase");

    // 玩家初始在世界中部
    int px = WORLD_W / 2, py = WORLD_H / 2;

    // 相机
    int camX = 0, camY = 0;

    // 敌人池初始化
    for (int i = 0; i < MAX_ENEMY; ++i) eAlive[i] = false;

    // 刷怪节奏
    int spawnTimer = 0;
    int spawnInterval = 40;   // 初值：每40帧刷一个
    int elapsedFrames = 0;

    while (true) {
        win.checkInput();
        if (win.keyPressed(VK_ESCAPE)) break;

        // -------- 玩家输入（归一化，避免斜向更快）--------
        int dx = 0, dy = 0;
        if (win.keyPressed(VK_LEFT) || win.keyPressed('A')) dx -= 1;
        if (win.keyPressed(VK_RIGHT) || win.keyPressed('D')) dx += 1;
        if (win.keyPressed(VK_UP) || win.keyPressed('W')) dy -= 1;
        if (win.keyPressed(VK_DOWN) || win.keyPressed('S')) dy += 1;

        if (dx != 0 || dy != 0) {
            int vx = dx * P_SPEED, vy = dy * P_SPEED;
            if ((dx && dy)) { vx = int(vx * 0.7071f); vy = int(vy * 0.7071f); }
            px += vx; py += vy;
        }

        // 世界边界限制
        if (px < 0) px = 0;
        if (py < 0) py = 0;
        if (px + P_SIZE > WORLD_W) px = WORLD_W - P_SIZE;
        if (py + P_SIZE > WORLD_H) py = WORLD_H - P_SIZE;

        // -------- 相机跟随 --------
        camX = px + P_SIZE / 2 - SCR_W / 2;
        camY = py + P_SIZE / 2 - SCR_H / 2;
        if (camX < 0) camX = 0;
        if (camY < 0) camY = 0;
        if (camX > WORLD_W - SCR_W) camX = WORLD_W - SCR_W;
        if (camY > WORLD_H - SCR_H) camY = WORLD_H - SCR_H;

        // -------- 刷怪：屏幕外生成 --------
        if (--spawnTimer <= 0) {
            spawnEnemyOffscreen(camX, camY);
            // 难度随时间提升：逐步缩短间隔（下限10帧）
            if (spawnInterval > 10 && (elapsedFrames % 120 == 0)) spawnInterval--;
            spawnTimer = spawnInterval;
        }

        // -------- 敌人AI：朝玩家移动 --------
        int pcx = px + P_SIZE / 2;   // 玩家中心
        int pcy = py + P_SIZE / 2;
        for (int i = 0; i < MAX_ENEMY; ++i) if (eAlive[i]) {
            int ex = eX[i], ey = eY[i];
            int vx = pcx - ex;
            int vy = pcy - ey;
            // 归一化方向
            int ax = 0, ay = 0;
            if (vx == 0 && vy == 0) { ax = 0; ay = 0; }
            else {
                // 用浮点更顺滑；也可以用近似表避免浮点
                float len = std::sqrt(float(vx * vx + vy * vy));
                float nx = vx / len, ny = vy / len;
                ax = int(nx * eSpeed[i]);
                ay = int(ny * eSpeed[i]);
                if (ax == 0 && vx != 0) ax = (vx > 0 ? 1 : -1); // 避免卡死
                if (ay == 0 && vy != 0) ay = (vy > 0 ? 1 : -1);
            }
            ex += ax; ey += ay;

            // 世界边界
            if (ex < 0) ex = 0;
            if (ey < 0) ey = 0;
            if (ex >= WORLD_W) ex = WORLD_W - 1;
            if (ey >= WORLD_H) ey = WORLD_H - 1;

            eX[i] = ex; eY[i] = ey;
        }

        // -------- 渲染 --------
        win.clear();

        // 背景（浅灰、简单填充）
        for (int y = 0; y < SCR_H; y += 8)
            for (int x = 0; x < SCR_W; x += 8)
                win.draw(x, y, 40, 40, 40);

        // 敌人（小白点，3x3）
        for (int i = 0; i < MAX_ENEMY; ++i) if (eAlive[i]) {
            int sx = eX[i] - camX;
            int sy = eY[i] - camY;
            if (sx >= -2 && sx < SCR_W + 2 && sy >= -2 && sy < SCR_H + 2)
                fill3(win, sx, sy, 240, 240, 240);
        }

        // 玩家（红色方块）
        for (int yy = 0; yy < P_SIZE; ++yy)
            for (int xx = 0; xx < P_SIZE; ++xx) {
                int sx = (px - camX) + xx;
                int sy = (py - camY) + yy;
                if (sx >= 0 && sx < SCR_W && sy >= 0 && sy < SCR_H)
                    win.draw(sx, sy, 255, 60, 60);
            }

        win.present();
        Sleep(16);
        ++elapsedFrames;
    }

    return 0;
}
