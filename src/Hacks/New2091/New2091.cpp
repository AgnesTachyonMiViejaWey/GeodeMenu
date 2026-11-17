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

SUBMIT_HACK(AutoExitComplete);
SUBMIT_HACK(AutoRestartComplete);
SUBMIT_HACK(HideAttemptsLabel);

class $modify (New2091PlayLayer, PlayLayer)
{
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
    }
};
