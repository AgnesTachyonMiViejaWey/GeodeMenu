#include "MacroManager.hpp"
#include <algorithm>
#include <sstream>

using namespace geode::prelude;

MacroManager* MacroManager::get()
{
    static MacroManager instance;
    return &instance;
}

bool MacroManager::isInjecting()
{
    return injecting;
}

void MacroManager::setInjecting(bool injecting)
{
    this->injecting = injecting;
}

int MacroManager::getLoadedLevel()
{
    return loadedLevel;
}

void MacroManager::setLoadedLevel(int levelID)
{
    loadedLevel = levelID;
}

size_t MacroManager::getInputCount()
{
    return inputs.size();
}

size_t MacroManager::getCursor()
{
    return cursor;
}

void MacroManager::clear()
{
    inputs.clear();
    cursor = 0;
    lastFrame = 0;
}

void MacroManager::record(unsigned int frame, int button, bool down, bool player1)
{
    MacroInput input;
    input.frame = frame;
    input.button = button;
    input.down = down;
    input.player1 = player1;

    inputs.push_back(input);
    lastFrame = frame;
}

void MacroManager::truncateFrom(unsigned int frame)
{
    auto it = std::lower_bound(inputs.begin(), inputs.end(), frame, [](MacroInput const& input, unsigned int f) {
        return input.frame < f;
    });

    inputs.erase(it, inputs.end());

    cursor = std::min(cursor, inputs.size());
    lastFrame = frame;
}

void MacroManager::seek(unsigned int frame)
{
    auto it = std::lower_bound(inputs.begin(), inputs.end(), frame, [](MacroInput const& input, unsigned int f) {
        return input.frame < f;
    });

    cursor = (size_t)std::distance(inputs.begin(), it);
    lastFrame = frame;
}

std::vector<MacroInput> MacroManager::takeDue(unsigned int frame)
{
    std::vector<MacroInput> due = {};

    // el tiempo fue hacia atrás (reinicio o checkpoint): recolocar el cursor
    if (frame < lastFrame)
        seek(frame);

    lastFrame = frame;

    while (cursor < inputs.size() && inputs[cursor].frame <= frame)
    {
        due.push_back(inputs[cursor]);
        cursor++;
    }

    return due;
}

bool MacroManager::save(int levelID)
{
    if (inputs.empty())
        return false;

    std::string out = "QMACRO 1\n";

    for (auto const& input : inputs)
        out += fmt::format("{} {} {} {}\n", input.frame, input.button, input.down ? 1 : 0, input.player1 ? 1 : 0);

    auto path = Mod::get()->getSaveDir() / fmt::format("macro-{}.qmacro", levelID);
    auto res = file::writeString(path, out);

    if (!res.isOk())
    {
        log::error("Failed to save macro for level {}", levelID);
        return false;
    }

    log::info("Saved macro for level {} ({} inputs)", levelID, inputs.size());
    return true;
}

bool MacroManager::load(int levelID)
{
    clear();

    auto path = Mod::get()->getSaveDir() / fmt::format("macro-{}.qmacro", levelID);

    if (!path.exists())
        return false;

    auto res = file::readString(path);

    if (!res.isOk())
        return false;

    std::istringstream stream(res.unwrap());
    std::string header;
    std::getline(stream, header);

    if (header.rfind("QMACRO", 0) != 0)
    {
        log::error("Macro file for level {} has an invalid header", levelID);
        return false;
    }

    unsigned int frame = 0;
    int button = 0;
    int down = 0;
    int player1 = 0;

    while (stream >> frame >> button >> down >> player1)
    {
        MacroInput input;
        input.frame = frame;
        input.button = button;
        input.down = down != 0;
        input.player1 = player1 != 0;

        inputs.push_back(input);
    }

    std::stable_sort(inputs.begin(), inputs.end(), [](MacroInput const& a, MacroInput const& b) {
        return a.frame < b.frame;
    });

    log::info("Loaded macro for level {} ({} inputs)", levelID, inputs.size());
    return !inputs.empty();
}
