#include "../../Client/Module.hpp"
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/utils/general.hpp>

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

QOL_MODULE(QolCopyID, "qol-copy-id", "Auto Copy Level ID", "Copies the level ID to your clipboard whenever you open a level page")
QOL_MODULE(QolMutePractice, "qol-mute-practice", "Mute Music In Practice", "Turns the level music off while you are in practice mode")
QOL_MODULE(QolMuteDeath, "qol-mute-death", "Mute Music On Death", "Silences the music when you die, until you respawn")

static void qolSetMusicVolume(bool muted)
{
    auto engine = FMODAudioEngine::get();

    if (!engine)
        return;

    engine->setBackgroundMusicVolume(muted ? 0.f : GameManager::get()->m_bgVolume);
}

class $modify (QolLevelInfo, LevelInfoLayer)
{
    bool init(GJGameLevel* level, bool challenge)
    {
        if (!LevelInfoLayer::init(level, challenge))
            return false;

        if (QolCopyID::get()->getRealEnabled() && level && level->m_levelID.value() > 0)
            geode::utils::clipboard::write(std::to_string(level->m_levelID.value()));

        return true;
    }
};

class $modify (QolPlayLayer, PlayLayer)
{
    void togglePracticeMode(bool practice)
    {
        PlayLayer::togglePracticeMode(practice);

        if (QolMutePractice::get()->getRealEnabled() || !practice)
            qolSetMusicVolume(practice && QolMutePractice::get()->getRealEnabled());
    }

    void destroyPlayer(PlayerObject* p0, GameObject* p1)
    {
        bool wasDead = m_player1 && m_player1->m_isDead;

        PlayLayer::destroyPlayer(p0, p1);

        if (!wasDead && m_player1 && m_player1->m_isDead && QolMuteDeath::get()->getRealEnabled())
            qolSetMusicVolume(true);
    }

    void resetLevel()
    {
        PlayLayer::resetLevel();

        bool keepMuted = m_isPracticeMode && QolMutePractice::get()->getRealEnabled();
        qolSetMusicVolume(keepMuted);
    }

    void onQuit()
    {
        qolSetMusicVolume(false);
        PlayLayer::onQuit();
    }
};
