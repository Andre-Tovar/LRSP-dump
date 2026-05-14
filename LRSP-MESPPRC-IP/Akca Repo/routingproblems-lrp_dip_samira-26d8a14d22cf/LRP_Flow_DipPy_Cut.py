#!/usr/bin/env python3

from __future__ import division
from __future__ import print_function
from __future__ import absolute_import
from builtins import str
from builtins import range
from past.utils import old_div
import sys
import os

from ESPPRC import *

from iteration_utilities import first

try:
    import src.dippy as dippy
    from src.dippy import DipSolStatOptimal

except ImportError:
    import coinor.dippy as dippy
    from coinor.dippy import DipSolStatOptimal

debug_print = False

import warnings
warnings.filterwarnings("ignore")

import time

import numpy as np

from math import ceil, floor
tol = pow(pow(2, -24), old_div(2.0, 3.0))

timeLimit = 100
totalPriceItersLimit = 1
roundPriceItersLimit = 0

#from pulp import LpVariable, LpBinary, lpSum, LpProblem, LpMinimize
from pulp import *

def solve(CUSTOMERS, FACILITIES, NODES, VEHICLES, VEHICLE_DICT, CUSTOMER_SUBSETS,
                   FIXED_COST, ARC_COST, DEMAND,
                   FACILITY_CAPACITY, VEHICLE_CAPACITY, MINIMUM_NUM_FACILITIES, MINIMUM_NUM_VEHICLES,
                    FILE_PATH, CUSTOMERS_LEN):

    file = open(FILE_PATH, "w+")

    prob = dippy.DipProblem("Location and Routing Problem")
    #prob = LpProblem("Location and Routing Problem", LpMinimize)

    assign_vars = LpVariable.dicts("Assignment", (NODES, NODES, VEHICLES), 0, 1, LpBinary)
    facility_vars = LpVariable.dicts("t", FACILITIES, 0, 1, LpBinary)
    use_vars = LpVariable.dicts("y", (CUSTOMERS, VEHICLES), 0, 1, LpBinary)
#     seq_num = LpVariable.dicts("u", (CUSTOMERS, VEHICLES), lowBound = 0, upBound = CUSTOMERS_LEN - 1, cat='Integer')

    prob += (lpSum(facility_vars[j] * FIXED_COST[j] for j in FACILITIES) + 
             lpSum(assign_vars[i][k][h] * ARC_COST[(i, k)] for h in VEHICLES for i in NODES for k in NODES))

    # Customer fulfilment constraints
    for i in CUSTOMERS:
        prob += lpSum(assign_vars[i][k][h] for h in VEHICLES for k in NODES) == 1

    # Flow constraints
    for i in NODES:
        for h in VEHICLES:
            prob += lpSum(assign_vars[i][k][h] - assign_vars[k][i][h] for k in NODES) == 0

    # Linking constraints
    for i in CUSTOMERS:
        for h in VEHICLES:
            prob += lpSum(assign_vars[i][k][h] for k in NODES) == use_vars[i][h]

    # Vehicle capacity constraints
    for h in VEHICLES:
        prob += lpSum(DEMAND[i] * use_vars[i][h] for i in CUSTOMERS) <= VEHICLE_CAPACITY

    # Facility capacity constraints	
    for j in FACILITIES:
        prob += lpSum(DEMAND[i] * use_vars[i][h] for h in VEHICLE_DICT[j] for i in CUSTOMERS)\
                                                                            <= FACILITY_CAPACITY[j] * facility_vars[j]

    # Extra: prevents assigning the vehicles that are specified for the specific facility
    # to the routes originating from other facilities
    for j in FACILITIES:
        for h in (set(VEHICLES) - set(VEHICLE_DICT[j])):
            for k in CUSTOMERS:
                prob += assign_vars[j][k][h] == 0
                
    # Preventing self-cycles    
    for i in NODES:
        for h in VEHICLES:
            prob += assign_vars[i][i][h] == 0
                
    # Subtour elimination constraints (MTZ formulation)
#     for i in CUSTOMERS:
#         for k in CUSTOMERS:
#             for h in VEHICLES:
#                 if i != k:
#                     prob += seq_num[i][h] - seq_num[k][h] + CUSTOMERS_LEN * assign_vars[i][k][h] <= CUSTOMERS_LEN - 1
    
#     for i in CUSTOMERS:
#         for h in VEHICLES:
#             prob += seq_num[i][h] <= (CUSTOMERS_LEN - 1) * use_vars[i][h]

    #print(prob)
    noEmpty = dict([(fac, True) for fac in FACILITIES])

    prob.VEHICLES = VEHICLES
    prob.NODES = NODES
    prob.VEHICLE_DICT = VEHICLE_DICT
    prob.assign_vars = assign_vars

    def solve_subproblem(prob, index, redCosts, convexDual):
        print("in solve subproblem")
        vec = index

        #print("in subproblem")
        for i, j in VEHICLE_DICT.items():
            if index in VEHICLE_DICT[i]:
                fac_index = i

        NumCust = len(CUSTOMERS)

        # Calculate effective objective coefficient of arcs
        effs = np.zeros((NumCust + 2, NumCust + 2))

        for i in range(0, NumCust + 2):
            effs[i][i] = 0

        for i in CUSTOMERS:
            for k in CUSTOMERS:
                effs[i][k] = redCosts[assign_vars[i][k][vec]] + redCosts[use_vars[i][vec]]

        for i in CUSTOMERS:
            effs[0][i] = redCosts[assign_vars[fac_index][i][vec]]

        for i in CUSTOMERS:
            effs[i][0] = redCosts[assign_vars[i][fac_index][vec]] + redCosts[use_vars[i][vec]]

        effs[0][NumCust + 1] = 0
        effs[NumCust + 1][0] = 0

        for i in CUSTOMERS:
                effs[i][NumCust + 1] = redCosts[assign_vars[i][fac_index][vec]] + redCosts[use_vars[i][vec]]

        for i in CUSTOMERS:
                effs[NumCust + 1][i] = redCosts[assign_vars[fac_index][i][vec]]

        DEMAND[0] = 0
        DEMAND[NumCust + 1] = 0
        N = NumCust + 1

        #print("effs: ", effs)
        #print("DEMAND: ", DEMAND)
        #print("VEHICLE_CAPACITY: ", VEHICLE_CAPACITY)
        #print("N: ", N)

        z, solution = ESPPRC(effs, DEMAND, VEHICLE_CAPACITY, N)

        #print("solved")
        #print("z: ", z)
        #print("solution: ", solution)
        #print("convexDual: ", convexDual)
        #print("tol: ", tol)
        #print("red_cost_fac: ", redCosts[facility_vars[fac_index]])


        # Get the reduced cost of the ESPPRC solution
        rc = z + redCosts[facility_vars[fac_index]] 

        dvs = []
        var_values = {}

        if (rc - convexDual < -tol) & (len(solution) > 2):

            var_values[assign_vars[fac_index][solution[1]][vec]] = 1

            for i in range(1, len(solution) - 2):
                var_values[assign_vars[solution[i]][solution[i+1]][vec]] = 1

            var_values[assign_vars[solution[-2]][fac_index][vec]] = 1
            #var_values[facility_vars[fac_index]] = 1

            for i in range(1, len(solution) - 1):
                var_values[use_vars[solution[i]][vec]] = 1

            #var_tuple = (z, rc - convexDual, var_values)
            dvs.append(var_values)

            if debug_print:
                print("found good solution with fac =", fac_index, " veh_index =", vec,
                " z = ", z, " solution = ", solution, "var_values =", var_values)

            return DipSolStatOptimal, dvs

        elif -convexDual < -tol:
            if debug_print:
                print("no facility no route")
            return DipSolStatOptimal, [{}]

        elif ((redCosts[facility_vars[fac_index]] - convexDual) < -tol) & (noEmpty[fac_index]):
            noEmpty[fac_index] = False
            if debug_print:
                print("facility, no route")
            #var_values[facility_vars[fac_index]] = 1
            dvs.append(var_values)
            #var_tuple = (0, redCosts[facility_vars[fac_index]] - convexDual, var_values)
            return DipSolStatOptimal, dvs

        return DipSolStatOptimal, [{}]

    # generate subtour elimination constraints
    def generate_cuts(prob, sol):
        print("generate_cuts - solution: ", {i:v for (i,v) in sol.items() if sol[i] > 0.5})
        print("in generate cut")
        
        tol = 1e-6
        one = 1 - tol         
        
        cons = []
        
        VEHICLES_LST = prob.VEHICLES
        NODES_LST = prob.NODES

        for h in VEHICLES_LST:
            for k, v in prob.VEHICLE_DICT.items():
                if h in v:
                    fac_idx = k
            temp = set()

            for i in NODES_LST:
                for k in NODES_LST:
                    if sol[prob.assign_vars[i][k][h]] > one:
                        temp.add(i)
                        temp.add(k)
            
            if len(temp) > 0:
                not_connected = temp.copy()

                while not_connected:
                    start = not_connected.pop()
                    nodes, arcs = get_subtour(prob, sol, start, temp, fac_idx, h)
                    print("arcs", arcs)
                    print("nodes", nodes)
                    if fac_idx not in nodes:
                        if len(arcs) > 0:
                            cons.append( lpSum(prob.assign_vars[i][k][h] for (i, k) in arcs) <= len(arcs) - 1 )
                    nodes.remove(start)
                    not_connected -= nodes
        print("cuts added to the model: ", cons)

        return(cons)

    def is_solution_feasible(prob, sol, tol):
        print("is_solution_feasible - solution: ", {i:v for (i,v) in sol.items() if sol[i] > 0.0})
        print("is solution feasible")
        VEHICLES_LST = prob.VEHICLES
        NODES_LST = prob.NODES
        not_subtour = []
        tol = 1e-6
        one = 1 - tol

        for h in VEHICLES_LST:
            for k, v in prob.VEHICLE_DICT.items():
                if h in v:
                    fac_idx = k
            temp = set()

            for i in NODES_LST:
                for k in NODES_LST:
                    if sol[prob.assign_vars[i][k][h]] > one:
                        temp.add(i)
                        temp.add(k)
            
            if len(temp) > 0:
                not_connected = temp.copy()

                while not_connected:
                    start = not_connected.pop()
                    nodes, arcs = get_subtour(prob, sol, start, temp, fac_idx, h)
                    not_subtour.append(fac_idx in nodes)
                    nodes.remove(start)
                    not_connected -= nodes
        print('is-feasible output: ', all(not_subtour))
        return(all(not_subtour))

    def get_subtour(prob, sol, node, temp, fac_idx, veh_idx):
        #print("get subtour")
        # returns: list of nodes and arcs
        # in subtour containing node
        nodes = set([node])
        #print("nodes: ", nodes)
        arcs = set([])
        not_processed = set(temp)
        #print("not_processed: ", not_processed)
        to_process = set([node])

        tol = 1e-6
        one = 1 - tol

        while to_process:
            #print("to_process: ", to_process)
            c = to_process.pop()
            not_processed.remove(c)
            
#             print("c= ", c)
#             print("to_process: ", to_process)
#             print("not_processed: ", not_processed)

            new_arcs = [ (c, k) for k in not_processed if sol[prob.assign_vars[c][k][veh_idx]] > one ]
            new_arcs += [ (k, c) for k in not_processed if sol[prob.assign_vars[k][c][veh_idx]] > one ]
            
            #print("new_arcs: ", new_arcs)

            new_nodes = [ i for i in not_processed if ((i, c) in new_arcs or (c, i) in new_arcs)]
            #print("new_nodes: ", new_nodes)
            
            arcs |= set(new_arcs)
            nodes |= set(new_nodes)
            to_process |= set(new_nodes)
            
            #print("arcs: ", arcs)
            #print("nodes: ", nodes)
            #print("to_process: ", to_process)
        
        flat_list = [item for sublist in arcs for item in sublist]
        
        for i in set(flat_list):
            if flat_list.count(i) != 2:
                return nodes, set([])

        return nodes, arcs

    #prob.relaxed_solver = solve_subproblem
    prob.generate_cuts = generate_cuts
    prob.is_solution_feasible = is_solution_feasible

    start = time.time()
    dippy.Solve(prob, {
        'TolZero': '%s' % tol,
        'doCut' : '1',
        'generateInitVars': '1'
        #'TimeLimit': '%s' % timeLimit
        #'TotalPriceItersLimit': '%s' % totalPriceItersLimit,
        #'RoundPriceItersLimit': '%s' % roundPriceItersLimit
    })
    end = time.time()
    file.write("Time elapsed " + str(end - start) + '\n')

    file.write('Obj value: ' + str(value(prob.objective)) + '\n')

    file.write('Optimal value of vars:' + '\n')
    for i in NODES:
        for k in NODES:
            for h in VEHICLES:
                if assign_vars[i][k][h].varValue != None:
                    if assign_vars[i][k][h].varValue > 0.5:
                        file.write("x[{}][{}][{}]: {}".format(i, k, h, assign_vars[i][k][h].varValue) + '\n')

    for f in FACILITIES:
        file.write("t[{}]: {}".format(f, facility_vars[f].varValue) + '\n')

    for i in CUSTOMERS:
        for h in VEHICLES:
            if use_vars[i][h].varValue != None:
                if use_vars[i][h].varValue > 0.5:
                    file.write("y[{}][{}]: {}".format(i, h, use_vars[i][h].varValue) + '\n')   
                    
#     for i in CUSTOMERS:
#         for h in VEHICLES:
#             file.write("u[{}][{}]: {}".format(i, h, seq_num[i][h].varValue) + '\n')

    print("Problem solved")
    print('Obj value: ', value(prob.objective))

    file.close()  