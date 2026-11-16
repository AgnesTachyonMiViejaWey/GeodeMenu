#pragma once

#include <Geode/Geode.hpp>
#include <vector>

enum class MacroState
{
    None = 0,
    Recording = 1,
    Playback = 2,
};

// una pulsación de botón guardada en el frame (tick de física) en que ocurrió
struct MacroInput
{
    unsigned int frame = 0;
    int button = 1;
    bool down = false;
    bool player1 = true;
};

class MacroManager
{
    protected:
        std::vector<MacroInput> inputs = {};
        size_t cursor = 0;
        unsigned int lastFrame = 0;
        int loadedLevel = -1;
        bool injecting = false;

    public:
        static MacroManager* get();

        // true mientras el macro está inyectando sus propios inputs
        bool isInjecting();
        void setInjecting(bool injecting);

        // id del nivel al que pertenecen los inputs en memoria (-1 = ninguno)
        int getLoadedLevel();
        void setLoadedLevel(int levelID);

        size_t getInputCount();
        size_t getCursor();

        void clear();

        // grabación
        void record(unsigned int frame, int button, bool down, bool player1);
        void truncateFrom(unsigned int frame);

        // reproducción
        void seek(unsigned int frame);
        std::vector<MacroInput> takeDue(unsigned int frame);

        // archivos (uno por nivel, en la carpeta de guardado del mod)
        bool save(int levelID);
        bool load(int levelID);
};
