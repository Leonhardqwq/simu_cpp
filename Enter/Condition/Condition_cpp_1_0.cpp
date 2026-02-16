#include "../../inc/tools/Enter.hpp"
#include "../../inc/util.hpp"


// true为并行，false为串行
double work_condition(EnterConfig config, bool parallel = false) {
    // config.show_info();
    // std::cout<<config.M<<std::flush;

    auto N = config.num_test;
    if (!parallel) {
        // 串行计算
        EnterSimulator sim(config);
        int ans = 0;
        for (int i = 0; i < N; ++i) {
            if (sim.simu()) {
                // condition
                if (
                    sim.zombie.x[948] < 700
                ) {
                    ans++;
                }
            }
            // printf("\n");
            if (config.show_progress && i % (N/100) == 0 && i > 0) {
                printf("(%5.2lf%%) %5.5f%%\n", 100.0 * i / N, 100.0 * ans / i);
                fflush(stdout);
            }
        }
        printf("%5.5f%%\n", 100.0 * ans / N);
        fflush(stdout);
        return 1.0*ans/N;
    }
    else{
        // 并行计算
        int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
        int per_thread = N / num_threads;
        int remain = N % num_threads;

        std::atomic<int> ans(0);
        std::atomic<int> finished(0);
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            int this_N = per_thread + (i < remain ? 1 : 0);
            threads.emplace_back([&, this_N]() {
                EnterSimulator sim(config);
                for (int j = 0; j < this_N; ++j) {
                    if (sim.simu()){
                        // condition
                        if (
                            sim.zombie.x[948] < 700
                        ) {
                            ans++;
                        }
                    }
                    finished++; 
                }
            });
        }

        // 进度展示
        if(config.show_progress)
            while (finished < N) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                int now = finished.load();
                if (now == 0)   continue;
                printf("(%5.2lf%%) %5.5f%%\n", 100.0 * now / N, 100.0 * ans.load() / now);
                fflush(stdout);
            }
        for (auto& t : threads) t.join();
        printf("%5.5f%%\n", 100.0 * ans.load() / N);
        fflush(stdout);
        return 1.0*ans.load()/N;
    }
    // config.show_info();
}

int main() {
    EnterConfig config("../enter_config.json");

    auto start = std::chrono::high_resolution_clock::now();
    auto ci = wilson_confidence_interval( work_condition(config, 1), config.num_test);
    auto end = std::chrono::high_resolution_clock::now(); 

    printf("Wilson Confidence Interval: [%.6lf%%, %.6lf%%]\n", 100.0*ci.first, 100.0*ci.second);
    printf("Total time: %.2lf seconds\n", std::chrono::duration<double>(end - start).count());
    printf("Press Enter to exit...\n");
    fflush(stdout);
    
    // std::cin.get();
    return 0;
}
