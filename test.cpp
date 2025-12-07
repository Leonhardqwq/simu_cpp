#include <cstdint>
#include <cstdio>
# include <fstream>
# include <iomanip>

#include <iostream>
# include <mutex>
# include <thread>

# include "inc/calculate_position.hpp"
# include "inc/calculate_hit_time.hpp"
# include "inc/util.hpp"
using namespace std;
const int M = 2000;

void test_hit_time(){
    HitTimeCalculator cal(PlantType::Gloomshroom, M);
    cal.calculate_hit_time();
    write_vector_to_csv(cal.state, "output.csv");
}

void test_x(){
    PositionCalculator cal(ZombieType::Pogo, M, false, {}, {});
    cal.type_cal = PositionCalculator::TypeCal::SLOWEST;

    cal.init();
    cal.v0 = cal.z.speed.first;
    cal.x[0] = 780.0f;
    cal.calculate_position();
    write_vector_to_csv(cal.x, "output.csv",true);
    printf("%d %d\n", cal.t_enter,cal.res);
}

void test_x_extrem(){
    PositionCalculator cal(ZombieType::JackInTheBox, M, false, {96}, {});
    cal.type_cal = PositionCalculator::TypeCal::FASTEST;
    
    auto v_range = cal.z.speed;
    auto v_ull_start = bit_cast<uint32_t>(static_cast<float>(v_range.first));
    auto v_ull_end = bit_cast<uint32_t>(static_cast<float>(v_range.second));

    vector<float> x;
    if (cal.type_cal == PositionCalculator::TypeCal::FASTEST)
        x.resize(M, 1000.0f);
    else
        x.resize(M, -1000.0f);

    for(auto i = v_ull_start; i <= v_ull_end; ++i){
        cal.init();
        cal.v0 = bit_cast<float>(i);
        cal.calculate_position();
        for(int j=0;j<M;j++)
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

// 多线程加速版本
void test_x_extrem_mt() {
    ///
    PositionCalculator cal(ZombieType::Catapult, M, false, {}, {});
    cal.type_cal = PositionCalculator::TypeCal::SLOWEST;
    //

    auto v_range = cal.z.speed;
    auto v_ull_start = bit_cast<uint32_t>(static_cast<float>(v_range.first));
    auto v_ull_end = bit_cast<uint32_t>(static_cast<float>(v_range.second));
    uint32_t total = v_ull_end - v_ull_start + 1;

    unsigned int n_threads = std::thread::hardware_concurrency();
    if (n_threads == 0) n_threads = 4; // fallback

    std::atomic<int> finished(0);

    std::vector<std::vector<float>> x_threads(n_threads);
    for (auto& x : x_threads) {
        if (cal.type_cal == PositionCalculator::TypeCal::FASTEST)
            x.resize(M, 1000.0f);
        else
            x.resize(M, -1000.0f);
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
                for (int j = 0; j < M; j++) {
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
        x.resize(M, 1000.0f);
    else
        x.resize(M, -1000.0f);
    for (unsigned int t = 0; t < n_threads; ++t) {
        for (int j = 0; j < M; ++j) {
            if (cal.type_cal == PositionCalculator::TypeCal::FASTEST) {
                if (x_threads[t][j] < x[j]) x[j] = x_threads[t][j];
            } else {
                if (x_threads[t][j] > x[j]) x[j] = x_threads[t][j];
            }
        }
    }
    write_vector_to_csv(x, "output.csv",true);
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

void test_tmp(){
    // auto res =  wilson_confidence_interval(0,1000000);
    // std::cout<<res.first*100<<"% ~ "<<res.second*100<<"%"<<std::endl;

    float p = 0;
    auto dlt1 = static_cast<float>(float(12.0)*double(0.01f)/uint32_t(21));
    auto dlt2 = static_cast<float>(0.5f*float(12.0)*double(0.01f)/uint32_t(21));

    for(int i=1;i<=169;i++) p+=dlt1;
    for(int i=1;i<=363;i++) p+=dlt2;

    // cout<<p;
}

int main(){
    test_x();
    // test_x_extrem_mt();
    // test_hit_time();
    // test_smash_rate_mt();
    // test_T();


    // test_tmp();

    return 0;
}