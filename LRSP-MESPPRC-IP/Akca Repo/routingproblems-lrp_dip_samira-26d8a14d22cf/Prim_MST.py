# A Python program for Prim's Minimum Spanning Tree (MST) algorithm. 
# The program is for adjacency matrix representation of the graph 


import sys # Library for INT_MAX 
import numpy as np
  
def Prim_MST(effs, sizeLimit):
    class Graph(): 
            
        def __init__(self, vertices): 
            self.V = vertices 
            self.graph = [[0 for column in range(vertices)] for row in range(vertices)]
            self.realVertices = list(range(1, vertices - 1))

        # A utility function to print the constructed MST stored in parent[] 
#         def printMST(self, parent, firstIndex, mstSet): 
#             print("Edge \tWeight")
#             vertices = self.realVertices.copy() #list(range(0, self.V))
#             vertices.remove(firstIndex)
#             for i in vertices:
#                 if mstSet[i]:
#                     if self.graph[parent[i]][ i ] <= self.graph[i][ parent[i] ]:
#                         print(parent[i], "-", i, "\t", self.graph[parent[i]][ i ])
#                     else:
#                         print(i, "-", parent[i], "\t", self.graph[i][ parent[i] ])

        # A utility function to find the vertex with  
        # minimum distance value, from the set of vertices  
        # not yet included in shortest path tree 
        def minKey(self, key, mstSet): 
            
            # Initilaize min value 
            min = sys.maxsize 

            for v in self.realVertices: #range(self.V): 
                if key[v] < min and mstSet[v] == False: 
                    min = key[v] 
                    min_index = v 

            return min_index 

        # Function to construct and print MST for a graph  
        # represented using adjacency matrix representation 
        def primMST(self, minValue, firstIndex, sizeLimit): 

            # Key values used to pick minimum weight edge in cut 
            key = [sys.maxsize] * self.V 
            parent = [None] * self.V # Array to store constructed MST 
            # Make the key of first index to the minimum element in the effs matrix
            # so that this vertex is picked as first vertex 
            key[firstIndex] = minValue

            mstSet = [False] * self.V 

#             parent[firstIndex] = -1 # First node is always the root of 

            # size of tree
            treeSize = 0

            for cout in self.realVertices: #range(self.V): 

                # Pick the minimum distance vertex from  
                # the set of vertices not yet processed.  
                # u is always equal to src in first iteration 
                u = self.minKey(key, mstSet) 

                # Put the minimum distance vertex in  
                # the shortest path tree 
                mstSet[u] = True
                treeSize += 1
#                 print('limit: {}, tree_size: {}'.format(sizeLimit, treeSize))
                if (treeSize == sizeLimit):
#                     print('in here')
                    break

                # Update dist value of the adjacent vertices  
                # of the picked vertex only if the current  
                # distance is greater than new distance and 
                # the vertex in not in the shotest path tree 
                for v in self.realVertices: #range(self.V): 

                    # graph[u][v] is non zero only for adjacent vertices of m 
                    # mstSet[v] is false for vertices not yet included in MST 
                    # Update the key only if graph[u][v] is smaller than key[v] 
                    if mstSet[v] == False and key[v] > min(self.graph[u][v],self.graph[v][u]): 
                            key[v] = min(self.graph[u][v],self.graph[v][u]) 
                            #print('key[{0}]: {1}'.format(v,key[v]))
                            parent[v] = u 

            #self.printMST(parent, firstIndex, mstSet)
            return [i for i, val in enumerate(mstSet) if val]
  
    firstIndex = np.argmin(effs[0][1:len(effs[0])-1]) + 1
    g = Graph(len(effs)) 
    g.graph = effs
    minValue = min([i for lis in g.graph for i in lis])

    finalCustSet = g.primMST(minValue - 1, firstIndex, sizeLimit)
    return finalCustSet
    #print('finalCustSet: ',finalCustSet)