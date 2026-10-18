// Tick Input Buffer v3 (experimental)
// OJO: esta versión MIDE y MUESTRA cuántos ticks de retraso tuvo cada input (como el log del mod original)
// pero todavía NO corrige la física. Sirve para comprobar que los hooks y las etapas funcionan.
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <chrono>
#include <optional>
#include <algorithm>

using namespace geode::prelude;
using Clock = std::chrono::steady_clock;

constexpr float TICK = 1.f / 240.f;

struct Stage { int all; int wave; };
constexpr Stage STAGES[4] = { {2, 2}, {4, 4}, {8, 8}, {16, 8} };
static int g_stage = 0; // 0..3 etapas, 4 = OFF

// Hora real del último press / release de teclado (solo teclado por ahora).
static std::optional<Clock::time_point> g_lastPress;
static std::optional<Clock::time_point> g_lastRelease;

class $modify(TickLayer, GJBaseGameLayer) {
    // El juego llama a handleButton cuando aplica el input en un tick.
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& ref = down ? g_lastPress : g_lastRelease;
        if (g_stage < 4 && PlayLayer::get() && ref.has_value()) {
            float lateSec = std::chrono::duration<float>(Clock::now() - *ref).count();
            bool wave = m_player1 && m_player1->m_isDart;
            int limit = wave ? STAGES[g_stage].wave : STAGES[g_stage].all;
            int ticks = std::min((int)(lateSec / TICK), limit);
            log::info("SAVED +{} TICKS ({:.2f} frames @60fps) {} [Limit: {}]",
                ticks, ticks * 0.25f, down ? "PRESS!" : "RELEASE!", limit);
            ref.reset();
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
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
        if (!repeat && (key == KEY_Space || key == KEY_Up || key == KEY_W)) {
            (down ? g_lastPress : g_lastRelease) = Clock::now();
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};
