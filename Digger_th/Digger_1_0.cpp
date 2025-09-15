#include <cstdio>
#include <iostream>
#include "../inc/util.hpp"

std::vector<double> digger_prob(int ice){
    std::vector<double> cnt(3000, 0.0);
    for (int frozen=399; frozen<=599; frozen++){
        for (int L=780-10;L<820-10;L++){
            int c = 962-ice+frozen;
            double v_lowerbound = 0.66f;
            double v_upperbound = 0.68f;
            auto lo = (int)ceil(1.0*L/v_upperbound);
            auto hi = (int)ceil(1.0*L/v_lowerbound);
            for (int k=lo;k<=hi;k++){
                auto v_lo = 1.0*L / k;
                auto v_hi = 1.0*L/(k-1);
                if(v_lo<v_lowerbound)  v_lo=v_lowerbound;
                if(v_hi>v_upperbound) v_hi=v_upperbound;
                int can_eat = c + 2*k;
                int real_eat = can_eat%8 ?  (can_eat/8+1)*8: can_eat;
                int land = k+131;

                int ans = real_eat-land; // 输出时间
                // int ans = real_eat; // 啃食时机

                cnt[ans] += (v_hi - v_lo) / (0.68f-0.66f) / 8040;
            }
        }
    }
    write_vector_to_csv(cnt, "result.csv");
    return cnt;
}
/*
void query(){
    int ice;
    printf("ice time: ");
    scanf("%d", &ice);
    if(ice<1){
        printf("wrong ice time\n");
        return;
    }

    auto prob = catapult_prob(ice);
    for(int i=0;i<prob.size();i++)
        if(prob[i]!=0){
            printf("Earliest shooting time: %d\n", i);
            break;
        }
    
    int t_lo, t_hi;
    printf("[t_lo, t_hi]: ");
    scanf("%d %d", &t_lo, &t_hi);
    if(t_lo>t_hi){
        printf("wrong range\n");
        return;
    }
    if(t_lo<0) t_lo=0;
    if(t_hi>=3000) t_hi=2999;

    double ans=0;
    for (int i=t_lo;i<=t_hi;i++)
        ans += prob[i];
    printf("Probability: %e\n\n", ans);
}

*/

void myquery(int ice){
    printf("ice time: %d\n", ice);
    if(ice<1){
        printf("wrong ice time\n");
        return;
    }

    auto prob = digger_prob(ice);
    for(int i=0;i<prob.size();i++)
        if(prob[i]!=0){
            printf("Earliest eating time: %d\n", i);
            break;
        }
}

int main(){
    // 1358-1614
    // while (1)   query();
    myquery(1464);
    return 0;
}