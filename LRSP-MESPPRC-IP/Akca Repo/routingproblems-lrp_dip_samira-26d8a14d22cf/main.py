import argparse
import os
import importlib

import numpy as np
from scipy.spatial import distance
from itertools import chain, combinations

import LRP_Flow_Pulp
import LRP_Flow_Pulp_VIs
import LRP_Flow_DipPy
import LRP_Flow_DipPy_VIs
import LRP_Flow_DipPy_Cut
import LRP_Flow_DipPy_CS

parser = argparse.ArgumentParser()
parser.add_argument("-m", "--method", help = "specify the method")
parser.add_argument("-i", "--instance", help = "specify the instance")
parser.add_argument("-o", "--output_folder", help = "specify output folder", default = os.getcwd())
parser.add_argument("-s", "--sizeLimit", type = int, help = "specify size limit for CS", default = 5)
parser.add_argument("-l", "--labelLimit", help = "specify label limit for LL", default = 15)


flags = parser.parse_args()

if not os.path.exists(flags.output_folder):
    os.makedirs(flags.output_folder)

instance_file = importlib.import_module(flags.instance)
globals().update(instance_file.__dict__)

print("read the instance")

DEMAND = {int(k): v for k,v in DEMAND.items()}
print("DEMAND:", DEMAND)
NODES_LOCATION = {int(k): v for k,v in NODES_LOCATION.items()}
FIXED_COST = {int(k): v for k,v in FIXED_COST.items()}
FACILITY_CAPACITY = {int(k): v for k,v in FACILITY_CAPACITY.items()}

# Set of all customers
CUSTOMERS = list(DEMAND.keys())
CUSTOMERS.sort()

CUSTOMERS_LEN = len(CUSTOMERS)

TOTAL_DEMAND = sum(DEMAND.values())

# Set of facilities
FACILITIES = list(FIXED_COST.keys())
FACILITIES.sort()

# Nodes (union of customers and facilities)
NODES = CUSTOMERS + FACILITIES

NODES_LEN = len(NODES)

VEHICLE_DICT = {i:list(range((i-len(CUSTOMERS)-1)*len(CUSTOMERS)+1,
                                (i-len(CUSTOMERS))*len(CUSTOMERS)+1)) for i in FACILITIES}
#print(VEHICLE_DICT)
VEHICLES = []
for val in VEHICLE_DICT.values():
    VEHICLES = VEHICLES + val
VEHICLES.sort()
#print(VEHICLES)

MINIMUM_NUM_VEHICLES = np.ceil(TOTAL_DEMAND/VEHICLE_CAPACITY) 

# Arc costs
ARC_COST = {}
for i in range(1, len(NODES)):
    for j in range(i + 1, len(NODES) + 1):
        temp = round(distance.euclidean(NODES_LOCATION[i], NODES_LOCATION[j]), 2)
        ARC_COST[(i, j)] = temp
        ARC_COST[(j, i)] = temp

for i in range(1, len(NODES) + 1):
    ARC_COST[(i, i)] = 0  

def powerset(iterable):
    return chain.from_iterable(combinations(iterable, r) for r in range(len(iterable)+1))

CUSTOMER_SUBSETS = list(powerset(CUSTOMERS))
del CUSTOMER_SUBSETS[0]

SORTED_FACILITY_CAPACITY = sorted(FACILITY_CAPACITY.items(), key = lambda x: x[1])  
MINIMUM_NUM_FACILITIES = 0
temp = 0
it = 0
for fac, cap in SORTED_FACILITY_CAPACITY:
    temp = temp + cap
    if temp >= TOTAL_DEMAND:
        MINIMUM_NUM_FACILITIES = it + 1
        break

if flags.method == "cbc":
    print("Solving with cbc ...")
    FILE_PATH = os.path.join(flags.output_folder, flags.instance + "_" + flags.method + "_solution.txt")
    LRP_Flow_Pulp.solve(CUSTOMERS, FACILITIES, NODES, VEHICLES, VEHICLE_DICT, CUSTOMER_SUBSETS,
                                  FIXED_COST, ARC_COST, DEMAND, FACILITY_CAPACITY, VEHICLE_CAPACITY, 
                                  MINIMUM_NUM_FACILITIES, MINIMUM_NUM_VEHICLES, FILE_PATH)
 
if flags.method == "cbcVI":
    print("Solving with cbcVI ...")
    FILE_PATH = os.path.join(flags.output_folder, flags.instance + "_" + flags.method + "_solution.txt")
    LRP_Flow_Pulp_VIs.solve(CUSTOMERS, FACILITIES, NODES, VEHICLES, VEHICLE_DICT, CUSTOMER_SUBSETS,
                                  FIXED_COST, ARC_COST, DEMAND, FACILITY_CAPACITY, VEHICLE_CAPACITY, 
                                  MINIMUM_NUM_FACILITIES, MINIMUM_NUM_VEHICLES, CUSTOMERS_LEN, FILE_PATH)

if flags.method == "dippy":
    print("Solving with dippy ...")
    FILE_PATH = os.path.join(flags.output_folder, flags.instance + "_" + flags.method + "_solution.txt")
    LRP_Flow_DipPy.solve(CUSTOMERS, FACILITIES, NODES, VEHICLES, VEHICLE_DICT, CUSTOMER_SUBSETS,
                                  FIXED_COST, ARC_COST, DEMAND, FACILITY_CAPACITY, VEHICLE_CAPACITY, 
                                  MINIMUM_NUM_FACILITIES, MINIMUM_NUM_VEHICLES, FILE_PATH)
 
if flags.method == "dippyVI":
    print("Solving with dippyVI ...")
    FILE_PATH = os.path.join(flags.output_folder, flags.instance + "_" + flags.method + "_solution.txt")
    LRP_Flow_DipPy_VIs.solve(CUSTOMERS, FACILITIES, NODES, VEHICLES, VEHICLE_DICT, CUSTOMER_SUBSETS,
                                  FIXED_COST, ARC_COST, DEMAND, FACILITY_CAPACITY, VEHICLE_CAPACITY, 
                                  MINIMUM_NUM_FACILITIES, MINIMUM_NUM_VEHICLES, CUSTOMERS_LEN, FILE_PATH)

if flags.method == "dippyCut":
    print("Solving with dippy and generated cut ...")
    FILE_PATH = os.path.join(flags.output_folder, flags.instance + "_" + flags.method + "_solution.txt")
    LRP_Flow_DipPy_Cut.solve(CUSTOMERS, FACILITIES, NODES, VEHICLES, VEHICLE_DICT, CUSTOMER_SUBSETS,
                   FIXED_COST, ARC_COST, DEMAND, FACILITY_CAPACITY, VEHICLE_CAPACITY, MINIMUM_NUM_FACILITIES,
                       MINIMUM_NUM_VEHICLES, FILE_PATH, CUSTOMERS_LEN)
    
if flags.method == "dippyCS":
    print("Solving with dippy heuristically ...")
    FILE_PATH = os.path.join(flags.output_folder, flags.instance + "_" + flags.method + "_solution.txt")
    LRP_Flow_DipPy_CS.solve(CUSTOMERS, FACILITIES, NODES, VEHICLES, VEHICLE_DICT, CUSTOMER_SUBSETS,
                   FIXED_COST, ARC_COST, DEMAND,
                   FACILITY_CAPACITY, VEHICLE_CAPACITY, MINIMUM_NUM_FACILITIES, MINIMUM_NUM_VEHICLES,
                    FILE_PATH, flags.sizeLimit)

if flags.method == "dippyLL":
    print("Solving with dippy heuristically ...")
    FILE_PATH = os.path.join(flags.output_folder, flags.instance + "_" + flags.method + "_solution.txt")
    LRP_Flow_DipPy_CS.solve(CUSTOMERS, FACILITIES, NODES, VEHICLES, VEHICLE_DICT, CUSTOMER_SUBSETS,
                   FIXED_COST, ARC_COST, DEMAND,
                   FACILITY_CAPACITY, VEHICLE_CAPACITY, MINIMUM_NUM_FACILITIES, MINIMUM_NUM_VEHICLES,
                    FILE_PATH, flags.labelLimit)