// Mod experimental: búfer de input por ticks (GD 2.2, 240 TPS).
// v2: firmas corregidas con los bindings reales (build log). Aún sin probar en juego.
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

struct Pending {
    int button; bool push; bool p2; double ts;
    Clock::time_point t; // hora real del click
    int tick;            // tick del frame en que se aplicará
};
static std::vector<Pending> g_pending;
static Clock::time_point g_frameStart = Clock::now();
static bool g_newFrame = true;
static float g_elapsed = 0.f; // tiempo simulado dentro del frame actual

class $modify(TickLayer, GJBaseGameLayer) {
    // Retenemos el click con su hora real en vez de dejar que se aplique al inicio del frame.
    void queueButton(int button, bool push, bool isPlayer2, double timestamp) {
        if (g_stage >= 4 || !PlayLayer::get()) {
            GJBaseGameLayer::queueButton(button, push, isPlayer2, timestamp);
            return;
        }
        g_pending.push_back({ button, push, isPlayer2, timestamp, Clock::now(), -1 });
    }

    // Se llama una vez por tick. Soltamos cada input cuando llega su tick.
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto release = [&](bool all) {
            for (auto it = g_pending.begin(); it != g_pending.end();) {
                if (all || it->tick * TICK <= g_elapsed + 1e-6f) {
                    GJBaseGameLayer::queueButton(it->button, it->push, it->p2, it->ts);
                    it = g_pending.erase(it);
                } else {
                    ++it;
                }
            }
        };

        if (g_stage >= 4) {
            release(true);
            GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
            if (isLastTick) g_newFrame = true;
            return;
        }

        if (g_newFrame) {
            auto now = Clock::now();
            float frameSec = std::chrono::duration<float>(now - g_frameStart).count();
            int total = std::max(1, (int)std::ceil(frameSec / TICK));
            bool wave = m_player1 && m_player1->m_isDart;
            int limit = wave ? STAGES[g_stage].wave : STAGES[g_stage].all;

            for (auto& in : g_pending) {
                float since = std::chrono::duration<float>(in.t - g_frameStart).count();
                int k = std::clamp((int)(since / TICK), 0, total);
                in.tick = std::min(k, limit);
                log::info("SAVED +{} TICKS ({:.2f} frames @60fps) {} [Limit: {}]",
                    in.tick, in.tick * 0.25f, in.push ? "PRESS!" : "RELEASE!", limit);
            }
            g_frameStart = now;
            g_elapsed = 0.f;
            g_newFrame = false;
        }

        release(isLastTick); // en el último tick soltamos todo lo que quede
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        g_elapsed += dt;
        if (isLastTick) g_newFrame = true;
    }
};

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (down && !repeat && key == KEY_F6) {
            g_stage = (g_stage + 1) % 5;
            if (g_stage == 4) log::info("F6 -> STAGE 5: OFF (100% raw input)");
            else log::info("F6 -> STAGE {}: {} ticks all | {} ticks wave",
                g_stage + 1, STAGES[g_stage].all, STAGES[g_stage].wave);
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};
