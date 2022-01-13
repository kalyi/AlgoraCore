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
#include "property/fastpropertymap.h"

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <climits>

namespace Algora {

typedef typename std::vector<Arc*> ArcList;
typedef typename std::vector<MultiArc*> MultiArcList;

template <typename AL, typename PM>
bool removeArcFromList(AL &list, PM &indexMap, const Arc *arc);
bool removeBundledArcFromList(PropertyMap<ParallelArcsBundle*> &bundleMap, const Arc *arc);
template <typename AL, typename PM>
bool isArcInList(const PM &indexMap, AL &list, const Arc *arc);
template <typename AL, typename PM>
bool isBundledArc(const PropertyMap<ParallelArcsBundle*> &bundleMap, AL &list,
                  const PM &indexMap, const Arc *arc);

class IncidenceListVertex::CheshireCat {
public:
    size_type index;
    bool checkConsisteny;

    ArcList outgoingArcs;
    ArcList incomingArcs;
    MultiArcList outgoingMultiArcs;
    MultiArcList incomingMultiArcs;

    ArcList deactivatedOutgoingArcs;
    ArcList deactivatedIncomingArcs;
    MultiArcList deactivatedOutgoingMultiArcs;
    MultiArcList deactivatedIncomingMultiArcs;

    PropertyMap<ParallelArcsBundle*> bundle;

    FastPropertyMap<size_type> &outIndex;
    FastPropertyMap<size_type> &inIndex;
    PropertyMap<size_type> multiOutIndex;
    PropertyMap<size_type> multiInIndex;

    CheshireCat(
            FastPropertyMap<size_type> &outIndex,
            FastPropertyMap<size_type> &inIndex,
            size_type i)
        : index(i), outIndex(outIndex), inIndex(inIndex) {
        multiOutIndex.setDefaultValue(outIndex.getDefaultValue());
        multiInIndex.setDefaultValue(inIndex.getDefaultValue());
        bundle.setDefaultValue(nullptr);
    }

    void clear() {
        for (Arc *a : outgoingArcs) {
            outIndex.resetToDefault(a);
        }
        for (Arc *a : incomingArcs) {
            outIndex.resetToDefault(a);
        }

        outgoingArcs.clear();
        incomingArcs.clear();
        outgoingMultiArcs.clear();
        incomingMultiArcs.clear();
        deactivatedOutgoingArcs.clear();
        deactivatedIncomingArcs.clear();
        deactivatedOutgoingMultiArcs.clear();
        deactivatedIncomingMultiArcs.clear();

        multiOutIndex.resetAll();
        multiInIndex.resetAll();
        bundle.resetAll();
    }
};

IncidenceListVertex::IncidenceListVertex(id_type id, FastPropertyMap<size_type> &sharedOutIndex,
                                         FastPropertyMap<size_type> &sharedInIndex,
                                         GraphArtifact *parent, size_type index)
    : Vertex(id, parent), grin(new CheshireCat(sharedOutIndex, sharedInIndex, index))
{
    grin->checkConsisteny = true;
}

IncidenceListVertex::~IncidenceListVertex()
{
    delete grin;
}

IncidenceListVertex::size_type IncidenceListVertex::getOutDegree(bool multiArcsAsSimple) const
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

[[deprecated("use addOutgoingSimpleArc() or addOutgoingMultiArc() instead")]]
void IncidenceListVertex::addOutgoingArc(Arc *a)
{
    MultiArc *ma = dynamic_cast<MultiArc*>(a);
    if (ma) {
        addOutgoingMultiArc(ma);
    } else {
        addOutgoingSimpleArc(a);
    }
}

void IncidenceListVertex::addOutgoingMultiArc(MultiArc *ma)
{
    if (grin->checkConsisteny && ma->getTail() != this) {
        throw std::invalid_argument("Arc has other tail.");
    }

    grin->multiOutIndex.setValue(ma, grin->outgoingMultiArcs.size());
    grin->outgoingMultiArcs.push_back(ma);
    ParallelArcsBundle *pab = dynamic_cast<ParallelArcsBundle*>(ma);
    if (pab) {
        pab->mapArcs([&](Arc *a) {
            grin->bundle.setValue(a, pab);
        });
    }
}

void IncidenceListVertex::addOutgoingSimpleArc(Arc *a)
{
    assert(!dynamic_cast<MultiArc*>(a));

    if (grin->checkConsisteny && a->getTail() != this) {
        throw std::invalid_argument("Arc has other tail.");
    }

    grin->outIndex.setValue(a, grin->outgoingArcs.size());
    grin->outgoingArcs.push_back(a);
}

bool IncidenceListVertex::removeOutgoingArc(const Arc *a)
{
    if (grin->checkConsisteny && a->getTail() != this) {
        throw std::invalid_argument("Arc has other tail.");
    }
    return removeArcFromList(grin->outgoingArcs, grin->outIndex, a)
            || removeArcFromList(grin->outgoingMultiArcs, grin->multiOutIndex, a)
            || removeBundledArcFromList(grin->bundle, a);
}

void IncidenceListVertex::clearOutgoingArcs()
{
    for (Arc *a : grin->outgoingArcs) {
        grin->outIndex.resetToDefault(a);
    }
    for (Arc *a : grin->outgoingMultiArcs) {
        grin->multiOutIndex.resetToDefault(a);
    }
    for (Arc *a : grin->deactivatedOutgoingArcs) {
        grin->outIndex.resetToDefault(a);
    }
    for (Arc *a : grin->deactivatedOutgoingMultiArcs) {
        grin->multiOutIndex.resetToDefault(a);
    }
    grin->outgoingArcs.clear();
    grin->outgoingMultiArcs.clear();
    grin->deactivatedOutgoingArcs.clear();
    grin->deactivatedOutgoingMultiArcs.clear();
}

IncidenceListVertex::size_type IncidenceListVertex::getInDegree(bool multiArcsAsSimple) const
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

[[deprecated("use addIncomingSimpleArc() or addIncomingMultiArc() instead")]]
void IncidenceListVertex::addIncomingArc(Arc *a)
{
    MultiArc *ma = dynamic_cast<MultiArc*>(a);
    if (ma) {
        addIncomingMultiArc(ma);
    } else {
        addIncomingSimpleArc(a);
    }
}

void IncidenceListVertex::addIncomingMultiArc(MultiArc *ma)
{
    if (grin->checkConsisteny && ma->getHead() != this) {
        throw std::invalid_argument("Arc has other head.");
    }

    grin->multiInIndex.setValue(ma, grin->incomingMultiArcs.size());
    grin->incomingMultiArcs.push_back(ma);
    ParallelArcsBundle *pab = dynamic_cast<ParallelArcsBundle*>(ma);
    if (pab) {
        pab->mapArcs([&](Arc *a) {
            grin->bundle.setValue(a, pab);
        });
    }
}

void IncidenceListVertex::addIncomingSimpleArc(Arc *a)
{
    assert(!dynamic_cast<MultiArc*>(a));

    if (grin->checkConsisteny && a->getHead() != this) {
        throw std::invalid_argument("Arc has other head.");
    }

    grin->inIndex.setValue(a, grin->incomingArcs.size());
    grin->incomingArcs.push_back(a);
}

bool IncidenceListVertex::removeIncomingArc(const Arc *a)
{
    if (grin->checkConsisteny && a->getHead() != this) {
        throw std::invalid_argument("Arc has other head.");
    }
    return removeArcFromList(grin->incomingArcs, grin->inIndex, a)
            || removeArcFromList(grin->incomingMultiArcs, grin->multiInIndex, a)
            || removeBundledArcFromList(grin->bundle, a);
}

void IncidenceListVertex::clearIncomingArcs()
{
    for (Arc *a : grin->incomingArcs) {
        grin->inIndex.resetToDefault(a);
    }
    for (Arc *a : grin->incomingMultiArcs) {
        grin->multiInIndex.resetToDefault(a);
    }
    for (Arc *a : grin->deactivatedIncomingArcs) {
        grin->inIndex.resetToDefault(a);
    }
    for (Arc *a : grin->deactivatedIncomingMultiArcs) {
        grin->multiInIndex.resetToDefault(a);
    }
    grin->incomingArcs.clear();
    grin->incomingMultiArcs.clear();
    grin->deactivatedIncomingArcs.clear();
    grin->deactivatedIncomingMultiArcs.clear();
}

void IncidenceListVertex::enableConsistencyCheck(bool enable)
{
    grin->checkConsisteny = enable;
}

IncidenceListVertex::size_type IncidenceListVertex::getIndex() const
{
    return grin->index;
}

bool IncidenceListVertex::activateOutgoingArc(Arc *a)
{
    if (removeArcFromList(grin->deactivatedOutgoingArcs, grin->outIndex, a)) {
        addOutgoingSimpleArc(a);
        return true;
    }
    if (removeArcFromList(grin->deactivatedOutgoingMultiArcs, grin->multiOutIndex, a)) {
        addOutgoingMultiArc(dynamic_cast<MultiArc*>(a));
        return true;
    }
    return false;
}

bool IncidenceListVertex::activateIncomingArc(Arc *a)
{
    if (removeArcFromList(grin->deactivatedIncomingArcs, grin->inIndex, a)) {
        addIncomingSimpleArc(a);
        return true;
    }
    if (removeArcFromList(grin->deactivatedIncomingMultiArcs, grin->multiInIndex, a)) {
        addIncomingMultiArc(dynamic_cast<MultiArc*>(a));
        return true;
    }
    return false;
}

bool IncidenceListVertex::deactivateOutgoingArc(Arc *a)
{
    if (!removeOutgoingArc(a)) {
        return false;
    }
    auto multiArc = dynamic_cast<MultiArc*>(a);
    if (multiArc) {
        grin->multiOutIndex.setValue(multiArc, grin->deactivatedOutgoingMultiArcs.size());
        grin->deactivatedOutgoingMultiArcs.push_back(multiArc);
    } else {
        grin->outIndex.setValue(a, grin->deactivatedOutgoingArcs.size());
        grin->deactivatedOutgoingArcs.push_back(a);
    }
    return true;
}

bool IncidenceListVertex::deactivateIncomingArc(Arc *a)
{
    if (!removeIncomingArc(a)) {
        return false;
    }
    auto multiArc = dynamic_cast<MultiArc*>(a);
    if (multiArc) {
        grin->multiInIndex.setValue(multiArc, grin->deactivatedIncomingArcs.size());
        grin->deactivatedIncomingMultiArcs.push_back(multiArc);
    } else {
        grin->inIndex.setValue(a, grin->deactivatedIncomingArcs.size());
        grin->deactivatedIncomingArcs.push_back(a);
    }
    return true;
}

void IncidenceListVertex::activateAllOutgoingArcs()
{
    for (auto *a : grin->deactivatedOutgoingArcs) {
        addOutgoingSimpleArc(a);
    }
    grin->deactivatedOutgoingArcs.clear();
    for (auto *a : grin->deactivatedOutgoingMultiArcs) {
        addOutgoingMultiArc(a);
    }
    grin->deactivatedOutgoingMultiArcs.clear();
}

void IncidenceListVertex::deactivateAllOutgoingArcs()
{
    auto i = grin->deactivatedOutgoingArcs.size();
    for (auto *a : grin->outgoingArcs) {
        grin->deactivatedOutgoingArcs.push_back(a);
        grin->outIndex.setValue(a, i);
        i++;
    }
    i = grin->deactivatedOutgoingMultiArcs.size();
    for (auto *a : grin->outgoingMultiArcs) {
        grin->deactivatedOutgoingMultiArcs.push_back(a);
        grin->multiOutIndex.setValue(a, i);
        i++;
    }
    grin->outgoingArcs.clear();
    grin->outgoingMultiArcs.clear();
}

void IncidenceListVertex::activateAllIncomingArcs()
{
    for (auto *a : grin->deactivatedIncomingArcs) {
        addIncomingSimpleArc(a);
    }
    grin->deactivatedIncomingArcs.clear();
    for (auto *a : grin->deactivatedIncomingMultiArcs) {
        addIncomingMultiArc(a);
    }
    grin->deactivatedIncomingMultiArcs.clear();
}

void IncidenceListVertex::deactivateAllIncomingArcs()
{
    auto i = grin->deactivatedIncomingArcs.size();
    for (auto *a : grin->incomingArcs) {
        grin->deactivatedIncomingArcs.push_back(a);
        grin->inIndex.setValue(a, i);
        i++;
    }
    i = grin->deactivatedIncomingMultiArcs.size();
    for (auto *a : grin->incomingMultiArcs) {
        grin->deactivatedIncomingMultiArcs.push_back(a);
        grin->multiInIndex.setValue(a, i);
        i++;
    }
    grin->incomingArcs.clear();
    grin->incomingMultiArcs.clear();
}

bool IncidenceListVertex::mapDeactivatedOutgoingArcs(const ArcMapping &avFun,
                                                     const ArcPredicate &breakCondition,
                                                     bool checkValidity) const
{
    for (Arc *a : grin->deactivatedOutgoingArcs) {
        if (breakCondition(a)) {
            return false;
        }
        if (!checkValidity || a->isValid()) {
            avFun(a);
        }
    }
    for (Arc *a : grin->deactivatedOutgoingMultiArcs) {
        if (breakCondition(a)) {
            return false;
        }
        if (!checkValidity || a->isValid()) {
            avFun(a);
        }
    }
    return true;
}

bool IncidenceListVertex::mapDeactivatedIncomingArcs(const ArcMapping &avFun,
                                                     const ArcPredicate &breakCondition,
                                                     bool checkValidity) const
{
    for (Arc *a : grin->deactivatedIncomingArcs) {
        if (breakCondition(a)) {
            return false;
        }
        if (!checkValidity || a->isValid()) {
            avFun(a);
        }
    }
    for (Arc *a : grin->deactivatedIncomingMultiArcs) {
        if (breakCondition(a)) {
            return false;
        }
        if (!checkValidity || a->isValid()) {
            avFun(a);
        }
    }
    return true;
}

void IncidenceListVertex::setIndex(size_type i)
{
    grin->index = i;
}

void IncidenceListVertex::hibernate()
{
    invalidate();
    reset(); // reset name
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
            || isBundledArc(grin->bundle, grin->outgoingMultiArcs, grin->outIndex, a);
}

bool IncidenceListVertex::hasIncomingArc(const Arc *a) const
{
    return isArcInList(grin->inIndex, grin->incomingArcs, a)
            || isArcInList(grin->multiInIndex, grin->incomingMultiArcs, a)
            || isBundledArc(grin->bundle, grin->incomingMultiArcs, grin->inIndex, a);
}

Arc *IncidenceListVertex::outgoingArcAt(size_type i, bool multiArcsAsSimple) const
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

Arc *IncidenceListVertex::incomingArcAt(size_type i, bool multiArcsAsSimple) const
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

IncidenceListVertex::size_type IncidenceListVertex::outIndexOf(const Arc *a) const
{
    auto i = grin->outIndex(a);
    if (i != grin->outIndex.getDefaultValue()) {
        return i;
    }
    return grin->multiOutIndex(a);
}

IncidenceListVertex::size_type IncidenceListVertex::inIndexOf(const Arc *a) const
{
    auto i = grin->inIndex(a);
    if (i != grin->inIndex.getDefaultValue()) {
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

bool IncidenceListVertex::mapOutgoingArcs(const ArcMapping &avFun,
                                          const ArcPredicate &breakCondition,
                                          bool checkValidity) const
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

bool IncidenceListVertex::mapIncomingArcs(const ArcMapping &avFun,
                                          const ArcPredicate &breakCondition,
                                          bool checkValidity) const
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

template <typename AL, typename PM>
bool removeArcFromList(AL &list, PM &indexMap, const Arc *arc) {
    auto i = indexMap(arc);
    if (i >= list.size() || list[i] != arc) {
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

template <typename AL, typename PM>
bool isArcInList(const PM &indexMap, AL &list, const Arc *arc) {
    auto i = indexMap(arc);
    return i < list.size() && list[i] == arc;
}

template <typename AL, typename PM>
bool isBundledArc(const PropertyMap<ParallelArcsBundle*> &bundleMap, AL &list,
                  const PM &indexMap, const Arc *arc) {
    ParallelArcsBundle *pmb = bundleMap(arc);
    if (!pmb) {
        return false;
    }
    assert(pmb->containsArc(arc));
    return isArcInList(indexMap, list, pmb);
}

}
