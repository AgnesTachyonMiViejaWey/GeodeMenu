#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include <unordered_map>
#include <utility>
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
        double macroTPS = 0.0;

        // por cada checkpoint de práctica: {inputs grabados, inputs ya reproducidos} al crearlo
        std::unordered_map<const void*, std::pair<size_t, size_t>> checkpoints = {};

    public:
        static MacroManager* get();

        // true mientras el macro está inyectando sus propios inputs
        bool isInjecting();
        void setInjecting(bool injecting);

        // id del nivel al que pertenecen los inputs en memoria (-1 = ninguno)
        int getLoadedLevel();
        void setLoadedLevel(int levelID);

        // TPS con el que se grabó el macro (0 = vanilla / sin TPS bypass)
        double getTPS();
        void setTPS(double tps);

        size_t getInputCount();
        size_t getCursor();

        void clear();

        // grabación
        void record(unsigned int frame, int button, bool down, bool player1);
        void truncateFrom(unsigned int frame);

        // checkpoints de modo práctica
        void markCheckpoint(const void* checkpoint);
        bool restoreCheckpoint(const void* checkpoint, MacroState state);

        // reproducción
        void seek(unsigned int frame);
        std::vector<MacroInput> takeDue(unsigned int frame);

        // archivos (uno por nivel, en la carpeta de guardado del mod)
        bool save(int levelID);
        bool load(int levelID);

        // archivo elegido por el usuario (exportar / importar)
        bool exportTo(asp::fs::Path const& path);
        bool importFrom(asp::fs::Path const& path);

    protected:
        std::string serialize();
        bool parse(std::string const& text);
};
