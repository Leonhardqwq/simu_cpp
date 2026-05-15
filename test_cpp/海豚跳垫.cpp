// 海豚起跳坐标的亚像素部分对啃食时机有影响，越左起跳，总路程里骑行的部分越多，水中行走的越少，啃食时机越早
// 但是最早起跳的海豚亚像素情况可能并非最优
// 感觉这是个不小的问题，之前撑杆的结论可能也有问题
// 如果跳过的植物是垫材这种某个时机放下去的，那确实可以按极快表取一个跳跃前路程最长的时机下垫
// 但是如果跳过的植物是长期存在的，那这里的亚像素情况怎么算最优呢
// 遍历浮点数好像能解决这个问题，但是就得对每种用冰情况都遍历，不能直接用减速波万能表的结论了
# include "../inc/calculate_position_v2.hpp"
# include "../inc/util.hpp"


// 海豚匀速跳跃 -0.5
float cal_dolphin_rider_x_jump(float x_z){
    float x = x_z;
    for (int i = 0; i < 113; ++i) 
        x = x - 0.5f;
    return x;
}

// 跳存在植物，返回最早啃食时机
// 垫材信息，守植物信息，冰时机，是否减速
std::pair<int, int> cal_min_dolphin_rider_eat(
    PlantDefType type_folder, int col_folder,
    PlantDefType type_defender, int col_defender,
    int t_ice, bool is_slowed
) {
    auto data_swim = read_2dvector_from_csv<float>(
        std::string("../data/dolphinrider_swim/") + (is_slowed?"slow":"norm") + ".csv"
    )[0];
    auto data_ride = read_2dvector_from_csv<float>(
        std::string("../universal_table/origin/") + std::to_string(t_ice) + "_fast.csv"
    )[12];

    auto p_folder = PlantDefData(type_folder, col_folder);
    auto p_defender = PlantDefData(type_defender, col_defender);
    
    int x_start = p_folder.x + p_folder.def.second + 29;
    if (x_start > 650) x_start = 650; // 落水即跳
    int x_eat = p_defender.x + p_defender.def.second - 30 + 1; // +1 修正距离计算

    int t_z_min = -1;
    std::vector<float> xs_z;
    for (int i = 0; i < data_ride.size(); ++i) {
        float x = data_ride[i];
        if (int(x) == x_start || (xs_z.empty() && int(x) <= x_start)) {
            xs_z.push_back(x);
            if (t_z_min == -1) t_z_min = i;
        }
        else if(int(x) < x_start) {
            break;
            xs_z.push_back(x_start);
        }
    }

    printf("walk&ride + jump + swim =  eat, ");
    printf("start jump     -> before land    -> land           -> eat, ");
    printf("walk distance\n");
    int t_swim=0;
    int t_min_ans = std::numeric_limits<int>::max();
    for (int i = 0; i < xs_z.size(); ++i) {
        int t_z = t_z_min + i;
        float x_z = xs_z[i];
        float x_before_land = cal_dolphin_rider_x_jump(x_z);
        float x_land = x_before_land - 94.0f;
        float x_swim = x_land - (x_eat);
        for (int j=0; j < data_swim.size(); ++j) {
            if (data_swim[j] > x_swim) {
                t_swim = j+1;
                break;
            }
        }
        int t_min = t_z + 232 + t_swim;
        if (t_min < t_min_ans) t_min_ans = t_min;
        printf(
            "%9d +  232 + %4d = %4d, %.10f -> %.10f -> %.10f -> %d, %.10f\n", 
            t_z, t_swim, t_min, 
            x_z, x_before_land, x_land, x_eat, 
            x_swim
        );
        break;
    }

    return {t_swim, t_min_ans};
}

std::string format_label(int col, const char* suffix) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d%s", col, suffix);
    return std::string(buffer);
}

void cal_all_cases(){
    // folder: 普, 南
    // defender: 普, 炮

    int len_ice_times = 6;
    std::vector<std::string> folder_strs, defender_strs;
    std::vector<std::string> norm_swim_time_strs, slow_swim_time_strs;
    std::vector<std::vector<std::string>> ice_t_eat_time_strs(len_ice_times); // 0, 0, 1, 11, 12, 96

    int ice_times[] = {0, 0, 1, 11, 12, 96};

    for (int folder_col = 8; folder_col >= 3; --folder_col) {
    for (PlantDefType folder_type : {PlantDefType::Normal, PlantDefType::Pumpkin}) {
    for (PlantDefType defender_type : {PlantDefType::Normal, PlantDefType::CobCannon}) {

        if (folder_type == PlantDefType::Pumpkin && folder_col == 8) continue; // 跳8只有1种情况

        int defender_col = folder_col - ((defender_type == PlantDefType::Normal) ? 1 : 2) - 1;

        // 分割线
        const char* folder_suffix =
            (folder_type == PlantDefType::Normal) ? "普" : "南瓜";
        const char* defender_suffix =
            (defender_type == PlantDefType::Normal) ? "普" : "炮";

        folder_strs.push_back(format_label(
            folder_col,
            folder_suffix
        ));
        defender_strs.push_back(format_label(
            defender_type != PlantDefType::CobCannon
                ? defender_col
                : defender_col * 11 + 1,
            defender_suffix
        ));
        printf("\n跳%s 啃%s\n", folder_strs.back().c_str(), defender_strs.back().c_str());

        /*
        int norm_walk_time = cal_min_dolphin_rider_eat(
            folder_type, folder_col, 
            defender_type, defender_col,
            0, false
        ).first;
        int slow_walk_time = cal_min_dolphin_rider_eat(
            folder_type, folder_col, 
            defender_type, defender_col,
            0, true
        ).first;

        norm_swim_time_strs.push_back(std::to_string(norm_walk_time));
        slow_swim_time_strs.push_back(std::to_string(slow_walk_time));
        */

        for (int i = 0; i < len_ice_times; ++i) {
            int t_ice = ice_times[i];
            int eat_time = cal_min_dolphin_rider_eat(
                folder_type, folder_col, 
                defender_type, defender_col,
                t_ice, i == 1 ? 1 : t_ice
            ).second;

            ice_t_eat_time_strs[i].push_back(std::to_string(eat_time));
        }
    }}}
    
    std::vector<std::vector<std::string>> ans_table = {
        folder_strs, defender_strs, 
        // norm_swim_time_strs, slow_swim_time_strs
    };
    ans_table.insert(ans_table.end(), ice_t_eat_time_strs.begin(), ice_t_eat_time_strs.end());
    write_2dvector_to_csv(ans_table, "../Dolphin/cases/output.csv", false);
}


int main(){
    cal_all_cases(); return 0;
    // cal_anim_x_extrem(ZombieType::DolphinRider_Swim, 10001, true);

    int t_ice = 1;
    bool is_slowed = 1;

    
    printf("\n%d冰 跳8 啃67炮\n", t_ice);
    cal_min_dolphin_rider_eat(
        PlantDefType::Normal, 8,
        PlantDefType::CobCannon, 6,
        t_ice, t_ice
    );

    printf("\n%d冰 跳8 啃6普\n", t_ice);
    cal_min_dolphin_rider_eat(
        PlantDefType::Normal, 8,
        PlantDefType::Normal, 6,
        t_ice, t_ice
    );

    printf("\n%d冰 跳8 啃56炮\n", t_ice);
    cal_min_dolphin_rider_eat(
        PlantDefType::Normal, 8,
        PlantDefType::CobCannon, 5,
        t_ice, t_ice
    );

    return 0;
}