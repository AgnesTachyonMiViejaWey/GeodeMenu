#include "../../Client/Module.hpp"
#include <Geode/modify/PlayLayer.hpp>
#include <ctime>

using namespace geode::prelude;

#define QOL_MODULE(cls, id, name, desc) \
class cls : public Module \
{ \
    public: \
        MODULE_SETUP(cls) \
        { \
            setName(name); \
            setID(id); \
            setCategory("QOL"); \
            setDescription(desc); \
        } \
}; \
SUBMIT_HACK(cls);

QOL_MODULE(QolFPS, "qol-fps", "FPS Counter", "Shows your current FPS in the corner of the screen")
QOL_MODULE(QolClock, "qol-clock", "Clock", "Shows the current time of day while playing")
QOL_MODULE(QolTimer, "qol-timer", "Session Timer", "Shows how long you have been in this level")
QOL_MODULE(QolAttempts, "qol-attempts", "Session Attempts", "Shows how many attempts you did since opening this level")
QOL_MODULE(QolBest, "qol-best", "Session Best", "Shows your best percentage since opening this level")
QOL_MODULE(QolPracticeDeaths, "qol-practice-deaths", "Practice Deaths", "Counts the deaths you have in practice mode in this level")

class $modify (QolPlayLayer, PlayLayer)
{
    struct Fields
    {
        CCLabelBMFont* label = nullptr;
        float elapsed = 0.f;
        float fps = 0.f;
        int attempts = 1;
        int practiceDeaths = 0;
        float best = 0.f;
    };

    void resetLevel()
    {
        PlayLayer::resetLevel();
        m_fields->attempts++;
    }

    void destroyPlayer(PlayerObject* p0, GameObject* p1)
    {
        bool wasDead = m_player1 && m_player1->m_isDead;

        PlayLayer::destroyPlayer(p0, p1);

        if (!wasDead && m_player1 && m_player1->m_isDead)
        {
            m_fields->best = std::max(m_fields->best, (float)getCurrentPercent());

            if (m_isPracticeMode)
                m_fields->practiceDeaths++;
        }
    }

    void postUpdate(float dt)
    {
        PlayLayer::postUpdate(dt);

        auto f = m_fields.self();

        f->elapsed += dt;

        if (dt > 0.f)
            f->fps = f->fps * 0.9f + (1.f / dt) * 0.1f;

        std::string text = "";

        if (QolFPS::get()->getRealEnabled())
            text += fmt::format("FPS {}\n", (int)(f->fps + 0.5f));

        if (QolClock::get()->getRealEnabled())
        {
            std::time_t t = std::time(nullptr);
            char buf[16] = {0};
            std::strftime(buf, sizeof(buf), "%H:%M", std::localtime(&t));
            text += fmt::format("{}\n", buf);
        }

        if (QolTimer::get()->getRealEnabled())
        {
            int s = (int)f->elapsed;
            text += fmt::format("{:02}:{:02}\n", s / 60, s % 60);
        }

        if (QolAttempts::get()->getRealEnabled())
            text += fmt::format("Att {}\n", f->attempts);

        if (QolBest::get()->getRealEnabled())
            text += fmt::format("Best {}%\n", (int)std::max(f->best, (float)getCurrentPercent()));

        if (QolPracticeDeaths::get()->getRealEnabled())
            text += fmt::format("Prac {}\n", f->practiceDeaths);

        if (text.empty())
        {
            if (f->label)
                f->label->setVisible(false);

            return;
        }

        if (!f->label && m_uiLayer)
        {
            f->label = CCLabelBMFont::create("", "bigFont.fnt");
            f->label->setScale(0.35f);
            f->label->setOpacity(170);
            f->label->setAnchorPoint({0.f, 1.f});
            f->label->setAlignment(kCCTextAlignmentLeft);
            f->label->setPosition(ccp(8, CCDirector::get()->getWinSize().height - 8));
            m_uiLayer->addChild(f->label, 100);
        }

        if (f->label)
        {
            f->label->setVisible(true);
            f->label->setString(text.c_str());
        }
    }
};
