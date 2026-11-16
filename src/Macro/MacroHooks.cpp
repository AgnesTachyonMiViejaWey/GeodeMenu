#include "MacroManager.hpp"
#include "../Client/Module.hpp"
#include "../Client/InputModule.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class MacroRecorder : public Module
{
    public:
        MODULE_SETUP(MacroRecorder)
        {
            setName("Macro Recorder");
            setID("macro-recorder");
            setCategory("Level");
            setDescription("Records your inputs in the level so they can be replayed later. The macro is saved automatically when you finish or leave the level. If Macro Player is also on, the player wins");
            setSafeModeTrigger(SafeModeTrigger::Attempt);
        }
};

class MacroPlayer : public Module
{
    public:
        MODULE_SETUP(MacroPlayer)
        {
            setName("Macro Player");
            setID("macro-player");
            setCategory("Level");
            setDescription("Replays the macro saved for this level. Your own inputs are ignored while it is on");
            setSafeModeTrigger(SafeModeTrigger::Attempt);
        }
};

SUBMIT_HACK(MacroRecorder);
SUBMIT_HACK(MacroPlayer);

static MacroState currentMacroState()
{
    if (MacroPlayer::get()->getRealEnabled())
        return MacroState::Playback;

    if (MacroRecorder::get()->getRealEnabled())
        return MacroState::Recording;

    return MacroState::None;
}

// TPS actual del juego: 0 = vanilla (sin TPS bypass)
static double macroCurrentTPS()
{
    auto enabled = Module::getByID("tps-bypass");

    if (!enabled || !enabled->getRealEnabled())
        return 0.0;

    auto value = static_cast<InputModule*>(Module::getByID("tps-bypass/value"));

    if (!value)
        return 0.0;

    return (double)value->getStringFloat();
}

static int macroLevelID(PlayLayer* pl)
{
    if (!pl || !pl->m_level)
        return 0;

    return pl->m_level->m_levelID.value();
}

static void macroSaveIfRecording(PlayLayer* pl)
{
    if (currentMacroState() != MacroState::Recording)
        return;

    auto mgr = MacroManager::get();
    int id = macroLevelID(pl);

    if (mgr->getLoadedLevel() == id && mgr->getInputCount() > 0)
        mgr->save(id);
}

class $modify (MacroBaseGameLayer, GJBaseGameLayer)
{
    void handleButton(bool down, int button, bool isPlayer1)
    {
        auto mgr = MacroManager::get();
        auto pl = PlayLayer::get();

        // solo nos interesa el PlayLayer, y nunca lo que inyecta el propio macro
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != this || mgr->isInjecting())
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);

        auto state = currentMacroState();

        // durante la reproducción se ignora el input manual
        if (state == MacroState::Playback)
            return;

        GJBaseGameLayer::handleButton(down, button, isPlayer1);

        if (state == MacroState::Recording)
        {
            mgr->setLoadedLevel(macroLevelID(pl));
            mgr->setTPS(macroCurrentTPS());
            mgr->record(m_gameState.m_currentProgress, button, down, isPlayer1);
        }
    }

    void processQueuedButtons(float dt, bool clearInputQueue)
    {
        auto pl = PlayLayer::get();

        if (pl && static_cast<GJBaseGameLayer*>(pl) == this && currentMacroState() == MacroState::Playback)
        {
            auto mgr = MacroManager::get();
            int id = macroLevelID(pl);

            if (mgr->getLoadedLevel() != id)
            {
                mgr->load(id);
                mgr->setLoadedLevel(id);
                mgr->seek(m_gameState.m_currentProgress);
            }

            mgr->setInjecting(true);

            for (auto const& input : mgr->takeDue(m_gameState.m_currentProgress))
                GJBaseGameLayer::handleButton(input.down, input.button, input.player1);

            mgr->setInjecting(false);
        }

        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
    }
};

class $modify (MacroPlayLayer, PlayLayer)
{
    struct Fields
    {
        CCLabelBMFont* label = nullptr;
    };

    void setupHasCompleted()
    {
        PlayLayer::setupHasCompleted();

        auto mgr = MacroManager::get();
        mgr->clear();
        mgr->setLoadedLevel(-1);

        auto label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setScale(0.35f);
        label->setOpacity(160);
        label->setAnchorPoint({0.f, 0.f});
        label->setPosition({6.f, 6.f});
        label->setVisible(false);

        m_uiLayer->addChild(label, 100);
        m_fields->label = label;
    }

    void storeCheckpoint(CheckpointObject* checkpoint)
    {
        PlayLayer::storeCheckpoint(checkpoint);

        // recordar en qué punto del macro se creó el checkpoint
        if (currentMacroState() != MacroState::None)
            MacroManager::get()->markCheckpoint(checkpoint);
    }

    void resetLevel()
    {
        PlayLayer::resetLevel();

        auto state = currentMacroState();

        if (state == MacroState::None)
            return;

        auto mgr = MacroManager::get();
        unsigned int frame = m_gameState.m_currentProgress;

        // en modo práctica se reaparece en el último checkpoint
        CheckpointObject* checkpoint = nullptr;

        if (m_isPracticeMode && m_checkpointArray && m_checkpointArray->count() > 0)
            checkpoint = static_cast<CheckpointObject*>(m_checkpointArray->lastObject());

        // checkpoint conocido: dejar el macro exactamente como estaba al crearlo
        if (checkpoint && mgr->restoreCheckpoint(checkpoint, state))
            return;

        // sin checkpoint conocido: usar el frame al que volvió el nivel
        // (sin checkpoint es un reinicio completo, así que se borra todo)
        if (state == MacroState::Recording)
            mgr->truncateFrom(checkpoint ? frame : 0);
        else
            mgr->seek(checkpoint ? frame : 0);
    }

    void levelComplete()
    {
        PlayLayer::levelComplete();

        macroSaveIfRecording(this);
    }

    void onQuit()
    {
        macroSaveIfRecording(this);

        PlayLayer::onQuit();
    }

    virtual void postUpdate(float dt)
    {
        PlayLayer::postUpdate(dt);

        auto label = m_fields->label;

        if (!label)
            return;

        auto mgr = MacroManager::get();

        switch (currentMacroState())
        {
            case MacroState::Recording:
                label->setVisible(true);
                label->setColor({255, 80, 80});
                label->setString(fmt::format("REC  {} inputs", mgr->getInputCount()).c_str());
                break;

            case MacroState::Playback:
            {
                bool mismatch = mgr->getInputCount() > 0 && mgr->getTPS() != macroCurrentTPS();

                label->setVisible(true);
                label->setColor(mismatch ? ccColor3B{255, 190, 60} : ccColor3B{80, 255, 120});
                label->setString(fmt::format("PLAY  {}/{}{}", mgr->getCursor(), mgr->getInputCount(), mismatch ? "  (TPS distinto)" : "").c_str());
                break;
            }

            default:
                label->setVisible(false);
                break;
        }
    }
};
