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
/// @file    GNEPlanCreator.h
/// @author  Pablo Alvarez Lopez
/// @date    Mar 2022
///
// Frame for create paths
/****************************************************************************/
#pragma once
#include <config.h>

#include <utils/foxtools/MFXGroupBoxModule.h>

// ===========================================================================
// class declaration
// ===========================================================================

class GNEFrame;

// ===========================================================================
// class definitions
// ===========================================================================

class GNEPlanCreator : public MFXGroupBoxModule {
    /// @brief FOX-declaration
    FXDECLARE(GNEPlanCreator)

public:
    /// @brief class for path
    class PlanPath {

    public:
        /// @brief constructor for single edge
        PlanPath(const SUMOVehicleClass vClass, GNEEdge* edge);

        /// @brief constructor for two edges
        PlanPath(GNEViewNet* viewNet, const SUMOVehicleClass vClass, GNEEdge* edgeFrom, GNEEdge* edgeTo);

        /// @brief constructor for two junctions
        PlanPath(GNEViewNet* viewNet, const SUMOVehicleClass vClass, GNEJunction* junctionFrom, GNEJunction* junctionTo);

        /// @brief get sub path
        const std::vector<GNEEdge*>& getSubPath() const;

        /// @brief check if current path is conflict due vClass
        bool isConflictVClass() const;

        /// @brief check if current path is conflict due is disconnected
        bool isConflictDisconnected() const;

    protected:
        /// @brief sub path
        std::vector<GNEEdge*> mySubPath;

        /// @brief flag to mark this path as conflicted
        bool myConflictVClass;

        /// @brief flag to mark this path as disconnected
        bool myConflictDisconnected;

    private:
        /// @brief default constructor
        PlanPath();

        /// @brief Invalidated copy constructor.
        PlanPath(PlanPath*) = delete;

        /// @brief Invalidated assignment operator.
        PlanPath& operator=(PlanPath*) = delete;
    };

    /// @brief default constructor
    GNEPlanCreator(GNEFrame* frameParent);

    /// @brief destructor
    ~GNEPlanCreator();

    /// @brief show GNEPlanCreator for the given tag
    void showPathCreatorModule(SumoXMLTag element, const bool firstElement, const bool consecutives);

    /// @brief show GNEPlanCreator
    void hidePathCreatorModule();

    /// @brief get vClass
    SUMOVehicleClass getVClass() const;

    /// @brief set vClass
    void setVClass(SUMOVehicleClass vClass);

    /// @brief add junction
    bool addJunction(GNEJunction* junction);

    /// @brief add TAZ
    bool addTAZ(GNEAdditional* taz);

    /// @brief add edge
    bool addEdge(GNEEdge* edge);

    /// @brief get current selected edges
    const std::vector<GNEEdge*>& getSelectedEdges() const;

    /// @brief get current selected junctions
    const std::vector<GNEJunction*>& getSelectedJunctions() const;
    
    /// @brief get current selected TAZs
    const std::vector<GNEAdditional*>& getSelectedTAZs() const;
    
    /// @brief add stoppingPlace
    bool addStoppingPlace(GNEAdditional* stoppingPlace);

    /// @brief get to stoppingPlace
    GNEAdditional* getToStoppingPlace(SumoXMLTag expectedTag) const;

    /// @brief add route
    bool addRoute(GNEDemandElement* route);

    /// @brief get route
    GNEDemandElement* getRoute() const;

    /// @brief get path route
    const std::vector<PlanPath>& getPath() const;

    /// @brief draw temporal route
    void drawTemporalRoute(const GUIVisualizationSettings& s) const;

    /// @brief create path
    bool createPath(const bool useLastRoute);

    /// @brief abort path creation
    void abortPathCreation();

    /// @brief remove path element
    void removeLastElement();

    /// @name FOX-callbacks
    /// @{
    /// @brief Called when the user click over button "Finish route creation"
    long onCmdCreatePath(FXObject*, FXSelector, void*);

    /// @brief Called when the user click over button "Use last route"
    long onCmdUseLastRoute(FXObject*, FXSelector, void*);

    /// @brief Called when update button "Use last route"
    long onUpdUseLastRoute(FXObject*, FXSelector, void*);

    /// @brief Called when the user click over button "Abort route creation"
    long onCmdAbortPathCreation(FXObject*, FXSelector, void*);

    /// @brief Called when the user click over button "Remove las inserted edge"
    long onCmdRemoveLastElement(FXObject*, FXSelector, void*);
    /// @}

protected:
    FOX_CONSTRUCTOR(GNEPlanCreator)

    // @brief creation mode
    enum Mode {
        CONSECUTIVE_EDGES        = 1 << 0,   // Path's edges are consecutives
        NONCONSECUTIVE_EDGES     = 1 << 1,   // Path's edges aren't consecutives
        START_EDGE               = 1 << 2,   // Path begins in edge
        END_EDGE                 = 1 << 3,   // Path ends in edge
        START_JUNCTION           = 1 << 4,   // Path begins in junction
        END_JUNCTION             = 1 << 5,   // Path ends in junction
        START_TAZ                = 1 << 6,   // Path begins in TAZ
        END_TAZ                  = 1 << 7,   // Path ends in TAZ
        STOP                     = 1 << 8,   // Path is stop
        ONLY_FROMTO              = 1 << 9,   // Path only had two elements (first and last)
        END_BUSSTOP              = 1 << 10,  // Path ends in a busStop
        END_TRAINSTOP            = 1 << 11,  // Path ends in a trainStop
        END_CONTAINERSTOP        = 1 << 12,  // Path ends in a containerStop
        ROUTE                    = 1 << 13,  // Path uses a route
        REQUIRE_FIRSTELEMENT     = 1 << 14,  // Path start always in a previous element
        SHOW_CANDIDATE_EDGES     = 1 << 15,  // Show candidate edges
        SHOW_CANDIDATE_JUNCTIONS = 1 << 16,  // show candidate junctions
    };

    /// @brief update InfoRouteLabel
    void updateInfoRouteLabel();

    /// @brief clear edges
    void clearPath();

    /// @brief recalculate path
    void recalculatePath();

    /// @brief set special candidates (This function will be called recursively)
    void setSpecialCandidates(GNEEdge* originEdge);

    /// @brief set edgereachability (This function will be called recursively)
    void setPossibleCandidates(GNEEdge* originEdge, const SUMOVehicleClass vClass);

    /// @brief current frame parent
    GNEFrame* myFrameParent;

    /// @brief current vClass
    SUMOVehicleClass myVClass;

    /// @brief current creation mode
    int myCreationMode;

    /// @brief vector with selected junctions
    std::vector<GNEJunction*> mySelectedJunctions;

    /// @brief vector with selected TAZs
    std::vector<GNEAdditional*> mySelectedTAZs;

    /// @brief vector with selected edges
    std::vector<GNEEdge*> mySelectedEdges;

    /// @brief to additional (usually a busStop)
    GNEAdditional* myToStoppingPlace;

    /// @brief route (usually a busStop)
    GNEDemandElement* myRoute;

    /// @brief vector with current path
    std::vector<PlanPath> myPath;

    /// @brief label with route info
    FXLabel* myInfoRouteLabel;

    /// @brief button for use last inserted route
    FXButton* myUseLastRoute;

    /// @brief button for finish route creation
    FXButton* myFinishCreationButton;

    /// @brief button for abort route creation
    FXButton* myAbortCreationButton;

    /// @brief button for removing last inserted element
    FXButton* myRemoveLastInsertedElement;

    /// @brief label for backSpace information
    FXLabel* myBackSpaceLabel;
private:
    /// @brief Invalidated copy constructor.
    GNEPlanCreator(GNEPlanCreator*) = delete;

    /// @brief Invalidated assignment operator.
    GNEPlanCreator& operator=(GNEPlanCreator*) = delete;
};
