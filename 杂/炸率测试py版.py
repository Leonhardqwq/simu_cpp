import copy
import random
import numpy as np
import pandas as pd
# 你需要修改的地方会用 ### 和 ### 作提示
###
show_pro = True     # 是否展示测试过程

not_flag = True     # 是否通常波
jack_type = 0    #Jack参数: 0无限制,1锁早爆,2锁晚爆

ice_t = [1]       # 冰时机列表
slow_t = []         # 减速时机列表
slow_with_melon = True      # 减速伴随溅射伤害
melon = 0           # 固定溅射数量

_stop_xt = []
# 啃食信息列表，其中每个元素为[啃食坐标,啃食时长]，且从左到右啃食坐标需要递减
# 例如  [[750,100],[600,100]]
# n列植物啃食坐标=80*n-10，南瓜+30，高坚果+20，炮-10

M_sup = 4000
end_time = 3000
###


M = M_sup
N_animation = 18
total_x = 20.9
x_each_animation = [0,
	0, 0.6, 0.6, 0.7, 3.6, 3.6, 1, 1, 0, 0.9,
	1, 1,   1.8, 1.9,   0,   0, 1.6, 1.6]
class Jack:
    def __init__(self,ty):
        # 334->335
        self.hp = 335 - 26*melon
        self.v = random.uniform(0.66, 0.68)
        self.v = 0.68
        self.timing = 6000
        self.x = self.x_list_xc_fix()
        
    def x_list_xc_fix(self):
        global M
        global stop_xt
        M = M_sup
        speed_para = self.v
        x = np.zeros(M)
        s = np.ones(2*M)
        x[0] = random.randint(780, 819) + (0 if not_flag else 40)
        completing_rate = 0
        for t in slow_t:
            # t -> t+1
            s[t+1:t+1000] = 0.5
        for t in ice_t:
            # 399-599 / 299-399  |  s[t]==1 (and s[t-1]==1)
            frozen = random.randint(400,600) if s[t]==1 and s[t-1]==1 else random.randint(300,400)
            frozen = 400
            frozen = frozen - 1 #
            s[t:t+frozen] = 0
            # 2000->1999
            s[t+frozen:t+1999] = 0.5 #
            if self.timing > t:
                self.timing = self.timing + frozen + 1
        # M = self.timing + 10

        completing_rate = 0.47/total_x*speed_para
        for i in range(1,M):
            speed = speed_para*s[i]

            dlt_x = 0.47/total_x*(1+N_animation)*speed*x_each_animation[int(1+completing_rate*N_animation)]
            x[i]=x[i-1]-dlt_x
            
            completing_rate = completing_rate+0.47/total_x*speed
            if completing_rate>=1:   completing_rate = completing_rate-1
            
            if x[i]<=-100:
                break
        return x
    
    def x_list_xc(self):
        global M
        global stop_xt
        M = M_sup
        speed_para = self.v
        x = np.zeros(M)
        s = np.ones(2*M)
        x[0] = random.randint(780, 819) + (0 if not_flag else 40)
        completing_rate = 0
        for t in slow_t:
            s[t:t+1000] = 0.5
        for t in ice_t:
            frozen = random.randint(400,600) if s[t]==1 else random.randint(300,400)
            frozen = 400
            s[t:t+frozen] = 0
            s[t+frozen:t+2000] = 0.5
            if self.timing > t:
                self.timing = self.timing + frozen
        # M = self.timing + 10

        stop_xt = copy.deepcopy(_stop_xt)
        ori_xt = copy.deepcopy(stop_xt)
        for i in range(1,M):
            if len(stop_xt)>0 and int(x[i-1])<=stop_xt[0][0]:
                stop_xt[0][1] = stop_xt[0][1]-1
                x[i] = x[i-1]
                if stop_xt[0][1] == 0:
                    tmp = stop_xt[0]
                    stop_xt.remove(tmp)
                    temp = ori_xt[0]
                    if max(s[temp[0]:(temp[0]+temp[1])])>0:
                        completing_rate = 0
                    ori_xt.remove(temp)
                    # print(i,'rate',completing_rate)
                continue
            speed = speed_para*s[i]
            completing_rate = completing_rate+0.47/total_x*speed
            if completing_rate>1:
                completing_rate = completing_rate-1
            dlt_x = 0.47/total_x*(1+N_animation)*speed*x_each_animation[int(1+completing_rate*N_animation)]
            x[i]=x[i-1]-dlt_x
            if x[i]<=-100:
                break
        return x


jack = Jack(jack_type)
jack.x = jack.x  + 780 - jack.x[0]
# pd.DataFrame({'t': range(len(jack.x)), 'x': np.round(jack.x, -5)}).to_csv('out.csv', index=False, header=True)
pd.DataFrame({'index': range(len(jack.x)), 'value': jack.x}).to_csv('out.csv', index=False, header=True)
print(jack.x[:])
