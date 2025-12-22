#include <cstdint>
# include <fstream>
# include <iomanip>

# include <mutex>

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
    // test_x(ZombieType::Pogo, 2000, false, {}, {}, PositionCalculator::TypeCal::RANDOM);
    cal_x_extrem(ZombieType::Catapult, M, false, {1}, {}, PositionCalculator::TypeCal::SLOWEST, true);


    // test_hit_time();
    // test_smash_rate_mt();
    // test_T();


    // test_tmp();

    return 0;
}