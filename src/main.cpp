#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>

using namespace geode::prelude;

static int g_divisiones = 2;

static void leerAjuste() {
    auto texto = Mod::get()->getSettingValue<std::string>("divisiones");
    int valor = 2;
    try {
        valor = std::stoi(texto);
    } catch (...) {}
    if (valor != 1 && valor != 2 && valor != 4) valor = 2;
    g_divisiones = valor;
}

$on_mod(Loaded) {
    leerAjuste();
    listenForSettingChanges<std::string>("divisiones", [](std::string) {
        leerAjuste();
    });
}

class $modify(CCScheduler) {
    void update(float dt) {
        int n = g_divisiones;
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
