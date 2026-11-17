#include "../../Client/Module.hpp"
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class AutoExitComplete : public Module
{
    public:
        MODULE_SETUP(AutoExitComplete)
        {
            setName("Auto Exit On Complete");
            setID("auto-exit-complete");
            setCategory("2.2091");
            setDescription("Leaves the level a moment after you complete it, skipping the end screen");
        }
};

class AutoRestartComplete : public Module
{
    public:
        MODULE_SETUP(AutoRestartComplete)
        {
            setName("Auto Restart On Complete");
            setID("auto-restart-complete");
            setCategory("2.2091");
            setDescription("Restarts the level a moment after you complete it. Overrides Auto Exit");
        }
};

class HideAttemptsLabel : public Module
{
    public:
        MODULE_SETUP(HideAttemptsLabel)
        {
            setName("Hide Attempts Label");
            setID("hide-attempts-label");
            setCategory("2.2091");
            setDescription("Hides the Attempt X text at the start of the level");
        }
};

class AutoCheckpoint : public Module
{
    public:
        MODULE_SETUP(AutoCheckpoint)
        {
            setName("Auto Checkpoint");
            setID("auto-checkpoint");
            setCategory("2.2091");
            setDescription("Places a checkpoint every 2 seconds while in practice mode");
        }
};

class DeathCounter : public Module
{
    public:
        MODULE_SETUP(DeathCounter)
        {
            setName("Death Counter");
            setID("death-counter");
            setCategory("2.2091");
            setDescription("Shows how many times you died in this session of the level");
        }
};

SUBMIT_HACK(AutoCheckpoint);
SUBMIT_HACK(DeathCounter);
SUBMIT_HACK(AutoExitComplete);
SUBMIT_HACK(AutoRestartComplete);
SUBMIT_HACK(HideAttemptsLabel);

class $modify (New2091PlayLayer, PlayLayer)
{
    struct Fields
    {
        float checkpointTimer = 0.f;
        int deaths = 0;
        CCLabelBMFont* deathLabel = nullptr;
    };

    void destroyPlayer(PlayerObject* p0, GameObject* p1)
    {
        bool wasDead = m_player1 && m_player1->m_isDead;

        PlayLayer::destroyPlayer(p0, p1);

        if (!wasDead && m_player1 && m_player1->m_isDead)
            m_fields->deaths++;
    }

    void afterComplete(float dt)
    {
        if (AutoRestartComplete::get()->getRealEnabled())
            this->resetLevel();
        else if (AutoExitComplete::get()->getRealEnabled())
            this->onQuit();
    }

    void levelComplete()
    {
        PlayLayer::levelComplete();

        if (AutoRestartComplete::get()->getRealEnabled() || AutoExitComplete::get()->getRealEnabled())
            this->scheduleOnce(schedule_selector(New2091PlayLayer::afterComplete), 1.5f);
    }

    void postUpdate(float dt)
    {
        PlayLayer::postUpdate(dt);

        if (m_attemptLabel && HideAttemptsLabel::get()->getRealEnabled())
            m_attemptLabel->setVisible(false);

        // checkpoint automático en práctica
        if (AutoCheckpoint::get()->getRealEnabled() && m_isPracticeMode && m_player1 && !m_player1->m_isDead)
        {
            m_fields->checkpointTimer += dt;

            if (m_fields->checkpointTimer >= 2.f)
            {
                m_fields->checkpointTimer = 0.f;
                this->createCheckpoint();
            }
        }

        // contador de muertes
        bool showDeaths = DeathCounter::get()->getRealEnabled();

        if (showDeaths && !m_fields->deathLabel && m_uiLayer)
        {
            auto label = CCLabelBMFont::create("", "bigFont.fnt");
            label->setScale(0.45f);
            label->setOpacity(170);
            label->setAnchorPoint({1.f, 1.f});
            label->setPosition(CCDirector::get()->getWinSize() - ccp(8, 8));
            m_uiLayer->addChild(label, 100);
            m_fields->deathLabel = label;
        }

        if (m_fields->deathLabel)
        {
            m_fields->deathLabel->setVisible(showDeaths);
            m_fields->deathLabel->setString(fmt::format("{}", m_fields->deaths).c_str());
        }
    }
};
