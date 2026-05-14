from iteration_utilities import first
import numpy as np

def ESPPRC_LL(effs, DEMAND, VEHICLE_CAPACITY, N, LABEL_LIMIT):
    class Vertex:
        def __init__(self, i, ew, d):
            self.index = i
            self.edgeWeights = ew
            self.demand = d
            self.partialPaths = []

        def add_partialPath(self, pp):
            self.partialPaths.append(pp)

        def dominancePropertiesOnExploringPartialPaths(self, paths):
            if len(paths) == 1:
                return paths
            dominated = [False for i in range(len(paths))]

            for i in range(len(paths) - 1):
                if dominated[i]:
                    continue
                for j in range(i + 1, len(paths)):
                    if dominated[j]:
                        continue
                    if (paths[i].totalCost <= paths[j].totalCost and
                        paths[i].totalDemand <= paths[j].totalDemand and
                        paths[i].unreachableNodes.issubset(paths[j].unreachableNodes)):
                        dominated[j] = True
                    elif (paths[j].totalCost <= paths[i].totalCost and
                          paths[j].totalDemand <= paths[i].totalDemand and
                          paths[j].unreachableNodes.issubset(paths[i].unreachableNodes)):
                        dominated[i] = True
            return [paths[p] for p in range(len(paths)) if dominated[p] == False]   

        def apply_dominanceProperties(self, exploringPartialPaths, LABEL_LIMIT):
            exploringPartialPaths = self.dominancePropertiesOnExploringPartialPaths(exploringPartialPaths)

            dominatedOriginal = [False for i in range(len(self.partialPaths))]
            dominatedExploring = [False for i in range(len(exploringPartialPaths))]

            for i in range(len(self.partialPaths)):
                if dominatedOriginal[i]:
                    continue
                for j in range(len(exploringPartialPaths)):
                    if dominatedExploring[j]:
                        continue
                    if (self.partialPaths[i].totalCost <= exploringPartialPaths[j].totalCost and
                        self.partialPaths[i].totalDemand <= exploringPartialPaths[j].totalDemand and
                        self.partialPaths[i].unreachableNodes.issubset(exploringPartialPaths[j].unreachableNodes)):
                        dominatedExploring[j] = True
                    elif (exploringPartialPaths[j].totalCost <= self.partialPaths[i].totalCost and
                          exploringPartialPaths[j].totalDemand <= self.partialPaths[i].totalDemand and
                          exploringPartialPaths[j].unreachableNodes.issubset(self.partialPaths[i].unreachableNodes)):
                        dominatedOriginal[i] = True
                       
            if (len(dominatedOriginal) - sum(dominatedOriginal) + len(dominatedExploring) - sum(dominatedExploring)
                        > LABEL_LIMIT):
                temp = ([self.partialPaths[i].totalCost for i in range(len(dominatedOriginal)) 
                         if dominatedOriginal[i] == False] +
                        [exploringPartialPaths[i].totalCost for i in range(len(dominatedExploring)) 
                         if dominatedExploring[i] == False])
                temp.sort()
                costThreshold = temp[LABEL_LIMIT - 1]
                for i in range(len(dominatedOriginal)):
                    if self.partialPaths[i].totalCost > costThreshold:
                        dominatedOriginal[i] = True
                for i in range(len(dominatedExploring)):
                    if exploringPartialPaths[i].totalCost > costThreshold:
                        dominatedExploring[i] = True
            
            self.partialPaths = ([self.partialPaths[p] for p in range(len(self.partialPaths)) if dominatedOriginal[p] == False] 
                    + [exploringPartialPaths[p] for p in range(len(exploringPartialPaths)) if dominatedExploring[p] == False])  
            #print('vertex ', self.index)
            #print('pps: ', self.partialPaths)
            return (not any(dominatedOriginal)) and all(dominatedExploring)

        def __str__(self):
            print(str(self.__class__) + ": " + str(self.__dict__))
            print('partialpaths:')
            for pp in self.partialPaths:
                print(pp)
            return '-' * 20

    class PartialPath:
        def __init__(self, v, Rc, Rv, s):
            self.vertex = v
            self.totalCost = Rc
            self.totalDemand = Rv
            self.otherVericesStatus = s
            self.unreachableNodes = set([k for k, v in s.items() if v != 0])
            self.totalUnreachableNodes = len(self.unreachableNodes)

        def extend(self, nextVertex, ew):
            nextRc = self.totalCost + ew
            nextRv = self.totalDemand + nextVertex.demand
            nextOtherVericesStatus = {}
            numNodesInNewPath = len([k for k, v in self.otherVericesStatus.items() if v > 0]) + 1

            for g in range(1, N+1):
                if self.otherVericesStatus[g] != 0:
                    nextOtherVericesStatus[g] = self.otherVericesStatus[g]
                elif g == nextVertex.index:
                    nextOtherVericesStatus[g] = numNodesInNewPath
                elif (self.otherVericesStatus[g] == 0) and (nextRv + DEMAND[g] > VEHICLE_CAPACITY):
                    nextOtherVericesStatus[g] = -1
                else:
                    nextOtherVericesStatus[g] = 0   
            return PartialPath(nextVertex, nextRc, nextRv, nextOtherVericesStatus)

        def __str__(self):
            return str(self.__class__) + ": " + str(self.__dict__)

    # Initialization
    verticesDict = {}

    for i in range(N+1):
        verticesDict[i] = Vertex(i, effs[i], DEMAND[i])

    verticesDict[0].add_partialPath(PartialPath(verticesDict[0], 0, 0, {i:0 for i in range(1,N+1)}))

    E = set([0])
    
#     num_all_iters = 0

    while len(E) > 0:
        currentVertex = verticesDict[first(E)]

        for exploringVertexIndex in range(1,N+1):
            if exploringVertexIndex == currentVertex.index:
                continue
            exploringVertex = verticesDict[exploringVertexIndex]
            exploringPartialPaths = []

            for pp in currentVertex.partialPaths:
                if pp.otherVericesStatus[exploringVertexIndex] == 0:
                    newPartialPath = pp.extend(exploringVertex, effs[currentVertex.index, exploringVertexIndex])
                    exploringPartialPaths.append(newPartialPath)
                    
#                     num_all_iters += 1

            notChanged = exploringVertex.apply_dominanceProperties(exploringPartialPaths, LABEL_LIMIT)

            if notChanged == False:
                E.add(exploringVertexIndex)
                
        E.remove(currentVertex.index)  

    costs = [i.totalCost for i in verticesDict[N].partialPaths]
    best_obj = min(costs)
    best_path = verticesDict[N].partialPaths[costs.index(min(costs))]

    cleanPath = {k:v for k, v in best_path.otherVericesStatus.items() if v > 0}
    optimal_path = [0]
    optimal_path = optimal_path + [k for k, v in sorted(cleanPath.items(), key=lambda item: item[1])]
    
#     print('num_iters: ', num_all_iters)
    return best_obj, optimal_path