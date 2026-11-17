#include "../../Client/Module.hpp"
#include "../../Macro/MacroManager.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// "Modo Pro": si haces click unos ticks antes de tiempo, el click se repite cada tick
// hasta que el juego lo acepta (suelo, orbe o pad). Así el frame perfect se vuelve fácil.
class FramePerfectAssist : public Module
{
    public:
        MODULE_SETUP(FramePerfectAssist)
        {
            setName("Frame Perfect Assist");
            setID("qol-frame-perfect");
            setCategory("QOL");
            setDescription("Pro mode: if you click a few ticks too early, the click is repeated until the game accepts it (ground, orb or pad). Makes frame perfect jumps much easier. Turned off while a macro is recording or playing");
            setSafeModeTrigger(SafeModeTrigger::Attempt);
        }
};

SUBMIT_HACK(FramePerfectAssist);

static constexpr unsigned int ASSIST_TICKS = 6;

class $modify (FramePerfectLayer, GJBaseGameLayer)
{
    struct Fields
    {
        bool held[2] = { false, false };
        unsigned int pressTick[2] = { 0, 0 };
        bool injecting = false;
    };

    void handleButton(bool down, int button, bool isPlayer1)
    {
        if (!m_fields->injecting && button == 1)
        {
            int i = isPlayer1 ? 0 : 1;
            m_fields->held[i] = down;

            if (down)
                m_fields->pressTick[i] = m_gameState.m_currentProgress;
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processQueuedButtons(float dt, bool clearInputQueue)
    {
        if (FramePerfectAssist::get()->getRealEnabled() && macroGetMode() == 0 && static_cast<GJBaseGameLayer*>(PlayLayer::get()) == this)
        {
            for (int i = 0; i < 2; i++)
            {
                auto player = i == 0 ? m_player1 : m_player2;

                if (!player || !m_fields->held[i] || player->m_isDead)
                    continue;

                // en modos de mantener (nave, ufo, wave, swing) repetir el click no sirve
                if (player->m_isShip || player->m_isBird || player->m_isDart || player->m_isSwing)
                    continue;

                unsigned int now = m_gameState.m_currentProgress;
                unsigned int since = now - m_fields->pressTick[i];

                if (now > m_fields->pressTick[i] && since <= ASSIST_TICKS)
                {
                    m_fields->injecting = true;
                    GJBaseGameLayer::handleButton(false, 1, i == 0);
                    GJBaseGameLayer::handleButton(true, 1, i == 0);
                    m_fields->injecting = false;
                }
            }
        }

        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
    }
};
