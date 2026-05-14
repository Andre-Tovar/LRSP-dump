#!/usr/bin/env python
# coding: utf-8

# In[64]:


import random
import json

# alpha parameter for setting the vehicle capacity
#alpha_lst = {0 : 'a', 0.25 : 'b', 0.5 : 'c', 0.75 : 'd', 1 : 'e'}
alpha_lst = {0 : 'a1', 0.25 : 'a2'}

#sizes = {0 : [10, 4], 1 : [15, 6], 2 : [20, 8]}
sizes = {0 : [15, 6]}

for s in sizes:
    for a in alpha_lst:
        for j in range(1, 4):
            file_name = ('r' + str(sizes[s][0]) + 'x' + str(sizes[s][1]) + '_' + 
                                            str(alpha_lst[a]) + '_' + str(j) + '.py')
                
            f = open(file_name, "w+")
            
            DEMAND = {}
            
            for i in range(1, sizes[s][0] + 1):
                x = random.uniform(0, 100)
                DEMAND[i] = round(x) 
                
            f.write("DEMAND = " + json.dumps(DEMAND) + "\n")
            
            NODES_LOCATION = {}
            
            for i in range(1, sizes[s][0] + sizes[s][1] + 1):
                x = random.uniform(0, 100)
                y = random.uniform(0, 100)
                NODES_LOCATION[i] = (x, y)  
                
            f.write("NODES_LOCATION = " + json.dumps(NODES_LOCATION) + "\n")
            
            VEHICLE_CAPACITY = (1 - a) * max(DEMAND.values()) + a * sum(DEMAND.values())
            
            f.write("VEHICLE_CAPACITY = " + json.dumps(VEHICLE_CAPACITY) + "\n")
            
            FIXED_COST = {}
            
            for i in range(sizes[s][0] + 1 , sizes[s][0] + sizes[s][1] + 1):
                FIXED_COST[i] = 0  
            
            f.write("FIXED_COST = " + json.dumps(FIXED_COST) + "\n")
            
            FACILITY_CAPACITY = {}
            
            for i in range(sizes[s][0] + 1 , sizes[s][0] + sizes[s][1] + 1):
                FACILITY_CAPACITY[i] = round(sum(DEMAND.values()))
                
            f.write("FACILITY_CAPACITY = " + json.dumps(FACILITY_CAPACITY) + "\n")
            
            f.close()           


# In[60]:




