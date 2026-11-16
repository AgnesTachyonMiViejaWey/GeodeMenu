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

double MacroManager::getTPS()
{
    return macroTPS;
}

void MacroManager::setTPS(double tps)
{
    macroTPS = tps;
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
    checkpoints.clear();
    cursor = 0;
    lastFrame = 0;
}

void MacroManager::markCheckpoint(const void* checkpoint)
{
    checkpoints[checkpoint] = { inputs.size(), cursor };
}

bool MacroManager::restoreCheckpoint(const void* checkpoint, MacroState state)
{
    auto it = checkpoints.find(checkpoint);

    if (it == checkpoints.end())
        return false;

    if (state == MacroState::Recording)
    {
        // volver exactamente al estado del macro cuando se creó el checkpoint
        if (it->second.first < inputs.size())
            inputs.resize(it->second.first);

        cursor = std::min(cursor, inputs.size());
    }
    else if (state == MacroState::Playback)
    {
        cursor = std::min(it->second.second, inputs.size());
    }

    return true;
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

std::string MacroManager::serialize()
{
    std::string out = fmt::format("QMACRO 2 {}\n", macroTPS);

    for (auto const& input : inputs)
        out += fmt::format("{} {} {} {}\n", input.frame, input.button, input.down ? 1 : 0, input.player1 ? 1 : 0);

    return out;
}

bool MacroManager::parse(std::string const& text)
{
    clear();

    std::istringstream stream(text);
    std::string header;
    std::getline(stream, header);

    std::istringstream headerStream(header);
    std::string magic;
    int version = 1;
    double tps = 0.0;
    headerStream >> magic >> version >> tps;

    if (magic != "QMACRO")
        return false;

    macroTPS = tps;

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

    return !inputs.empty();
}

bool MacroManager::save(int levelID)
{
    if (inputs.empty())
        return false;

    auto path = Mod::get()->getSaveDir() / fmt::format("macro-{}.qmacro", levelID);
    auto res = file::writeString(path, serialize());

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

    bool ok = parse(res.unwrap());

    if (ok)
        log::info("Loaded macro for level {} ({} inputs)", levelID, inputs.size());
    else
        log::error("Macro file for level {} is invalid or empty", levelID);

    return ok;
}

bool MacroManager::exportTo(asp::fs::Path const& path)
{
    if (inputs.empty())
        return false;

    return file::writeString(path, serialize()).isOk();
}

bool MacroManager::importFrom(asp::fs::Path const& path)
{
    auto res = file::readString(path);

    if (!res.isOk())
        return false;

    return parse(res.unwrap());
}
