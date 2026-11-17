#include "../../Client/Module.hpp"
#include "../../Client/InputModule.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// Para secciones tipo Silent Clubstep: congela el juego y avanza 1 tick a la vez
// con el botón "Step" para ensayar cada frame con calma (se usa en práctica).
class FrameStep : public Module
{
    public:
        MODULE_SETUP(FrameStep)
        {
            setName("Frame Step");
            setID("qol-frame-step");
            setCategory("QOL");
            setDescription("Adds Freeze and Step buttons in the level. Freeze stops the game, Step advances exactly one physics tick. Great for practicing frame perfect sections like Silent Clubstep");
            setSafeModeTrigger(SafeModeTrigger::Attempt);
        }
};

SUBMIT_HACK(FrameStep);

static bool g_frozen = false;
static int g_steps = 0;

static float stepTickLength()
{
    auto enabled = Module::getByID("tps-bypass");

    if (enabled && enabled->getRealEnabled())
    {
        auto value = static_cast<InputModule*>(Module::getByID("tps-bypass/value"));

        if (value && value->getStringFloat() > 0.f)
            return 1.f / value->getStringFloat();
    }

    return 1.f / 240.f;
}

class $modify (FrameStepLayer, GJBaseGameLayer)
{
    void update(float dt)
    {
        if (g_frozen && FrameStep::get()->getRealEnabled() && static_cast<GJBaseGameLayer*>(PlayLayer::get()) == this)
        {
            if (g_steps <= 0)
                return;

            g_steps--;

            // un poco más de un tick para que siempre se ejecute exactamente uno
            return GJBaseGameLayer::update(stepTickLength() * 1.001f);
        }

        GJBaseGameLayer::update(dt);
    }
};

class $modify (FrameStepPlayLayer, PlayLayer)
{
    struct Fields
    {
        CCMenu* menu = nullptr;
        CCLabelBMFont* freezeLabel = nullptr;
    };

    void onFreeze(CCObject*)
    {
        g_frozen = !g_frozen;
        g_steps = 0;

        if (m_fields->freezeLabel)
            m_fields->freezeLabel->setString(g_frozen ? "Resume" : "Freeze");
    }

    void onStep(CCObject*)
    {
        if (g_frozen)
            g_steps++;
    }

    void setupHasCompleted()
    {
        PlayLayer::setupHasCompleted();

        g_frozen = false;
        g_steps = 0;

        auto menu = CCMenu::create();
        menu->setPosition(ccp(CCDirector::get()->getWinSize().width / 2, 24));
        menu->setVisible(false);

        auto freezeSpr = ButtonSprite::create("Freeze", "bigFont.fnt", "GJ_button_04.png");
        freezeSpr->setScale(0.5f);
        auto freezeBtn = CCMenuItemSpriteExtra::create(freezeSpr, this, menu_selector(FrameStepPlayLayer::onFreeze));
        freezeBtn->setPosition(ccp(-45, 0));
        m_fields->freezeLabel = freezeSpr->m_label;

        auto stepSpr = ButtonSprite::create("Step", "bigFont.fnt", "GJ_button_01.png");
        stepSpr->setScale(0.5f);
        auto stepBtn = CCMenuItemSpriteExtra::create(stepSpr, this, menu_selector(FrameStepPlayLayer::onStep));
        stepBtn->setPosition(ccp(45, 0));

        menu->addChild(freezeBtn);
        menu->addChild(stepBtn);
        m_uiLayer->addChild(menu, 100);
        m_fields->menu = menu;
    }

    void postUpdate(float dt)
    {
        PlayLayer::postUpdate(dt);

        if (m_fields->menu)
            m_fields->menu->setVisible(FrameStep::get()->getRealEnabled());
    }

    void onQuit()
    {
        g_frozen = false;
        g_steps = 0;
        PlayLayer::onQuit();
    }
};
