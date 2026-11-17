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

            setDefaultString("6");
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
        bool injecting = false;
        RingObject* lastRing[2] = { nullptr, nullptr };
        unsigned int lastRingTick[2] = { 0, 0 };
    };

    void handleButton(bool down, int button, bool isPlayer1)
    {
        if (!m_fields->injecting && button == 1)
        {
            int i = isPlayer1 ? 0 : 1;
            m_fields->held[i] = down;

            if (down)
                m_fields->pressTick[i] = m_gameState.m_currentProgress;

            // click tardío: el orbe acaba de pasar, se vuelve a exponer un instante
            auto player = i == 0 ? m_player1 : m_player2;
            unsigned int now = m_gameState.m_currentProgress;

            if (down && player && player->m_touchedRings && player->m_touchedRings->count() == 0
                && m_fields->lastRing[i] && now >= m_fields->lastRingTick[i] && now - m_fields->lastRingTick[i] <= assistTicks()
                && FramePerfectAssist::get()->getRealEnabled() && FramePerfectLate::get()->getRealEnabled()
                && macroGetMode() == 0 && static_cast<GJBaseGameLayer*>(PlayLayer::get()) == this)
            {
                auto ring = m_fields->lastRing[i];
                m_fields->lastRing[i] = nullptr;

                player->m_touchedRings->addObject(ring);
                GJBaseGameLayer::handleButton(down, button, isPlayer1);
                player->m_touchedRings->removeObject(ring);
                return;
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

                // recordar el último orbe tocado (para el click tardío)
                if (player && player->m_touchedRings && player->m_touchedRings->count() > 0)
                {
                    if (auto ring = typeinfo_cast<RingObject*>(player->m_touchedRings->objectAtIndex(0)))
                    {
                        m_fields->lastRing[i] = ring;
                        m_fields->lastRingTick[i] = m_gameState.m_currentProgress;
                    }
                }

                if (!FramePerfectEarly::get()->getRealEnabled())
                    continue;

                if (!player || player->m_isDead)
                    continue;

                unsigned int now = m_gameState.m_currentProgress;
                unsigned int since = now - m_fields->pressTick[i];

                if (now > m_fields->pressTick[i] && since <= assistTicks())
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
