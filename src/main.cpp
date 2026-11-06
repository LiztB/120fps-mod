#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <vector>

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

// Entrenador de timing
struct PuntoRef {
    float x;
    bool down;
    int paso;
};
static bool g_entrenador = false;
static bool g_grabando = false;
static bool g_vioNivel = false;
static std::vector<PuntoRef> g_refs;
static std::string g_nivelRef;
static float g_xPrev = 0.f;
static float g_vx = 5.4f;

// pasos de física por cada frame de 60 FPS (240 por segundo)
constexpr float PASOS_POR_FRAME = 4.0f;

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
    g_entrenador = Mod::get()->getSettingValue<bool>("entrenador");
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

static std::string nombreNivel(PlayLayer* pl) {
    if (!pl || !pl->m_level) return "";
    std::string nombre = pl->m_level->m_levelName;
    return nombre;
}

static void mostrarTexto(PlayLayer* pl, std::string const& texto, ccColor3B color) {
    auto etiqueta = static_cast<CCLabelBMFont*>(pl->getChildByID("timing-trainer-label"_spr));
    if (!etiqueta) {
        etiqueta = CCLabelBMFont::create("", "bigFont.fnt");
        etiqueta->setID("timing-trainer-label"_spr);
        etiqueta->setScale(0.5f);
        auto win = CCDirector::get()->getWinSize();
        etiqueta->setPosition({win.width / 2.f, win.height - 40.f});
        pl->addChild(etiqueta, 1000);
    }
    etiqueta->setString(texto.c_str());
    etiqueta->setColor(color);
    etiqueta->stopAllActions();
    etiqueta->setOpacity(255);
    etiqueta->runAction(CCSequence::create(
        CCDelayTime::create(1.0f), CCFadeOut::create(0.3f), nullptr));
}

static void alPulsar(PlayLayer* pl, bool down) {
    if (!pl || !pl->m_player1) return;
    float x = pl->m_player1->getPositionX();
    int paso = static_cast<int>(pl->m_gameState.m_currentProgress);

    if (g_grabando) {
        if (g_refs.empty()) g_nivelRef = nombreNivel(pl);
        g_refs.erase(
            std::remove_if(g_refs.begin(), g_refs.end(),
                           [x](PuntoRef const& r) { return r.x >= x; }),
            g_refs.end());
        g_refs.push_back({x, down, paso});
        return;
    }

    if (!g_entrenador || g_refs.empty() || nombreNivel(pl) != g_nivelRef) return;

    bool hay = false;
    float mejor = 0.f;
    int mejorPaso = 0;
    for (auto const& r : g_refs) {
        if (r.down != down) continue;
        float dx = x - r.x;
        if (!hay || std::fabs(dx) < std::fabs(mejor)) {
            mejor = dx;
            mejorPaso = r.paso;
            hay = true;
        }
    }
    if (!hay || std::fabs(mejor) > 250.f) return;

    float framesX = mejor / g_vx;
    float framesPaso = static_cast<float>(paso - mejorPaso) / PASOS_POR_FRAME;

    float frames = framesPaso;
    bool aprox = false;
    if (std::fabs(framesPaso - framesX) > 3.f) {
        frames = framesX;
        aprox = true;
    }

    std::string accion = down ? "Clic: " : "Soltar: ";
    std::string extra = aprox ? " (aprox)" : "";
    char numero[16];
    std::snprintf(numero, sizeof(numero), "%.1f", std::fabs(frames));

    if (std::fabs(frames) < 0.25f) {
        mostrarTexto(pl, accion + "Perfecto" + extra, ccColor3B{80, 255, 80});
    } else if (frames > 0.f) {
        mostrarTexto(pl, accion + "+" + numero + " tarde" + extra, ccColor3B{255, 120, 80});
    } else {
        mostrarTexto(pl, accion + numero + " temprano" + extra, ccColor3B{80, 180, 255});
    }
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
        if (g_grabando && g_vioNivel) {
            g_grabando = false;
            g_vioNivel = false;
            Mod::get()->setSettingValue<bool>("entrenador_grabar", false);
        }
        return;
    }

    if (g_grabando) g_vioNivel = true;

    float x = pl->m_player1->getPositionX();
    float v = x - g_xPrev;
    if (v > 0.5f && v < 40.f) g_vx = v;
    g_xPrev = x;

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
    g_grabando = Mod::get()->getSettingValue<bool>("entrenador_grabar");
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
    listenForSettingChanges<bool>("entrenador", [](bool) {
        leerAjustes();
    });
    listenForSettingChanges<bool>("entrenador_grabar", [](bool valor) {
        g_grabando = valor;
        g_vioNivel = false;
        if (valor) {
            g_refs.clear();
            g_nivelRef.clear();
        }
    });
}

class $modify(GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (!g_inyectando && button == 1 && isPlayer1 && m_player1) {
            alPulsar(PlayLayer::get(), down);

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

// Botón de ajustes en la pausa
class $modify(MiPausa, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        auto boton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Ajustes"), this, menu_selector(MiPausa::abrirAjustes));
        menu->addChild(boton);
        menu->setScale(0.6f);
        menu->setPosition({win.width - 55.f, 25.f});
        this->addChild(menu);
    }

    void abrirAjustes(CCObject*) {
        openSettingsPopup(Mod::get());
    }
};

// Botón de ajustes antes de entrar al nivel
class $modify(MiNivelInfo, LevelInfoLayer) {
    bool init(GJGameLevel* nivel, bool desafio) {
        if (!LevelInfoLayer::init(nivel, desafio)) return false;
        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        auto boton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Ajustes"), this, menu_selector(MiNivelInfo::abrirAjustes));
        menu->addChild(boton);
        menu->setScale(0.6f);
        menu->setPosition({win.width - 55.f, 25.f});
        this->addChild(menu);
        return true;
    }

    void abrirAjustes(CCObject*) {
        openSettingsPopup(Mod::get());
    }
};
