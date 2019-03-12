/**
 * Copyright (C) 2013 - 2019 : Kathrin Hanauer
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

#include "incidencelistvertex.h"

#include "incidencelistgraph.h"
#include "graph/arc.h"
#include "graph/parallelarcsbundle.h"
#include "graph.visitor/arcvisitor.h"
#include "property/propertymap.h"

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <climits>

namespace Algora {

#define NO_INDEX ULLONG_MAX

typedef typename std::vector<Arc*> ArcList;
typedef typename std::vector<MultiArc*> MultiArcList;

template <typename AL>
bool removeArcFromList(AL &list, PropertyMap<unsigned long long> &indexMap, const Arc *arc);
bool removeBundledArcFromList(PropertyMap<ParallelArcsBundle*> &bundleMap, const Arc *arc);
template <typename AL>
bool isArcInList(const PropertyMap<unsigned long long> &indexMap, AL &list, const Arc *arc);
template <typename AL>
bool isBundledArc(const PropertyMap<ParallelArcsBundle*> bundleMap, AL &list, const PropertyMap<unsigned long long> &indexMap, const Arc *arc);

class IncidenceListVertex::CheshireCat {
public:
    unsigned long long index;
    bool checkConsisteny;

    ArcList outgoingArcs;
    ArcList incomingArcs;
    MultiArcList outgoingMultiArcs;
    MultiArcList incomingMultiArcs;

    PropertyMap<ParallelArcsBundle*> bundle;

    PropertyMap<unsigned long long> outIndex;
    PropertyMap<unsigned long long> inIndex;
    PropertyMap<unsigned long long> multiOutIndex;
    PropertyMap<unsigned long long> multiInIndex;

    CheshireCat(unsigned long long i) : index(i) {
        bundle.setDefaultValue(nullptr);
        outIndex.setDefaultValue(NO_INDEX);
        inIndex.setDefaultValue(NO_INDEX);
        multiOutIndex.setDefaultValue(NO_INDEX);
        multiInIndex.setDefaultValue(NO_INDEX);
    }

    void clear() {
        outgoingArcs.clear();
        incomingArcs.clear();
        outgoingMultiArcs.clear();
        incomingMultiArcs.clear();

        bundle.resetAll();
        outIndex.resetAll();
        inIndex.resetAll();
        multiOutIndex.resetAll();
        multiInIndex.resetAll();
    }
};

IncidenceListVertex::IncidenceListVertex(unsigned long long id, GraphArtifact *parent, unsigned long long index)
    : Vertex(id, parent), grin(new CheshireCat(index))
{
    grin->checkConsisteny = true;
}

IncidenceListVertex::~IncidenceListVertex()
{
    delete grin;
}

unsigned long long IncidenceListVertex::getOutDegree(bool multiArcsAsSimple) const
{
    auto deg = grin->outgoingArcs.size();
    if (multiArcsAsSimple) {
        return deg + grin->outgoingMultiArcs.size();
    }
    for (MultiArc *ma : grin->outgoingMultiArcs) {
        deg += ma->getSize();
    }
    return deg;
}

void IncidenceListVertex::addOutgoingArc(Arc *a)
{
    if (grin->checkConsisteny && a->getTail() != this) {
        throw std::invalid_argument("Arc has other tail.");
    }
    MultiArc *ma = dynamic_cast<MultiArc*>(a);
    if (ma) {
        grin->multiOutIndex.setValue(ma, grin->outgoingMultiArcs.size());
        grin->outgoingMultiArcs.push_back(ma);
        ParallelArcsBundle *pab = dynamic_cast<ParallelArcsBundle*>(ma);
        if (pab) {
            pab->mapArcs([&](Arc *a) {
                grin->bundle.setValue(a, pab);
            });
        }
    } else {
        grin->outIndex.setValue(a, grin->outgoingArcs.size());
        grin->outgoingArcs.push_back(a);
    }
}

void IncidenceListVertex::removeOutgoingArc(const Arc *a)
{
    if (grin->checkConsisteny && a->getTail() != this) {
        throw std::invalid_argument("Arc has other tail.");
    }
    if (!removeArcFromList(grin->outgoingArcs, grin->outIndex, a)
            && !removeArcFromList(grin->outgoingMultiArcs, grin->multiOutIndex, a)
            && !removeBundledArcFromList(grin->bundle, a)) {
        throw std::invalid_argument("Unknown outgoing arc.");
    }
}

void IncidenceListVertex::clearOutgoingArcs()
{
    grin->outgoingArcs.clear();
    grin->outgoingMultiArcs.clear();
    grin->outIndex.resetAll();
    grin->multiOutIndex.resetAll();
}

unsigned long long IncidenceListVertex::getInDegree(bool multiArcsAsSimple) const
{
    auto deg = grin->incomingArcs.size();
    if (multiArcsAsSimple) {
        return deg + grin->incomingMultiArcs.size();
    }
    for (MultiArc *ma : grin->incomingMultiArcs) {
        deg += ma->getSize();
    }
    return deg;
}

bool IncidenceListVertex::isSource() const
{
    return grin->incomingArcs.empty() && grin->incomingMultiArcs.empty();
}

bool IncidenceListVertex::isSink() const
{
    return grin->outgoingArcs.empty() && grin->outgoingMultiArcs.empty();
}

void IncidenceListVertex::addIncomingArc(Arc *a)
{
    if (grin->checkConsisteny && a->getHead() != this) {
        throw std::invalid_argument("Arc has other head.");
    }
    MultiArc *ma = dynamic_cast<MultiArc*>(a);
    if (ma) {
        grin->multiInIndex.setValue(ma, grin->incomingMultiArcs.size());
        grin->incomingMultiArcs.push_back(ma);
        ParallelArcsBundle *pab = dynamic_cast<ParallelArcsBundle*>(ma);
        if (pab) {
            pab->mapArcs([&](Arc *a) {
                grin->bundle.setValue(a, pab);
            });
        }
    } else {
        grin->inIndex.setValue(a, grin->incomingArcs.size());
        grin->incomingArcs.push_back(a);
    }
}

void IncidenceListVertex::removeIncomingArc(const Arc *a)
{
    if (grin->checkConsisteny && a->getHead() != this) {
        throw std::invalid_argument("Arc has other head.");
    }
    if (!removeArcFromList(grin->incomingArcs, grin->inIndex, a)
            && !removeArcFromList(grin->incomingMultiArcs, grin->multiInIndex, a)
            && !removeBundledArcFromList(grin->bundle, a)) {
        throw std::invalid_argument("Unknown incoming arc.");
    }
}

void IncidenceListVertex::clearIncomingArcs()
{
    grin->incomingArcs.clear();
    grin->incomingMultiArcs.clear();
}

void IncidenceListVertex::enableConsistencyCheck(bool enable)
{
    grin->checkConsisteny = enable;
}

unsigned long long IncidenceListVertex::getIndex() const
{
    return grin->index;
}

void IncidenceListVertex::setIndex(unsigned long long i)
{
    grin->index = i;
}

void IncidenceListVertex::hibernate()
{
    invalidate();
    grin->clear();
}

void IncidenceListVertex::recycle()
{
    revalidate();
}

bool IncidenceListVertex::hasOutgoingArc(const Arc *a) const
{
    return isArcInList(grin->outIndex, grin->outgoingArcs, a)
            || isArcInList(grin->multiOutIndex, grin->outgoingMultiArcs, a)
            || isBundledArc(grin->bundle, grin->outgoingMultiArcs, grin->multiOutIndex, a);
}

bool IncidenceListVertex::hasIncomingArc(const Arc *a) const
{
    return isArcInList(grin->inIndex, grin->incomingArcs, a)
            || isArcInList(grin->multiInIndex, grin->incomingMultiArcs, a)
            || isBundledArc(grin->bundle, grin->incomingMultiArcs, grin->multiInIndex, a);
}

Arc *IncidenceListVertex::outgoingArcAt(unsigned int i, bool multiArcsAsSimple) const
{
    if (i < grin->outgoingArcs.size()) {
        return grin->outgoingArcs.at(i);
    }
    i -= grin->outgoingArcs.size();
    if (multiArcsAsSimple) {
        if (i < grin->outgoingMultiArcs.size()) {
            return grin->outgoingMultiArcs.at(i);
        }
    } else {
        for(MultiArc *a : grin->outgoingMultiArcs) {
            if (i < a->getSize()) {
                return a;
            }
            i -= a->getSize();
        }
    }
    throw std::invalid_argument("Index must be less than outdegree.");
}

Arc *IncidenceListVertex::incomingArcAt(unsigned int i, bool multiArcsAsSimple) const
{
    if (i < grin->incomingArcs.size()) {
        return grin->incomingArcs.at(i);
    }
    i -= grin->incomingArcs.size();
    if (multiArcsAsSimple) {
        if (i < grin->incomingMultiArcs.size()) {
            return grin->incomingMultiArcs.at(i);
        }
    } else {
        for(MultiArc *a : grin->incomingMultiArcs) {
            if (i < a->getSize()) {
                return a;
            }
            i -= a->getSize();
        }
    }
    throw std::invalid_argument("Index must be less than indegree.");
}

unsigned long long IncidenceListVertex::outIndexOf(const Arc *a) const
{
    auto i = grin->outIndex(a);
    if (i != NO_INDEX) {
        return i;
    }
    return grin->multiOutIndex(a);
}

unsigned long long IncidenceListVertex::inIndexOf(const Arc *a) const
{
    auto i = grin->inIndex(a);
    if (i != NO_INDEX) {
        return i;
    }
    return grin->multiInIndex(a);
}

void IncidenceListVertex::acceptOutgoingArcVisitor(ArcVisitor *aVisitor) const
{
    mapOutgoingArcs(aVisitor->getVisitorFunction(), arcFalse);
}

void IncidenceListVertex::acceptIncomingArcVisitor(ArcVisitor *aVisitor) const
{
    mapIncomingArcs(aVisitor->getVisitorFunction(), arcFalse);
}

bool IncidenceListVertex::mapOutgoingArcs(const ArcMapping &avFun, const ArcPredicate &breakCondition, bool checkValidity) const
{
    for (Arc *a : grin->outgoingArcs) {
        if (breakCondition(a)) {
            return false;
        }
        if (!checkValidity || a->isValid()) {
            avFun(a);
        }
    }
    for (Arc *a : grin->outgoingMultiArcs) {
        if (breakCondition(a)) {
            return false;
        }
        if (!checkValidity || a->isValid()) {
            avFun(a);
        }
    }
    return true;
}

bool IncidenceListVertex::mapIncomingArcs(const ArcMapping &avFun, const ArcPredicate &breakCondition, bool checkValidity) const
{
    for (Arc *a : grin->incomingArcs) {
        if (breakCondition(a)) {
            return false;
        }
        if (!checkValidity || a->isValid()) {
            avFun(a);
        }
    }
    for (Arc *a : grin->incomingMultiArcs) {
        if (breakCondition(a)) {
            return false;
        }
        if (!checkValidity || a->isValid()) {
            avFun(a);
        }
    }
    return true;
}

template <typename AL>
bool removeArcFromList(AL &list, PropertyMap<unsigned long long> &indexMap, const Arc *arc) {
    auto i = indexMap(arc);
    if (i == NO_INDEX) {
        return false;
    }
    assert(list[i] == arc);
    auto swap = list.back();
    list[i] = swap;
    indexMap.setValue(swap, i);
    indexMap.resetToDefault(arc);
    list.pop_back();
    return true;
}

bool removeBundledArcFromList(PropertyMap<ParallelArcsBundle*> &bundleMap, const Arc *arc) {
    ParallelArcsBundle *pmb = bundleMap(arc);
    if (!pmb) {
        return false;
    }
    assert(pmb->containsArc(arc));
    pmb->removeArc(arc);
    bundleMap.resetToDefault(arc);
    return true;
}

template <typename AL>
bool isArcInList(const PropertyMap<unsigned long long> &indexMap, AL &list, const Arc *arc) {
    bool found = indexMap(arc) != NO_INDEX;
    if (found) {
        assert(list[indexMap(arc)] == arc);
    }
    return found;
}

template <typename AL>
bool isBundledArc(const PropertyMap<ParallelArcsBundle*> bundleMap, AL &list, const PropertyMap<unsigned long long> &indexMap, const Arc *arc) {
    ParallelArcsBundle *pmb = bundleMap(arc);
    if (!pmb) {
        return false;
    }
    assert(pmb->containsArc(arc));
    return isArcInList(indexMap, list, pmb);
}

}
