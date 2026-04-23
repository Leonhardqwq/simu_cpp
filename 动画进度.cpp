#include <cstdint>
# include <fstream>
# include <iomanip>
#include <vector>

# include "inc/calculate_position_v2.hpp"
# include "inc/util.hpp"

using namespace std;
const int M = 400; // 215 小鬼落地

void test_reanim() {
    // 24, 6 小鬼落地
    // 16, 33 巨人砸 0.64f
    // 24, 34 巨人投 0.74f
    // 24, 10 舞王
    // 24, 43
    Reanim reanim(24, 43);
    // Reanim reanim(ZombieData(ZombieType::Dancing), 0.5f);
    CdState state;
    int n_repeated = 0;
    vector<float> thresholds = {
        0.6f
    };

    state.slow_cd = 0;
    for(int i = 1; i <= M; i++){
        state.tick();
        reanim.update(state);
        n_repeated += reanim.wrap_with_count();

        printf("%.3d %.2d %.10f:", (int)(i), n_repeated, reanim.progress);
        for (const auto& threshold : thresholds)
            printf(" %s %.10f", (reanim.progress > threshold) ? "> " : "<=", threshold);
            // printf(" %s %.10f\n", (reanim.progress > threshold) ? ">" : (reanim.progress == threshold) ? "=" : "<", threshold);
        printf("\n");
    }
}
// 小鬼落地 持续时间计算
void cal_duration(){
    Reanim reanim(24, 6);
    CdState state;
    int n_repeated = 0;
    vector<float> thresholds = {
        0.74f
    };
    int count = 0;
    for(int idx = 0; idx <= M; idx++){
        state.reset(); state.slow_cd = idx;
        reanim.progress = 0.0f; n_repeated = 0;
        // 初值 炮生效帧末的slow_cd
        for(int i = 1; i <= M; i++){
            state.tick();
            reanim.update(state);
            n_repeated += reanim.wrap_with_count();
            if (
                // reanim.progress > thresholds[0]
                n_repeated >= 1
            ) {
                // printf("(%d, %d), \n", idx, i);
                printf("%d, %s", i, (idx + 1) % 10 == 0 ? "\n" : "");
                break;
            }
        }
    }
}

int main(){
    // cal_duration();
    test_reanim();

    return 0;
}

