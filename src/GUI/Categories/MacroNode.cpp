#include "MacroNode.hpp"
#include "../../Macro/MacroManager.hpp"
#include "../../Client/ButtonModule.hpp"
#include "../../Notifications/NotificationManager.hpp"
#include <LocalisationManager.hpp>

using namespace qolmod;

static std::string tr(std::string key)
{
    return LocalisationManager::get()->getLocalisedString(key);
}

static void setMode(bool record, bool play)
{
    macroSetMode(record ? 1 : (play ? 2 : 0));
}

static CCMenuItem* makeBtn(std::string text, const char* bg, CCObject* target, SEL_MenuHandler sel)
{
    auto spr = ButtonSprite::create(text.c_str(), "bigFont.fnt", bg);
    spr->setScale(0.6f);
    return qolmod::Button::create(spr, target, sel);
}

bool MacroNode::init()
{
    if (!CategoryNode::init())
        return false;

    // menú propio (recibe los toques) con el mismo tamaño que la página
    auto menu = CCMenu::create();
    menu->setAnchorPoint(ccp(0, 0));
    menu->setPosition(ccp(0, 0));
    menu->setContentSize(getContentSize());
    menu->setTag(9001);
    this->addChild(menu, 50);

    // todo anclado al borde (el tamaño real se ajusta después), y los botones
    // van directo en este CCMenu para que reciban los toques
    statusLabel = AdvLabelBMFont::createWithString("", "bigFont.fnt");
    statusLabel->setScale(0.45f);
    menu->addChildAtPosition(statusLabel, Anchor::Top, ccp(0, -20));

    auto off = makeBtn(tr("ui/macro-off"), "GJ_button_06.png", this, menu_selector(MacroNode::onModeOff));
    auto rec = makeBtn(tr("ui/macro-record"), "GJ_button_06.png", this, menu_selector(MacroNode::onModeRecord));
    auto play = makeBtn(tr("ui/macro-play"), "GJ_button_06.png", this, menu_selector(MacroNode::onModePlay));
    menu->addChildAtPosition(off, Anchor::Top, ccp(-85, -55));
    menu->addChildAtPosition(rec, Anchor::Top, ccp(0, -55));
    menu->addChildAtPosition(play, Anchor::Top, ccp(85, -55));

    nameInput = qolmod::InputNode::create(150, tr("ui/macro-name"));
    nameInput->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
    nameInput->setMaxCharCount(24);
    menu->addChildAtPosition(nameInput, Anchor::Top, ccp(-45, -95));

    auto save = makeBtn(tr("ui/macro-save"), "GJ_button_01.png", this, menu_selector(MacroNode::onSave));
    menu->addChildAtPosition(save, Anchor::Top, ccp(70, -95));

    selectedLabel = AdvLabelBMFont::createWithString("", "bigFont.fnt");
    selectedLabel->setScale(0.4f);
    menu->addChildAtPosition(selectedLabel, Anchor::Top, ccp(-30, -127));

    auto del = makeBtn(tr("ui/macro-delete"), "GJ_button_06.png", this, menu_selector(MacroNode::onDelete));
    menu->addChildAtPosition(del, Anchor::Top, ccp(80, -127));

    listMenu = CCMenu::create();
    listMenu->setAnchorPoint(ccp(0.5f, 1.f));
    listMenu->setContentSize(ccp(300, 60));
    listMenu->setLayout(RowLayout::create()->setGap(6)->setGrowCrossAxis(true)->setAxisAlignment(AxisAlignment::Center)->setAutoScale(false));
    menu->addChildAtPosition(listMenu, Anchor::Top, ccp(0, -145));

    auto exp = makeBtn(tr("ui/macro-btn-export"), "GJ_button_04.png", this, menu_selector(MacroNode::onExport));
    auto imp = makeBtn(tr("ui/macro-btn-import"), "GJ_button_04.png", this, menu_selector(MacroNode::onImport));
    menu->addChildAtPosition(exp, Anchor::Bottom, ccp(-60, 22));
    menu->addChildAtPosition(imp, Anchor::Bottom, ccp(60, 22));

    updateList();
    refresh();
    this->schedule(schedule_selector(MacroNode::update), 0.25f);
    return true;
}

void MacroNode::setContentSize(const CCSize& size)
{
    CategoryNode::setContentSize(size);

    if (auto menu = this->getChildByTag(9001))
    {
        menu->setContentSize(size);
        menu->updateLayout();
    }
}

void MacroNode::update(float dt)
{
    refresh();
}

void MacroNode::refresh()
{
    auto mgr = MacroManager::get();
    int m = macroGetMode();
    std::string mode = tr(m == 2 ? "ui/macro-play" : (m == 1 ? "ui/macro-record" : "ui/macro-off"));

    statusLabel->setString(fmt::format("{}  |  {} {}  |  TPS {}", mode, mgr->getInputCount(), tr("ui/macro-inputs"), mgr->getTPS() > 0 ? fmt::format("{}", (int)mgr->getTPS()) : std::string("240")).c_str());

    auto name = mgr->getSelectedName();
    selectedLabel->setString(name.empty() ? tr("ui/macro-none").c_str() : name.c_str());
}

void MacroNode::updateList()
{
    listMenu->removeAllChildren();

    for (auto const& name : MacroManager::get()->getNames())
    {
        auto btn = makeBtn(name, "GJ_button_05.png", this, menu_selector(MacroNode::onSelect));
        btn->setUserObject("name", CCString::create(name));
        listMenu->addChild(btn);
    }

    listMenu->updateLayout();
}

void MacroNode::onModeOff(CCObject*)
{
    setMode(false, false);
    refresh();
}

void MacroNode::onModeRecord(CCObject*)
{
    setMode(true, false);
    refresh();
}

void MacroNode::onModePlay(CCObject*)
{
    setMode(false, true);
    refresh();
}

void MacroNode::onSave(CCObject*)
{
    auto mgr = MacroManager::get();
    auto name = nameInput->getString();

    if (name.empty())
        name = mgr->getSelectedName();

    if (mgr->saveNamed(name))
    {
        NotificationManager::get()->notifyToast(tr("ui/macro-saved"));
        updateList();
    }
    else
    {
        NotificationManager::get()->notifyToast(tr("ui/macro-save-failed"));
    }

    refresh();
}

void MacroNode::onSelect(CCObject* sender)
{
    auto name = static_cast<CCString*>(static_cast<CCNode*>(sender)->getUserObject("name"))->getCString();
    auto mgr = MacroManager::get();

    // se carga ya (para ver los inputs) y se vuelve a cargar al entrar a un nivel
    if (mgr->loadNamed(name))
        mgr->setLoadedLevel(-1);

    mgr->setSelectedName(name);
    nameInput->setString(name);
    refresh();
}

void MacroNode::onDelete(CCObject*)
{
    auto mgr = MacroManager::get();

    if (!mgr->getSelectedName().empty())
        mgr->removeNamed(mgr->getSelectedName());

    updateList();
    refresh();
}

void MacroNode::onExport(CCObject*)
{
    macroExportClick();
}

void MacroNode::onImport(CCObject*)
{
    macroImportClick();
}
