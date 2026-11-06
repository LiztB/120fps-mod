#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <deque>

using namespace geode::prelude;

static int g_max = 2;
static int g_actual = 2;
static int g_wave = 2;
static bool g_waveActivo = true;
static bool g_auto = true;
static int g_lentos = 0;
static int g_rapidos = 0;

static int g_bufferMax = 0;
static int g_retraso = 0;
static int g_cola = 0;
static bool g_soltado = false;
static bool g_liberar = false;
static bool g_inyectando = false;

struct Evento {
    bool down;
    int button;
    bool isPlayer1;
    int frames;
};
static std::deque<Evento> g_eventos;

static int leerDivisiones(char const* clave, int porDefecto) {
    auto texto = Mod::get()->getSettingValue<std::string>(clave);
    int valor = porDefecto;
    try {
        valor = std::stoi(texto);
    } catch (...) {}
    if (valor != 1 && valor != 2 && valor != 4) valor = porDefecto;
    return valor;
}

static int leerFrames(char const* clave) {
    auto texto = Mod::get()->getSettingValue<std::string>(clave);
    int valor = 0;
    try {
        valor = std::stoi(texto);
    } catch (...) {}
    if (valor < 0 || valor > 30) valor = 0;
    return valor;
}

static void leerAjustes() {
    g_max = leerDivisiones("divisiones", 2);
    g_actual = g_max;
    g_wave = leerDivisiones("divisiones_wave", 2);
    g_waveActivo = Mod::get()->getSettingValue<bool>("wave_activo");
    g_auto = Mod::get()->getSettingValue<bool>("automatico");
    g_bufferMax = leerFrames("buffer_frames");
    g_retraso = leerFrames("retraso_frames");
    g_lentos = 0;
    g_rapidos = 0;
    g_cola = 0;
    g_liberar = false;
    g_eventos.clear();
}

static bool enWave() {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_player1) return false;
    return pl->m_player1->m_isDart;
}

// cubo, ball, robot y spider
static bool esDeSuelo(PlayerObject* p) {
    return !(p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing);
}

static void inyectar(bool down, int button, bool isPlayer1) {
    auto pl = PlayLayer::get();
    if (!pl) return;
    g_inyectando = true;
    pl->handleButton(down, button, isPlayer1);
    g_inyectando = false;
}

static void procesarBuffer() {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_player1) {
        g_eventos.clear();
        g_cola = 0;
        g_liberar = false;
        return;
    }

    for (auto& e : g_eventos) e.frames--;
    while (!g_eventos.empty() && g_eventos.front().frames <= 0) {
        auto e = g_eventos.front();
        g_eventos.pop_front();
        inyectar(e.down, e.button, e.isPlayer1);
    }

    if (g_cola > 0) {
        g_cola--;
        if (pl->m_player1->m_isOnGround) {
            if (g_soltado) {
                inyectar(true, 1, true);
                g_liberar = true;
            }
            g_cola = 0;
        }
    } else if (g_liberar) {
        inyectar(false, 1, true);
        g_liberar = false;
    }
}

$on_mod(Loaded) {
    leerAjustes();
    listenForSettingChanges<std::string>("divisiones", [](std::string) {
        leerAjustes();
    });
    listenForSettingChanges<std::string>("divisiones_wave", [](std::string) {
        leerAjustes();
    });
    listenForSettingChanges<std::string>("buffer_frames", [](std::string) {
        leerAjustes();
    });
    listenForSettingChanges<std::string>("retraso_frames", [](std::string) {
        leerAjustes();
    });
    listenForSettingChanges<bool>("wave_activo", [](bool) {
        leerAjustes();
    });
    listenForSettingChanges<bool>("automatico", [](bool) {
        leerAjustes();
    });
}

class $modify(GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (!g_inyectando && button == 1 && isPlayer1 && m_player1) {
            bool suelo = esDeSuelo(m_player1);

            if (!g_eventos.empty() || (g_retraso > 0 && !suelo)) {
                g_eventos.push_back({down, button, isPlayer1, g_retraso > 0 ? g_retraso : 1});
                return;
            }

            if (g_bufferMax > 0 && suelo) {
                if (down) {
                    if (!m_player1->m_isOnGround) {
                        g_cola = g_bufferMax;
                        g_soltado = false;
                    }
                } else if (g_cola > 0) {
                    g_soltado = true;
                }
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

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
        } else {
            float parte = dt / n;
            for (int i = 0; i < n; i++) {
                CCScheduler::update(parte);
            }
        }

        procesarBuffer();
    }
};
