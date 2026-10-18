// Mod experimental: búfer de input por ticks (GD 2.2, 240 TPS).
// Sin compilar (no había SDK en mi entorno). Si algo no compila, copia el error.
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;
using Clock = std::chrono::steady_clock;

constexpr float TICK = 1.f / 240.f;

struct Stage { int all; int wave; };
constexpr Stage STAGES[4] = { {2, 2}, {4, 4}, {8, 8}, {16, 8} };
static int g_stage = 0; // 0..3 etapas, 4 = OFF

struct Pending { int button; bool down; bool p1; Clock::time_point t; };
static std::vector<Pending> g_pending;
static Clock::time_point g_prevFrame = Clock::now();

class $modify(TickLayer, GJBaseGameLayer) {
    // Guardamos el click con su hora real en vez de dejar que el juego lo aplique al inicio del frame.
    void queueButton(int button, bool down, bool p1) {
        if (g_stage >= 4 || !PlayLayer::get()) {
            GJBaseGameLayer::queueButton(button, down, p1);
            return;
        }
        g_pending.push_back({ button, down, p1, Clock::now() });
    }

    // Partimos el frame en tramos de ticks y aplicamos cada input en su tick.
    void processCommands(float dt) {
        auto prev = g_prevFrame;
        g_prevFrame = Clock::now();

        if (g_pending.empty() || g_stage >= 4) {
            GJBaseGameLayer::processCommands(dt);
            return;
        }

        int total = std::max(1, (int)std::round(dt / TICK));
        bool wave = m_player1 && m_player1->m_isDart;
        int limit = wave ? STAGES[g_stage].wave : STAGES[g_stage].all;

        struct Item { Pending in; int tick; };
        std::vector<Item> items;
        for (auto& in : g_pending) {
            float since = std::chrono::duration<float>(in.t - prev).count();
            int k = std::clamp((int)(since / TICK), 0, total);
            int tick = std::min(k, limit);
            items.push_back({ in, tick });
            log::info("SAVED +{} TICKS ({:.2f} frames @60fps) {} [Limit: {}]",
                tick, tick * 0.25f, in.down ? "PRESS!" : "RELEASE!", limit);
        }
        g_pending.clear();
        std::stable_sort(items.begin(), items.end(),
            [](auto& a, auto& b) { return a.tick < b.tick; });

        int cursor = 0;
        for (auto& it : items) {
            if (it.tick > cursor) {
                GJBaseGameLayer::processCommands((it.tick - cursor) * TICK);
                cursor = it.tick;
            }
            GJBaseGameLayer::queueButton(it.in.button, it.in.down, it.in.p1);
        }
        GJBaseGameLayer::processCommands(std::max(total - cursor, 1) * TICK);
    }
};

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat) {
        if (down && !repeat && key == KEY_F6) {
            g_stage = (g_stage + 1) % 5;
            if (g_stage == 4) log::info("F6 -> STAGE 5: OFF (100% raw input)");
            else log::info("F6 -> STAGE {}: {} ticks all | {} ticks wave",
                g_stage + 1, STAGES[g_stage].all, STAGES[g_stage].wave);
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat);
    }
};
