# include "inc/calculate_imp.hpp"
# include "inc/util.hpp"
#include <iostream>


void print_imp(){
    ImpCalculator cal(
        300,    
        Scene::ROOF, 
        0.0f, 
        608.665f, 
        0, 
        CdState{0, 0}, 
        1
    );
    cal.init();
    cal.calculate_imp();

    printf("%d %d\n", cal.res, cal.t_enter);

    for (size_t i = 0; i < cal.x_ans.size(); i++) 
        printf("time:%.3d, x:%.1f y:%.1f h:%.10f\n", int(i), cal.x_ans[i], cal.y_ans[i], cal.dy_ans[i]);
}

void calc_extrem(){
    auto x_ull_start = bit_cast<uint32_t>(400.0f);
    auto x_ull_end = bit_cast<uint32_t>(854.0f);    
    std::cout<<x_ull_end - x_ull_start + 1<<std::endl;
    for(auto i = x_ull_start; i <= x_ull_end; ++i){
        auto x = bit_cast<float>(i);
        if (i% 10000 == 0) {
            printf("%u %u %u\n", x_ull_start, i, x_ull_end);
        }
    }

}

int main() {
    print_imp();

    // calc_extrem();
    






    return 0;
}