# pragma once
# include "rng.hpp"
# include "common.hpp"
# include "util.hpp"
#include <thread>
#include <cstddef>
#include <algorithm>
#include <atomic>


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
        ///* 新版本
        if (state.is_slowed()) return dx * 0.01f * (reanim.fps * 0.5f);
        return dx * 0.01f * reanim.fps;
        //*/ 
        /* 老版本
        if (state.is_slowed()) dx *= 0.01f * (reanim.fps * 0.5f);
        else                   dx *= 0.01f * reanim.fps;
        return dx;
        //*/
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
    // jack: action_cd
    // dancing: action_cd, x_target
    // digger: x_target
    // screendoor: 使用random.choice([Zombie1, Zombie2])
public:
    rng rng;

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
    int action_cd;  // jack, dancing 类外更新
    int x_target = 40 + 20;   
    // digger 类外更新, 代表啃食植物防御左限, 默认1列南瓜
    // dancing 类外更新, 代表触碰植物防御右限，8列炮为 600 + 40
    enum Dancecheat {
        NONE,
        FAST,
        SLOW
    } dc_type;

    // 过程变量
    float v0;
    float v1;
    std::vector<int> frozen_t[2];   // 0first / 1second

    // 结果
    std::vector<float> x;
    int t_enter = -1; 
    int res = -1;    // jack爆炸时机, snorkel潜入时机, 矿工站稳时机, 舞王召唤时机, 鸭子可啃食时机

    PositionCalculator(
        ZombieType t, int M, bool huge_wave,
        std::vector<int> i_t, std::vector<int> s_t
    ) : z(ZombieData(t)), M(M), huge_wave(huge_wave), ice_t(i_t), splash_t(s_t) { type_cal = TypeCal::RANDOM;}

    // x0, v0, frozen time
    void init(){
        t_enter = -1; res = -1; action_cd = 0;
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
        else if (z.type == ZombieType::Dancing){
            calculate_dancing();
        }
        else if (z.type == ZombieType::DuckyTube1 || z.type == ZombieType::DuckyTube2){
            calculate_ducky_tube();
        }
        else{
            calculate_animation();
        }
    }
    
private:
    // 辅助方法：检查并设置进入时机
    bool check_enter(float x_val, int time) {
        if (int(x_val) <= z.threshold && t_enter == -1) {
            t_enter = time;
            return true;
        }
        return false;
    }

    // 辅助方法：向左移动
    void walk_left(float dx) {x.push_back(x.back() - dx);}

    // 辅助方法：向右移动
    void walk_right(float dx) {x.push_back(x.back() + dx);}

    // 辅助方法：保持原位
    void stay() {x.push_back(x.back());}

    void calculate_zomboni() {
        float v;
        size_t i = 1;
        for(; i < M; i++){
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
            if (check_enter(x_old, i)) {i++; break;}
        }
        // end
        for (; i < M; i++) stay();
    }

    void calculate_animation() {
        // int end_frame = z.begin_frame + z.n_frames - 1;
        Reanim reanim(z, v0);
        reanim.init_progress();
        ZombieData z_now = z;

        CdState state;
        // state.slow_cd = 100000; // 测试用，始终减速
        size_t i = 1;
        for (; i < M; i++) {
            // dancing cheat
            if (
                dc_type != Dancecheat::NONE && 
                (z.type == ZombieType::Zombie1 || z.type == ZombieType::Zombie2)
            ) {
                // update_status {}
                if (i != 1)
                    v0 = v1 == 0 ? rng.randfloat(z_now.speed.first, z_now.speed.second) : v1;
                if (dc_type == Dancecheat::FAST) 
                    z_now = rng.randint(2) ? zombie_walk1 : zombie_walk2;            
                else 
                    z_now = zombie_dance;
                reanim = Reanim(z_now, v0);
            }
            
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
            float dx = DxCalculator::get_dx_from_ground(z_now, reanim, state);
            walk_left(dx);

            // enter
            if (check_enter(x[i-1], t)) {i++; break;}

            // progress
            reanim.update(state);
            reanim.wrap();

            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }
        // end
        for (; i < M; i++) stay();
    }

    void calculate_constant() {
        CdState state;
        size_t i = 1;
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            
            // plant ice
            IceEffect::apply_with_type_check(t, ice_t, frozen_t, state, z.type);

            // cd
            state.tick();

            // position
            float dx = DxCalculator::get_dx_constant(v0, state);
            walk_left(dx);

            // enter
            if (check_enter(x[i-1], t)) {i++; break;}

            // projectile splash
            SplashEffect::apply_with_type_check(t, splash_t, x[i], z.def_x.first, state, z.type);
        }
        // end
        for (; i < M; i++) stay();
    }

    void calculate_digger() {
        Reanim reanim(12.0f, 21);
        int n_repeated = 0;
        int status = 32; // 32: dig, 33: drill, 36: dizzy, 37: walk_right

        CdState state;
        action_cd = 130;
        size_t i = 1;

        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            switch (status){
            case 32: // dig
            case 33: // drill
                IceEffect::consume_only(t, ice_t, state);
                break;
            case 36: // dizzy
            case 37: // walk_right
                IceEffect::apply(t, ice_t, frozen_t, state);
                break;
            }
            // cd
            switch (status){
            case 33: // drill
                if (action_cd > 0 && state.frozen_cd == 0) action_cd--;
            default:
                state.tick();
            }
            // status
            switch (status) {
            case 32: // dig
                if (x[i-1] < 10) 
                    status = 33; 
                break;
            case 33: // drill
                if (action_cd == 0) {
                    status = 36;
                    if (res == -1) res = i;
                }
                break;
            case 36: // dizzy
                if (state.frozen_cd <= 0 && n_repeated >= 2) {
                    status = 37;
                    // 固定速度
                    v0 = 0.12f;
                    reanim = Reanim(z, v0);
                }
                break;
            case 37: // walk_right
                break;
            }
            // position
            switch (status) {
            case 32: { // dig
                float dx = DxCalculator::get_dx_constant(v0, state);
                walk_left(dx);                
                break;
            }
            case 33: // drill
            case 36: // dizzy
                stay();
                break;
            case 37: { // walk_right
                float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
                walk_right(dx);
                break;
            }}
            // eat
            switch (status) {
            case 37: // walk_right
                if (!state.is_frozen() && t_enter==-1 && int(x[i-1]) + z.atk.second >= x_target) {
                    int op = state.get_eat_interval();
                    if (i % op == 0) 
                        t_enter = i;
                }
                break;
            }
            // progress
            switch (status) {
            case 36: // dizzy
                reanim.update(state);
                n_repeated += reanim.wrap_with_count();
                break;
            case 37: // walk_right
                reanim.update(state);
                reanim.wrap();
                break;
            }
        }
    }

    void calculate_dolphin_rider() {
        int status = 51; // 51: walk_with_dolphin, 52: jump in pool, 53: ride
        Reanim reanim(z, v0);
        reanim.init_progress();

        CdState state;
        size_t i = 1;
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            float x_old = x[i-1];
            // plant ice
            if (status == 51) // walk_with_dolphin
                IceEffect::apply(t, ice_t, frozen_t, state);
            else if (status == 52) // jump in pool
                IceEffect::apply_slow_only(t, ice_t, state);
            else if (status == 53) // ride
                IceEffect::apply_pool(t, ice_t, state);
            // cd
            state.tick();
            // status
            if (!state.is_frozen()) switch (status) {
                case 51: {// walk_with_dolphin
                    int x_int = int(x[i-1]);
                    if (700 < x_int && x_int <= 720) {
                        reanim = Reanim(16.0f, 45);
                        status = 52; 
                    }
                    break;
                }
                case 52: // jump in pool
                    if (reanim.progress >= 1) {
                        status = 53;
                        x_old -= 70;
                    }
                    break;
                case 53: // ride
                    if (state.frozen_cd <= 0 && int(x[i-1]) <= 10) {
                        status = 51;
                        // 速度重置
                        v0 = v1 == 0 ? rng.randfloat(z.speed.first, z.speed.second) : v1;
                        // v0 = static_cast<float>(z.speed.second);
                        reanim = Reanim(z, v0);
                    }
                    break;
            } 
            // position
            if (status == 51) { // walk_with_dolphin
                float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
                walk_left(dx);
            }
            else if (status == 52) // jump in pool
                stay();
            else if (status == 53) { // ride
                float dx = DxCalculator::get_dx_constant(v0, state);
                x.push_back(x_old - dx);
            }

            // enter
            if (check_enter(x[i-1], t)) {i++; break;}
            // progress
            if (status == 51) { // walk_with_dolphin
                reanim.update(state);
                reanim.wrap();
            }
            else if (status == 52) // jump in pool
                reanim.update_unconditional();

            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }
        // end
        for (; i < M; i++) stay();
    }

    void calculate_snorkel() {
        int status = 57; // 57: walking, 58: jump in pool, 59: swim
        Reanim reanim(z, v0);
        reanim.init_progress();

        CdState state;
        size_t i = 1;
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            float x_old = x[i-1];
            // plant ice
            if (status == 57) // walking
                IceEffect::apply(t, ice_t, frozen_t, state);
            else if (status == 58) // jump in pool
                IceEffect::apply_slow_only(t, ice_t, state);
            else if (status == 59) // swim
                IceEffect::apply_pool(t, ice_t, state);
            // cd
            state.tick();
            // status
            if (!state.is_frozen()) switch (status) {
                case 57: {// walking
                    int x_int = int(x[i-1]);
                    if (700 < x_int && x_int <= 720) {
                        status = 58; 
                        v0 = 0.2f;
                        reanim = Reanim(16.0f, 19);
                    }
                    break;
                }
                case 58: // jump in pool
                    if (reanim.progress >= 1) {
                        status = 59;
                        if (res == -1) res = i;
                    }
                    break;
                case 59: // swim
                    if (int(x[i-1]) <= 25) {
                        status = 57;
                        // 速度重置
                        v0 = v1 == 0 ? rng.randfloat(z.speed.first, z.speed.second) : v1;
                        // v0 = static_cast<float>(z.speed.first);
                        reanim = Reanim(z, v0);
                        x_old -= 15.0f;
                    }
                    break;
            }
            // position
            if (status == 57) { // walking
                float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
                x.push_back(x_old - dx);
            }
            else if (status == 58) // jump in pool
                walk_left(v0);
            else if (status == 59) { // swim
                float dx = DxCalculator::get_dx_constant(v0, state);
                walk_left(dx);
            }

            // enter
            if (check_enter(x[i-1], t)) {i++; break;}

            // progress
            if (status == 57) { // walking
                reanim.update(state);
                reanim.wrap();
            }
            else if (status == 58) // jump in pool
                reanim.update_unconditional();

            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }
        // end
        for (; i < M; i++) stay();
    }

    void calculate_catapult_shoot() {
        int status = 0; // 0: walking, 67: shoot
        Reanim reanim(24.0f, 32);
        CdState state;
        size_t i = 1;
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);
            // cd
            state.tick();
            // status ??? 不考虑ice4
            if (status == 0) { // walking
                if (!state.is_frozen() && x[i-1] <= 650)
                    status = 67; 
            }
            else if (status == 67) { // shoot
                if (!state.is_frozen() && reanim.progress > 0.545f) {
                    if (t_enter == -1) 
                        t_enter = i;
                    break;
                }
            }
            // position
            if (status == 0) { // walking
                float dx = DxCalculator::get_dx_constant(v0, state);
                walk_left(dx);
            }
            else if (status == 67) { // shoot
                stay();
            }
            // progress
            if (status == 67) {
                reanim.update(state);
                reanim.wrap();
            }
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }
        // end
        for (; i < M; i++) stay();
    }

    void calculate_dancing() {
        int status = 40; // 40: moonwalk, 29: point, 43: summoning, 44: walking
        action_cd += 300; // randint(12)
        int n_repeated = 0;
        Reanim reanim(24.0f, z.n_frames);
        reanim.init_progress();
        CdState state;
        size_t i = 1;
        for (; i < M; i++) {
            int t = static_cast<int>(i);
            // plant ice
            IceEffect::apply(t, ice_t, frozen_t, state);
            if (status == 40) {
                if (state.is_frozen() && reanim.fps != 0.0f) 
                    reanim.fps = 0.0f;
            }
            // cd
            if (action_cd > 0 && !state.is_frozen()) action_cd--;
            state.tick();
            // status
            if (!state.is_frozen()) switch (status) {
                case 40: // moonwalk
                    if (action_cd == 0) {
                        status = 29;
                        reanim = Reanim(24.0f, 10);
                        n_repeated = 0;
                    }
                    break;
                case 29: // point
                    if (n_repeated > 0) {
                        status = 43;
                        action_cd = 200;
                        if (res == -1) res = i;
                    }
                    break;
                case 43: // summoning
                    if (action_cd == 0) {
                        status = 44;
                    }
                    break;
            }
            // position
            switch (status) {
            case 40: {// moonwalk
                float dx = DxCalculator::get_dx_from_ground(z, reanim, state);
                walk_right(dx);
                break;
            }
            case 29: // point
            case 43: // summoning
            case 44:  // walking
                stay();
                break;
            }
            // eat
            if (!state.is_frozen()) switch (status) {
            case 40: // moonwalk
                if (int(x[i-1]) + z.atk.first <= x_target) {
                    int op = state.get_eat_interval();
                    if (i % op == 0) {
                        action_cd = 1;
                    }
                }
                break;
            case 44: // walking
                // if (t_enter==-1 && int(x[i-1]) + z.atk.first <= x_target) {
                if (t_enter==-1) { // 暂时不考虑啃食判定问题
                    int op = state.get_eat_interval();
                    if (i % op == 0) 
                        t_enter = i;
                }
                break;
            }
            // progress
            switch (status) {
            case 40: // moonwalk
                reanim.update(state);
                reanim.wrap();
                break;
            case 29: // point
                reanim.update(state);
                n_repeated += reanim.wrap_with_count();
                break;
            }
            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }
        // end
        for (; i < M; i++) stay();
    }

    void calculate_ducky_tube() {
        int action = 0; // 0: none, 1: entering pool, 2: leaving pool
        bool is_in_water = false;
        float dy = 0.0f;
        ZombieData z_now = z.type == ZombieType::DuckyTube1 ? zombie_walk1 : zombie_walk2;
        Reanim reanim(z_now, v0);
        reanim.init_progress();

        CdState state;
        size_t i = 1;
        for (; i < M; i++) {
            // dancing cheat
            if (dc_type != Dancecheat::NONE) {/*TODO*/}
            
            int t = static_cast<int>(i);
            int x_old = int(x[i-1]);
            // plant ice
            if (!is_in_water)
                IceEffect::apply(t, ice_t, frozen_t, state);
            else 
                IceEffect::apply_pool(t, ice_t, state);
            // cd
            state.tick();
            // status
            if (!state.is_frozen()) {
                if (action == 1 || action == 2 || is_in_water) switch (action){
                    // update_action_in_pool {}
                    case 1: {// entering pool
                        dy -= 1;
                        if (dy <= -40) {
                            dy = -40;
                            action = 0;
                            // update_status {}
                            v0 = v1 == 0 ? rng.randfloat(z_now.speed.first, z_now.speed.second) : v1;
                            z_now = zombie_swim;
                            reanim = Reanim(z_now, v0);
                            if (res == -1) res = i; // 进入池塘的一帧算作可啃食时机
                        }
                        break;
                    }
                    case 2: // leaving pool
                        dy += 1;
                        if (dy >= 0) {
                            dy = 0;
                            action = 0;
                            is_in_water = false;
                        }
                        break;
                }
            }
            // position
            float dx = DxCalculator::get_dx_from_ground(z_now, reanim, state);
            walk_left(dx);

            // water status
            bool current_in_water = 40 <= x_old && x_old < 680;
            if (is_in_water && !current_in_water) {
                action = 2; // leaving pool
                // update_status {}
                v0 = v1 == 0 ? rng.randfloat(z_now.speed.first, z_now.speed.second) : v1;
                z_now = rng.randint(2) ? zombie_walk1 : zombie_walk2;
                reanim = Reanim(z_now, v0);
            }
            else if (!is_in_water && current_in_water) {
                action = 1; // entering pool
                is_in_water = true;
            }

            // enter
            if (check_enter(x[i-1], t)) {i++; break;}

            // progress
            reanim.update(state);
            reanim.wrap();

            // projectile splash
            SplashEffect::apply(t, splash_t, x[i], z.def_x.first, state);
        }
        // end
        for (; i < M; i++) stay();
    }
};


// ===================== 极值计算 ====================

std::vector<float> cal_x_extrem(
    ZombieType type, int M_sup, bool huge_wave, std::vector<int> ice_t, std::vector<int> splash_t,
    PositionCalculator::TypeCal test_type_zombie=PositionCalculator::TypeCal::FASTEST, bool parallel = false, bool output = true,
    float v0 = 0.0f, float v1 = 0.0f, 
    PositionCalculator::Dancecheat dc_type = PositionCalculator::Dancecheat::NONE
){
    PositionCalculator cal(type, M_sup, huge_wave, ice_t, splash_t);
    cal.type_cal = test_type_zombie;
    cal.dc_type = dc_type;
    std::vector<float> x;
    auto v_range = cal.z.speed;
    auto v_ull_start = bit_cast<uint32_t>(static_cast<float>(v_range.first));
    auto v_ull_end = bit_cast<uint32_t>(static_cast<float>(v_range.second));
    if (!parallel){
        if (cal.type_cal == PositionCalculator::TypeCal::FASTEST)
            x.resize(M_sup, 1000.0f);
        else
            x.resize(M_sup, -1000.0f);

        for(auto i = v_ull_start; i <= v_ull_end; ++i){
            cal.init();
            if (v0 != 0.0f && v1 == 0.0f) {
                cal.v1 = bit_cast<float>(i);
                cal.v0 = v0;            
            }
            else {
                cal.v0 = bit_cast<float>(i);
                if (v1 != 0.0f)
                    cal.v1 = v1;
            }
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
        if (output)
            write_vector_to_csv(x, "output.csv");
    }
    else{
        uint32_t total = v_ull_end - v_ull_start + 1;

        unsigned int n_threads = std::thread::hardware_concurrency();
        if (n_threads == 0) n_threads = 4; // fallback

        std::atomic<int> finished(0);

        std::vector<std::vector<float>> x_threads(n_threads);
        for (auto& xx : x_threads) {
            if (cal.type_cal == PositionCalculator::TypeCal::FASTEST)
                xx.resize(M_sup, 1000.0f);
            else
                xx.resize(M_sup, -1000.0f);
        }

        std::vector<std::thread> threads;
        for (unsigned int t = 0; t < n_threads; ++t) {
            threads.emplace_back([&, t]() {
                PositionCalculator cal_local(cal.z.type, cal.M, cal.huge_wave, cal.ice_t, cal.splash_t);
                cal_local.type_cal = cal.type_cal;
                cal_local.dc_type = cal.dc_type;
                uint32_t start = v_ull_start + t * total / n_threads;
                uint32_t end = (t == n_threads - 1) ? v_ull_end : v_ull_start + (t + 1) * total / n_threads - 1;
                for (uint32_t i = start; i <= end; ++i) {
                    cal_local.init();
                    if (v0 != 0.0f && v1 == 0.0f) {
                        cal_local.v1 = bit_cast<float>(i);
                        cal_local.v0 = v0;            
                    }
                    else {
                        cal_local.v0 = bit_cast<float>(i);
                        if (v1 != 0.0f)
                            cal_local.v1 = v1;
                    }
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
        if (output)
            write_vector_to_csv(x, "output.csv",true);
    }
    return x;
}

std::vector<float> cal_digger_x_extrem(
    int M_sup, bool huge_wave, std::vector<int> ice_t, 
    PositionCalculator::TypeCal test_type_zombie=PositionCalculator::TypeCal::FASTEST,
    bool parallel = false, bool output = true
) {
    PositionCalculator cal(ZombieType::Digger, M_sup, huge_wave, ice_t, {});
    cal.type_cal = test_type_zombie;
    std::vector<float> x_min, x_max;
    x_min.resize(M_sup, 1000.0f);
    x_max.resize(M_sup, -1000.0f);
    auto v_range = cal.z.speed;
    auto v_ull_start = bit_cast<uint32_t>(static_cast<float>(v_range.first));
    auto v_ull_end = bit_cast<uint32_t>(static_cast<float>(v_range.second));
    if (!parallel){
        for(auto i = v_ull_start; i <= v_ull_end; ++i){
            cal.init();
            cal.v0 = bit_cast<float>(i);
            cal.calculate_position();
            for(int j=0;j<M_sup;j++){
                if (cal.x[j] < x_min[j])
                    x_min[j] = cal.x[j];
                if (cal.x[j] > x_max[j])
                    x_max[j] = cal.x[j];
            }

            if (i% 10000 == 0) {
                printf("%u %u %u\n", v_ull_start, i, v_ull_end);
            }
        }
    }
    else{
        uint32_t total = v_ull_end - v_ull_start + 1;

        unsigned int n_threads = std::thread::hardware_concurrency();
        if (n_threads == 0) n_threads = 4; // fallback

        std::atomic<int> finished(0);

        std::vector<std::vector<float>> x_min_threads(n_threads);
        std::vector<std::vector<float>> x_max_threads(n_threads);
        for (auto& xx: x_min_threads) 
            xx.resize(M_sup, 1000.0f);
        for (auto& xx : x_max_threads) 
            xx.resize(M_sup, -1000.0f);


        std::vector<std::thread> threads;
        for (unsigned int t = 0; t < n_threads; ++t) {
            threads.emplace_back([&, t]() {
                PositionCalculator cal_local(cal.z.type, cal.M, cal.huge_wave, cal.ice_t, cal.splash_t);
                cal_local.type_cal = cal.type_cal;
                uint32_t start = v_ull_start + t * total / n_threads;
                uint32_t end = (t == n_threads - 1) ? v_ull_end : v_ull_start + (t + 1) * total / n_threads - 1;
                for (uint32_t i = start; i <= end; ++i) {
                    cal_local.init();
                    cal_local.v0 = bit_cast<float>(i);
                    cal_local.calculate_position();
                    for(int j=0;j<M_sup;j++){
                        if (cal_local.x[j] < x_min_threads[t][j])
                            x_min_threads[t][j] = cal_local.x[j];
                        if (cal_local.x[j] > x_max_threads[t][j])
                            x_max_threads[t][j] = cal_local.x[j];
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
        for (unsigned int t = 0; t < n_threads; ++t) {
            for (int j = 0; j < M_sup; ++j) {
                if (x_min_threads[t][j] < x_min[j]) x_min[j] = x_min_threads[t][j];
                if (x_max_threads[t][j] > x_max[j]) x_max[j] = x_max_threads[t][j];
            }
        }
    }
    std::vector<float> x;
    bool all_move_right = false;
    bool first_move_right = false;
    if (test_type_zombie == PositionCalculator::TypeCal::FASTEST) {
        for (int j = 0; j < M_sup; ++j) {
            if (!all_move_right && x_max[j] < 10.0f) all_move_right = true;
            if (!first_move_right && x_min[j] < 10.0f) first_move_right = true;
            if (all_move_right) 
                x.push_back(x_max[j]);
            else if (first_move_right)
                x.push_back(bit_cast<float>(bit_cast<uint32_t>(10.0f)-1)); // 10.0f
            else 
                x.push_back(x_min[j]);
        }
    }
    else {
        for (int j = 0; j < M_sup; ++j) {
            if (!all_move_right && x_max[j] < 10.0f) all_move_right = true;
            x.push_back(all_move_right ? x_min[j] : x_max[j]);
        }
    }
    if (output)
        write_vector_to_csv(x, "output.csv", true);
    return x;
}
