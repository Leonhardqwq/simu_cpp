# include "../inc/calculate_position_v2.hpp"
# include "../inc/calculate_hit_time.hpp"
# include "../inc/util.hpp"
# include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>
using namespace std;
const int M = 11000;


void cal(int ice_t = 0, PositionCalculator::TypeCal type_cal = PositionCalculator::TypeCal::FASTEST){
    struct ConfigItem {
        string name;
        ZombieType zombie_type;
        bool huge_wave;
    };

    vector<ConfigItem> configs = {
        {"普僵", ZombieType::Zombie1, false},
        {"普僵dc快", ZombieType::Unknown, false},
        {"普僵dc慢", ZombieType::Unknown, false},
        {"旗帜", ZombieType::Flag, false},
        {"撑杆", ZombieType::PoleVaulting, false},
        {"读报", ZombieType::Newspaper, false},
        {"铁门", ZombieType::ScreenDoor, false},
        {"橄榄", ZombieType::Football, false},
        {"舞王", ZombieType::Dancing, false},
        {"潜水", ZombieType::Snorkel, false},
        {"旗帜波潜水", ZombieType::Snorkel, true},
        {"冰车", ZombieType::Unknown, false},
        {"海豚", ZombieType::DolphinRider, false},
        {"旗帜波海豚", ZombieType::DolphinRider, true},
        {"小丑", ZombieType::JackInTheBox, false},
        {"气球", ZombieType::Balloon, false},
        {"矿工", ZombieType::Unknown, false},
        {"旗帜波矿工", ZombieType::Unknown, true},
        {"跳跳", ZombieType::Pogo, false},
        {"扶梯", ZombieType::Ladder, false},
        {"投篮", ZombieType::Catapult, false},
        {"巨人", ZombieType::Gargantuar, false},
    };
    
    vector<vector<float>> x_table;

    for (const auto& item : configs) {
        const string& name = item.name;
        ZombieType zombie_type = item.zombie_type;
        bool huge_wave = item.huge_wave;
        printf("%s\n", name.c_str());

        vector<float> x;
        if (zombie_type == ZombieType::Unknown) {
            x.resize(M, NAN);
        }
        else if (zombie_type == ZombieType::Zombie1) {
            auto x1 = cal_x_extrem(
                ZombieType::Zombie1, M, huge_wave, {ice_t}, {}, 
                type_cal, true, false
            );
            auto x2 = cal_x_extrem(
                ZombieType::Zombie2, M, huge_wave, {ice_t}, {}, 
                type_cal, true, false
            );
            for (size_t i = 0; i < x1.size(); ++i) {
                if (type_cal == PositionCalculator::TypeCal::FASTEST)
                    x.push_back(min(x1[i], x2[i]));
                else
                    x.push_back(max(x1[i], x2[i]));
            }
            ZombieData z(zombie_type);
            auto threshold = z.threshold;
            for (int i = M - 1; i > 0; i--) 
                if (int(x[i-1]) <= threshold) 
                    x[i] = NAN;
        }
        else if (zombie_type == ZombieType::Dancing) {
            PositionCalculator cal(zombie_type, M, huge_wave, {ice_t}, {});
            cal.type_cal = type_cal;
            cal.init();
            cal.action_cd = type_cal == PositionCalculator::TypeCal::FASTEST ? 11 : 0;
            cal.calculate_position();
            x = cal.x;
            for (int i = M - 1; i >= cal.res; i--) 
                x[i] = NAN;
        }
        else {
            x = cal_x_extrem(
                zombie_type, M, huge_wave, {ice_t}, {},
                type_cal, true, false
            );
            ZombieData z(zombie_type);
            auto threshold = z.threshold;
            for (int i = M - 1; i > 0; i--) 
                if (int(x[i-1]) <= threshold) 
                    x[i] = NAN;
        }
        x_table.push_back(x);
    }
    
    write_2dvector_to_csv(x_table, "x_table.csv", true);
}
int main(){
    cal(0, PositionCalculator::TypeCal::SLOWEST);
    return 0;
}