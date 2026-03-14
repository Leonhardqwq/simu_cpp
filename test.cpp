#include <cstdint>
# include "inc/calculate_position_v2.hpp"
# include "inc/calculate_hit_time.hpp"
#include <atomic>
# include "inc/util.hpp"
using namespace std;
const int M = 3000;


void cal_x(
    ZombieType type, int M_sup, bool huge_wave, std::vector<int> ice_t, std::vector<int> splash_t,
    PositionCalculator::TypeCal test_type_zombie, 
    int x0=0, float v0=0.0f, float v1=0.0f
){
    PositionCalculator cal(type, M_sup, huge_wave, ice_t, splash_t);
    cal.type_cal = test_type_zombie;

    cal.init();
    if (v0 != 0.0f) cal.v0 = v0;
    // if (v1 != 0.0f) cal.v1 = v1;
    if (x0 != 0)    cal.x[0] = static_cast<float>(x0);
    // 舞王rnd
    cal.action_cd = cal.rng.randint(12);
    cal.action_cd = 0;
        
    cal.calculate_position();
    write_vector_to_csv(cal.x, "output.csv",true);
    printf("%d %d\n", cal.t_enter,cal.res);
}


void test_hit_time(){
    HitTimeCalculator cal(PlantType::Gloomshroom, M);
    cal.calculate_hit_time();
    write_vector_to_csv(cal.state, "output.csv");
}


// 简易砸率计算器
void test_smash_rate(){
    int n_trials = 1000000;
    int n_success = 0;
    vector<int> walk_times = {
        225 + 601,
        601 - 105 - 34 +  601 - 134
    };
    PositionCalculator cal(ZombieType::Gargantuar, M, false, {}, {});
    for (int i = 0; i < n_trials; ++i) {
        cal.init();
        cal.calculate_position();
        // 845 - 335 = 510
        float x_end = cal.x[walk_times[0]];
        for (int j = 1; j < walk_times.size(); ++j) {
            cal.init();
            cal.calculate_position();
            auto x_delta = cal.x[walk_times[j]] - cal.x[0];
            x_end += x_delta;
        }
        // printf("%f\n", x_end);
        if (i % 10000 == 0) {
            printf("Trial %d / %d\n", i, n_trials);
        }
        if (int(x_end) <= 510)
            n_success++;
    }
    printf("Smash Rate: %f%%\n", static_cast<float>(n_success) * 100.0f / static_cast<float>(n_trials));
    auto res =  wilson_confidence_interval(1.0*n_success/n_trials,n_trials);
    std::cout<<res.first*100<<"% ~ "<<res.second*100<<"%"<<std::endl;
}

void test_smash_rate_mt() {
    int n_trials = 100000000;
    int n_threads = std::thread::hardware_concurrency();
    if (n_threads == 0) n_threads = 4; // fallback

    std::atomic<int> n_success(0);
    std::atomic<int> finished(0);

    vector<int> walk_times = {
        225 + 601,
        601 - 105 - 34 +  601 - 134
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < n_threads; ++t) {
        threads.emplace_back([&, t]() {
            int local_trials = n_trials / n_threads;
            if (t == n_threads - 1) local_trials += n_trials % n_threads;
            int local_success = 0;
            PositionCalculator cal(ZombieType::Gargantuar, M, false, {}, {});
            for (int i = 0; i < local_trials; ++i) {
                cal.init();
                cal.calculate_position();
                float x_end = cal.x[walk_times[0]];
                for (int j = 1; j < walk_times.size(); ++j) {
                    cal.init();
                    cal.calculate_position();
                    auto x_delta = cal.x[walk_times[j]] - cal.x[0];
                    x_end += x_delta;
                }
                if (int(x_end) <= 510)
                    local_success++;
                if (i % 10000 == 0) {
                    finished += 10000;
                }
            }
            n_success += local_success;
        });
    }

    while (finished < n_trials) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        printf("%5.2f%%\n", 100.0 * finished.load() / n_trials);
        fflush(stdout);
    }
    for (auto& th : threads) th.join();

    printf("Smash Rate: %f%%\n", static_cast<float>(n_success) * 100.0f / static_cast<float>(n_trials));
    auto res = wilson_confidence_interval(1.0 * n_success / n_trials, n_trials);
    std::cout << res.first * 100 << "% ~ " << res.second * 100 << "%" << std::endl;
}


void test_T(){
    PositionCalculator cal(ZombieType::Gargantuar, M, false, {}, {});
    cal.init();
    cal.v0 = (cal.z.speed.first+cal.z.speed.second)/2.0f;
    cal.v0 = cal.z.speed.first;
    cal.calculate_position();

    auto fps = cal.z.n_frames/cal.z.total_x*cal.v0*47;
    auto T = int(ceil(1.0f / static_cast<float>(fps*double(0.01f)/cal.z.n_frames)));

    std::cout<<T<<std::endl;
    std::cout<<- cal.x[T] + cal.x[0];
}

void test_tmp(PositionCalculator::TypeCal type_cal){
}

int main(){
/*
    cal_digger_x_extrem(
        M, false, 
        {}, 
        PositionCalculator::TypeCal::FASTEST,
        true, true
    );
//*/
    
///*
    cal_x_extrem(
        ZombieType::Balloon, M, false, 
        {1}, {}, 
        PositionCalculator::TypeCal::FASTEST,
        true
    );
//*/
/*
    cal_x(
        ZombieType::DuckyTube1, M, false, 
        {}, {}, 
        PositionCalculator::TypeCal::FASTEST
        // , 780+31
        // , 0.89001f
        // , 0.90773f
    );
//*/

    // test_hit_time();
    // test_smash_rate_mt();
    // test_T();

    // test_tmp(PositionCalculator::TypeCal::FASTEST);

    return 0;
}