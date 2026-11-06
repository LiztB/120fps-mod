#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

static int g_max = 2;        // lo que eliges en el menú
static int g_actual = 2;     // lo que usa el mod ahora mismo
static bool g_auto = true;
static int g_lentos = 0;
static int g_rapidos = 0;

static void leerAjustes() {
    auto texto = Mod::get()->getSettingValue<std::string>("divisiones");
    int valor = 2;
    try {
        valor = std::stoi(texto);
    } catch (...) {}
    if (valor != 1 && valor != 2 && valor != 4) valor = 2;
    g_max = valor;
    g_actual = valor;
    g_auto = Mod::get()->getSettingValue<bool>("automatico");
    g_lentos = 0;
    g_rapidos = 0;
}

$on_mod(Loaded) {
    leerAjustes();
    listenForSettingChanges<std::string>("divisiones", [](std::string) {
        leerAjustes();
    });
    listenForSettingChanges<bool>("automatico", [](bool) {
        leerAjustes();
    });
}

class $modify(GJBaseGameLayer) {
    void update(float dt) {
        if (g_auto && g_max > 1) {
            if (dt > 0.0235f) {
                g_lentos++;
                g_rapidos = 0;
            } else if (dt < 0.0185f) {
                g_rapidos++;
                g_lentos = 0;
            } else {
                g_lentos = 0;
                g_rapidos = 0;
            }

            if (g_lentos >= 3 && g_actual > 1) {
                g_actual /= 2;
                g_lentos = 0;
            }
            if (g_rapidos >= 120 && g_actual < g_max) {
                g_actual *= 2;
                g_rapidos = 0;
            }
        }

        int n = g_actual;
        if (n <= 1) {
            GJBaseGameLayer::update(dt);
            return;
        }
        float parte = dt / n;
        for (int i = 0; i < n; i++) {
            GJBaseGameLayer::update(parte);
        }
    }
};
