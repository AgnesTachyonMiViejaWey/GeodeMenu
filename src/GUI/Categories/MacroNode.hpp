#pragma once

#include "../../Client/CategoryNode.hpp"
#include "../InputNode.hpp"
#include "../../Utils/AdvancedLabel/AdvLabelBMFont.hpp"
#include <Button.hpp>

using namespace geode::prelude;

class MacroNode : public CategoryNode
{
    protected:
        AdvLabelBMFont* statusLabel = nullptr;
        AdvLabelBMFont* selectedLabel = nullptr;
        CCMenu* listMenu = nullptr;
        qolmod::InputNode* nameInput = nullptr;

    public:
        CREATE_FUNC(MacroNode)

        bool init();
        virtual void setContentSize(const CCSize& size);
        void update(float dt);
        void updateList();
        void refresh();

        void onModeOff(CCObject* sender);
        void onModeRecord(CCObject* sender);
        void onModePlay(CCObject* sender);
        void onSave(CCObject* sender);
        void onSelect(CCObject* sender);
        void onDelete(CCObject* sender);
        void onExport(CCObject* sender);
        void onImport(CCObject* sender);
};

SUBMIT_CATEGORY("Macro", MacroNode)
