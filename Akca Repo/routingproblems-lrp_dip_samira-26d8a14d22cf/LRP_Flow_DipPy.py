#!/usr/bin/env python3

from __future__ import division
from __future__ import print_function
from __future__ import absolute_import
from builtins import str
from builtins import range
from past.utils import old_div
import sys
import os
import copy

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

timeLimit = 3600
totalPriceItersLimit = 1
roundPriceItersLimit = 0

#from pulp import LpVariable, LpBinary, lpSum, LpProblem, LpMinimize
from pulp import *
#from pulp import LpVariable, LpBinary, lpSum, value, LpProblem, LpMaximize, LpAffineExpression


def solve(CUSTOMERS, FACILITIES, NODES, VEHICLES, VEHICLE_DICT, CUSTOMER_SUBSETS,
                   FIXED_COST, ARC_COST, DEMAND,
                   FACILITY_CAPACITY, VEHICLE_CAPACITY, MINIMUM_NUM_FACILITIES, MINIMUM_NUM_VEHICLES,
                    FILE_PATH):
    
    file = open(FILE_PATH, "w+")
    
    prob = dippy.DipProblem("Location and Routing Problem")
    #prob = LpProblem("Location and Routing Problem", LpMinimize)

    assign_vars = LpVariable.dicts("Assignment", (NODES, NODES, VEHICLES), 0, 1, LpBinary)
    facility_vars = LpVariable.dicts("t", FACILITIES, 0, 1, LpBinary)
    use_vars = LpVariable.dicts("y", (CUSTOMERS, VEHICLES), 0, 1, LpBinary)

    prob += (lpSum(facility_vars[j] * FIXED_COST[j] for j in FACILITIES) + 
             lpSum(assign_vars[i][k][h] * ARC_COST[(i, k)] for h in VEHICLES for i in NODES for k in NODES))

    # Customer fulfilment constraints
    for i in CUSTOMERS:
        prob += lpSum(assign_vars[i][k][h] for h in VEHICLES for k in NODES) == 1

    # Flow constraints
    for i in NODES:
        for h in VEHICLES:
            prob.relaxation[h] += lpSum(assign_vars[i][k][h] - assign_vars[k][i][h] for k in NODES) == 0

    # Linking constraints
    for i in CUSTOMERS:
        for h in VEHICLES:
            prob.relaxation[h] += lpSum(assign_vars[i][k][h] for k in NODES) == use_vars[i][h]

    # Subtour elimination
    # for h in VEHICLES:
    #     for s in CUSTOMER_SUBSETS:
    #         for i in s:
    #             prob.relaxation[h] += use_vars[i][h] - lpSum(assign_vars[k][l][h] for k in s for l in (set(NODES) - set(s)) ) <= 0

    # Vehicle capacity constraints
    for h in VEHICLES:
        prob.relaxation[h] += lpSum(DEMAND[i] * use_vars[i][h] for i in CUSTOMERS) <= VEHICLE_CAPACITY

    # Facility capacity constraints	
    for j in FACILITIES:
        prob += lpSum(DEMAND[i] * use_vars[i][h] for h in VEHICLE_DICT[j] for i in CUSTOMERS)\
                                                                            <= FACILITY_CAPACITY[j] * facility_vars[j]

    # Extra: prevents assigning the vehicles that are specified for the specific facility
    # to the routes originating from other facilities
    for j in FACILITIES:
        for h in (set(VEHICLES) - set(VEHICLE_DICT[j])):
            for k in CUSTOMERS:
                prob.relaxation[h] += assign_vars[j][k][h] == 0

    # Preventing self-cycles    
    for i in NODES:
        for h in VEHICLES:
            prob.relaxation[h] += assign_vars[i][i][h] == 0

    #print(prob)
    noEmpty = dict([(fac, True) for fac in FACILITIES])

    def solve_subproblem(prob, index, redCosts, convexDual):
        #print("in subproblem")
        vec = index

        #print("in subproblem")
        #print("VEHICLE_DICT: ", VEHICLE_DICT)
        #print("vec: ", vec)
        for i, j in VEHICLE_DICT.items():
            if index in VEHICLE_DICT[i]:
                fac_index = i
                #print("fac_index: ", fac_index)

        #print("done: ")
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
            #print("dvsss", dvs)

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

    prob.relaxed_solver = solve_subproblem

    def first_fit_heuristic():
        # 1. Create a map of facilities to their remaining capacity

        # 2. Create a list of unassigned customers

        # 3. Among all the edges between available facilities and unassigned customers,
        # selected the edge with minimum arc cost. This is the start of a route. 
        # Find the shortest arc in a greedy manner, afterwards, until the route is closed, 
        # by going to the facility either because of shortest distance or because of meeting vehicle capacity.

        # 4. Add this route to the list of routes.

        # 5. Update map of facilities to their remaining capacity and the list of unassigned customers

        # 6. Go back to step 3 if the list of unassigned customers is not empty.

        unassigned_customers = copy.deepcopy(CUSTOMERS)

        facility_vars_heuristic = {fac: 0 for fac in FACILITIES}
        veh_routes = {veh: {} for veh in VEHICLES}
        total_cost = 0

        rem_fac_cap = copy.deepcopy(FACILITY_CAPACITY)
        rem_vehicles = copy.deepcopy(VEHICLE_DICT)
        #print("inside heu, VEHICLE_DICT: ",VEHICLE_DICT)


        # step 1
        for count in range(len(CUSTOMERS)): #select facility
            if len(unassigned_customers) == 0:
                    #print('all customers are assigned!')
                    break;

            min_remaining_demand = min([DEMAND[cust] for cust in unassigned_customers])
            #print('min_remaining_demand: ', min_remaining_demand)
            #print('rem_fac_cap: ', rem_fac_cap)
            #print('rem_vehicles: ', rem_vehicles)

            available_facilities = [fac for fac, cap in rem_fac_cap.items() if cap >= min_remaining_demand 
                                   and rem_vehicles[fac]]
            #print('available_facilities: ', available_facilities)

            available_facilities_dist_to_nearest_cust = {fac:min([ARC_COST[fac, cust] for cust in unassigned_customers
                                                                 if DEMAND[cust] <= rem_fac_cap[fac]])
                                                         for fac in available_facilities}
            #print('available_facilities_dist_to_nearest_cust: ', available_facilities_dist_to_nearest_cust)
			
			# Give the key with the minimum arc cost value
            selected_facility = min(available_facilities_dist_to_nearest_cust, key=available_facilities_dist_to_nearest_cust.get)
            #print('selected_facility: ', selected_facility)

            v = rem_vehicles[selected_facility][0]
            #print('v: ', v)

            current_node = selected_facility
            rem_veh_cap = VEHICLE_CAPACITY

            route = [current_node]
            cost = 0

            while True: #nodes
                #print('inside nodes')

                potential_customers = [c for c in unassigned_customers if DEMAND[c] <= min(rem_fac_cap[selected_facility],
                                                                                           rem_veh_cap)]
                #print('potential_customers: ', potential_customers)

                if len(potential_customers) > 0:
                    if facility_vars_heuristic[selected_facility] == 0:
                        facility_vars_heuristic[selected_facility] = 1
                        total_cost += FIXED_COST[selected_facility]

                    if current_node == selected_facility:
                        pot_arc_costs = {i:ARC_COST[current_node,i] for i in potential_customers}
                    else:
                        pot_arc_costs = {i:ARC_COST[current_node,i] for i in potential_customers + [selected_facility]}

                    #print('pot_arc_costs: ', pot_arc_costs)
                    next_node = min(pot_arc_costs, key=pot_arc_costs.get)
                    #print('next_node: ', next_node)

                    if next_node == selected_facility:
                        route.append(next_node)
                        #print('route: ', route)
                        cost += ARC_COST[current_node,next_node]
                        total_cost += ARC_COST[current_node,next_node]
                        break
                    else:
                        route.append(next_node)
                        #print('route: ', route)
                        cost += ARC_COST[current_node,next_node]
                        total_cost += ARC_COST[current_node,next_node]
                        rem_fac_cap[selected_facility] -= DEMAND[next_node]
                        rem_veh_cap -= DEMAND[next_node]
                        #print('cost: ', cost)
                        #print('rem_fac_cap[{}]: {} '.format(selected_facility,rem_fac_cap))
                        #print('rem_veh_cap: ', rem_veh_cap)
                        unassigned_customers.remove(next_node)
                        #print('unassigned_customers: ', unassigned_customers)

                        current_node = next_node
                else:
                    #print('inside nodes else')
                    next_node = selected_facility
                    route.append(next_node)
                    #print('route: ', route)
                    cost += ARC_COST[current_node,next_node]
                    total_cost += ARC_COST[current_node,next_node]
                    break

            veh_routes[v] = {'r': route, 'c': cost}
            rem_vehicles[selected_facility].remove(v)

        if unassigned_customers:
            print('solution is not feasible - unassigned_customers: ', unassigned_customers)
        #print('facilities used: ', facility_vars_heuristic)
        final_veh_routes = {key:value for key,value in veh_routes.items() if len(value)>0}
        #print('veh_routes: ', final_veh_routes)
        #print('total_cost: ', total_cost)
        #print("end of heu, VEHICLE_DICT: ",VEHICLE_DICT)
        return unassigned_customers, facility_vars_heuristic, final_veh_routes

    def init_first_fit(prob):

        unassigned_customers_heuristic, facility_vars_heuristic, final_veh_routes_heuristic = first_fit_heuristic()
        #print("in init first fit")

        # initializing and filling all vars
        bvs = []

        for v, route_info in final_veh_routes_heuristic.items():
            var_values = {}
            route = route_info['r']
            for n1 in range(len(route) - 1):
                if n1 > 0:
                    var_values[use_vars[route[n1]][v]] = 1
                var_values[assign_vars[route[n1]][route[n1+1]][v]] = 1
            # dv = (cost of this route, var values corresponding to this route)
            dv = (route_info['c'], var_values)
            bvs.append((v, dv))
                    
        
        if debug_print:
            print(bvs)
        #print("end of init first fit")
        #print("bvs",bvs)    
        return bvs

    prob.init_vars = init_first_fit

    def first_fit():
        # Use generic first-fit heuristic that is shared
        # between heuristics and initial variable generation
        unassigned_customers_heuristic, facility_vars_heuristic, final_veh_routes_heuristic = first_fit_heuristic()

        # Convert to decision variable values
        sol = {}

        # if there is any unassigned customer, the solution is not feasible, so return empty sol
        if unassigned_customers_heuristic:
            return sol

        # initializing and filling all vars
        for f in FACILITIES:
            sol[facility_vars[f]] = facility_vars_heuristic[f]

        for v in VEHICLES:
            for c in CUSTOMERS:
                sol[use_vars[c][v]] = 0
            for n1 in NODES:
                for n2 in NODES:
                    sol[assign_vars[n1][n2][v]] = 0

        for v, route_info in final_veh_routes_heuristic.items():
            route = route_info['r']
            for n1 in range(len(route) - 1):
                if n1 > 0:
                    sol[use_vars[route[n1]][v]] = 1
                sol[assign_vars[route[n1]][route[n1+1]][v]] = 1

        #print("end of first fit, VEHICLE_DICT: ",VEHICLE_DICT)

        return sol

    def heuristics(prob, xhat, cost):
        sols = []
        if prob.root_heuristic:
            prob.root_heuristic = False # Don't run twice
            sol = first_fit()
            sols.append(sol)

        #print("end of main heu, VEHICLE_DICT: ",VEHICLE_DICT)
        if len(sols) > 0:
            return sols

    prob.heuristics = heuristics
    prob.root_heuristic = True

    start = time.time()

    dippy.Solve(prob, {
        'TolZero': '%s' % tol,
        'doPriceCut': '1',
        'doCut' : '0',
        'generateInitVars': '0',
        'TimeLimit': '%s' % timeLimit
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
                    if assign_vars[i][k][h].varValue > 0:
                        file.write("x[{}][{}][{}]: {}".format(i, k, h, assign_vars[i][k][h].varValue) + '\n')

    for f in FACILITIES:
        file.write("t[{}]: {}".format(f, facility_vars[f].varValue) + '\n')

    for i in CUSTOMERS:
        for h in VEHICLES:
            if use_vars[i][h].varValue != None:
                if use_vars[i][h].varValue > 0:
                    file.write("y[{}][{}]: {}".format(i, h, use_vars[i][h].varValue) + '\n')

    print("Problem solved")
    print('Obj value: ', value(prob.objective))
    
    file.close() 