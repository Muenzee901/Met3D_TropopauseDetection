/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2016-2018 Bianca Tost
**  Copyright 2015-2016 Christoph Heidelmann
**
**  Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  Met.3D is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Met.3D is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Met.3D.  If not, see <http://www.gnu.org/licenses/>.
**
*******************************************************************************/
#ifndef NWPACTORVARIABLE_H
#define NWPACTORVARIABLE_H

// standard library imports

// related third party imports
#include <GL/glew.h>
#include <QtCore>
#include <QtProperty>
#include <QSpinBox>

// local application imports
#include "gxfw/gl/texture.h"
#include "gxfw/synccontrol.h"
#include "gxfw/nwpactorvariableproperties.h"
#include "data/structuredgrid.h"
#include "data/weatherpredictiondatasource.h"
#include "data/singlevariableanalysis.h"
#include "data/gridaggregation.h"
#include "actors/transferfunction1d.h"
#include "actors/spatial1dtransferfunction.h"
#include "util/mstopwatch.h"
#include "data/verticalprofile.h"

#define MSTOPWATCH_ENABLED

namespace Met3D
{

class MNWPMultiVarActor;

/**
  @brief MNWPActorVariable represents an NWP variable for an @ref
  MNWPMultiVarActor. Properties such as data source and textures are
  stored, and synchronization functionality is provided (to sync with
  @ref MSyncControl).
  */
class MNWPActorVariable : public QObject, public MSynchronizedObject
{
    Q_OBJECT

public:
    /**
      Initializes this variable's QtPropertyBrowser properties. The properties
      are displayed as a subgroup of the parent actor @p actor.
     */
    MNWPActorVariable(MNWPMultiVarActor *actor);

    virtual ~MNWPActorVariable();

    /**
      Variable-specific initialization (OpenGL etc.). Called by
      @ref MNWPMultiVarActor::initialize().
     */
    virtual void initialize();

    MNWPMultiVarActor* getActor() { return actor; }

    /**
      Connect an analysis control to this variable.
     */
    void setSingleVariableAnalysisControl(
            MSingleVariableAnalysisControl *analysisControl)
    { this->singleVariableAnalysisControl = analysisControl; }

    /**
      Synchronize time, ensemble of this variable with the synchronization
      control @p sync.

      @see MSyncControl
     */
    void synchronizeWith(MSyncControl *sync, bool updateGUIProperties=true);

    bool synchronizationEvent(MSynchronizationType syncType, QVector<QVariant> data);

    /**
      Updates colour hints for synchronization (green property background
      for matching sync, red for not matching sync). If @p scene is specified,
      the colour hints are only updated for the specified scene (e.g. used
      when the variable's actor is added to a new scene).
     */
    void updateSyncPropertyColourHints(MSceneControl *scene = nullptr);

    void setPropertyColour(QtProperty *property, const QColor& colour,
                           bool resetColour, MSceneControl *scene = nullptr);

    /**
      Updates the current data field.
      */
    MDataRequestHelper constructAsynchronousDataRequest();

    virtual void asynchronousDataRequest(bool synchronizationRequest=false);

    /**
      Called by @ref MNWPMultiVarActor::onQtPropertyChanged(). Reacts to the
      variable-specific property signals.
     */
    virtual bool onQtPropertyChanged(QtProperty *property);

    virtual void saveConfiguration(QSettings *settings);

    virtual void loadConfiguration(QSettings *settings);

    bool setEnsembleMode(QString emName);

    bool setTransferFunction(QString tfName);

    /**
      setTransferFunctionToFirstAvailable sets used transfer function to first
      transfer function in transfer function list below 'None' if one is present.
     */
    void setTransferFunctionToFirstAvailable();

    /**
      Set @p true if the datafield's "flags" bitfield, if available, should
      be transferred to GPU memory.
      */
    void useFlags(bool b);

    /** Returns the current ensemble member setting of this variable. */
    int getEnsembleMember();

    /**
      Returns a property group labelled with @p name in the property browser.
      If the group does not exist, a new one is created.
     */
    QtProperty* getPropertyGroup(QString name);

    /**
      This function can be called by an @ref MRequestProperties instance
      to notify the actor variable of a property change that requires the
      data field to be reloaded. Set @p gridTopologyMayChange to @p true
      if the new data field may be on a different grid than the current one.
     */
    void triggerAsynchronousDataRequest(bool gridTopologyMayChange);

//TODO: Is there a more elegant way to notify property groups of changed
//      actor properties? (mr, 11Apr2014)
    /**
      The actor can call this function to notify the actor variable of a
      changed actor property (e.g. the isovalue of an isosurface).
     */
    void actorPropertyChangeEvent(MPropertyType::ChangeNotification ptype,
                                  void *value);

    /**
      Returns true if the variable contains data that can be rendered.
     */
    bool hasData();

    QString debugOutputAsString();

    /* Data loader and variable specifications. */
    QString                       dataSourceID;
    MWeatherPredictionDataSource *dataSource;
    MVerticalLevelType            levelType;
    QString                       variableName;
    int                           numEnsembleMembers;

    /* CPU memory object that stores the current data field. */
    MStructuredGrid *grid;

    /**
      Special case for ensemble mode "multiple members": This NWPActorVariable
      keeps its own instance of a grid aggregation source that assembles the
      required set of multiple ensemble member grids.
     */
    //TODO (mr, 18Apr2018) -- This architecture should probably be changed to
    //  aggregation sources in the generic pipelines. May change in the future.
    MGridAggregationDataSource *aggregationDataSource;
    MGridAggregation *gridAggregation;
    bool multipleEnsembleMembersEnabled;

    /* OpenGL GPU texture objects and units that belong to this variable. */
    GL::MTexture *textureDataField;
    int          textureUnitDataField;
    GL::MTexture *textureLonLatLevAxes;
    int          textureUnitLonLatLevAxes;
    GL::MTexture *textureSurfacePressure;        // grids on hyb-sig-pres lev
    int          textureUnitSurfacePressure;
    GL::MTexture *textureHybridCoefficients;     // grids on hyb-sig-pres lev
    int          textureUnitHybridCoefficients;
    GL::MTexture *textureDataFlags;
    int          textureUnitDataFlags;
    GL::MTexture *texturePressureTexCoordTable;  // grids on PL & ML lev
    int          textureUnitPressureTexCoordTable;
    GL::MTexture *textureAuxiliaryPressure;        // grids on hyb-sig-pres lev
    int          textureUnitAuxiliaryPressure;

    /* Dummy textures that can be bound to texture units not used for the
       current grid (i.e. bind these to textureHybridCoefficients if grids
       other than hybrid sigma pressure are used. */
    GL::MTexture *textureDummy1D;
    GL::MTexture *textureDummy2D;
    GL::MTexture *textureDummy3D;
    int          textureUnitUnusedTextures;

    /* TransferFunction attached to this variable. */
    MTransferFunction1D *transferFunction;
    int                 textureUnitTransferFunction;

    /* Property group to accomodate this variable's subproperties. */
    QtProperty *varPropertyGroup;
    QtProperty *varRenderingPropertyGroup;

    /* Buttons to remove/change this variable from its actor. Signal is handled
    by MNWPMultiVarActor and derived classes. */
    QtProperty *removeVariableProperty;
    QtProperty *changeVariableProperty;

public slots:
    /**
      Set the current forecast valid time and update the scene.
      */
    bool setValidDateTime(const QDateTime& datetime);

    /**
      Set the current forecast init time and update the scene.
      */
    bool setInitDateTime(const QDateTime& datetime);

    /**
     Programatically change the current ensemble member. If @p member is -1,
     the ensemble mode will be changed to "mean".
     */
    bool setEnsembleMember(int member);

    /**
     Connects to the MGLResourcesManager::actorCreated() signal. If the new
     actor is a transfer function, it is added to the list of transfer
     functions displayed by the transferFunctionProperty.
     */
    void onActorCreated(MActor *actor);

    /**
     Connects to the MGLResourcesManager::actorDeleted() signal. If the deleted
     actor is a transfer function, update the list of transfer functions
     displayed by the transferFunctionProperty, possibly disconnect from the
     transfer function.
     */
    void onActorDeleted(MActor *actor);

    /**
     Connects to the MGLResourcesManager::actorRenamed() signal. If the renamed
     actor is a transfer function, it is renamed in the list of transfer
     functions displayed by the transferFunctionProperty.
     */
    void onActorRenamed(MActor *actor, QString oldName);

    void asynchronousDataAvailable(MDataRequest request);

protected:
    friend class MNWPVolumeRaycasterActor;
    friend class MVerticalRegridProperties;
    friend class MSkewTActor;
    friend class MNWPHorizontalSectionActor;
    friend class MFrontDetectionActor;
    friend class MTropopauseDetectionActor;

    virtual void releaseDataItems();
    virtual void releaseAggregatedDataItems();

    /**
      Determine the current time value of the given enum property.
     */
    QDateTime getPropertyTime(QtProperty *enumProperty);

    /**
     Update the init time property (init time refers to the base time of the
     forecast) from the current data source.
     */
    void updateInitTimeProperty();

    /**
     Update the valid time property from the current init time and the current
     data source.
     */
    void updateValidTimeProperty();

    void updateTimeProperties();

    void initEnsembleProperties();

    void updateEnsembleProperties();

    /**
      Updates the list of available members in @p ensembleSingleMemberProperty.
      Returns @p true if the displayed member has changed (because the
      previously displayed member is not available anymore).
     */
    bool updateEnsembleSingleMemberProperty();

    /**
      This function is called whenever a new data field has been made current.
      Override it in derived classes to react to changing data fields.
      */
    virtual void dataFieldChangedEvent() { }

    /**
      This function is called from asynchronousDataAvailable() to allow
      derived classes to perform operations on a received grid.
     */
    virtual void asynchronousDataAvailableEvent(MStructuredGrid *grid)
    { Q_UNUSED(grid); }

    virtual bool setTransferFunctionFromProperty();

    /** Actor that this instance belongs to. */
    MNWPMultiVarActor *actor;

    /** Analysis control this variable is connected to. */
    MSingleVariableAnalysisControl *singleVariableAnalysisControl;

    /* Data statistics properties */
    QtProperty *dataStatisticsPropertyGroup;
    QtProperty *showDataStatisticsProperty;
    QtProperty *significantDigitsProperty;
    QtProperty *histogramDisplayModeProperty;

    /* Synchronization properties */
    QtProperty *synchronizationPropertyGroup;
    QtProperty *synchronizationProperty;
    bool        synchronizeInitTime;
    QtProperty *synchronizeInitTimeProperty;
    bool        synchronizeValidTime;
    QtProperty *synchronizeValidTimeProperty;
    bool        synchronizeEnsemble;
    QtProperty *synchronizeEnsembleProperty;

    /* Time management. */
    QList<QDateTime> availableValidTimes;
    QList<QDateTime> availableInitTimes;
    QtProperty *initTimeProperty;
    QtProperty *validTimeProperty;

    /* Ensemble management. */
    QtProperty *ensembleModeProperty;
    QtProperty *ensembleSingleMemberProperty;
    QtProperty *ensembleMultiMemberSelectionProperty;
    QtProperty *ensembleMultiMemberProperty;
    QtProperty *ensembleThresholdProperty;
    QString     ensembleFilterOperation;
    QSet<unsigned int> selectedEnsembleMembers;
    QList<unsigned int> selectedEnsembleMembersAsSortedList;
    int         ensembleMemberLoadedFromConfiguration;

    /* Debug properties. */
    QtProperty *dumpGridDataProperty;

    /** If true, load the grid's "flag" data field to the GPU, if available. */
    bool useFlagsIfAvailable;

    /**
     Opens a data source selection dialog. If the user selected a new variable,
     @ref initialize() is called to load the new data field.
     */
    virtual bool changeVariable();

    QtProperty *datasourceNameProperty;
    QtProperty *variableLongNameProperty;
    QtProperty *changeVariablePropertyGroup;

    QtProperty *transferFunctionProperty;
    QtProperty *spatialTransferFunctionProperty;

    /** Data request information. */
    QSet<MDataRequest> pendingRequests; // to quickly decide whether to accept a request
    struct MRequestQueueInfo
    {
        MDataRequest request;
        bool available;
#ifdef DIRECT_SYNCHRONIZATION
        bool syncchronizationRequest;
#endif
    };
    QQueue<MRequestQueueInfo> pendingRequestsQueue; // to ensure correct request order

    /** Stopwatches to monitor time required to execute data requests. */
#ifdef MSTOPWATCH_ENABLED
    QHash<MDataRequest, MStopwatch*> stopwatches;
#endif

    /** Evaluated in 2D section dataFieldChangedEvent() to update target grid
    and render region parameters after a variable change. */
    bool gridTopologyMayHaveChanged;

    QHash<QString, QtProperty*>  propertySubGroups;
    MRequestPropertiesFactory   *requestPropertiesFactory;
    QList<MRequestProperties*>   propertiesList;

private:
    /**
      Internal function containing common code for @ref setValidDateTime() and
      @ref setInitDateTime().

      Returns true if a new datetime is set in @p timeProperty.
      */
    bool internalSetDateTime(const QList<QDateTime>& availableTimes,
                             const QDateTime& datetime,
                             QtProperty* timeProperty);

    void runStatisticalAnalysis(double significantDigits,
                                int histogramDisplayMode);

    bool suppressUpdate;
};



/**
  @brief Properties specific to 2D cross sections (properties specific to
  horizontal, vertical etc. sections in derived classes).
  */
class MNWP2DSectionActorVariable : public MNWPActorVariable
{
public:
    MNWP2DSectionActorVariable(MNWPMultiVarActor *actor);

    virtual ~MNWP2DSectionActorVariable();

    void initialize() override;

    bool onQtPropertyChanged(QtProperty *property) override;

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

    MRegularLonLatGrid *targetGrid2D;
    GL::MTexture        *textureTargetGrid;
    int                 textureUnitTargetGrid;
    int                 imageUnitTargetGrid;

    struct ContourSettings
    {

        ContourSettings(MActor *actor = nullptr,
                        const uint8_t index = 0,
                        bool enabled = true, double thickness = 1.5,
                        bool useTF = false,
                        QColor colour = QColor(0, 0, 0, 255),
                        bool labelsEnabled = false,
                        QString levelsString = "");

        bool            enabled;
        QVector<double> levels;
        double          thickness;
        bool            useTF;
        QColor          colour;
        bool            labelsEnabled;
        int             startIndex;
        int             stopIndex;

        QtProperty *groupProperty;
        QtProperty *enabledProperty;
        QtProperty *levelsProperty;
        QtProperty *thicknessProperty;
        QtProperty *useTFProperty;
        QtProperty *colourProperty;
        QtProperty *labelsEnabledProperty;
        QtProperty *removeProperty;
    };

    /**
      Indicator whether to render contour labels. It is incremented if user
      enables labels for a contour set and decremented if user disables labels
      for a contour set. If renderContourLabel equals 0, no labels should be
      drawn.
      */
    int renderContourLabels;

    void addContourSet(const bool enabled = true,
            const double thickness = 1.5, const bool useTF = false,
            const QColor colour = QColor(0, 0, 0, 255),
            const bool labelsEnabled = false, QString levelString = "");

    bool removeContourSet(int index);

    struct RenderMode
    {
        RenderMode() {}
        enum Type
        {
            Invalid = -1,
            Disabled = 0,
            FilledContours = 1,
            PseudoColour = 2,
            LineContours = 3,
            FilledAndLineContours = 4,
            PseudoColourAndLineContours = 5,
            TexturedContours = 6,
            FilledAndTexturedContours = 7,
            LineAndTexturedContours = 8,
            PseudoColourAndTexturedContours = 9,
            FilledAndLineAndTexturedContours = 10,
            PseudoColourAndLineAndTexturedContours = 11
        };
    };

    void setRenderMode(RenderMode::Type mode);

    /** Returns the name of the given render mode as QString. */
    QString renderModeToString(RenderMode::Type renderMode);    

    /** Returns enum associated with the given name. Returns Invalid if no mode
        exists with the given name. */
    virtual RenderMode::Type stringToRenderMode(QString renderModeName);

protected:
    /**
      Parses the string @p cLevelStr for contour level definitions. The string
      can either define a range of values as "[from,to,step]", e.g.
      "[0,100,10]" or "[0.5,10,0.5]", or a list of values as
      "val1,val2,val3,...", e.g. "1,2,3,4,5" or "0,0.5,1,1.5,5,10". If either
      of the two formats was successfully parsed, the extracted values are
      written to @p contours and @p true is returned.
     */
    bool parseContourLevelString(QString cLevelStr,
                                 ContourSettings *contours);

    /**
      Called when the contour values are updated.
     */
    virtual void contourValuesUpdateEvent(
            ContourSettings *levels) { Q_UNUSED(levels) }

    virtual void updateContourLabels() { }

    struct RenderSettings
    {
        RenderSettings() {}

        RenderMode::Type renderMode;

        bool contoursUseTF;

        QtProperty *groupProperty;
        QtProperty *renderModeProperty;
        QtProperty *addContourSetProperty;
        QtProperty *contourSetGroupProperty;
        QtProperty *contoursUseTFProperty;
    };

    RenderSettings renderSettings;    
    QVector<ContourSettings> contourSetList;

private:
    QtProperty *saveXSecGridProperty;

};



/**
  @brief Variable properties specific to horizontal sections.
 */
class MNWP2DHorizontalActorVariable : public MNWP2DSectionActorVariable
{
public:
    MNWP2DHorizontalActorVariable(MNWPMultiVarActor *actor);

    ~MNWP2DHorizontalActorVariable();

    void initialize() override;

    bool onQtPropertyChanged(QtProperty *property) override;

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

    /**
     Computes start indices (member variables) @ref i0, @ref j0 and number of
     grid points @ref nlons, @ref nlats that are required to render the part of
     the data grid (or multiple parts if the bounding box is larger than 360deg
     in longitude) that fits into the bounding box given by the corner
     coordinates @p llcrnrlon, @p llcrnrlatm @p urcrnrlon, @p urcrnrlat.

     @note If the data region falls apart into disjoint parts OR if it is
     rendered multiple times only ONE VISUALIZATION GRID is rendered (but with
     no vertices in places where there are no grid points). Those fragments
     that don't map to available data points are discarded in the fragment
     shader.

     Correctly handles the following cases:
       *  bbox is smaller than data grid --> part of data grid is rendered
       *  bbox is larger than data grid --> region of bbox not occupied by
          data grid is empty
       *  bbox is larger than 360deg in lon so that parts of data grid appear
          multiple times --> data grid is repeated
       *  bbox is placed such that data region is cut into disjunct regions
          --> disjuct regions are rendered
     */
    void computeRenderRegionParameters(double llcrnrlon, double llcrnrlat,
                                       double urcrnrlon, double urcrnrlat);

    void updateContourIndicesFromTargetGrid(float slicePosition_hPa,
            ContourSettings *contourSet = nullptr);

    /**
      Returns the contour labels of each variable that can be rendered on the screen.
      @p noOverlapping determines if these labels may overlap. Otherwise the pixel
      size of each label is computed according to the current @p sceneView and
      overlap tests are performed.
     */
    QList<MLabel*> getContourLabels(bool noOverlapping = false,
                                    MSceneViewGLWidget* sceneView = nullptr);

    RenderMode::Type stringToRenderMode(QString renderModeName) override;

    bool setSpatialTransferFunction(QString stfName);

    /* SpatialTransferFunction attached to this variable. */
    MSpatial1DTransferFunction *spatialTransferFunction;
    int                         textureUnitSpatialTransferFunction;

protected:
    friend class MNWPHorizontalSectionActor;
    friend class MNWPSurfaceTopographyActor;

    void dataFieldChangedEvent() override;

    void contourValuesUpdateEvent(ContourSettings *levels) override;

    /** Render region parameters; stores the index range of the data grid
        that is rendered for the current bbox. */
    unsigned int i0, j0;
    int nlons, nlats;

    /** Current bbox that is rendered. */
    double llcrnrlon;
    double llcrnrlat;
    double urcrnrlon;
    double urcrnrlat;

    /**
     Contains signed distance from left border of grid to left border of
     bounding box in a multiple of 360 and needs to be added to vertex position
     during rendering to position grid correctly.

     During rendering the grid is placed correctly to its given coordinates but
     if the bounding box starts outside this region (distance between western
     border of bounding box and western border of grid greater than 360° in
     western or eastern direction), the placement does not work correctly
     anymore. Thus we need to add a inital shift to the world space coordinate
     of a vertex during rendering.
    */
    float shiftForWesternLon;

    /** Properties of the contour labels */
    QtProperty* contourLabelSuffixProperty;
    QString contourLabelSuffix;

    /**
     Represents all collected contour labels as text labels
    */
    QList<MLabel*> contourLabels;

    /**
      Searches for potential iso-label position and determines which labels are
      drawn.
     */
    void updateContourLabels() override;

    /**
     Checks if a given cell contains an isoline for the given isovalue @p iso.
     */
    bool isoLineInGridCell(const MRegularLonLatGrid* grid, const int jl,
                           const int il, const int jr, const int ir,
                           const float iso);

    /**
      Adds a new contour label (as MLabel) to the contour label list.
     */
    void addNewContourLabel(const QVector3D& posPrev, const QVector3D& posNext,
                            const float isoPrev, const float isoNext,
                            const float isoValue, const int index);

    /**
     Samples the grid at grid cell lat/lon and searches for a new contour
     label.
     */
    void checkGridForContourLabel(const MRegularLonLatGrid* grid,
                                  const int lat, const int lon,
                                  const int deltaLat, const int deltaLon,
                                  const float isoValue, const int index);

private:

    bool setSpatialTransferFunctionFromProperty();
};



/**
  @brief Variable properties specific to vertical sections.
 */
class MNWP2DVerticalActorVariable : public MNWP2DSectionActorVariable
{
public:
    MNWP2DVerticalActorVariable(MNWPMultiVarActor *actor);

protected:
    friend class MNWPVerticalSectionActor;

    void dataFieldChangedEvent() override;

    void contourValuesUpdateEvent(ContourSettings *levels) override;

    /**
     Compute the vertical levels that need to be rendered to cover the range
     p_bot .. p_top. The computed bounds are used to discard non-visible levels
     in @ref M_MultiVar_ML_VSec_Actor::renderToCurrentContext().
     */
    void updateVerticalLevelRange(double p_bot_hPa, double p_top_hPa);

    double p_bot_hPa;
    double p_top_hPa;

    /* Range of vertical levels that is considered for rendering. */
    int gridVerticalLevelStart;
    int gridVerticalLevelCount;
};



/**
  @brief Variable properties specific to volume rendering.
 */
class MNWP3DVolumeActorVariable : public MNWPActorVariable
{
public:
    MNWP3DVolumeActorVariable(MNWPMultiVarActor *actor);

    ~MNWP3DVolumeActorVariable();

    void initialize() override;

    void asynchronousDataAvailableEvent(MStructuredGrid *grid) override;

    void releaseDataItems() override;

    GL::MTexture *textureMinMaxAccelStructure;   // 3D grids for raycasting
    int          textureUnitMinMaxAccelStructure;

protected:
    bool setTransferFunctionFromProperty() override;

private:
};



/**
  @brief Variable properties specific to skew-t-diagram.
 */
class MNWPSkewTActorVariable : public MNWPActorVariable
{
public:
    MNWPSkewTActorVariable(MNWPMultiVarActor *actor);

    ~MNWPSkewTActorVariable();

    bool onQtPropertyChanged(QtProperty *property) override;

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

protected:
    friend class MSkewTActor;

    void dataFieldChangedEvent() override;

    /* Rendering properties. **/
    QColor profileColour;
    QtProperty *profileColourProperty;
    double lineThickness;
    QtProperty *lineThicknessProperty;

    /* Profile data (CPU and vertex buffer). */
    MVerticalProfile profile;
    GL::MVertexBuffer *profileVertexBuffer;

    QMap<int, MVerticalProfile> profileAggregation;
    QMap<int, GL::MVertexBuffer*> profileVertexBufferAggregation;

    void updateProfile(QVector2D lonLatLocation);
};

} // namespace Met3D

#endif // NWPACTORVARIABLE_H
