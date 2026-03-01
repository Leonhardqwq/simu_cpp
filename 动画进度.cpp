#include <cstdint>
# include <fstream>
# include <iomanip>
#include <vector>

# include "inc/calculate_position_new.hpp"
# include "inc/util.hpp"

using namespace std;
const int M = 500;

int main(){
    Reanim reanim(16, 33);
    CdState state;
    int n_repeated = 0;
    vector<float> thresholds = {
        0.64f
    };

    // state.slow_cd = 1000000;
    for(int i = 1; i <= M; i++){
        reanim.update(state);
        n_repeated += reanim.wrap_with_count();

        printf("%.3d %.2d %.10f:", (int)(i), n_repeated, reanim.progress);
        for (const auto& threshold : thresholds)
            printf(" %s %.10f", (reanim.progress > threshold) ? "> " : "<=", threshold);
            // printf(" %s %.10f\n", (reanim.progress > threshold) ? ">" : (reanim.progress == threshold) ? "=" : "<", threshold);
        printf("\n");
    }

    return 0;
}

// 24, 6 小鬼落地
// 16, 33 巨人砸 0.64f
// 24, 34 巨人投 0.74f
