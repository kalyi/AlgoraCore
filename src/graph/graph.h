/**
 * Copyright (C) 2013 - 2016 : Kathrin Hanauer
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


#ifndef GRAPH_H
#define GRAPH_H

#include "graphartifact.h"
#include "vertex.h"

#include "graph.visitor/vertexvisitor.h"
#include "graph_functional.h"

#include <vector>

namespace Algora {

class Vertex;

class Graph : public GraphArtifact
{
public:
    typedef unsigned int size_type;

    explicit Graph(GraphArtifact *parent = 0)
        : GraphArtifact(parent) { }
    virtual ~Graph() { }

    // Vertices
    virtual Vertex *addVertex() = 0;
    virtual void removeVertex(Vertex *v) = 0;
    virtual bool containsVertex(Vertex *v) const = 0;
    virtual Vertex *getAnyVertex() const = 0;

    virtual void onVertexAdd(VertexMapping vvFun) { vertexGreetings.push_back(vvFun); }
    virtual void onVertexRemove(VertexMapping vvFun) { vertexFarewells.push_back(vvFun); }

    // Accomodate visitors
    virtual void acceptVertexVisitor(VertexVisitor *nVisitor) {
        mapVertices(nVisitor->getVisitorFunction());
    }
    virtual void mapVertices(VertexMapping vvFun) {
        mapVerticesUntil(vvFun, vertexFalse);
    }
    virtual void mapVerticesUntil(VertexMapping vvFun, VertexPredicate breakCondition) = 0;

    // Misc
    virtual bool isEmpty() const = 0;
    virtual size_type getSize() const = 0;

protected:
   std::vector<VertexMapping> vertexGreetings;
   std::vector<VertexMapping> vertexFarewells;

   virtual void greetVertex(Vertex *v) {
       for (const VertexMapping &f : vertexGreetings) { f(v); }
   }
   virtual void dismissVertex(Vertex *v) {
       for (const VertexMapping &f : vertexFarewells) { f(v); }
   }

    Vertex *createVertex() {
        return new Vertex(this);
    }
};

}

#endif // GRAPH_H
