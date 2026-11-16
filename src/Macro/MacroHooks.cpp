#include "MacroManager.hpp"
#include "../Client/Module.hpp"
#include "../Client/InputModule.hpp"
#include "../Client/ButtonModule.hpp"
#include "../Notifications/NotificationManager.hpp"
#include <LocalisationManager.hpp>
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
            setCategory("Macro");
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
            setCategory("Macro");
            setDescription("Replays the macro saved for this level. Your own inputs are ignored while it is on");
            setSafeModeTrigger(SafeModeTrigger::Attempt);
        }
};

SUBMIT_HACK(MacroRecorder);
SUBMIT_HACK(MacroPlayer);

static int macroLevelID(PlayLayer* pl);

static void macroToast(std::string key)
{
    NotificationManager::get()->notifyToast(LocalisationManager::get()->getLocalisedString(key));
}

static int macroLevelIDNow()
{
    return macroLevelID(PlayLayer::get());
}

class MacroExport : public ButtonModule
{
    public:
        MODULE_SETUP(MacroExport)
        {
            setName("Export Macro");
            setID("macro-export");
            setCategory("Macro");
            setDescription("Saves the current macro to a file you choose, so you can keep it or share it");
        }

        virtual void onClick()
        {
            #if GEODE_COMP_GD_VERSION >= 22081

            auto mgr = MacroManager::get();

            // sin nada en memoria: intentar con el macro guardado del nivel actual
            if (mgr->getInputCount() == 0 && PlayLayer::get())
            {
                mgr->load(macroLevelIDNow());
                mgr->setLoadedLevel(macroLevelIDNow());
            }

            if (mgr->getInputCount() == 0)
            {
                macroToast("ui/macro-export-empty");
                return;
            }

            file::FilePickOptions options;
            options.defaultPath = Mod::get()->getConfigDir() / "macro.qmacro";

            file::FilePickOptions::Filter filter;
            filter.description = "Macro File";
            filter.files = { "*.qmacro" };
            options.filters.push_back(filter);

            async::spawn(file::pick(file::PickMode::SaveFile, options), [](Result<std::optional<asp::fs::Path>> result)
            {
                if (!result.isOk() || !result.unwrap().has_value())
                    return;

                auto path = result.unwrap().value();

                macroToast(MacroManager::get()->exportTo(path) ? "ui/macro-exported" : "ui/macro-export-failed");
            });

            #endif
        }
};

class MacroImport : public ButtonModule
{
    public:
        MODULE_SETUP(MacroImport)
        {
            setName("Import Macro");
            setID("macro-import");
            setCategory("Macro");
            setDescription("Loads a macro file and saves it for the level you are in. Open the level first");
        }

        virtual void onClick()
        {
            #if GEODE_COMP_GD_VERSION >= 22081

            if (!PlayLayer::get())
            {
                macroToast("ui/macro-import-need-level");
                return;
            }

            int levelID = macroLevelIDNow();

            file::FilePickOptions options;
            options.defaultPath = Mod::get()->getConfigDir();

            file::FilePickOptions::Filter filter;
            filter.description = "Macro File";
            filter.files = { "*.qmacro" };
            options.filters.push_back(filter);

            async::spawn(file::pick(file::PickMode::OpenFile, options), [levelID](Result<std::optional<asp::fs::Path>> result)
            {
                if (!result.isOk() || !result.unwrap().has_value())
                    return;

                auto path = result.unwrap().value();
                auto mgr = MacroManager::get();

                if (mgr->importFrom(path))
                {
                    mgr->setLoadedLevel(levelID);
                    mgr->save(levelID);
                    macroToast("ui/macro-imported");
                }
                else
                {
                    macroToast("ui/macro-import-failed");
                }
            });

            #endif
        }
};

SUBMIT_HACK(MacroExport);
SUBMIT_HACK(MacroImport);

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
    {
        mgr->save(id);

        if (!mgr->getSelectedName().empty())
            mgr->saveNamed(mgr->getSelectedName());
    }
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
        bool wasPractice = false;
    };

    void setupHasCompleted()
    {
        PlayLayer::setupHasCompleted();

        auto mgr = MacroManager::get();
        mgr->clear();
        mgr->setLoadedLevel(-1);

        // macro elegido en la pestaña Macro: cargarlo para este nivel
        if (currentMacroState() == MacroState::Playback && !mgr->getSelectedName().empty() && mgr->loadNamed(mgr->getSelectedName()))
            mgr->setLoadedLevel(macroLevelID(this));

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

        // reinicio en modo normal después de práctica: dejar de grabar
        if (state == MacroState::Recording && m_fields->wasPractice && !m_isPracticeMode)
        {
            macroSaveIfRecording(this);

            if (auto m = Module::getByID("macro-recorder"))
                m->setUserEnabled(false);

            m_fields->wasPractice = false;
            return;
        }

        m_fields->wasPractice = m_isPracticeMode;

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
                label->setString(fmt::format("REC  {} {}", mgr->getInputCount(), LocalisationManager::get()->getLocalisedString("ui/macro-inputs")).c_str());
                break;

            case MacroState::Playback:
            {
                bool mismatch = mgr->getInputCount() > 0 && mgr->getTPS() != macroCurrentTPS();

                label->setVisible(true);
                label->setColor(mismatch ? ccColor3B{255, 190, 60} : ccColor3B{80, 255, 120});
                label->setString(fmt::format("PLAY  {}/{}{}", mgr->getCursor(), mgr->getInputCount(), mismatch ? fmt::format("  ({})", LocalisationManager::get()->getLocalisedString("ui/macro-tps-mismatch")) : std::string("")).c_str());
                break;
            }

            default:
                label->setVisible(false);
                break;
        }
    }
};
