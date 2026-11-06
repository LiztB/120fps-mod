#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>

using namespace geode::prelude;

static int g_max = 2;
static int g_actual = 2;
static int g_wave = 2;
static bool g_waveActivo = true;
static bool g_auto = true;
static int g_lentos = 0;
static int g_rapidos = 0;

static int leerDivisiones(char const* clave, int porDefecto) {
    auto texto = Mod::get()->getSettingValue<std::string>(clave);
    int valor = porDefecto;
    try {
        valor = std::stoi(texto);
    } catch (...) {}
    if (valor != 1 && valor != 2 && valor != 4) valor = porDefecto;
    return valor;
}

static void leerAjustes() {
    g_max = leerDivisiones("divisiones", 2);
    g_actual = g_max;
    g_wave = leerDivisiones("divisiones_wave", 2);
    g_waveActivo = Mod::get()->getSettingValue<bool>("wave_activo");
    g_auto = Mod::get()->getSettingValue<bool>("automatico");
    g_lentos = 0;
    g_rapidos = 0;
}

static bool enWave() {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_player1) return false;
    return pl->m_player1->m_isDart;
}

$on_mod(Loaded) {
    leerAjustes();
    listenForSettingChanges<std::string>("divisiones", [](std::string) {
        leerAjustes();
    });
    listenForSettingChanges<std::string>("divisiones_wave", [](std::string) {
        leerAjustes();
    });
    listenForSettingChanges<bool>("wave_activo", [](bool) {
        leerAjustes();
    });
    listenForSettingChanges<bool>("automatico", [](bool) {
        leerAjustes();
    });
}

class $modify(CCScheduler) {
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
        if (g_waveActivo && enWave() && n > g_wave) n = g_wave;

        if (n <= 1) {
            CCScheduler::update(dt);
            return;
        }
        float parte = dt / n;
        for (int i = 0; i < n; i++) {
            CCScheduler::update(parte);
        }
    }
};
