# include "inc/calculate_imp.hpp"
# include "inc/util.hpp"
#include <cstdio>
#include <iostream>
#include <utility>
#include <vector>


void print_imp(){
    ImpCalculator cal(
        500,    
        Scene::ROOF, 
        0.0f, 
        680.9999389648f, 
        0, 
        CdState{0, 0}, 
        1
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

void calc_extrem(int M = 400) {
    std::vector<std::vector<float>> extrem_giga(2);
    extrem_giga[0].resize(M, 1000.0f);
    extrem_giga[1].resize(M, 0.0f);
    auto x_ull_start = bit_cast<uint32_t>(400.0f);
    auto x_ull_end = bit_cast<uint32_t>(854.0f);    
    for(auto i = x_ull_start+1; i <= x_ull_end; ++i){
        auto x = bit_cast<float>(i);
        std::vector<float> ans;
        for (auto rnd : {0.0f, 100.0f}){
            ImpCalculator cal(
                M,    
                Scene::POOL, // 修改
                rnd, 
                x, 
                0, 
                CdState{0, 0}, 
                0
            );
            cal.init();
            cal.calculate_imp();
            if (cal.res == -1) printf("计算失败\n");
            auto x_imp = cal.x_ans[cal.res];
            auto x_int_imp = int(x_imp);
            if (x < extrem_giga[0][x_int_imp])
                extrem_giga[0][x_int_imp] = x;
            if (x > extrem_giga[1][x_int_imp])
                extrem_giga[1][x_int_imp] = x;
            ans.push_back(x_imp);
        }

        if (i% 100000 == 0) {
            printf("Progress: %.2f%%, x: %.2f, range: %.2f - %.2f\n", (i - x_ull_start) / float(x_ull_end - x_ull_start) * 100, x, ans[0], ans[1]);
            fflush(stdout);
        }
    }

    for (int i = 0; i < 2; ++i) 
        for (int j = 0; j < M; ++j) 
            if (extrem_giga[i][j] == 1000.0f) 
                extrem_giga[i][j] = 0.0f;
        
    
    write_2dvector_to_csv(extrem_giga, "extrem_giga.csv", true);
}

int main() {
    // print_imp();
    calc_extrem(500);
    // rnd_scan();

    return 0;
}