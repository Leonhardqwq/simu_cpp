# include "../inc/calculate_position_v2.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <utility>
using namespace std;
const int M = 11000;


void cal(int ice_t = 0, PositionCalculator::TypeCal type_cal = PositionCalculator::TypeCal::FASTEST, string output_file = "x_table.csv", vector<string> cal_list = {}) {
    struct ConfigItem {
        string name;
        ZombieType zombie_type;
        bool huge_wave;
        PositionCalculator::Dancecheat dc_type;
    };

    vector<ConfigItem> configs = {
        {"普僵", ZombieType::Zombie1, false, PositionCalculator::Dancecheat::NONE},
        {"普僵dc快", ZombieType::Zombie1, false, PositionCalculator::Dancecheat::FAST},
        {"普僵dc慢", ZombieType::Zombie1, false, PositionCalculator::Dancecheat::SLOW},
        {"旗帜", ZombieType::Flag, false, PositionCalculator::Dancecheat::NONE},
        {"撑杆", ZombieType::PoleVaulting, false, PositionCalculator::Dancecheat::NONE},
        {"读报", ZombieType::Newspaper, false, PositionCalculator::Dancecheat::NONE},
        {"铁门", ZombieType::ScreenDoor, false, PositionCalculator::Dancecheat::NONE},
        {"橄榄", ZombieType::Football, false, PositionCalculator::Dancecheat::NONE},
        {"舞王", ZombieType::Dancing, false, PositionCalculator::Dancecheat::NONE},
        {"潜水", ZombieType::Snorkel, false, PositionCalculator::Dancecheat::NONE},
        {"旗帜波潜水", ZombieType::Snorkel, true, PositionCalculator::Dancecheat::NONE},
        {"冰车", ZombieType::Zomboni, false, PositionCalculator::Dancecheat::NONE},
        {"海豚", ZombieType::DolphinRider, false, PositionCalculator::Dancecheat::NONE},
        {"旗帜波海豚", ZombieType::DolphinRider, true, PositionCalculator::Dancecheat::NONE},
        {"小丑", ZombieType::JackInTheBox, false, PositionCalculator::Dancecheat::NONE},
        {"气球", ZombieType::Balloon, false, PositionCalculator::Dancecheat::NONE},
        {"矿工", ZombieType::Digger, false, PositionCalculator::Dancecheat::NONE},
        {"旗帜波矿工", ZombieType::Digger, true, PositionCalculator::Dancecheat::NONE},
        {"跳跳", ZombieType::Pogo, false, PositionCalculator::Dancecheat::NONE},
        {"扶梯", ZombieType::Ladder, false, PositionCalculator::Dancecheat::NONE},
        {"投篮", ZombieType::Catapult, false, PositionCalculator::Dancecheat::NONE},
        {"巨人", ZombieType::Gargantuar, false, PositionCalculator::Dancecheat::NONE},
    };
    
    vector<vector<float>> x_table;
    if (!cal_list.empty()) {
        for (auto it = configs.begin(); it != configs.end(); it++) 
            if (find(cal_list.begin(), cal_list.end(), it->name) == cal_list.end()) 
                it->zombie_type = ZombieType::Unknown;
    }

    for (const auto& item : configs) {
        const string& name = item.name;
        ZombieType zombie_type = item.zombie_type;
        bool huge_wave = item.huge_wave;
        PositionCalculator::Dancecheat dc_type = item.dc_type;
        printf("%s %s\n", name.c_str(), output_file.c_str());

        vector<float> x;
        if (zombie_type == ZombieType::Unknown) 
            x.resize(M, NAN);
        else if (zombie_type == ZombieType::Zombie1 || zombie_type == ZombieType::Zombie2 || zombie_type == ZombieType::ScreenDoor) {
            vector<float> x1, x2;
            x1 = cal_x_extrem(
                ZombieType::Zombie1, M, huge_wave, {ice_t}, {}, 
                type_cal, true, false,
                0.0f, 0.0f, dc_type
            );
            x2 = cal_x_extrem(
                ZombieType::Zombie2, M, huge_wave, {ice_t}, {}, 
                type_cal, true, false,
                0.0f, 0.0f, dc_type
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
        else if (zombie_type == ZombieType::Digger) {
            x = cal_digger_x_extrem(
                M, huge_wave, {ice_t}, 
                type_cal, true, false
            );
            for (int i = M - 1; i > 0; i--) {
                if (int(x[i-1]) > 750) 
                    x[i] = NAN;      
                else break;          
            }
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
        write_2dvector_to_csv(x_table, output_file, true);
    }
}
int main(){
    string output_folder = "new";
    vector<string> cal_list = {
        // "普僵", 
        // "普僵dc快", 
        // "普僵dc慢",
        // "旗帜", "撑杆", "读报", "铁门", "橄榄", 
        // "舞王", "潜水", "旗帜波潜水", "冰车", "海豚", 
        // "旗帜波海豚", "小丑", "气球","矿工", "旗帜波矿工",
        // "跳跳", "扶梯", 
        // "投篮", 
        "巨人"
    };
    for (int ice_t : {
        // 0, 1, 11, 12, 
        // 96
        300
    }) {
        cal(ice_t, PositionCalculator::TypeCal::FASTEST, "data/" + output_folder + "/" + to_string(ice_t) + "_fast.csv", cal_list);
        cal(ice_t, PositionCalculator::TypeCal::SLOWEST, "data/" + output_folder + "/" + to_string(ice_t) + "_slow.csv", cal_list);
    }
    return 0;
}