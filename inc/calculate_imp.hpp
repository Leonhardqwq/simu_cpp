# pragma once
#include "calculate_position_new.hpp"

class ImpCalculator {
private:
    rng rng;

public:
    // 自动入参
    int M;

    // 手动入参
    Scene scene=Scene::POOL;
    float randfloat100=-1;
    float x_giga;
    unsigned int row_giga;
    CdState state_giga;
    bool higher_imp = false;

    // 过程变量
    float rnd;

    // 结果
    std::vector<float> x_ans;
    std::vector<float> y_ans;
    std::vector<float> dy_ans;    
    int t_enter = -1; // 啃食时机
    int res = -1;    // 转为walking时机

    ImpCalculator(
        int M,
        Scene scene,
        float randfloat100,
        float x_giga,
        unsigned int row_giga,
        CdState state_giga,
        bool higher_imp
    ) : M(M), scene(scene), randfloat100(randfloat100), x_giga(x_giga), row_giga(row_giga), state_giga(state_giga), higher_imp(higher_imp) {}

private:
    void stay() {
        x_ans.push_back(x_ans.back());
        y_ans.push_back(y_ans.back());
        dy_ans.push_back(dy_ans.back());
    }

    float get_y_by_col(Scene s, unsigned int row, unsigned int col) {
        float y;
        if (s == Scene::ROOF) {
            float offset = static_cast<float>(col < 5 ? 20 * (5 - col) : 0);
            y = (85 * row + offset + 80) - 10;
        } else if (s == Scene::POOL) {
            y = static_cast<float>(85 * row + 80);
        } else {
            y = static_cast<float>(100 * row + 80);
        }
        return y;
    }

    float get_y_by_row_and_x(Scene s, unsigned int row, float x) {
        if (s == Scene::ROOF) {
            float offset = static_cast<float>(x < 440 ?
                (440.0f - static_cast<long double>(x)) * 0.25 :
                0);
            return get_y_by_col(s, row, 8) + offset;
        } else {
            return get_y_by_col(s, row, 0);
        }
    }

    // 气球 跳跳 例外
    float zombie_init_y(Scene s, float x, unsigned int row) {
        auto y = get_y_by_row_and_x(s, row, x + 40) - 30;
        return y;
    }

public:
    void init() {
        t_enter = -1; res = -1;
        x_ans.clear(); y_ans.clear(); dy_ans.clear();
        if (randfloat100 == -1) 
            rnd = rng.randfloat(0, 100);
        else 
            rnd = randfloat100;
    }

    void calculate_imp() {
        // 僵尸status 巨人投掷
        float d2y = x_giga - 360.0f;
        // 特殊检查
        if (d2y <= 40.0f) {
            printf("Giga could not throw imp.\n");
            fflush(stdout);
        }

        float min_d2y = 40.0f;
        if (scene == Scene::ROOF){
            d2y -= 180.0f;
            min_d2y = -140.0f;
        }
        if (d2y < min_d2y) {
            d2y = min_d2y;
        }
        else if (d2y > 140.0f) {
            d2y -= rnd;
        }

        int status = 71;
        float x = x_giga - 133.0f;
        float y = zombie_init_y(scene, x_giga, row_giga);
        float dx = 3.0f;
        float dy = 88.0f;
        d2y = 0.5f * (d2y / dx) * 0.05f;

        CdState state;
        Reanim reanim(24.0f, 6);
        int n_repeated = 0;
        state.slow_cd = state_giga.slow_cd;
        size_t i = higher_imp ? 0 : 1;
        if (!higher_imp) {
            x_ans.push_back(x);
            y_ans.push_back(y);
            dy_ans.push_back(dy);
        }
        
        // flying 71
        for (; i < M; i++) {
            // cd
            state.tick();
            // status
            d2y -= 0.05f;
            dy += d2y;
            x -= dx;
            auto new_y = get_y_by_row_and_x(scene, row_giga, x) - 30.0f;
            auto new_dy = dy + new_y - y;
            y = new_y;
            dy = new_dy;
            if (dy <= 0) {
                dy = 0;
                status = 72;
                // position
                x_ans.push_back(x);
                y_ans.push_back(y);
                dy_ans.push_back(dy);  
                // progress
                reanim.update(state);
                i++; break;
            }
            // position
            x_ans.push_back(x);
            y_ans.push_back(y);
            dy_ans.push_back(dy);  
        }

        // landing 72
        for (; i < M; i++) {
            // cd
            state.tick();
            // status
            if (n_repeated > 0) {
                status = 0;
                if (res == -1) res = i;
                // position
                stay();
                // eat
                int op = state.get_eat_interval();
                if (i % op == 0) {
                    if (t_enter == -1) 
                        t_enter = i;
                }
                i++; break;
            }
            // position
            stay();
            // progress
            reanim.update(state);
            n_repeated += reanim.wrap_with_count();
        }

        // norm 0
        for (; i < M; i++) {
            // cd
            state.tick();
            // position
            stay();
            // eat
            if (state.frozen_cd > 0) continue;
            if (t_enter != -1) continue;
            int op = state.get_eat_interval();
            if (i % op == 0) {
                t_enter = i;
            }
        }

    }
    
};
