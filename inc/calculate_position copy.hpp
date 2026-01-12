# pragma once
# include "rng.hpp"
# include "common.hpp"
# include "util.hpp"
#include <thread>
#include <cstddef>
#include <algorithm>

// ==================== 封装工具 ====================

// 冰冻/减速倒计时管理
struct CdState {
    int frozen_cd = 0;
    int slow_cd = 0;
    int ice_idx = 0;
    int splash_idx = 0;

    // 重置
    void reset() {
        frozen_cd = 0;
        slow_cd = 0;
        ice_idx = 0;
        splash_idx = 0;
    }

    // 冷却递减
    void tick() {
        if (frozen_cd > 0) frozen_cd--;
        if (slow_cd > 0) slow_cd--;
    }

    bool is_slowed() const {return slow_cd > 0;}

    bool is_frozen() const {return frozen_cd > 0;}

    // 获取吃植物间隔
    int get_eat_interval() const {
        return is_slowed() ? 8 : 4;
    }
};

// 动画进度管理
struct Reanim {
    float progress = 0.0f;
    float fps = 0.0f;
    unsigned n_frames = 0;

    Reanim() = default;
    Reanim(float fps_, unsigned n_frames_) : fps(fps_), n_frames(n_frames_) {}
    Reanim(const ZombieData& z, float v0){calc_fps_from_zombie(z, v0);}

    // 根据僵尸数据计算fps
    void calc_fps_from_zombie(const ZombieData& z, float v0) {
        fps = z.n_frames / z.total_x * v0 * 47;
        n_frames = z.n_frames;
    }

    // 初始化进度
    void init_progress() {
        progress += static_cast<float>(fps * double(0.01f) / n_frames);
    }

    // 更新进度
    void update(const CdState& state) {
        if (state.is_frozen()) return;
        if (state.is_slowed())  progress += static_cast<float>(fps * 0.5f * double(0.01f) / n_frames);
        else                    progress += static_cast<float>(fps * double(0.01f) / n_frames);
    }

    // 无条件更新进度（不考虑冰冻状态，用于跳水等特殊阶段）
    void update_unconditional() {   progress += static_cast<float>(fps * double(0.01f) / n_frames); }

    // 进度回绕
    void wrap() {for (; progress >= 1; progress--);}

    // 带计数的进度回绕（返回回绕次数）
    int wrap_with_count() {
        int count = 0;
        for (; progress >= 1; progress--, count++);
        return count;
    }
};

// 动画位移计算器（仅计算位移，不更新进度）
class DxCalculator {
public:
    // 动画位移
    static float get_dx_from_ground(const ZombieData& z, const Reanim& reanim, const CdState& state) {
        /*
            float current_frame = progress * (z.n_frames-1) + z.begin_frame;
            double floored_current_frame = floor(current_frame);
            floored_current_frame = round(floored_current_frame);
            auto x_each = z.anim[static_cast<unsigned int>(floored_current_frame) - z.begin_frame];            
        */
        float dx = z.anim[static_cast<unsigned int>(reanim.progress*(z.n_frames-1))];
        if (state.is_frozen()) return 0.0f;
        if (state.is_slowed()) return dx * 0.01f * (reanim.fps * 0.5f);
        return dx * 0.01f * reanim.fps;
    }
    // 常量位移
    static float get_dx_constant(float v0, const CdState& state) {
        if (state.is_frozen()) return 0.0f;
        if (state.is_slowed()) return v0 * 0.4f;
        return v0;
    }
};

// 冰冻效果处理器
class IceEffect {
public:
    // 冰冻+减速
    static void apply(
        int time_point, 
        const std::vector<int>& ice_t, 
        const std::vector<int> frozen_t[2],
        CdState& state
    ) {
        if (state.ice_idx < ice_t.size() && ice_t[state.ice_idx] == time_point) {
            int freeze_duration = state.is_slowed()?frozen_t[1][state.ice_idx]:frozen_t[0][state.ice_idx];
            state.frozen_cd = std::max(freeze_duration, state.frozen_cd);
            state.slow_cd = std::max(2000, state.slow_cd);
            state.ice_idx++;
        }
    }

    // (冰冻)+减速，用于匀速移动僵尸类型校验
    static void apply_with_type_check(
        int time_point,
        const std::vector<int>& ice_t,
        const std::vector<int> frozen_t[2],
        CdState& state,
        ZombieType type
    ) {
        if (state.ice_idx < ice_t.size() && ice_t[state.ice_idx] == time_point) {
            if (type == ZombieType::Catapult) {
                int freeze_duration = state.is_slowed()?frozen_t[1][state.ice_idx]:frozen_t[0][state.ice_idx];
                state.frozen_cd = std::max(freeze_duration, state.frozen_cd);
            } // else (Balloon, Pogo)
            state.slow_cd = std::max(2000, state.slow_cd);
            state.ice_idx++;
        }
    }

    // 减速
    static void apply_slow_only(
        int time_point,
        const std::vector<int>& ice_t,
        CdState& state
    ) {
        if (state.ice_idx < ice_t.size() && ice_t[state.ice_idx] == time_point) {
            state.slow_cd = std::max(2000, state.slow_cd);
            state.ice_idx++;
        }
    }

    // 无效
    static void consume_only(
        int time_point,
        const std::vector<int>& ice_t,
        CdState& state
    ) {
        if (state.ice_idx < ice_t.size() && ice_t[state.ice_idx] == time_point) 
            state.ice_idx++;
    }

    // 固定冰冻+减速 （水中）
    static void apply_pool(
        int time_point,
        const std::vector<int>& ice_t,
        CdState& state
    ) {
        if (state.ice_idx < ice_t.size() && ice_t[state.ice_idx] == time_point) {
            state.frozen_cd = std::max(300, state.frozen_cd);
            state.slow_cd = std::max(2000, state.slow_cd);
            state.ice_idx++;
        }
    }
};

// 溅射效果处理器
class SplashEffect {
public:
    // 减速
    static void apply(
        int time_point,
        const std::vector<int>& splash_t,
        float x_current,
        int def_x_first,
        CdState& state
    ) {
        if (state.splash_idx < splash_t.size() && splash_t[state.splash_idx] == time_point) {
            if (int(x_current) + def_x_first <= 800) 
                state.slow_cd = std::max(1000, state.slow_cd);
            state.splash_idx++;
        }
    }

    // （减速），用于匀速移动僵尸类型校验
    static void apply_with_type_check(
        int time_point,
        const std::vector<int>& splash_t,
        float x_current,
        int def_x_first,
        CdState& state,
        ZombieType type
    ) {
        if (state.splash_idx < splash_t.size() && splash_t[state.splash_idx] == time_point) {
            if (type != ZombieType::Balloon) {
                if (int(x_current) + def_x_first <= 800) 
                    state.slow_cd = std::max(1000, state.slow_cd);
            } // else Balloon 不受溅射影响
            state.splash_idx++;
        }
    }
};


// ==================== 位置计算器 ====================

class PositionCalculator {
private:
    rng rng;

public:
    // 自动入参
    int M;
    ZombieData z;
    bool huge_wave;
    std::vector<int> ice_t;
    std::vector<int> splash_t;   

    // 手动入参 
    enum TypeCal{
        RANDOM,
        FASTEST,
        SLOWEST,
    } type_cal;
    int action_cd;  // jack 类外更新

    // 过程变量
    float v0;
    std::vector<int> frozen_t[2];   // 0first / 1second

    // 结果
    std::vector<float> x;
    int t_enter = -1; 
    int res = -1;    // jack爆炸时机, snorkel潜入时机, 矿工站稳时机

    PositionCalculator(
        ZombieType t, int M, bool huge_wave,
        std::vector<int> i_t, std::vector<int> s_t
    ) : z(ZombieData(t)), M(M), huge_wave(huge_wave), ice_t(i_t), splash_t(s_t) { type_cal = TypeCal::RANDOM;}

    // x0, v0, frozen time
    void init(){
        t_enter = -1;res = -1;
        x.clear();        
        float x0;
        switch (type_cal) {
            case TypeCal::RANDOM:
                x0 = float(z.spawn_l + rng.randint(z.spawn_offset));
                break;
            case TypeCal::FASTEST:
                x0 = float(z.spawn_l);
                break;
            case TypeCal::SLOWEST:
                x0 = float(z.spawn_l + z.spawn_offset - 1);
                break;
        }
        if (huge_wave)  x0 += z.spawn_hugewave_offset;
        x.push_back(x0);
        
        if(z.type == ZombieType::Zomboni)   return;

        v0 = rng.randfloat(z.speed.first, z.speed.second);
        frozen_t[0].clear(); frozen_t[1].clear();
        for(auto t:ice_t){
            int t_rand0, t_rand1;
            if (type_cal == TypeCal::RANDOM){
                t_rand0 = 400 + rng.randint(201);
                t_rand1 = 300 + rng.randint(101);
            }
            else if (type_cal == TypeCal::FASTEST){
                t_rand0 = 400;
                t_rand1 = 300;
            }
            else if (type_cal == TypeCal::SLOWEST){
                t_rand0 = 600;
                t_rand1 = 400;
            }
            frozen_t[0].push_back(t_rand0);
            frozen_t[1].push_back(t_rand1);
        }
    }

    void calculate_position() {
        if (z.type == ZombieType::Zomboni) {
            calculate_zomboni();
        }
        else if(
            z.type == ZombieType::Balloon ||
                   z.type == ZombieType::Pogo ||
            z.type == ZombieType::Catapult
        ){
            calculate_constant();
        }
        else if(z.type == ZombieType::Digger){
            calculate_digger();
        }
        else if(z.type == ZombieType::DolphinRider){
            calculate_dolphin_rider();
        }
        else if (z.type == ZombieType::Snorkel){
            calculate_snorkel();
        }
        else if(z.type == ZombieType::Catapult_Shoot){
            calculate_catapult_shoot();
        }
        else{
            calculate_animation();
        }
    }
    
private:
    // 辅助方法：检查并设置进入时机
    void check_enter(float x_val, int time) {
        if (int(x_val) <= z.threshold && t_enter == -1) 
            t_enter = time;
    }

    // 辅助方法：向左移动
    void walk_left(float dx) {x.push_back(x.back() - dx);}

    // 辅助方法：向右移动
    void walk_right(float dx) {x.push_back(x.back() + dx);}

    // 辅助方法：保持原位
    void stay() {x.push_back(x.back());}

    void calculate_zomboni() {
        float v;
        for(size_t i=1;i<M;i++){
            float x_old = x[i-1];
            double dx = std::floor(x_old - 700)/400;

            if(x_old>400){
                double dx = std::floor(double(x_old-700)) / -400;
                if (dx>0)
                    dx = dx * -0.1999999992549419 + 0.25;
                else
                    dx = 0.25;
                v = static_cast<float>(dx);
            }
            
            float x_new = x_old - v;
            x.push_back(x_new);
            if (x_old <= z.threshold && t_enter == -1)   t_enter = i;
        }
    }

    void calculate_animation() {
        // int end_frame = z.begin_frame + z.n_frames - 1;
        Reanim reanim(z, v0);
        reanim.init_progress();

        CdState state;
        
        for (size_t i = 1; i < M; i++) {
            int t = static_cast<int>(i);
            
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);

            // cd
            if (z.type == ZombieType::JackInTheBox)
                if (action_cd > 0 && state.frozen_cd == 0) action_cd--;
            
            state.tick();

            // status
            if (z.type == ZombieType::JackInTheBox)
                if (action_cd <= 0 && res == -1) res = i;
            
            // position
            float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
            walk_left(dx);

            // enter
            check_enter(x[i-1], t);

            // progress
            reanim.update(state);
            reanim.wrap();

            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }
    }

    void calculate_constant() {
        CdState state;
        
        for (size_t i = 1; i < M; i++) {
            int t = static_cast<int>(i);
            
            // plant ice
            IceEffect::apply_with_type_check(t, ice_t, frozen_t, state, z.type);

            // cd
            state.tick();

            // position
            float dx = DxCalculator::get_dx_constant(v0, state);
            walk_left(dx);

            // enter
            check_enter(x[i-1], t);

            // projectile splash
            SplashEffect::apply_with_type_check(t, splash_t, x[i], z.def_x.first, state, z.type);
        }
    }

    void calculate_digger() {
        Reanim reanim(12.0f, 21);
        int n_repeated = 0;

        CdState state;
        action_cd = 130;
        size_t i = 1;

        // dig
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::consume_only(t, ice_t, state);
            // cd
            state.tick();
            // status
            if (x[i-1] < 10) {
                // position
                stay();
                i++; break;
            }
            // position
            float dx = DxCalculator::get_dx_constant(v0, state);
            walk_left(dx);
        }

        // drill
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::consume_only(t, ice_t, state);
            // cd
            if (action_cd > 0 && state.frozen_cd == 0) action_cd--;
            state.tick();
            // status
            if (action_cd == 0) {
                if (res == -1) res = i;
                // position
                stay();
                // progress
                reanim.init_progress();
                i++; break;
            }
            // position
            stay();
        }

        // dizzy
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);
            // cd
            state.tick();
            // status
            if (state.frozen_cd <= 0 && n_repeated >= 2) {
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

        // norm
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);
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

    void calculate_dolphin_rider() {
        Reanim reanim(z, v0);
        reanim.init_progress();

        CdState state;
        size_t i = 1;

        // walking
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);
            // cd
            state.tick();
            // status
            if (int(x[i-1]) <= 720 && state.frozen_cd <= 0) {
                reanim = Reanim(16.0f, 45);
                // position
                stay();
                // progress
                reanim.init_progress();
                // projectile splash
                SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
                i++; break;
            }
            // position
            float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
            walk_left(dx);
            // progress
            reanim.update(state);
            reanim.wrap();
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }

        // jump in pool ???
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply_slow_only(t, ice_t, state);
            // cd
            state.tick();
            // status
            if (reanim.progress >= 1 && state.frozen_cd <= 0) {
                // position
                float dx = DxCalculator::get_dx_constant(v0, state);
                x.push_back(x[i-1] - dx - 70);
                // projectile splash
                SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
                i++; break;
            }
            // position
            stay();
            // progress
            reanim.update_unconditional();
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }

        // ride
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply_pool(t, ice_t, state);
            // cd
            state.tick();
            // status
            if (state.frozen_cd <= 0 && int(x[i-1]) <= 10) {
                // 速度重置
                v0 = rng.randfloat(z.speed.first, z.speed.second);
                // v0 = static_cast<float>(z.speed.second);
                reanim = Reanim(z, v0);
                // position
                float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
                walk_left(dx);
                // progress
                reanim.update(state);
                reanim.wrap();
                // projectile splash
                SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
                i++; break;
            }
            // position
            float dx = DxCalculator::get_dx_constant(v0, state);
            walk_left(dx);
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }

        // walking
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);
            // cd
            state.tick();
            // position
            float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
            walk_left(dx);
            // enter
            check_enter(x[i-1], t);
            // progress
            reanim.update(state);
            reanim.wrap();
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }
    }

    void calculate_snorkel() {
        Reanim reanim(z, v0);
        reanim.init_progress();

        CdState state;
        size_t i = 1;

        // walking
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);
            // cd
            state.tick();
            // status
            if (int(x[i-1]) <= 720 && state.frozen_cd <= 0) {
                v0 = 0.2f; 
                reanim = Reanim(16.0f, 19);
                // position
                walk_left(v0);
                // progress
                reanim.init_progress();
                // projectile splash
                SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
                i++; break;
            }
            // position
            float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
            walk_left(dx);
            // progress
            reanim.update(state);
            reanim.wrap();
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }

        // jump in pool ???
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply_slow_only(t, ice_t, state);
            // cd
            state.tick();
            // status
            if (reanim.progress >= 1 && state.frozen_cd <= 0) {
                // status
                if (res == -1) res = i;
                float dx = DxCalculator::get_dx_constant(v0, state);
                walk_left(dx);
                // projectile splash
                SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
                i++; break;
            }
            // position
            walk_left(v0);
            // progress
            reanim.update_unconditional();
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }

        // swim
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply_pool(t, ice_t, state);
            // cd
            state.tick();
            // status
            if (state.frozen_cd <= 0 && int(x[i-1]) <= 25) {
                // 速度重置
                v0 = rng.randfloat(z.speed.first, z.speed.second);
                // v0 = static_cast<float>(z.speed.first);
                reanim = Reanim(z, v0);
                // position
                float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
                x.push_back(x[i-1] - 15.0f - dx);
                // progress
                reanim.update(state);
                reanim.wrap();
                // projectile splash
                SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
                i++; break;
            }
            // position
            float dx = DxCalculator::get_dx_constant(v0, state);
            walk_left(dx);
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }

        // walking
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);
            // cd
            state.tick();
            // position
            float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
            walk_left(dx);
            // enter
            check_enter(x[i-1], t);
            // progress
            reanim.update(state);
            reanim.wrap();
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }
    }

    void calculate_catapult_shoot() {
        Reanim reanim(24.0f, 32);

        CdState state;
        size_t i = 1;

        // move
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);
            // cd
            state.tick();
            // status ???
            if (state.frozen_cd <= 0 && x[i-1] <= 650) {
                // position
                stay();
                // progress
                reanim.update(state);
                // projectile splash
                SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
                i++; break;
            }
            // position
            float dx = DxCalculator::get_dx_constant(v0, state);
            walk_left(dx);
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }

        // shoot
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);
            // cd
            state.tick();
            // status
            if (reanim.progress > 0.545f && state.frozen_cd <= 0) {
                if (t_enter == -1) t_enter = i;
                break;
            }
            // position
            stay();
            // progress
            reanim.update(state);
            reanim.wrap();
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }
        // stay
        for (; i < M; i++) stay();
    }
};

void cal_x(
    ZombieType type, int M_sup, bool huge_wave, std::vector<int> ice_t, std::vector<int> splash_t,
    PositionCalculator::TypeCal test_type_zombie, 
    int x0=0, float v0=0.0f
){
    PositionCalculator cal(type, M_sup, huge_wave, ice_t, splash_t);
    cal.type_cal = test_type_zombie;

    cal.init();
    if (v0 != 0.0f) cal.v0 = v0;
    if (x0 != 0)    cal.x[0] = static_cast<float>(x0);
        
    cal.calculate_position();
    write_vector_to_csv(cal.x, "output.csv",true);
    printf("%d %d\n", cal.t_enter,cal.res);
}

void cal_x_extrem(
    ZombieType type, int M_sup, bool huge_wave, std::vector<int> ice_t, std::vector<int> splash_t,
    PositionCalculator::TypeCal test_type_zombie=PositionCalculator::TypeCal::FASTEST, bool parallel = false
){
    PositionCalculator cal(type, M_sup, huge_wave, ice_t, splash_t);
    cal.type_cal = test_type_zombie;
    
    auto v_range = cal.z.speed;
    auto v_ull_start = bit_cast<uint32_t>(static_cast<float>(v_range.first));
    auto v_ull_end = bit_cast<uint32_t>(static_cast<float>(v_range.second));
    if (!parallel){
        std::vector<float> x;
        if (cal.type_cal == PositionCalculator::TypeCal::FASTEST)
            x.resize(M_sup, 1000.0f);
        else
            x.resize(M_sup, -1000.0f);

        for(auto i = v_ull_start; i <= v_ull_end; ++i){
            cal.init();
            cal.v0 = bit_cast<float>(i);
            cal.calculate_position();
            for(int j=0;j<M_sup;j++)
                if(cal.x[j]<x[j] && cal.type_cal == PositionCalculator::TypeCal::FASTEST)
                    x[j] = cal.x[j];
                else if (cal.x[j] > x[j] && cal.type_cal == PositionCalculator::TypeCal::SLOWEST)
                    x[j] = cal.x[j];
            if (i% 10000 == 0) {
                printf("%u %u %u\n", v_ull_start, i, v_ull_end);
            }
        }
        write_vector_to_csv(x, "output.csv");
    }
    else{
        uint32_t total = v_ull_end - v_ull_start + 1;

        unsigned int n_threads = std::thread::hardware_concurrency();
        if (n_threads == 0) n_threads = 4; // fallback

        std::atomic<int> finished(0);

        std::vector<std::vector<float>> x_threads(n_threads);
        for (auto& x : x_threads) {
            if (cal.type_cal == PositionCalculator::TypeCal::FASTEST)
                x.resize(M_sup, 1000.0f);
            else
                x.resize(M_sup, -1000.0f);
        }

        std::vector<std::thread> threads;
        for (unsigned int t = 0; t < n_threads; ++t) {
            threads.emplace_back([&, t]() {
                /// 
                PositionCalculator cal_local(cal.z.type, cal.M, cal.huge_wave, cal.ice_t, cal.splash_t);
                cal_local.type_cal = cal.type_cal;
                uint32_t start = v_ull_start + t * total / n_threads;
                uint32_t end = (t == n_threads - 1) ? v_ull_end : v_ull_start + (t + 1) * total / n_threads - 1;
                for (uint32_t i = start; i <= end; ++i) {
                    cal_local.init();
                    cal_local.v0 = bit_cast<float>(i);
                    cal_local.calculate_position();
                    for (int j = 0; j < M_sup; j++) {
                        if (cal_local.type_cal == PositionCalculator::TypeCal::FASTEST) {
                            if (cal_local.x[j] < x_threads[t][j])
                                x_threads[t][j] = cal_local.x[j];
                        } else {
                            if (cal_local.x[j] > x_threads[t][j])
                                x_threads[t][j] = cal_local.x[j];
                        }
                    }
                    finished++;
                }
            });
        }

        while (finished < total) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            printf("%5.5f%%\n", 100.0 * finished.load() / total);
            fflush(stdout);
        }
        for (auto& th : threads) th.join();

        // 合并结果
        std::vector<float> x;
        if (cal.type_cal == PositionCalculator::TypeCal::FASTEST)
            x.resize(M_sup, 1000.0f);
        else
            x.resize(M_sup, -1000.0f);
        for (unsigned int t = 0; t < n_threads; ++t) {
            for (int j = 0; j < M_sup; ++j) {
                if (cal.type_cal == PositionCalculator::TypeCal::FASTEST) {
                    if (x_threads[t][j] < x[j]) x[j] = x_threads[t][j];
                } else {
                    if (x_threads[t][j] > x[j]) x[j] = x_threads[t][j];
                }
            }
        }
        write_vector_to_csv(x, "output.csv",true);
    }
}
