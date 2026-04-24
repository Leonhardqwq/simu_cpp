# include "../inc/calculate_position_v2.hpp"
# include "../inc/util.hpp"
#include <string>

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

// 跳存在植物
// 垫材信息，守植物信息，冰时机，是否减速
// 最后一行的walk为理论最短落地到啃食时间
void cal_min_pole_vault_eat(
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
    for (int i = 0; i < xs_z.size(); ++i) {
        int t_z = t_z_min + i;
        float x_z = xs_z[i];
        float x_jump = cal_pole_vault_x_jump(x_z, p_folder.x);
        float x_land = x_jump - 150.0f;
        float x_walk = x_land - (x_eat);
        int t_walk = 0;
        for (int j=0; j < data_walk.size(); ++j) {
            if (data_walk[j] > x_walk) {
                t_walk = j+1;
                break;
            }
        }
        int t_min = t_z + 181 + t_walk;
        printf(
            "%4d +  181 + %4d = %4d, %.10f -> %.10f -> %.10f -> %d, %.10f\n", 
            t_z, t_walk, t_min, 
            x_z, x_jump, x_land, x_eat, 
            x_walk
        );
    }
}



int main(){
    // cal_pole_vault_x_extrem(10001, true);

    int t_ice = 0;



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
        t_ice, t_ice
    );

    printf("\n%d冰 跳9小喷(+4) 啃78炮\n", t_ice);
    cal_min_pole_vault_eat(
        PlantDefType::Normal, 9, 4,
        PlantDefType::CobCannon, 7,
        t_ice, t_ice
    );



    printf("\n%d冰 跳9普 啃8普\n", t_ice);
    cal_min_pole_vault_eat(
        PlantDefType::Normal, 9, 0,
        PlantDefType::Normal, 8,
        t_ice, t_ice
    );

    printf("\n%d冰 跳9小喷(-5) 啃8普\n", t_ice);
    cal_min_pole_vault_eat(
        PlantDefType::Normal, 9, -5,
        PlantDefType::Normal, 8,
        t_ice, t_ice
    );

    printf("\n%d冰 跳9小喷(+4) 啃8普\n", t_ice);
    cal_min_pole_vault_eat(
        PlantDefType::Normal, 9, 4,
        PlantDefType::Normal, 8,
        t_ice, t_ice
    );



    printf("\n%d冰 跳78炮 啃56炮\n", t_ice);
    cal_min_pole_vault_eat(
        PlantDefType::CobCannon, 7, 0,
        PlantDefType::CobCannon, 5,
        t_ice, t_ice
    );

    printf("\n跳跃中冰 跳78炮 啃56炮\n");
    cal_min_pole_vault_eat(
        PlantDefType::CobCannon, 7, 0,
        PlantDefType::CobCannon, 5,
        0, 1
    );

    return 0;
}