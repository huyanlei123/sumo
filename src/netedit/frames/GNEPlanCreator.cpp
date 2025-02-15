/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.dev/sumo
// Copyright (C) 2001-2023 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    GNEPlanCreator.cpp
/// @author  Pablo Alvarez Lopez
/// @date    Mar 2022
///
// Frame for create paths
/****************************************************************************/
#include <config.h>

#include <netedit/GNEApplicationWindow.h>
#include <netedit/GNENet.h>
#include <netedit/GNEViewNet.h>
#include <netedit/GNEViewParent.h>
#include <netedit/elements/additional/GNETAZ.h>
#include <netedit/frames/common/GNEInspectorFrame.h>
#include <utils/gui/div/GLHelper.h>
#include <utils/gui/div/GUIDesigns.h>
#include <utils/gui/windows/GUIAppEnum.h>

#include "GNEPlanCreator.h"


// ===========================================================================
// FOX callback mapping
// ===========================================================================

FXDEFMAP(GNEPlanCreator) PathCreatorMap[] = {
    FXMAPFUNC(SEL_COMMAND, MID_GNE_PATHCREATOR_ABORT,           GNEPlanCreator::onCmdAbortPathCreation),
    FXMAPFUNC(SEL_COMMAND, MID_GNE_PATHCREATOR_FINISH,          GNEPlanCreator::onCmdCreatePath),
    FXMAPFUNC(SEL_COMMAND, MID_GNE_PATHCREATOR_USELASTROUTE,    GNEPlanCreator::onCmdUseLastRoute),
    FXMAPFUNC(SEL_UPDATE,  MID_GNE_PATHCREATOR_USELASTROUTE,    GNEPlanCreator::onUpdUseLastRoute),
    FXMAPFUNC(SEL_COMMAND, MID_GNE_PATHCREATOR_REMOVELAST,      GNEPlanCreator::onCmdRemoveLastElement)
};

// Object implementation
FXIMPLEMENT(GNEPlanCreator,                MFXGroupBoxModule,     PathCreatorMap,                 ARRAYNUMBER(PathCreatorMap))


// ===========================================================================
// method definitions
// ===========================================================================

GNEPlanCreator::PlanPath::PlanPath(const SUMOVehicleClass vClass, GNEEdge* edge) :
    mySubPath({edge}),
    myConflictVClass(false),
    myConflictDisconnected(false) {
    // check if we have to change vClass flag
    if (edge->getNBEdge()->getNumLanesThatAllow(vClass) == 0) {
        myConflictVClass = true;
    }
}


GNEPlanCreator::PlanPath::PlanPath(GNEViewNet* viewNet, const SUMOVehicleClass vClass, GNEEdge* edgeFrom, GNEEdge* edgeTo) :
    myConflictVClass(false),
    myConflictDisconnected(false) {
    // calculate subpath
    mySubPath = viewNet->getNet()->getPathManager()->getPathCalculator()->calculateDijkstraPath(vClass, {edgeFrom, edgeTo});
    // if subPath is empty, try it with pedestrian (i.e. ignoring vCass)
    if (mySubPath.empty()) {
        mySubPath = viewNet->getNet()->getPathManager()->getPathCalculator()->calculateDijkstraPath(SVC_PEDESTRIAN, {edgeFrom, edgeTo});
        if (mySubPath.empty()) {
            mySubPath = { edgeFrom, edgeTo };
            myConflictDisconnected = true;
        } else {
            myConflictVClass = true;
        }
    }
}


GNEPlanCreator::PlanPath::PlanPath(GNEViewNet* viewNet, const SUMOVehicleClass vClass, GNEJunction* junctionFrom, GNEJunction* junctionTo) :
    myConflictVClass(false),
    myConflictDisconnected(false) {
    // calculate subpath
    mySubPath = viewNet->getNet()->getPathManager()->getPathCalculator()->calculateDijkstraPath(vClass, junctionFrom, junctionTo);
    // if subPath is empty, try it with pedestrian (i.e. ignoring vCass)
    if (mySubPath.empty()) {
        mySubPath = viewNet->getNet()->getPathManager()->getPathCalculator()->calculateDijkstraPath(SVC_PEDESTRIAN, junctionFrom, junctionTo);
        if (mySubPath.empty()) {
            myConflictDisconnected = true;
        } else {
            myConflictVClass = true;
        }
    }
}


const std::vector<GNEEdge*>&
GNEPlanCreator::PlanPath::getSubPath() const {
    return mySubPath;
}


bool
GNEPlanCreator::PlanPath::isConflictVClass() const {
    return myConflictVClass;
}


bool
GNEPlanCreator::PlanPath::isConflictDisconnected() const {
    return myConflictDisconnected;
}


GNEPlanCreator::PlanPath::PlanPath() :
    myConflictVClass(false),
    myConflictDisconnected(false) {
}


GNEPlanCreator::GNEPlanCreator(GNEFrame* frameParent) :
    MFXGroupBoxModule(frameParent, TL("Route creator")),
    myFrameParent(frameParent),
    myVClass(SVC_PASSENGER),
    myCreationMode(0),
    myToStoppingPlace(nullptr),
    myRoute(nullptr) {
    // create label for route info
    myInfoRouteLabel = new FXLabel(getCollapsableFrame(), TL("No edges selected"), 0, GUIDesignLabelFrameInformation);
    // create button for use last route
    myUseLastRoute = GUIDesigns::buildFXButton(getCollapsableFrame(), TL("Use last route"), "", "", GUIIconSubSys::getIcon(GUIIcon::ROUTE), this, MID_GNE_PATHCREATOR_USELASTROUTE, GUIDesignButton);
    myUseLastRoute->disable();
    // create button for finish route creation
    myFinishCreationButton = GUIDesigns::buildFXButton(getCollapsableFrame(), TL("Finish route creation"), "", "", nullptr, this, MID_GNE_PATHCREATOR_FINISH, GUIDesignButton);
    myFinishCreationButton->disable();
    // create button for abort route creation
    myAbortCreationButton = GUIDesigns::buildFXButton(getCollapsableFrame(), TL("Abort route creation"), "", "", nullptr, this, MID_GNE_PATHCREATOR_ABORT, GUIDesignButton);
    myAbortCreationButton->disable();
    // create button for remove last inserted edge
    myRemoveLastInsertedElement = GUIDesigns::buildFXButton(getCollapsableFrame(), TL("Remove last edge"), "", "", nullptr, this, MID_GNE_PATHCREATOR_REMOVELAST, GUIDesignButton);
    myRemoveLastInsertedElement->disable();
    // create backspace label (always shown)
    myBackSpaceLabel = new FXLabel(this, "BACKSPACE: undo click", 0, GUIDesignLabelFrameInformation);
}


GNEPlanCreator::~GNEPlanCreator() {}


void
GNEPlanCreator::showPathCreatorModule(SumoXMLTag element, const bool firstElement, const bool consecutives) {
    // declare flag
    bool showPathCreator = true;
    // first abort creation
    abortPathCreation();
    // hide use last inserted route
    myUseLastRoute->hide();
    // disable buttons
    myFinishCreationButton->disable();
    myAbortCreationButton->disable();
    myRemoveLastInsertedElement->disable();
    // show info label
    myInfoRouteLabel->show();
    myBackSpaceLabel->show();
    // reset creation mode
    myCreationMode = 0;
    // set first element
    if (firstElement) {
        myCreationMode |= REQUIRE_FIRSTELEMENT;
    }
    // set consecutive or non consecuives
    if (consecutives) {
        myCreationMode |= CONSECUTIVE_EDGES;
    } else {
        myCreationMode |= NONCONSECUTIVE_EDGES;
    }
    // set specific mode depending of tag
    switch (element) {
        // routes
        case SUMO_TAG_ROUTE:
        case GNE_TAG_ROUTE_EMBEDDED:
            myCreationMode |= SHOW_CANDIDATE_EDGES;
            myCreationMode |= START_EDGE;
            myCreationMode |= END_EDGE;
            break;
        // vehicles
        case SUMO_TAG_VEHICLE:
        case GNE_TAG_FLOW_ROUTE:
        case GNE_TAG_WALK_ROUTE:
            myCreationMode |= ROUTE;
            // show use last inserted route
            myUseLastRoute->show();
            // disable other elements
            myFinishCreationButton->hide();
            myAbortCreationButton->hide();
            myRemoveLastInsertedElement->hide();
            myInfoRouteLabel->hide();
            myBackSpaceLabel->hide();
            break;
        case SUMO_TAG_TRIP:
        case SUMO_TAG_FLOW:
        case GNE_TAG_VEHICLE_WITHROUTE:
        case GNE_TAG_FLOW_WITHROUTE:
            myCreationMode |= SHOW_CANDIDATE_EDGES;
            myCreationMode |= START_EDGE;
            myCreationMode |= END_EDGE;
            break;
        case GNE_TAG_TRIP_JUNCTIONS:
        case GNE_TAG_FLOW_JUNCTIONS:
            myCreationMode |= SHOW_CANDIDATE_JUNCTIONS;
            myCreationMode |= START_JUNCTION;
            myCreationMode |= END_JUNCTION;
            myCreationMode |= ONLY_FROMTO;
            break;
        case GNE_TAG_TRIP_TAZS:
        case GNE_TAG_FLOW_TAZS:
            myCreationMode |= START_TAZ;
            myCreationMode |= END_TAZ;
            myCreationMode |= ONLY_FROMTO;
            break;
        // edges
        case GNE_TAG_WALK_EDGES:
        case GNE_TAG_TRANSHIP_EDGES:
            myCreationMode |= SHOW_CANDIDATE_EDGES;
            myCreationMode |= START_EDGE;
            myCreationMode |= END_EDGE;
            break;
        // edge->edge
        case GNE_TAG_PERSONTRIP_EDGE_EDGE:
        case GNE_TAG_RIDE_EDGE_EDGE:
        case GNE_TAG_WALK_EDGE_EDGE:
        case GNE_TAG_TRANSPORT_EDGE:
        case GNE_TAG_TRANSHIP_EDGE:
            myCreationMode |= SHOW_CANDIDATE_EDGES;
            myCreationMode |= ONLY_FROMTO;
            myCreationMode |= START_EDGE;
            myCreationMode |= END_EDGE;
            break;
        // edge->taz
        case GNE_TAG_PERSONTRIP_EDGE_TAZ:
        case GNE_TAG_WALK_EDGE_TAZ:
            myCreationMode |= SHOW_CANDIDATE_EDGES;
            myCreationMode |= ONLY_FROMTO;
            myCreationMode |= START_EDGE;
            myCreationMode |= END_TAZ;
            break;
        // edge->busStop
        case GNE_TAG_PERSONTRIP_EDGE_BUSSTOP:
        case GNE_TAG_RIDE_EDGE_BUSSTOP:
        case GNE_TAG_WALK_EDGE_BUSSTOP:
            myCreationMode |= SHOW_CANDIDATE_EDGES;
            myCreationMode |= ONLY_FROMTO;
            myCreationMode |= START_EDGE;
            myCreationMode |= END_BUSSTOP;
            break;
        // edge->trainStop
        case GNE_TAG_PERSONTRIP_EDGE_TRAINSTOP:
        case GNE_TAG_RIDE_EDGE_TRAINSTOP:
        case GNE_TAG_WALK_EDGE_TRAINSTOP:
            myCreationMode |= SHOW_CANDIDATE_EDGES;
            myCreationMode |= ONLY_FROMTO;
            myCreationMode |= START_EDGE;
            myCreationMode |= END_TRAINSTOP;
            break;
        // edge->containerStop
        case GNE_TAG_TRANSPORT_CONTAINERSTOP:
        case GNE_TAG_TRANSHIP_CONTAINERSTOP:
            myCreationMode |= SHOW_CANDIDATE_EDGES;
            myCreationMode |= ONLY_FROMTO;
            myCreationMode |= START_EDGE;
            myCreationMode |= END_CONTAINERSTOP;
            break;
        // taz->taz
        case GNE_TAG_PERSONTRIP_TAZ_TAZ:
        case GNE_TAG_WALK_TAZ_TAZ:
            myCreationMode |= ONLY_FROMTO;
            myCreationMode |= START_TAZ;
            myCreationMode |= END_TAZ;
            break;
        // taz->edge
        case GNE_TAG_PERSONTRIP_TAZ_EDGE:
        case GNE_TAG_WALK_TAZ_EDGE:
            myCreationMode |= ONLY_FROMTO;
            myCreationMode |= START_TAZ;
            myCreationMode |= END_EDGE;
            break;
        // taz->busStop
        case GNE_TAG_PERSONTRIP_TAZ_BUSSTOP:
        case GNE_TAG_WALK_TAZ_BUSSTOP:
            myCreationMode |= ONLY_FROMTO;
            myCreationMode |= START_TAZ;
            myCreationMode |= END_BUSSTOP;
            break;
        // taz->trainStop
        case GNE_TAG_PERSONTRIP_TAZ_TRAINSTOP:
        case GNE_TAG_WALK_TAZ_TRAINSTOP:
            myCreationMode |= ONLY_FROMTO;
            myCreationMode |= START_TAZ;
            myCreationMode |= END_TRAINSTOP;
            break;
        // junction->junction
        case GNE_TAG_PERSONTRIP_JUNCTION_JUNCTION:
        case GNE_TAG_WALK_JUNCTION_JUNCTION:
            myCreationMode |= SHOW_CANDIDATE_JUNCTIONS;
            myCreationMode |= START_JUNCTION;
            myCreationMode |= END_JUNCTION;
            myCreationMode |= ONLY_FROMTO;
            break;
        // stops (person and containers)
        case GNE_TAG_STOPPERSON_BUSSTOP:
            myCreationMode |= STOP;
            myCreationMode |= END_BUSSTOP;
            break;
        case GNE_TAG_STOPPERSON_TRAINSTOP:
            myCreationMode |= STOP;
            myCreationMode |= END_TRAINSTOP;
            break;
        case GNE_TAG_STOPCONTAINER_CONTAINERSTOP:
            myCreationMode |= STOP;
            myCreationMode |= END_CONTAINERSTOP;
            break;
        case GNE_TAG_STOPPERSON_EDGE:
        case GNE_TAG_STOPCONTAINER_EDGE:
            myCreationMode |= STOP;
            myCreationMode |= START_EDGE;
            break;
        // generic datas
        case SUMO_TAG_EDGEREL:
            myCreationMode |= ONLY_FROMTO;
            myCreationMode |= START_EDGE;
            myCreationMode |= END_EDGE;
            break;
        default:
            showPathCreator = false;
            break;
    }
    // check if show path creator
    if (showPathCreator) {
        // recalc before show (to avoid graphic problems)
        recalc();
        // show modul
        show();
    } else {
        // hide modul
        hide();
    }
}


void
GNEPlanCreator::hidePathCreatorModule() {
    // clear path
    clearPath();
    // hide modul
    hide();
}


SUMOVehicleClass
GNEPlanCreator::getVClass() const {
    return myVClass;
}


void
GNEPlanCreator::setVClass(SUMOVehicleClass vClass) {
    myVClass = vClass;
}


bool
GNEPlanCreator::addJunction(GNEJunction* junction) {
    // check if junctions are allowed
    if (((myCreationMode & START_JUNCTION) == 0) && ((myCreationMode & END_JUNCTION) == 0)) {
        return false;
    }
    // continue depending of number of selected edge
    if (mySelectedJunctions.size() > 0) {
        // check double junctions
        if (mySelectedJunctions.back() == junction) {
            // Write warning
            WRITE_WARNING(TL("Double junctions aren't allowed"));
            // abort add junction
            return false;
        }
    }
    // check number of junctions
    if (mySelectedJunctions.size() == 2 && (myCreationMode & Mode::ONLY_FROMTO)) {
        // Write warning
        WRITE_WARNING(TL("Only two junctions are allowed"));
        // abort add junction
        return false;
    }
    // All checks ok, then add it in selected elements
    mySelectedJunctions.push_back(junction);
    // enable abort route button
    myAbortCreationButton->enable();
    // enable finish button
    myFinishCreationButton->enable();
    // disable undo/redo
    myFrameParent->getViewNet()->getViewParent()->getGNEAppWindows()->disableUndoRedo(TL("route creation"));
    // enable or disable remove last junction button
    if (mySelectedJunctions.size() > 1) {
        myRemoveLastInsertedElement->enable();
    } else {
        myRemoveLastInsertedElement->disable();
    }
    // recalculate path
    recalculatePath();
    // update info route label
    updateInfoRouteLabel();
    return true;
}


bool
GNEPlanCreator::addTAZ(GNEAdditional* TAZ) {
    // check if TAZs are allowed
    if (((myCreationMode & START_TAZ) == 0) && ((myCreationMode & END_TAZ) == 0)) {
        return false;
    }
    // continue depending of number of selected edge
    if (mySelectedTAZs.size() > 0) {
        // check double TAZs
        if (mySelectedTAZs.back() == TAZ) {
            // Write warning
            WRITE_WARNING(TL("Double TAZs aren't allowed"));
            // abort add TAZ
            return false;
        }
    }
    // check number of TAZs
    if ((mySelectedTAZs.size() == 2) && (myCreationMode & Mode::ONLY_FROMTO)) {
        // Write warning
        WRITE_WARNING(TL("Only two TAZs are allowed"));
        // abort add TAZ
        return false;
    }
    // All checks ok, then add it in selected elements
    mySelectedTAZs.push_back(TAZ);
    // enable abort route button
    myAbortCreationButton->enable();
    // enable finish button
    myFinishCreationButton->enable();
    // disable undo/redo
    myFrameParent->getViewNet()->getViewParent()->getGNEAppWindows()->disableUndoRedo(TL("route creation"));
    // enable or disable remove last TAZ button
    if (mySelectedTAZs.size() > 1) {
        myRemoveLastInsertedElement->enable();
    } else {
        myRemoveLastInsertedElement->disable();
    }
    // update info route label
    updateInfoRouteLabel();
    return true;
}


bool
GNEPlanCreator::addEdge(GNEEdge* edge) {
    // check if edges are allowed
    if (((myCreationMode & START_EDGE) == 0) && ((myCreationMode & END_EDGE) == 0)) {
        return false;
    }
    // continue depending of number of selected eges
    if (mySelectedEdges.size() > 0) {
        // check double edges
        if (mySelectedEdges.back() == edge) {
            // Write warning
            WRITE_WARNING(TL("Double edges aren't allowed"));
            // abort add edge
            return false;
        }
        // check consecutive edges
        if (myCreationMode & Mode::CONSECUTIVE_EDGES) {
            // check that new edge is consecutive
            const auto& outgoingEdges = mySelectedEdges.back()->getToJunction()->getGNEOutgoingEdges();
            if (std::find(outgoingEdges.begin(), outgoingEdges.end(), edge) == outgoingEdges.end()) {
                // Write warning
                WRITE_WARNING(TL("Only consecutives edges are allowed"));
                // abort add edge
                return false;
            }
        }
    }
    // check number of edges
    if (mySelectedEdges.size() == 2 && (myCreationMode & Mode::ONLY_FROMTO)) {
        // Write warning
        WRITE_WARNING(TL("Only two edges are allowed"));
        // abort add edge
        return false;
    }
    // All checks ok, then add it in selected elements
    mySelectedEdges.push_back(edge);
    // enable abort route button
    myAbortCreationButton->enable();
    // enable finish button
    myFinishCreationButton->enable();
    // disable undo/redo
    myFrameParent->getViewNet()->getViewParent()->getGNEAppWindows()->disableUndoRedo(TL("route creation"));
    // enable or disable remove last edge button
    if (mySelectedEdges.size() > 1) {
        myRemoveLastInsertedElement->enable();
    } else {
        myRemoveLastInsertedElement->disable();
    }
    // recalculate path
    recalculatePath();
    // update info route label
    updateInfoRouteLabel();
    // if is a stop, create inmediately
    if (myCreationMode & STOP) {
        if (createPath(false)) {
            return true;
        } else {
            mySelectedEdges.pop_back();
            // recalculate path again
            recalculatePath();
            // update info route label
            updateInfoRouteLabel();
            return false;
        }
    } else {
        return true;
    }
}


const std::vector<GNEEdge*>&
GNEPlanCreator::getSelectedEdges() const {
    return mySelectedEdges;
}


const std::vector<GNEJunction*>&
GNEPlanCreator::getSelectedJunctions() const {
    return mySelectedJunctions;
}


const std::vector<GNEAdditional*>&
GNEPlanCreator::getSelectedTAZs() const {
    return mySelectedTAZs;
}


bool
GNEPlanCreator::addStoppingPlace(GNEAdditional* stoppingPlace) {
    if (stoppingPlace == nullptr) {
        return false;
    }
    // check if stoppingPlaces are allowed
    if (((myCreationMode & END_BUSSTOP) == 0) && ((myCreationMode & END_TRAINSTOP) == 0) && ((myCreationMode & END_CONTAINERSTOP) == 0)) {
        return false;
    }
    if (((myCreationMode & END_BUSSTOP) != 0) && (stoppingPlace->getTagProperty().getTag() != SUMO_TAG_BUS_STOP)) {
        return false;
    }
    if (((myCreationMode & END_TRAINSTOP) != 0) && (stoppingPlace->getTagProperty().getTag() != SUMO_TAG_TRAIN_STOP)) {
        return false;
    }
    if (((myCreationMode & END_CONTAINERSTOP) != 0) && (stoppingPlace->getTagProperty().getTag() != SUMO_TAG_CONTAINER_STOP)) {
        return false;
    }
    // avoid select first an stopping place
    if (((myCreationMode & START_EDGE) != 0) && mySelectedEdges.empty()) {
        WRITE_WARNING(TL("first select an edge"));
        return false;
    }
    // check if previously stopping place from was set
    if (myToStoppingPlace) {
        return false;
    } else {
        myToStoppingPlace = stoppingPlace;
    }
    // enable abort route button
    myAbortCreationButton->enable();
    // enable finish button
    myFinishCreationButton->enable();
    // disable undo/redo
    myFrameParent->getViewNet()->getViewParent()->getGNEAppWindows()->disableUndoRedo("route creation");
    // enable or disable remove last stoppingPlace button
    if (myToStoppingPlace) {
        myRemoveLastInsertedElement->enable();
    } else {
        myRemoveLastInsertedElement->disable();
    }
    // recalculate path
    recalculatePath();
    // update info route label
    updateInfoRouteLabel();
    // if is a stop, create inmediately
    if (myCreationMode & STOP) {
        if (createPath(false)) {
            return true;
        } else {
            myToStoppingPlace = nullptr;
            // recalculate path again
            recalculatePath();
            // update info route label
            updateInfoRouteLabel();
            return false;
        }
    } else {
        return true;
    }
}


GNEAdditional*
GNEPlanCreator::getToStoppingPlace(SumoXMLTag expectedTag) const {
    if (myToStoppingPlace && (myToStoppingPlace->getTagProperty().getTag() == expectedTag)) {
        return myToStoppingPlace;
    } else {
        return nullptr;
    }
}


bool
GNEPlanCreator::addRoute(GNEDemandElement* route) {
    // check if routes aren allowed
    if ((myCreationMode & ROUTE) == 0) {
        return false;
    }
    // check if previously a route was added
    if (myRoute) {
        return false;
    }
    // set route and create path
    myRoute = route;
    createPath(false);
    myRoute = nullptr;
    // recalculate path
    recalculatePath();
    updateInfoRouteLabel();
    return true;
}


GNEDemandElement*
GNEPlanCreator::getRoute() const {
    return myRoute;
}


const std::vector<GNEPlanCreator::PlanPath>&
GNEPlanCreator::getPath() const {
    return myPath;
}


void
GNEPlanCreator::drawTemporalRoute(const GUIVisualizationSettings& s) const {
    const double lineWidth = 0.35;
    const double lineWidthin = 0.25;
    // Add a draw matrix
    GLHelper::pushMatrix();
    // Start with the drawing of the area traslating matrix to origin
    glTranslated(0, 0, GLO_MAX - 0.1);
    // check if draw bewteen junction or edges
    if (myPath.size() > 0) {
        // set first color
        GLHelper::setColor(RGBColor::GREY);
        // iterate over path
        for (int i = 0; i < (int)myPath.size(); i++) {
            // get path
            const GNEPlanCreator::PlanPath& path = myPath.at(i);
            // draw line over
            for (int j = 0; j < (int)path.getSubPath().size(); j++) {
                const GNELane* lane = path.getSubPath().at(j)->getLanes().back();
                if (((i == 0) && (j == 0)) || (j > 0)) {
                    GLHelper::drawBoxLines(lane->getLaneShape(), lineWidth);
                }
                // draw connection between lanes
                if ((j + 1) < (int)path.getSubPath().size()) {
                    const GNELane* nextLane = path.getSubPath().at(j + 1)->getLanes().back();
                    if (lane->getLane2laneConnections().exist(nextLane)) {
                        GLHelper::drawBoxLines(lane->getLane2laneConnections().getLane2laneGeometry(nextLane).getShape(), lineWidth);
                    } else {
                        GLHelper::drawBoxLines({lane->getLaneShape().back(), nextLane->getLaneShape().front()}, lineWidth);
                    }
                }
            }
        }
        glTranslated(0, 0, 0.1);
        // iterate over path again
        for (int i = 0; i < (int)myPath.size(); i++) {
            // get path
            const GNEPlanCreator::PlanPath& path = myPath.at(i);
            // set path color color
            if ((myCreationMode & SHOW_CANDIDATE_EDGES) == 0) {
                GLHelper::setColor(RGBColor::ORANGE);
            } else if (path.isConflictDisconnected()) {
                GLHelper::setColor(s.candidateColorSettings.conflict);
            } else if (path.isConflictVClass()) {
                GLHelper::setColor(s.candidateColorSettings.special);
            } else {
                GLHelper::setColor(RGBColor::ORANGE);
            }
            // draw line over
            for (int j = 0; j < (int)path.getSubPath().size(); j++) {
                const GNELane* lane = path.getSubPath().at(j)->getLanes().back();
                if (((i == 0) && (j == 0)) || (j > 0)) {
                    GLHelper::drawBoxLines(lane->getLaneShape(), lineWidthin);
                }
                // draw connection between lanes
                if ((j + 1) < (int)path.getSubPath().size()) {
                    const GNELane* nextLane = path.getSubPath().at(j + 1)->getLanes().back();
                    if (lane->getLane2laneConnections().exist(nextLane)) {
                        GLHelper::drawBoxLines(lane->getLane2laneConnections().getLane2laneGeometry(nextLane).getShape(), lineWidthin);
                    } else {
                        GLHelper::drawBoxLines({ lane->getLaneShape().back(), nextLane->getLaneShape().front() }, lineWidthin);
                    }
                }
            }
        }
    } else if (mySelectedJunctions.size() > 0) {
        // set color
        GLHelper::setColor(RGBColor::ORANGE);
        // draw line between junctions
        for (int i = 0; i < (int)mySelectedJunctions.size() - 1; i++) {
            // get two points
            const Position posA = mySelectedJunctions.at(i)->getPositionInView();
            const Position posB = mySelectedJunctions.at(i + 1)->getPositionInView();
            const double rot = ((double)atan2((posB.x() - posA.x()), (posA.y() - posB.y())) * (double) 180.0 / (double)M_PI);
            const double len = posA.distanceTo2D(posB);
            // draw line
            GLHelper::drawBoxLine(posA, rot, len, 0.25);
        }
    } else if (mySelectedTAZs.size() > 0) {
        // set color
        GLHelper::setColor(RGBColor::ORANGE);
        // draw line between TAZs
        for (int i = 0; i < (int)mySelectedTAZs.size() - 1; i++) {
            // get two points
            const Position posA = mySelectedTAZs.at(i)->getPositionInView();
            const Position posB = mySelectedTAZs.at(i + 1)->getPositionInView();
            const double rot = ((double)atan2((posB.x() - posA.x()), (posA.y() - posB.y())) * (double) 180.0 / (double)M_PI);
            const double len = posA.distanceTo2D(posB);
            // draw line
            GLHelper::drawBoxLine(posA, rot, len, 0.25);
        }
    }
    // Pop last matrix
    GLHelper::popMatrix();
}


bool
GNEPlanCreator::createPath(const bool useLastRoute) {
    // call create path implemented in frame parent
    return myFrameParent->createPath(useLastRoute);
}


void
GNEPlanCreator::abortPathCreation() {
    // first check that there is elements
    if ((mySelectedJunctions.size() > 0) || (mySelectedTAZs.size() > 0) || (mySelectedEdges.size() > 0) || myToStoppingPlace || myRoute) {
        // unblock undo/redo
        myFrameParent->getViewNet()->getViewParent()->getGNEAppWindows()->enableUndoRedo();
        // clear edges
        clearPath();
        // disable buttons
        myFinishCreationButton->disable();
        myAbortCreationButton->disable();
        myRemoveLastInsertedElement->disable();
        // update info route label
        updateInfoRouteLabel();
        // update view (to see the new route)
        myFrameParent->getViewNet()->updateViewNet();
    }
}


void
GNEPlanCreator::removeLastElement() {
    if (mySelectedEdges.size() > 1) {
        // remove special color of last selected edge
        mySelectedEdges.back()->resetCandidateFlags();
        // remove last edge
        mySelectedEdges.pop_back();
        // change last edge flag
        if ((mySelectedEdges.size() > 0) && mySelectedEdges.back()->isSourceCandidate()) {
            mySelectedEdges.back()->setSourceCandidate(false);
            mySelectedEdges.back()->setTargetCandidate(true);
        }
        // enable or disable remove last edge button
        if (mySelectedEdges.size() > 1) {
            myRemoveLastInsertedElement->enable();
        } else {
            myRemoveLastInsertedElement->disable();
        }
        // recalculate path
        recalculatePath();
        // update info route label
        updateInfoRouteLabel();
        // update view
        myFrameParent->getViewNet()->updateViewNet();
    }
}


long
GNEPlanCreator::onCmdCreatePath(FXObject*, FXSelector, void*) {
    // call create path
    return createPath(false);
}


long
GNEPlanCreator::onCmdUseLastRoute(FXObject*, FXSelector, void*) {
    // call create path with useLastRoute = true
    return createPath(true);
}

long
GNEPlanCreator::onUpdUseLastRoute(FXObject* sender, FXSelector, void*) {
    if ((myCreationMode & ROUTE) && myFrameParent->getViewNet()->getLastCreatedRoute()) {
        return sender->handle(this, FXSEL(SEL_COMMAND, ID_ENABLE), nullptr);
    } else {
        return sender->handle(this, FXSEL(SEL_COMMAND, ID_DISABLE), nullptr);
    }
}

long
GNEPlanCreator::onCmdAbortPathCreation(FXObject*, FXSelector, void*) {
    // just call abort path creation
    abortPathCreation();
    return 1;
}


long
GNEPlanCreator::onCmdRemoveLastElement(FXObject*, FXSelector, void*) {
    // just call remove last element
    removeLastElement();
    return 1;
}


void
GNEPlanCreator::updateInfoRouteLabel() {
    if (myPath.size() > 0) {
        // declare variables for route info
        double length = 0;
        double speed = 0;
        int pathSize = 0;
        for (const auto& path : myPath) {
            for (const auto& edge : path.getSubPath()) {
                length += edge->getNBEdge()->getLength();
                speed += edge->getNBEdge()->getSpeed();
            }
            pathSize += (int)path.getSubPath().size();
        }
        // declare ostringstream for label and fill it
        std::ostringstream information;
        information
                << TL("- Selected edges: ") << toString(mySelectedEdges.size()) << "\n"
                << TL("- Path edges: ") << toString(pathSize) << "\n"
                << TL("- Length: ") << toString(length) << "\n"
                << TL("- Average speed: ") << toString(speed / pathSize);
        // set new label
        myInfoRouteLabel->setText(information.str().c_str());
    } else {
        myInfoRouteLabel->setText(TL("No edges selected"));
    }
}


void
GNEPlanCreator::clearPath() {
    // clear junction, TAZs, edges, additionals and route
    mySelectedJunctions.clear();
    mySelectedTAZs.clear();
    mySelectedEdges.clear();
    myToStoppingPlace = nullptr;
    myRoute = nullptr;
    // clear path
    myPath.clear();
    // update info route label
    updateInfoRouteLabel();
}


void
GNEPlanCreator::recalculatePath() {
    // first clear path
    myPath.clear();
    // set edges
    std::vector<GNEEdge*> edges;
    // add route edges
    if (myRoute) {
        edges = myRoute->getParentEdges();
    } else {
        // add selected edges
        for (const auto& edge : mySelectedEdges) {
            edges.push_back(edge);
        }
        // add to stopping place edge
        if (myToStoppingPlace) {
            edges.push_back(myToStoppingPlace->getParentLanes().front()->getParentEdge());
        }
    }
    // fill paths
    if (edges.size() == 1) {
        myPath.push_back(PlanPath(myVClass, edges.front()));
    } else if (mySelectedJunctions.size() == 2) {
        // add path between two junctions
        myPath.push_back(PlanPath(myFrameParent->getViewNet(), myVClass, mySelectedJunctions.front(), mySelectedJunctions.back()));
    } else {
        // add every segment
        for (int i = 1; i < (int)edges.size(); i++) {
            myPath.push_back(PlanPath(myFrameParent->getViewNet(), myVClass, edges.at(i - 1), edges.at(i)));
        }
    }
}


void
GNEPlanCreator::setSpecialCandidates(GNEEdge* originEdge) {
    // first calculate reachability for pedestrians (we use it, because pedestran can walk in almost all edges)
    myFrameParent->getViewNet()->getNet()->getPathManager()->getPathCalculator()->calculateReachability(SVC_PEDESTRIAN, originEdge);
    // change flags
    for (const auto& edge : myFrameParent->getViewNet()->getNet()->getAttributeCarriers()->getEdges()) {
        for (const auto& lane : edge.second->getLanes()) {
            if (lane->getReachability() > 0) {
                lane->getParentEdge()->resetCandidateFlags();
                lane->getParentEdge()->setSpecialCandidate(true);
            }
        }
    }
}

void
GNEPlanCreator::setPossibleCandidates(GNEEdge* originEdge, const SUMOVehicleClass vClass) {
    // first calculate reachability for pedestrians
    myFrameParent->getViewNet()->getNet()->getPathManager()->getPathCalculator()->calculateReachability(vClass, originEdge);
    // change flags
    for (const auto& edge : myFrameParent->getViewNet()->getNet()->getAttributeCarriers()->getEdges()) {
        for (const auto& lane : edge.second->getLanes()) {
            if (lane->getReachability() > 0) {
                lane->getParentEdge()->resetCandidateFlags();
                lane->getParentEdge()->setPossibleCandidate(true);
            }
        }
    }
}

/****************************************************************************/
