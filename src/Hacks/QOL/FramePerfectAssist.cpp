#include "../../Client/Module.hpp"
#include "../../Client/InputModule.hpp"
#include "../../Macro/MacroManager.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>

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

class FramePerfectWindow : public InputModule
{
    public:
        MODULE_SETUP(FramePerfectWindow)
        {
            setID("qol-frame-perfect/window");

            setDefaultString("8");
            setPlaceholderString("Ticks");
            setHint("ticks");

            setStringFilter("1234567890");
            setMaxCharCount(2);
        }
};

class FramePerfectEarly : public Module
{
    public:
        MODULE_SETUP(FramePerfectEarly)
        {
            setID("qol-frame-perfect/early");
            setDefaultEnabled(true);
        }
};

class FramePerfectLate : public Module
{
    public:
        MODULE_SETUP(FramePerfectLate)
        {
            setID("qol-frame-perfect/late");
            setDefaultEnabled(true);
        }
};

SUBMIT_HACK(FramePerfectAssist);
SUBMIT_OPTION(FramePerfectAssist, FramePerfectEarly);
SUBMIT_OPTION(FramePerfectAssist, FramePerfectLate);
SUBMIT_OPTION(FramePerfectAssist, FramePerfectWindow);

// ventana configurable: 1 a 30 ticks
static unsigned int assistTicks()
{
    int v = FramePerfectWindow::get()->getStringInt();
    return (unsigned int)std::clamp(v, 1, 30);
}

class $modify (FramePerfectLayer, GJBaseGameLayer)
{
    struct Fields
    {
        bool held[2] = { false, false };
        unsigned int pressTick[2] = { 0, 0 };
        bool pending[2] = { false, false };

        // click tardío (idea de Dynamic): último orbe tocado y cuándo se dejó de tocar
        Ref<GameObject> lastRing[2];
        bool touching[2] = { false, false };
        int closeTick[2] = { -1, -1 };
        bool injecting = false;
    };

    void handleButton(bool down, int button, bool isPlayer1)
    {
        if (!m_fields->injecting && button == 1)
        {
            int i = isPlayer1 ? 0 : 1;

            // click tardío: el orbe acaba de pasar, se vuelve a exponer solo para este click
            auto lp = i == 0 ? m_player1 : m_player2;
            int tickNow = (int)m_gameState.m_currentProgress;

            if (down && lp && lp->m_touchingRings && lp->m_touchingRings->count() == 0 && m_fields->lastRing[i]
                && m_fields->closeTick[i] >= 0 && tickNow - m_fields->closeTick[i] + 1 <= (int)assistTicks()
                && FramePerfectAssist::get()->getRealEnabled() && FramePerfectLate::get()->getRealEnabled()
                && macroGetMode() == 0 && static_cast<GJBaseGameLayer*>(PlayLayer::get()) == this)
            {
                Ref<GameObject> ring = m_fields->lastRing[i];
                m_fields->lastRing[i] = nullptr;
                m_fields->closeTick[i] = -1;

                lp->m_touchingRings->addObject(ring);
                GJBaseGameLayer::handleButton(down, button, isPlayer1);
                lp->m_touchingRings->removeObject(ring);
                m_fields->held[i] = down;
                return;
            }

            m_fields->held[i] = down;

            if (down)
            {
                m_fields->pressTick[i] = m_gameState.m_currentProgress;
                m_fields->pending[i] = true;
            }
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

                // seguir el orbe que se está tocando
                if (player && player->m_touchingRings)
                {
                    bool touching = player->m_touchingRings->count() > 0;

                    if (touching)
                        m_fields->lastRing[i] = static_cast<GameObject*>(player->m_touchingRings->objectAtIndex(0));
                    else if (m_fields->touching[i])
                        m_fields->closeTick[i] = (int)m_gameState.m_currentProgress;

                    m_fields->touching[i] = touching;
                }

                if (!FramePerfectEarly::get()->getRealEnabled())
                    continue;

                if (!player || player->m_isDead || !m_fields->pending[i])
                    continue;

                unsigned int now = m_gameState.m_currentProgress;

                // el nivel se reinició (el tiempo volvió atrás) o la ventana ya pasó
                if (now < m_fields->pressTick[i] || now - m_fields->pressTick[i] > assistTicks())
                {
                    m_fields->pending[i] = false;
                    continue;
                }

                if (now > m_fields->pressTick[i])
                {
                    m_fields->injecting = true;
                    GJBaseGameLayer::handleButton(false, 1, i == 0);
                    GJBaseGameLayer::handleButton(true, 1, i == 0);

                    // si ya soltaste el botón, el click repetido también se suelta
                    if (!m_fields->held[i])
                        GJBaseGameLayer::handleButton(false, 1, i == 0);

                    m_fields->injecting = false;
                }
            }
        }

        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
    }
};
