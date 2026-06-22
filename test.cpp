#include <cstdint>
# include "inc/calculate_position_v2.hpp"
# include "inc/calculate_hit_time.hpp"
#include <atomic>
#include <vector>
# include "inc/util.hpp"
using namespace std;
const int M = 5000;


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

void cal_x_extrem_both(ZombieType type, int M_sup, bool huge_wave, std::vector<int> ice_t, std::vector<int> splash_t){
    vector<vector<float>> x;
    x.push_back(
        cal_x_extrem(
            type, M_sup, huge_wave, 
            ice_t, splash_t, 
            PositionCalculator::TypeCal::FASTEST,
            true,false
    ));
    x.push_back(
        cal_x_extrem(
            type, M_sup, huge_wave, 
            ice_t, splash_t, 
            PositionCalculator::TypeCal::SLOWEST,
            true,false
    ));
    write_2dvector_to_csv(x, "output.csv", true);
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


void test_tmp(){
    auto z = ZombieData(ZombieType::JackInTheBox);
    auto arr = z.anim;

    for (const auto& frame : arr)        
        printf("%f\n", frame);
    printf("\n");


    Reanim reanim(z, 0.68f);
    CdState state;
    state.slow_cd = 100000; // 测试用，始终减

    float ans = 0.0f;
    for (int i = 0; i < 1000; ++i) {
        auto dx = DxCalculator::get_dx_from_ground(z, reanim, state);
        reanim.update(state);
        reanim.wrap();
        printf("dx: %f, progress: %f\n", dx, reanim.progress);
        if (ans < dx) ans = dx;
    }
    printf("Maximum dx: %f\n", ans);
    cout << 706.3 - 691 << " " << 15.3;
    // +88
    // 952 stay
    // 953 move
    // 953 + 88 = 1041
    // 1042 explode
    // 1042+110+200 = 1352
    // 1043 explode 1044 explode
    // 1353

    // 704.097 - 691 = 13.097
    // +74
    // 953 + 74 = 1027

}

int main(){
/*
    cal_x_extrem_both(
        ZombieType::Gargantuar, M, false, 
        {100, }, {}
    );
//*/
/*
    cal_anim_x_extrem(
        ZombieType::JackInTheBox, 2000, 1
    );
*/


/*
    cal_digger_x_extrem(
        M, false, 
        {}, 
        PositionCalculator::TypeCal::FASTEST,
        true, true
    );
//*/
    // cal_pole_vault_x_extrem();

/*
    cal_x(
        ZombieType::DuckyTube2, M, false, 
        {}, {}, 
        PositionCalculator::TypeCal::FASTEST
        , 780
        , 0.37f
        // , 0.90773f
    );
//*/

    // test_hit_time();
    // test_smash_rate_mt();
    // test_T();

    test_tmp();

    return 0;
}