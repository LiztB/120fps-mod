#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>

using namespace geode::prelude;

// 2 = física y clics a 120, 4 = a 240
constexpr int DIVISIONES = 2;

class $modify(CCScheduler) {
    void update(float dt) {
        float parte = dt / DIVISIONES;
        for (int i = 0; i < DIVISIONES; i++) {
            CCScheduler::update(parte);
        }
    }
};
