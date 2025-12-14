#include "../../inc/tools/Enter.hpp"
# include "../../inc/util.hpp"

int main(){
    EnterConfig config("../enter_config.json");
    // test_x(ZombieType::Pogo, 2000, false, {}, {}, PositionCalculator::TypeCal::RANDOM);
    cal_x_extrem(
        config.type, config.M, config.hugewave, 
        config.ice_t, config.splash_t, 
        PositionCalculator::TypeCal(config.test_type_zombie)
        , true
    );


    return 0;
}