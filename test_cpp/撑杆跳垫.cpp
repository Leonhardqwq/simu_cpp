# include "../inc/calculate_position_v2.hpp"
# include "../inc/util.hpp"

// 撑杆行走极值计算，减速行走需要搜索“测试”修改初始减速条件
std::vector<std::vector<float>> cal_pole_vault_x_extrem(int M_sup = 500, bool output = true) {
    std::vector<std::vector<float>> x, y={{},{}};
    x.push_back(
        cal_x_extrem(
            ZombieType::PoleVaulting_Walk, M_sup, 0, {}, {},
            PositionCalculator::TypeCal::FASTEST,
            true,false
    ));
    x.push_back(
        cal_x_extrem(
            ZombieType::PoleVaulting_Walk, M_sup, 0, {}, {},
            PositionCalculator::TypeCal::SLOWEST,
            true,false
    ));
    for (size_t i = 1; i < x[0].size(); ++i) {
        y[0].push_back(870.0f - x[0][i]);
        y[1].push_back(879.0f - x[1][i]);
    }
    if (output)
        write_2dvector_to_csv(y, "output.csv", true);
    return y;
}

// 撑杆修正起跳点
// 坐标修正量:  -(int(x_z) - x_p - 80)*180/179.1666 
// 修正起跳点:  -(int(x_z) - x_p - 80)*180/179.1666 + int(x_z) + frac(x_z)
//           = frac(x_z) - int(x_z) * (180/179.1666 - 1) + (x_p + 80)*180/179.1666
// 修正起跳点关于 int(x_z) 单减，关于 frac(x_z) 单增
// 最小修正起跳点 = int(x_z) 最大 & frac(x_z) 最小
// 最大修正起跳点 = int(x_z) 最小 & frac(x_z) 最大
// 如跳9普，<760可跳，最小修正起跳点759.000
float cal_pole_vault_x_jump(float x_z, int x_p){
    Reanim reanim(24, 43);
    float aAnimDuration = int(reanim.n_frames) / reanim.fps * 100.0f;
    int aJumpDistance = int(x_z) - x_p - 80;
    float mVelX = aJumpDistance / aAnimDuration;
    float x = x_z;
    for (int i = 0; i < 180; ++i) 
        x = x - mVelX;
    return x;
}

// 跳存在植物，返回{落地到啃食最短时间, 最早啃食时机}
// 垫材信息，守植物信息，冰时机，是否减速
// 最后一行的walk为理论最短落地到啃食时间
std::pair<int, int> cal_min_pole_vault_eat(
    PlantDefType type_folder, int col_folder, int offset_folder,
    PlantDefType type_defender, int col_defender, 
    int t_ice, bool is_slowed
) {
    auto data_walk = read_2dvector_from_csv<float>(
        std::string("../data/polevaulting_walk/") + (is_slowed?"slow":"norm") + ".csv"
    )[0];
    auto data_run = read_2dvector_from_csv<float>(
        std::string("../universal_table/origin/") + std::to_string(t_ice) + "_fast.csv"
    )[4];


    auto p_folder = PlantDefData(type_folder, col_folder, offset_folder);
    auto p_defender = PlantDefData(type_defender, col_defender);
    
    int x_start = p_folder.x + p_folder.def.second + 29;
    int x_eat = p_defender.x + p_defender.def.second - 50 + 1; // +1 修正距离计算

    int t_z_min = -1;
    std::vector<float> xs_z;
    for (int i = 0; i < data_run.size(); ++i) {
        float x_z = data_run[i];
        if (int(x_z) == x_start) {
            xs_z.push_back(x_z);
            if (t_z_min == -1) t_z_min = i;
        }
        else if(int(x_z) < x_start) {
            xs_z.push_back(x_start);
            break;
        }
    }

    printf(" run + jump + walk =  eat, ");
    printf("start jump     -> fix jump       -> land           -> eat, ");
    printf("walk distance\n");
    int t_walk = 0;
    int t_min_ans = std::numeric_limits<int>::max();
    for (int i = 0; i < xs_z.size(); ++i) {
        int t_z = t_z_min + i;
        float x_z = xs_z[i];
        float x_jump = cal_pole_vault_x_jump(x_z, p_folder.x);
        float x_land = x_jump - 150.0f;
        float x_walk = x_land - (x_eat);
        for (int j=0; j < data_walk.size(); ++j) {
            if (data_walk[j] > x_walk) {
                t_walk = j+1;
                break;
            }
        }
        int t_min = t_z + 181 + t_walk;
        if (t_min < t_min_ans) t_min_ans = t_min;
        printf(
            "%4d +  181 + %4d = %4d, %.10f -> %.10f -> %.10f -> %d, %.10f\n", 
            t_z, t_walk, t_min, 
            x_z, x_jump, x_land, x_eat, 
            x_walk
        );
    }
    return {t_walk, t_min_ans};
}

std::string format_label(int col, const char* suffix) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d%s", col, suffix);
    return std::string(buffer);
}

void cal_all_cases(){
    struct FolderInfo {
        PlantDefType type;
        int col;
        int offset;
    };
    struct CaseInfo {
        PlantDefType folder_type;
        int folder_col;
        int offset_folder;
        PlantDefType defender_type;
        int defender_col;
    };

    // folder: 9普, 9小喷(-5), 89炮, 8普, 8南, 78炮
    FolderInfo folder_infos[] = {
        {PlantDefType::Normal, 9, 0},
        {PlantDefType::Normal, 9, -5},
        {PlantDefType::CobCannon, 8, 0},
        {PlantDefType::Normal, 8, 0},
        {PlantDefType::Pumpkin, 8, 0},
        {PlantDefType::CobCannon, 7, 0},
    };

    std::vector<CaseInfo> case_infos;

    for (const auto& folder : folder_infos) {
        for (const auto& defender_type : {PlantDefType::Normal, PlantDefType::CobCannon}) {
            // defender: 普, 炮
            int defender_col = folder.col - ((defender_type == PlantDefType::Normal) ? 1 : 2);
            case_infos.push_back({
                folder.type, folder.col, folder.offset,
                defender_type, defender_col
            });
        }
    }

    int len_ice_times = 6;
    std::vector<std::string> folder_strs, defender_strs;
    std::vector<std::string> norm_walk_time_strs, slow_walk_time_strs;
    std::vector<std::vector<std::string>> ice_t_eat_time_strs(len_ice_times); // 0, 0, 1, 11, 12, 96

    int ice_times[] = {0, 0, 1, 11, 12, 96};
    for (const auto& c : case_infos) {
        const char* folder_suffix =
            (c.folder_type == PlantDefType::Normal)
                ? (c.offset_folder == 0 ? "普" : "小喷")
                : (c.folder_type == PlantDefType::CobCannon ? "炮" : "南瓜");
        const char* defender_suffix =
            (c.defender_type == PlantDefType::Normal) ? "普" : "炮";

        folder_strs.push_back(format_label(
            c.folder_type != PlantDefType::CobCannon
                ? c.folder_col
                : c.folder_col * 11 + 1,
            folder_suffix
        ));
        defender_strs.push_back(format_label(
            c.defender_type != PlantDefType::CobCannon
                ? c.defender_col
                : c.defender_col * 11 + 1,
            defender_suffix
        ));

        printf("\n跳%s 啃%s\n", folder_strs.back().c_str(), defender_strs.back().c_str());

        int norm_walk_time = cal_min_pole_vault_eat(
            c.folder_type, c.folder_col, c.offset_folder,
            c.defender_type, c.defender_col,
            0, false
        ).first;
        int slow_walk_time = cal_min_pole_vault_eat(
            c.folder_type, c.folder_col, c.offset_folder,
            c.defender_type, c.defender_col,
            0, true
        ).first;
        norm_walk_time_strs.push_back(std::to_string(norm_walk_time));
        slow_walk_time_strs.push_back(std::to_string(slow_walk_time));

        for (int i = 0; i < len_ice_times; ++i) {
            int t_ice = ice_times[i];
            int eat_time = cal_min_pole_vault_eat(
                c.folder_type, c.folder_col, c.offset_folder,
                c.defender_type, c.defender_col,
                t_ice, i == 1 ? 1 : t_ice
            ).second;

            ice_t_eat_time_strs[i].push_back(std::to_string(eat_time));
        }
    }
    
    std::vector<std::vector<std::string>> ans_table = {
        folder_strs, defender_strs, 
        norm_walk_time_strs, slow_walk_time_strs
    };
    ans_table.insert(ans_table.end(), ice_t_eat_time_strs.begin(), ice_t_eat_time_strs.end());
    write_2dvector_to_csv(ans_table, "../Pole/cases/output.csv", false);
}


int main(){
    cal_all_cases(); return 0;
    // cal_pole_vault_x_extrem(10001, true);

    int t_ice = 1;
    bool is_slowed = 1;

    printf("\n%d冰 跳9普 啃78炮\n", t_ice);
    cal_min_pole_vault_eat(
        PlantDefType::Normal, 9, 0,
        PlantDefType::CobCannon, 7,
        t_ice, t_ice
    );

    printf("\n%d冰 跳9小喷(-5) 啃78炮\n", t_ice);
    cal_min_pole_vault_eat(
        PlantDefType::Normal, 9, -5,
        PlantDefType::CobCannon, 7,
        t_ice, is_slowed
    );



    printf("\n%d冰 跳9普 啃8普\n", t_ice);
    cal_min_pole_vault_eat(
        PlantDefType::Normal, 9, 0,
        PlantDefType::Normal, 8,
        t_ice, is_slowed
    );

    printf("\n%d冰 跳9小喷(-5) 啃8普\n", t_ice);
    cal_min_pole_vault_eat(
        PlantDefType::Normal, 9, -5,
        PlantDefType::Normal, 8,
        t_ice, is_slowed
    );



    printf("\n%d冰 跳78炮 啃56炮\n", t_ice);
    cal_min_pole_vault_eat(
        PlantDefType::CobCannon, 7, 0,
        PlantDefType::CobCannon, 5,
        t_ice, is_slowed
    );


    return 0;
}