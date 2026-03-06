# include "inc/calculate_imp.hpp"
#include "inc/common.hpp"
# include "inc/util.hpp"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <utility>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>


void print_imp(){
    ImpCalculator cal(
        500,    
        Scene::ROOF, 
        -1, 
        634.221f, 
        0, 
        CdState{0, 0}, 
        0
    );
    cal.init();
    cal.calculate_imp();

    printf("land:%d, eat:%d, x:%.10f\n", cal.res, cal.t_enter, cal.x_ans[cal.res]);

    for (size_t i = 0; i < cal.x_ans.size(); i++) 
        printf("time:%.3d, x:%.1f y:%.1f h:%.10f\n", int(i), cal.x_ans[i], cal.y_ans[i], cal.dy_ans[i]);
}

void rnd_scan() {
    std::string out_path = "rnd_scan.csv";
    float rnd_start = 0.0f;
    float rnd_end = 100.0f;
    float rnd_step = 0.1f;

    const int M = 1000;
    const Scene scene = Scene::ROOF;
    const float x_giga = 680.0000610352f;
    const unsigned int row_giga = 0;
    const CdState state_giga{0, 0};
    const bool higher_imp = true;

    std::vector<float> results;
    for (float rnd = rnd_start; rnd <= rnd_end + 1e-6f; rnd += rnd_step) {
        ImpCalculator cal(M, scene, rnd, x_giga, row_giga, state_giga, higher_imp);
        cal.init();
        cal.calculate_imp();
        results.push_back(cal.x_ans[cal.res]);
    }

    std::ofstream fout(out_path);
    if (!fout) 
        std::cerr << "Failed to open output file: " << out_path << "\n";
    
    fout << "rnd,landing_x\n";
    fout << std::fixed << std::setprecision(6);
    for (size_t i = 0; i < results.size(); ++i) {
        float rnd = rnd_start + i * rnd_step;
        fout << rnd << "," << results[i] << "\n";
    }

    std::cout << "Wrote " << results.size() << " rows to " << out_path << "\n";
}

void calc_extrem(Scene scene, int M = 500, bool parallel = false) {
    auto x_ull_start = bit_cast<uint32_t>(400.0f);
    auto x_ull_end = bit_cast<uint32_t>(818.0f) - 1;
    uint32_t total = x_ull_end - x_ull_start;

    std::vector<float> x_giga(total);
    std::vector<float> x_min_imp(total);
    std::vector<float> x_max_imp(total);

    if (!parallel) {
        std::vector<std::vector<float>> output;
        for (auto i = x_ull_start + 1; i <= x_ull_end; ++i) {
            auto x = bit_cast<float>(i);
            uint32_t idx = i - (x_ull_start + 1);
            x_giga[idx] = x;
            for (auto rnd : {0.0f, 100.0f}) {
                ImpCalculator cal(
                    M,
                    scene,
                    rnd,
                    x,
                    0,
                    CdState{0, 0},
                    0
                );
                cal.init();
                cal.calculate_imp();
                if (cal.res == -1) throw std::runtime_error("计算失败");
                auto x_imp = cal.x_ans[cal.res];
                if (rnd == 0.0f)
                    x_min_imp[idx] = x_imp;
                else
                    x_max_imp[idx] = x_imp;
            }
            if (i % 100000 == 0) {
                printf("Progress: %.2f%%, x: %.2f, range: %.2f - %.2f\n", (i - x_ull_start) / float(x_ull_end - x_ull_start) * 100, x, x_min_imp.back(), x_max_imp.back());
                fflush(stdout);
            }
        }
    }
    else {
        unsigned int n_threads = std::thread::hardware_concurrency();
        if (n_threads == 0) n_threads = 4;

        std::atomic<uint32_t> finished(0);
        std::vector<std::thread> threads;

        for (unsigned int t = 0; t < n_threads; ++t) {
            threads.emplace_back([&, t]() {
                uint32_t start = x_ull_start + 1 + t * total / n_threads;
                uint32_t end = (t == n_threads - 1)
                    ? x_ull_end
                    : x_ull_start + (t + 1) * total / n_threads;

                for (uint32_t i = start; i <= end; ++i) {
                    auto x = bit_cast<float>(i);
                    uint32_t idx = i - (x_ull_start + 1);
                    x_giga[idx] = x;
                    for (auto rnd : {0.0f, 100.0f}) {
                        ImpCalculator cal(
                            M,
                            scene,
                            rnd,
                            x,
                            0,
                            CdState{0, 0},
                            0
                        );
                        cal.init();
                        cal.calculate_imp();
                        if (cal.res == -1) throw std::runtime_error("计算失败");
                        auto x_imp = cal.x_ans[cal.res];
                        if (rnd == 0.0f)
                            x_min_imp[idx] = x_imp;
                        else
                            x_max_imp[idx] = x_imp;
                    }                    
                    finished++;
                }
            });
        }
        while (finished < total) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            printf("%5.2f%%\n", 100.0 * finished.load() / total);
            fflush(stdout);
        }
        for (auto& th : threads) th.join();
    }

    std::vector<std::vector<float>> output = {x_giga, x_min_imp, x_max_imp};
    // write_2dvector_to_csv(output, scene == Scene::ROOF ? "imp_re.csv" : "imp_de.csv", true);
    // write_2dvector_to_bin(output, scene == Scene::ROOF ? "imp_re.bin" : "imp_de.bin");
    
    std::vector<std::vector<float>> x_ans;
    x_ans.resize(2);
    x_ans[0].resize(409, 1000.0f);  // min
    x_ans[1].resize(409, 0.0f); // max
    int cnt = 0;
    for (size_t i = 0; i < x_giga.size(); ++i) {
        float x = x_giga[i];
        for (float x_imp = x_max_imp[i]; x_imp >= x_min_imp[i]; x_imp -= 3.0f) {
            int x_imp_int = int(x_imp);
            if (x < x_ans[0][x_imp_int]) 
                x_ans[0][x_imp_int] = x;
            if (x > x_ans[1][x_imp_int]) 
                x_ans[1][x_imp_int] = x;
        }
    }
    std::replace(x_ans[0].begin(), x_ans[0].end(), 1000.0f, 0.0f);
    // write_2dvector_to_csv(x_ans, scene == Scene::ROOF ? "re.csv" : "de.csv", true);
}

int main() {
    // print_imp();
    calc_extrem(Scene::DAY, 209, true);
    // rnd_scan();

    return 0;
}
