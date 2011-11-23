#!/usr/bin/python

import sys

N = 3

if len(sys.argv) > 1:
    N = int(sys.argv[1])

MaxNode = (2 ** N)  -1
MaxLayout = (2 ** MaxNode) -1


def layout2nodes(layout):
    ret = []
    for x in range(0, MaxNode):
        if (layout & (1 << x)):
            ret.append(x+1)

    return ret

def node2dataNodes(node):
    ret = []
    for x in range(0,N):
        if(node & (1 << x)):
            ret.append(x+1)

    return ret


def layoutDataSums(layout):
    ret = []

    for x in range(0,N):
        ret.append(0)

    for x in layout2nodes(layout):
        for y in node2dataNodes(x):
            ret[y -1]+=1

    return ret

def nodeDataSums(nodes):
    ret = []

    for x in range(0,N):
        ret.append(0)

    for x in nodes:
        for y in node2dataNodes(x):
            ret[y -1]+=1

    return ret


def checkMonotonic(array):
    max = array[0]

    for x in range(1,len(array)):
        if array[x] > max:
            return False
        else:
            max = array[x]

    return True

def makeMonotonic(nodes, nodeCounts):
    maxIdx = 0
    max = nodeCounts[maxIdx]

    for x in range(1,len(nodeCounts)):
        if nodeCounts[x] > max:
            swap = (1 << maxIdx) | (1<< x)

            nodes = renameNodes(nodes, swap)

            return makeMonotonic(nodes,nodeDataSums(nodes))
        elif nodeCounts[x] < max:
            max = nodeCounts[x]
            maxIdx = x

    return [nodes, nodeCounts]


def renameNodes(nodes, swap):
    for x in range(0, len(nodes)):
        n = nodes[x]
        contains = n & swap
        if (contains != 0) and (contains != swap):
            nodes[x] ^= swap

    return nodes

def main():
    for x in xrange(1, MaxLayout +1):
        nodes = layout2nodes(x)
        sums = nodeDataSums(nodes)
        if checkMonotonic(sums):
            print nodes, sums,  makeMonotonic(nodes, sums)


main()
