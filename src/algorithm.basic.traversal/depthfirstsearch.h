/**
 * Copyright (C) 2013 - 2018 : Kathrin Hanauer
 *
 * This file is part of Algora.
 *
 * Algora is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Algora is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Algora.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact information:
 *   http://algora.xaikal.org
 */

#ifndef DEPTHFIRSTSEARCH_H
#define DEPTHFIRSTSEARCH_H

//#define DEBUG_DFS

#ifdef DEBUG_DFS
#include <iostream>
#define PRINT_DEBUG(msg) std::cout << msg << std::endl;
#define IF_DEBUG(cmd) cmd;
#else
#define PRINT_DEBUG(msg)
#define IF_DEBUG(cmd)
#endif

#include "graphtraversal.h"
#include "graph/digraph.h"
#include "property/propertymap.h"
#include "graph/graph_functional.h"

namespace Algora {

class Vertex;

struct DFSResult {
    int dfsNumber = -1;
    int lowNumber = -1;
    const Vertex *parent = 0;
    explicit DFSResult(int dfs=-1, int low=-1, Vertex *p=0) :
        dfsNumber(dfs), lowNumber(low), parent(p) {}
    DFSResult(const Vertex *p) : parent(p) {}
};

template <template<typename T> class ModifiablePropertyType = PropertyMap>
class DepthFirstSearch : public GraphTraversal<DFSResult>
{
public:
    DepthFirstSearch(bool computeValues = true)
        : GraphTraversal<DFSResult>(computeValues),
          maxDfsNumber(-1),
          treeArc(arcNothing), nonTreeArc(arcNothing)
    { }

    virtual ~DepthFirstSearch() { }

    void onTreeArcDiscover(const ArcMapping &aFun) {
        treeArc = aFun;
    }

    void onNonTreeArcDiscover(const ArcMapping &aFun) {
        nonTreeArc = aFun;
    }


    // GraphTraversal interface
    unsigned int numVerticesReached() const {
        return maxDfsNumber + 1;
    }

    // DiGraphAlgorithm interface
public:
    virtual void run() override
    {
        const Vertex *source = startVertex != nullptr ? startVertex : diGraph->getAnyVertex();

        int nextDepth = 0;
        ModifiablePropertyType<bool> discovered(false);
        bool stop = false;
        dfs(source, nextDepth, discovered, stop);
        maxDfsNumber = nextDepth - 1;
    }

    virtual std::string getName() const noexcept override { return "DFS"; }
    virtual std::string getShortName() const noexcept override { return "dfs"; }

    // ValueComputingAlgorithm interface
public:
    virtual unsigned int deliver() override
    {
        return maxDfsNumber + 1;
    }

private:
    int maxDfsNumber;
    ArcMapping treeArc;
    ArcMapping nonTreeArc;

    void dfs(const Vertex *v, int &depth, ModifiableProperty<bool> &discovered, bool &stop) {
        discovered[v] = true;
        DFSResult *cur = nullptr;
        if (computePropertyValues) {
            cur = &(*property)[v];
            cur->dfsNumber = depth;
            cur->lowNumber = depth;
        }
        depth++;
        PRINT_DEBUG(v << " : low = " << cur->lowNumber);

        if (!onVertexDiscovered(v)) {
            return;
        }

        stop |= vertexStopCondition(v);

        if (stop) {
            return;
        }

        auto vm = [&](const Vertex *v, const Vertex *u, Arc *arc) {
            PRINT_DEBUG("Considering child " << u << " of " << v);

            bool consider = onArcDiscovered(arc);
            stop |= arcStopCondition(arc);

            if (stop || !consider) {
                return;
            }

            if (!discovered[u]) {
                if (computePropertyValues) {
                    (*property)[u] = DFSResult(v);
                }
                PRINT_DEBUG("Set parent of " << u << " to " << (*property)[u].parent);
                treeArc(arc);

                dfs(u, depth, discovered, stop);

                if (stop) {
                    return;
                }

                if (computePropertyValues) {
                    if ((*property)[u].lowNumber < cur->lowNumber) {
                        PRINT_DEBUG("Updating low from " << cur->lowNumber << " to " << (*property)[u].lowNumber);
                        cur->lowNumber = (*property)[u].lowNumber;
                        PRINT_DEBUG("Low is now " << (*property)[v].lowNumber)
                    }
                }
            } else {
                nonTreeArc(arc);
                if (computePropertyValues && cur->parent != u && (*property)[u].dfsNumber < cur->lowNumber) {
                    PRINT_DEBUG("Updating low from " << cur->lowNumber << " to " << (*property)[u].dfsNumber);
                    cur->lowNumber = (*property)[u].dfsNumber;
                    PRINT_DEBUG("Low is now " << (*property)[v].lowNumber);
                }
            }
        };
        if (this->onUndirectedGraph || !this->onReverseGraph) {
            diGraph->mapOutgoingArcs(v, [&](Arc *a) { vm(v, a->getHead(), a); });
        }
        if (this->onUndirectedGraph || this->onReverseGraph) {
            diGraph->mapIncomingArcs(v, [&](Arc *a) { vm(v, a->getTail(), a); });
        }
    }
};



}

#endif // DEPTHFIRSTSEARCH_H
