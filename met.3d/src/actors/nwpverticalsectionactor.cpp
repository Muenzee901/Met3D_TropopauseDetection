/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2021 Marc Rautenhaus [*, previously +]
**  Copyright 2016-2018 Bianca Tost [+]
**  Copyright 2017      Philipp Kaiser [+]
**
**  * Regional Computing Center, Visual Data Analysis Group
**  Universitaet Hamburg, Hamburg, Germany
**
**  + Computer Graphics and Visualization Group
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
#include "nwpverticalsectionactor.h"

// standard library imports
#include <iostream>
#include <math.h>

// related third party imports
#include <QObject>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "util/mexception.h"
#include "gxfw/mglresourcesmanager.h"
#include "gxfw/msceneviewglwidget.h"
#include "gxfw/selectdatasourcedialog.h"
#include "data/structuredgrid.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MNWPVerticalSectionActor::MNWPVerticalSectionActor()
    : MNWPMultiVarActor(),
      MBoundingBoxInterface(this, MBoundingBoxConnectionType::VERTICAL),
      labelDistance(1),
      waypointsModel(nullptr),
      modifyWaypoint(-1),
      modifyWaypoint_worldZ(0.),
      textureVerticalSectionPath(nullptr),
      textureUnitVerticalSectionPath(-1),
      texturePressureLevels(nullptr),
      textureUnitPressureLevels(-1),
      vbVerticalWaypointLines(nullptr),
      numVerticesVerticalWaypointLines(0),
      vbInteractionHandlePositions(nullptr),
      numInteractionHandlePositions(0),
      p_top_hPa(100.),
      p_bot_hPa(1050.),
      opacity(1.),
      interpolationNodeSpacing(0.15),
      updatePath(false),
      offsetPickPositionToHandleCentre(QVector2D(0., 0.))
{
    enablePicking(true);

    // Create and initialise QtProperties for the GUI.
    // ===============================================
    beginInitialiseQtProperties();

    setActorType(staticActorType());
    setName(getActorType());


    labelDistanceProperty = addProperty(INT_PROPERTY, "distance (in tick marks)",
                                         labelPropertiesSupGroup);
    properties->mInt()->setValue(labelDistanceProperty, labelDistance);
    properties->mInt()->setMinimum(labelDistanceProperty, 0);
    labelDistanceProperty->setToolTip("Depends on order in pressure levels list.");

    QStringList waypointsList =  MSystemManagerAndControl::getInstance()
            ->getWaypointsModelsIdentifiers();
    waypointsModelProperty = addProperty(ENUM_PROPERTY, "waypoints model",
                                         actorPropertiesSupGroup);
    // Set default waypoints to second entry if there exists a second entry.
    properties->mEnum()->setEnumNames(waypointsModelProperty, waypointsList);
    if (waypointsList.size() > 1)
    {
        enableActorUpdates(false);
        properties->setEnumItem(waypointsModelProperty, waypointsList.at(1));
        setWaypointsModel(MSystemManagerAndControl::getInstance()
                          ->getWaypointsModel(waypointsList.at(1)));
        enableActorUpdates(true);
    }

    // Bounding box of the actor.
    insertBoundingBoxProperty(actorPropertiesSupGroup);

    QString defaultPressureLineLevel = QString("1000.,900.,800.,700.,600.,500.")
                                       + QString(",400.,300.,200.,100.,90.,80.")
                                       + QString(",70.,60.,50.,40.,30.,20.");

    pressureLineLevelsProperty = addProperty(STRING_PROPERTY, "pressure levels",
                                                actorPropertiesSupGroup);
    properties->mString()->setValue(pressureLineLevelsProperty,
                                    defaultPressureLineLevel);

    selectedPressureLineLevels = parseFloatRangeString(defaultPressureLineLevel);

    opacityProperty = addProperty(DECORATEDDOUBLE_PROPERTY, "opacity",
                                  actorPropertiesSupGroup);
    properties->setDDouble(opacityProperty, opacity, 0, 1, 2, 0.05, " (0-1)");

    interpolationNodeSpacingProperty = addProperty(
                DECORATEDDOUBLE_PROPERTY, "interpolation node spacing",
                actorPropertiesSupGroup);
    properties->setDDouble(
                interpolationNodeSpacingProperty, interpolationNodeSpacing,
                0.000001, 180, 6, 0.05, " (degrees)");

    endInitialiseQtProperties();
}


MNWPVerticalSectionActor::~MNWPVerticalSectionActor()
{
    if (textureUnitVerticalSectionPath >=0)
        releaseTextureUnit(textureUnitVerticalSectionPath);
    if (textureUnitPressureLevels >=0)
        releaseTextureUnit(textureUnitPressureLevels);

    if (vbVerticalWaypointLines) delete vbVerticalWaypointLines;
    if (vbInteractionHandlePositions) delete vbInteractionHandlePositions;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

#define SHADER_VERTEX_ATTRIBUTE 0

void MNWPVerticalSectionActor::reloadShaderEffects()
{
    LOG4CPLUS_DEBUG(mlog, "loading shader programs" << flush);

    beginCompileShaders(5);

    compileShadersFromFileWithProgressDialog(
                sectionGridShader,
                "src/glsl/vsec_interpolation_filledcontours.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                marchingSquaresShader,
                "src/glsl/vsec_marching_squares.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                pressureLinesShader,
                "src/glsl/vsec_pressureisolines.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                simpleGeometryShader,
                "src/glsl/simple_coloured_geometry.fx.glsl");
    compileShadersFromFileWithProgressDialog(
                positionSpheresShader,
                "src/glsl/trajectory_positions.fx.glsl");

    endCompileShaders();
}


void MNWPVerticalSectionActor::saveConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::saveConfiguration(settings);

    settings->beginGroup(MNWPVerticalSectionActor::getSettingsID());

    settings->setValue("labelDistance", labelDistance);

    if (waypointsModel != nullptr)
    {
        settings->setValue("waypointsModelID", waypointsModel->getID());
    }

    MBoundingBoxInterface::saveConfiguration(settings);

    settings->setValue("pressureLevels",
                       properties->mString()->value(pressureLineLevelsProperty));
    settings->setValue("opacity", opacity);
    settings->setValue("interpolationNodeSpacing", interpolationNodeSpacing);

    settings->endGroup();
}


void MNWPVerticalSectionActor::loadConfiguration(QSettings *settings)
{
    MNWPMultiVarActor::loadConfiguration(settings);

    settings->beginGroup(MNWPVerticalSectionActor::getSettingsID());

    labelDistance = settings->value("labelDistance", 1).toInt();
    properties->mInt()->setValue(labelDistanceProperty, labelDistance);

    QString wpID = settings->value("waypointsModelID").toString();
    setWaypointsModel(MSystemManagerAndControl::getInstance()
                      ->getWaypointsModel(wpID));

    MBoundingBoxInterface::loadConfiguration(settings);

    QString defaultPressureLineLevel = QString("1000.,900.,800.,700.,600.,500.")
                                       + QString(",400.,300.,200.,100.,90.,80.")
                                       + QString(",70.,60.,50.,40.,30.,20.");
    const QString pressureLevels =
            settings->value("pressureLevels",
                            defaultPressureLineLevel).toString();
    properties->mString()->setValue(pressureLineLevelsProperty,
                                    pressureLevels);
    properties->mDDouble()->setValue(
                opacityProperty,
                settings->value("opacity").toFloat());
    properties->mDDouble()->setValue(
                interpolationNodeSpacingProperty,
                settings->value("interpolationNodeSpacing", 0.15).toFloat());

    settings->endGroup();

    if (this->isInitialized())
    {
        generateIsoPressureLines();
    }
}


int MNWPVerticalSectionActor::checkIntersectionWithHandle(
        MSceneViewGLWidget *sceneView,
        float clipX, float clipY)
{
    // See notes 22-23Feb2012 and 21Nov2012.

    if (waypointsModel == nullptr) return -1;

    float clipRadius = MSystemManagerAndControl::getInstance()->getHandleSize();

    // cout << "checkIntersection(" << clipX << ", " << clipY << " / "
    //      << clipRadius << ")\n" << flush;

    // NOTE: This function considers both waypoints and midpoints between the
    // waypoints. If the user drags a midpoint, both adjacent waypoints are
    // moved. i.e. the entire segment.

    // Default: No waypoint has been touched by the mouse. Note: This instance
    // variable is used in renderToCurrentContext; if it is >= 0 the waypoint
    // with the corresponding index is highlighted.
    modifyWaypoint = -1;

    // Loop over all way-/midpoints and check whether the mouse cursor is inside
    // a circle with radius "clipRadius" around the waypoint (in clip space).
    for (int i = 0; i < waypointsModel->sizeIncludingMidpoints(); i++)
    {
        // Transform the waypoint coordinates to clip space. As only lat/lon
        // of the waypoint is stored, assume a worldZ = 0.
        QVector3D wpPositionBottom = QVector3D(
                waypointsModel->positionLonLatIncludingMidpoints(i),
                sceneView->worldZfromPressure(p_bot_hPa));
        QVector3D wpPositionTop = QVector3D(
                waypointsModel->positionLonLatIncludingMidpoints(i),
                sceneView->worldZfromPressure(p_top_hPa));

        // Obtain the camera position and the view direction
        const QVector3D& cameraPos = sceneView->getCamera()->getOrigin();
        QVector3D viewDirBot = wpPositionBottom - cameraPos;
        QVector3D viewDirTop = wpPositionTop - cameraPos;

        // Scale the radius (in world space) with respect to the viewer distance
        float radiusBot = static_cast<float>(clipRadius * viewDirBot.length() / 100.0);
        float radiusTop = static_cast<float>(clipRadius * viewDirTop.length() / 100.0);

        QMatrix4x4 *mvpMatrixInverted =
                sceneView->getModelViewProjectionMatrixInverted();
        // Compute the world position of the current mouse position
        QVector3D mouseWorldPos = *mvpMatrixInverted * QVector3D(clipX, clipY, 1);

        // Get the ray direction from the camera to the mouse position
        QVector3D l = mouseWorldPos - cameraPos;
        l.normalize();

        // Compute (o - c) // ray origin (o) - sphere center (c)
        QVector3D ocBot = cameraPos - wpPositionBottom;
        QVector3D ocTop = cameraPos - wpPositionTop;
        // Length of (o - c) = || o - c ||
        float lenOCBot = static_cast<float>(ocBot.length());
        float lenOCTop = static_cast<float>(ocTop.length());
        // Compute l * (o - c)
        float locBot = static_cast<float>(QVector3D::dotProduct(l, ocBot));
        float locTop = static_cast<float>(QVector3D::dotProduct(l, ocTop));

        // Solve equation:
        // d = - (l * (o - c) +- sqrt( (l * (o - c))² - || o - c ||² + r² )
        // Since the equation can be solved only if root discriminant is >= 0
        // just compute the discriminant
        float rootBot = locBot * locBot - lenOCBot * lenOCBot + radiusBot * radiusBot;
        float rootTop = locTop * locTop - lenOCTop * lenOCTop + radiusTop * radiusTop;


        // If root discriminant is positive or zero, there's an intersection
        if (rootBot >= 0)
        {
            modifyWaypoint = i;
            modifyWaypoint_worldZ = wpPositionBottom.z();
            QMatrix4x4 *mvpMatrix = sceneView->getModelViewProjectionMatrix();
            QVector3D posPoleClip = *mvpMatrix * wpPositionBottom;
            offsetPickPositionToHandleCentre = QVector2D(posPoleClip.x() - clipX,
                                     posPoleClip.y() - clipY);
            break;
        }
        else if (rootTop >= 0)
        {
            modifyWaypoint = i;
            modifyWaypoint_worldZ = wpPositionTop.z();
            QMatrix4x4 *mvpMatrix = sceneView->getModelViewProjectionMatrix();
            QVector3D posCentreClip = *mvpMatrix * wpPositionTop;
            offsetPickPositionToHandleCentre = QVector2D(posCentreClip.x() - clipX,
                                     posCentreClip.y() - clipY);
            break;
        }

    } // for (waypoints)

    return modifyWaypoint;
}


void MNWPVerticalSectionActor::addPositionLabel(MSceneViewGLWidget *sceneView,
                                                int handleID,
                                                float clipX, float clipY)
{
    if (waypointsModel == nullptr) return;

    // Select an arbitrary z-value to construct a point in clip space that,
    // transformed to world space, lies on the ray passing through the camera
    // and the location on the worldZ==0 plane "picked" by the mouse.
    // (See notes 22-23Feb2012).
    QVector3D mousePosClipSpace = QVector3D(clipX, clipY, modifyWaypoint_worldZ);

    // The point p at which the ray intersects the worldZ==0 plane is found by
    // computing the value d in p=d*l+l0, where l0 is a point on the ray and l
    // is a vector in the direction of the ray. d can be found with
    //        (p0 - l0) * n
    //   d = ----------------
    //            l * n
    // where p0 is a point on the worldZ==0 plane and n is the normal vector
    // of the plane.
    //       http://en.wikipedia.org/wiki/Line-plane_intersection

    // To compute l0, the MVP matrix has to be inverted.
    QMatrix4x4 *mvpMatrixInverted =
            sceneView->getModelViewProjectionMatrixInverted();
    QVector3D l0 = *mvpMatrixInverted * mousePosClipSpace;

    // Compute l as the vector from l0 to the camera origin.
    QVector3D cameraPosWorldSpace = sceneView->getCamera()->getOrigin();
    QVector3D l = (l0 - cameraPosWorldSpace);

    // The plane's normal vector simply points upward, the origin in world
    // space is lcoated on the plane.
    QVector3D n = QVector3D(0, 0, 1);
    QVector3D p0 = waypointsModel->positionLonLatIncludingMidpoints(handleID)
            + QVector3D(0, 0, modifyWaypoint_worldZ);

    // Compute the mouse position in world space.
    float d = QVector3D::dotProduct(p0 - l0, n) / QVector3D::dotProduct(l, n);
    QVector3D mousePosWorldSpace = l0 + d * l;

    // Get properties for label font size and colour and bounding box.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);
    QVector3D pos = waypointsModel->positionLonLatIncludingMidpoints(handleID);
    double lon = pos.x();
    double lat = pos.y();

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    MTextManager* tm = glRM->getTextManager();
    positionLabel = tm->addText(
                QString("lon:%1, lat:%2").arg(lon, 0, 'f', 2).arg(lat, 0, 'f', 2),
                MTextManager::LONLATP, lon, lat,
                sceneView->pressureFromWorldZ(modifyWaypoint_worldZ),
                labelsize, labelColour, MTextManager::LOWERRIGHT,
                labelbbox, labelBBoxColour);

    double dist = computePositionLabelDistanceWeight(sceneView->getCamera(),
                                                     mousePosWorldSpace);
    QVector3D anchorOffset = dist * sceneView->getCamera()->getXAxis();
    positionLabel->anchorOffset = -anchorOffset;

    emitActorChangedSignal();
}


void MNWPVerticalSectionActor::dragEvent(MSceneViewGLWidget *sceneView,
                                    int handleID, float clipX, float clipY)
{
    // http://stackoverflow.com/questions/2093096/implementing-ray-picking

    if (waypointsModel == nullptr) return;

    // Select an arbitrary z-value to construct a point in clip space that,
    // transformed to world space, lies on the ray passing through the camera
    // and the location on the worldZ==0 plane "picked" by the mouse.
    // (See notes 22-23Feb2012).
    QVector3D mousePosClipSpace = QVector3D(clipX + offsetPickPositionToHandleCentre.x(),
                                            clipY + offsetPickPositionToHandleCentre.y(),
                                            modifyWaypoint_worldZ);

    // The point p at which the ray intersects the worldZ==0 plane is found by
    // computing the value d in p=d*l+l0, where l0 is a point on the ray and l
    // is a vector in the direction of the ray. d can be found with
    //        (p0 - l0) * n
    //   d = ----------------
    //            l * n
    // where p0 is a point on the worldZ==0 plane and n is the normal vector
    // of the plane.
    //       http://en.wikipedia.org/wiki/Line-plane_intersection

    // To compute l0, the MVP matrix has to be inverted.
    QMatrix4x4 *mvpMatrixInverted =
            sceneView->getModelViewProjectionMatrixInverted();
    QVector3D l0 = *mvpMatrixInverted * mousePosClipSpace;

    // Compute l as the vector from l0 to the camera origin.
    QVector3D cameraPosWorldSpace = sceneView->getCamera()->getOrigin();
    QVector3D l = (l0 - cameraPosWorldSpace);

    // The plane's normal vector simply points upward, the origin in world
    // space is lcoated on the plane.
    QVector3D n = QVector3D(0, 0, 1);
    QVector3D p0 = waypointsModel->positionLonLatIncludingMidpoints(handleID)
            + QVector3D(0, 0, modifyWaypoint_worldZ);

    // Compute the mouse position in world space.
    float d = QVector3D::dotProduct(p0 - l0, n) / QVector3D::dotProduct(l, n);
    QVector3D mousePosWorldSpace = l0 + d * l;

    // DEBUG output.
//    cout << "dragging handle " << handleID << endl << flush;
//    cout << "\tmouse position clip space = (" << mousePosClipSpace.x() << "," << mousePosClipSpace.y() << "," << mousePosClipSpace.z() << ")\n" << flush;
//    cout << "\tl0 = (" << l0.x() << "," << l0.y() << "," << l0.z() << ")\n" << flush;
//    QVector3D l0Clip = *(mvpMatrix) * l0;
//    cout << "\tcheck: l0, transformed back to clip space = (" << l0Clip.x() << "," << l0Clip.y() << "," << l0Clip.z() << ")\n" << flush;
//    cout << "\tl = (" << l.x() << "," << l.y() << "," << l.z() << ")\n" << flush;
//    cout << "\tmouse position, world space, on worldZ==0 plane = (" << mousePosWorldSpace.x() << "," << mousePosWorldSpace.y()
//         << "," << mousePosWorldSpace.z()
//         << "); d = " << d << "\n" << flush;

    if (positionLabel != nullptr)
    {


        // Get properties for label font size and colour and bounding box.
        int labelsize = properties->mInt()->value(labelSizeProperty);
        QColor labelColour = properties->mColor()->value(labelColourProperty);
        bool labelbbox = properties->mBool()->value(labelBBoxProperty);
        QColor labelBBoxColour =
                properties->mColor()->value(labelBBoxColourProperty);
        MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
        MTextManager* tm = glRM->getTextManager();
        positionLabel = tm->addText(
                    QString("lon:%1, lat:%2")
                    .arg(mousePosWorldSpace.x(), 0, 'f', 2)
                    .arg(mousePosWorldSpace.y(), 0, 'f', 2),
                    MTextManager::LONLATP, mousePosWorldSpace.x(),
                    mousePosWorldSpace.y(),
                    sceneView->pressureFromWorldZ(modifyWaypoint_worldZ),
                    labelsize, labelColour, MTextManager::LOWERRIGHT,
                    labelbbox, labelBBoxColour);

        double dist = computePositionLabelDistanceWeight(sceneView->getCamera(),
                                                         mousePosWorldSpace);
        QVector3D anchorOffset = dist * sceneView->getCamera()->getXAxis();
        positionLabel->anchorOffset = -anchorOffset;
    }

    // Set the waypoint's coordinates. This will trigger a dataChanged signal
    // of the waypoints model, which in turn will call
    // generatePathFromWaypoints() and redraw the scene.
    waypointsModel->setPositionLonLatIncludingMidpoints(
                handleID, mousePosWorldSpace.x(), mousePosWorldSpace.y());
}


void MNWPVerticalSectionActor::setWaypointsModel(MWaypointsTableModel *model)
{
    // If the actor is currently connected to a different model, disconnect.
    if ( waypointsModel != nullptr )
    {
        disconnect(waypointsModel,
                   SIGNAL(dataChanged(QModelIndex, QModelIndex)),
                   this,
                   SLOT(generatePathFromWaypoints(QModelIndex, QModelIndex)));
        disconnect(waypointsModel,
                   SIGNAL(rowsRemoved(QModelIndex, int, int)),
                   this,
                   SLOT(actOnWaypointInsertDelete(QModelIndex, int, int)));
        disconnect(waypointsModel,
                   SIGNAL(rowsInserted(QModelIndex, int, int)),
                   this,
                   SLOT(actOnWaypointInsertDelete(QModelIndex, int, int)));
    }

    // Store the pointer to the new model and connect to its signals.
    waypointsModel = model;

    enableActorUpdates(false);

    if ( waypointsModel != nullptr )
    {
        connect(waypointsModel,
                SIGNAL(dataChanged(QModelIndex, QModelIndex)),
                SLOT(generatePathFromWaypoints(QModelIndex, QModelIndex)));
        connect(waypointsModel,
                SIGNAL(rowsRemoved(QModelIndex, int, int)),
                SLOT(actOnWaypointInsertDelete(QModelIndex, int, int)));
        connect(waypointsModel,
                SIGNAL(rowsInserted(QModelIndex, int, int)),
                SLOT(actOnWaypointInsertDelete(QModelIndex, int, int)));

        // Update GUI property.
        properties->setEnumItem(waypointsModelProperty, waypointsModel->getID());
    }
    else
    {
        // Remove labels -- otherwise labels of the previous waypoints model
        // will remain visible.
        removeAllLabels();

        // Set GUI property to "None".
        properties->setEnumItem(waypointsModelProperty, "None");
    }

    enableActorUpdates(true);

    // Trigger re-computation of vsec-grid on next render cycle.
    updatePath = true;
}


MWaypointsTableModel* MNWPVerticalSectionActor::getWaypointsModel()
{
    return waypointsModel;
}


double MNWPVerticalSectionActor::getBottomPressure_hPa()
{
    return p_bot_hPa;
}


double MNWPVerticalSectionActor::getTopPressure_hPa()
{
    return p_top_hPa;
}


const QList<MVerticalLevelType> MNWPVerticalSectionActor::supportedLevelTypes()
{
    return (QList<MVerticalLevelType>()
            << HYBRID_SIGMA_PRESSURE_3D << PRESSURE_LEVELS_3D
            << AUXILIARY_PRESSURE_3D);
}


MNWPActorVariable* MNWPVerticalSectionActor::createActorVariable(
        const MSelectableDataSource& dataSource)
{
    MNWP2DVerticalActorVariable* newVar = new MNWP2DVerticalActorVariable(this);

    newVar->dataSourceID = dataSource.dataSourceID;
    newVar->levelType = dataSource.levelType;
    newVar->variableName = dataSource.variableName;
    newVar->setRenderMode(MNWP2DSectionActorVariable::RenderMode::Disabled);

    return newVar;
}


void MNWPVerticalSectionActor::onBoundingBoxChanged()
{
    labels.clear();
    // Switching to no bounding box only needs a redraw, but no recomputation
    // because it disables rendering of the actor.
    if (bBoxConnection->getBoundingBox() == nullptr)
    {
        emitActorChangedSignal();
        return;
    }
    // The vertical extent of the section has been changed.
    p_top_hPa = bBoxConnection->topPressure_hPa();
    p_bot_hPa = bBoxConnection->bottomPressure_hPa();
    targetGridToBeUpdated = true;

    if (suppressActorUpdates()) return;

    // Adapt iso pressure lines set to new boundaries.
    generateIsoPressureLines();
    updatePath = true;
    emitActorChangedSignal();
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MNWPVerticalSectionActor::onQtPropertyChanged(QtProperty *property)
{
    // Parent signal processing.
    MNWPMultiVarActor::onQtPropertyChanged(property);

    if (property == labelDistanceProperty)
    {
        labelDistance = properties->mInt()->value(labelDistanceProperty);

        if (suppressActorUpdates()) return;

        generateLabels();
        emitActorChangedSignal();
    }

    else if (property == pressureLineLevelsProperty)
    {
        QString pressureLevelStr = properties->mString()->value(
                    pressureLineLevelsProperty);
        selectedPressureLineLevels = parseFloatRangeString(pressureLevelStr);

        if (suppressActorUpdates()) return;

        generateIsoPressureLines();
        generateLabels();
        emitActorChangedSignal();
    }

    else if (property == opacityProperty)
    {
        // The vertical extent of the section has been changed.
        opacity = properties->mDDouble()->value(opacityProperty);
        emitActorChangedSignal();
    }

    else if ( (property == labelSizeProperty)       ||
              (property == labelColourProperty)     ||
              (property == labelBBoxProperty)       ||
              (property == labelBBoxColourProperty)     )
    {
        if (suppressActorUpdates()) return;

        generateLabels();
        emitActorChangedSignal();
    }

    else if (property == waypointsModelProperty)
    {        
        if (suppressActorUpdates()) return;

        QString wpID = properties->getEnumItem(waypointsModelProperty);
        setWaypointsModel(MSystemManagerAndControl::getInstance()
                          ->getWaypointsModel(wpID));

        emitActorChangedSignal();
    }

    else if (property == interpolationNodeSpacingProperty)
    {
        interpolationNodeSpacing = properties->mDDouble()->value(
                    interpolationNodeSpacingProperty);

        targetGridToBeUpdated = true;

        if (suppressActorUpdates()) return;

        updatePath = true;
        emitActorChangedSignal();
    }
}


void MNWPVerticalSectionActor::generatePathFromWaypoints(
        QModelIndex mindex1, QModelIndex mindex2, QGLWidget *currentGLContext)
{
//TODO: implement great circles.
    // Great circles (ECMWF seems to use a perfect sphere for the IFS):
    // * http://www.movable-type.co.uk/scripts/gis-faq-5.1.html
    // * http://williams.best.vwh.net/avform.htm
    // * http://trac.osgeo.org/openlayers/wiki/GreatCircleAlgorithms
    // * Jeff W's Python implementation with the Vincenty distance.
    // * 3D Engine Design Virtual Globes, Sect 2.4?

    if (mindex1.isValid() && mindex2.isValid())
    {
        // The index variable provide row and column of the changed item in the
        // table. row = number of waypoint, column = table column (i.e. name,
        // lat, lon, fl, ...).

        // Only react to the signal if the position of a waypoint has changed.
        if ( !((mindex1.column() == MWaypointsTableModel::LAT) ||
               (mindex1.column() == MWaypointsTableModel::LON)) ) return;
    }

    // Implementation for linear connection.

    // A valid track must have at least two waypoints.
    if (waypointsModel->size() < 2) return;

    // This method assumes that all variables are on the same grid -- use
    // lon/lat data from variable 0.
    if (variables.size() == 0) return;
    MNWPActorVariable* v0 = variables.at(0);

    // The vector "path" accomodates a list of points that resemble the
    // vertical section path. Entries are QVector4D, storing (lon, lat, i, j),
    // with i,j being the indices of the closes model grid point.
    path.clear();

    // Approximate spacing between points along the cross section path.
    float deltaS = interpolationNodeSpacing;

    // Determine model grid spacing and the upper left corner coordinates
    // of the model grid; used to locate the grid cells of the points along
    // the section.
    float gridDeltaLon = fabs(v0->grid->lons[1] - v0->grid->lons[0]);
    float gridDeltaLat = fabs(v0->grid->lats[1] - v0->grid->lats[0]);
    float gridLonStart = v0->grid->lons[0]; // lon and lat of the grid at
    float gridLatStart = v0->grid->lats[0]; // index 0/0.

    float gridLonMin   = min(v0->grid->lons[0], v0->grid->lons[v0->grid->nlons-1]);
    float gridLonMax   = max(v0->grid->lons[0], v0->grid->lons[v0->grid->nlons-1]);
    float gridLatMin   = min(v0->grid->lats[0], v0->grid->lats[v0->grid->nlats-1]);
    float gridLatMax   = max(v0->grid->lats[0], v0->grid->lats[v0->grid->nlats-1]);

    // If the grid is cyclic in longitude (e.g. hemispheric grids), adjust
    // gridLonMax to avoid a gap at the grid boundary (cf. notes 16Apr2012).
    double lon_west = MMOD(gridLonMin, 360.);
    double lon_east = MMOD(gridLonMax + gridDeltaLon, 360.);
    bool gridIsCyclic = ( fabs(lon_west - lon_east) < M_LONLAT_RESOLUTION);
    if (gridIsCyclic) gridLonMax += gridDeltaLon;

    // Vector that accomodates vertices for the vertical lines drawn at the
    // waypoints.
    QVector<QVector3D> verticesVerticalWaypointLines;
    QVector<QVector3D> verticesInteractionHandlePositions;

    // Copy first waypoint (p1 of the first segment).
    QVector2D p = waypointsModel->positionLonLat(0);
    path.append(p);

    // Create vertices for a vertical line and interaction handle at this
    // waypoint.
    verticesVerticalWaypointLines.append(QVector3D(p, p_bot_hPa));
    verticesVerticalWaypointLines.append(QVector3D(p, p_top_hPa));
    verticesInteractionHandlePositions.append(QVector3D(p, p_bot_hPa));
    verticesInteractionHandlePositions.append(QVector3D(p, p_top_hPa));

    for (int i = 0; i < waypointsModel->size()-1; i++)
    {
        // Add intermediate points between p1=wp[i] and p2=wp[i+1].

        QVector2D p1 = waypointsModel->positionLonLat(i  );
        QVector2D p2 = waypointsModel->positionLonLat(i+1);

        float lengthOfSegment = (p2-p1).length();
        int   numPoints = int(round(lengthOfSegment / deltaS));
        float deltaLon = (p2.x()-p1.x()) / numPoints;
        float deltaLat = (p2.y()-p1.y()) / numPoints;

        // Generate points between p1 and p2 (excluding p1 and p2).
        for (int n = 1; n < numPoints; n++)
        {
            // !! See above comments for the intial point!
            QVector2D p = QVector2D(p1.x()+n*deltaLon, p1.y()+n*deltaLat);
            path.append(p);
        }

        // Copy segment endpoint p2 (which is also p1 of the next segment).
        QVector2D p = p2;
        path.append(p);

        // Compute segment midpoint for interaction handle.
        QVector2D p_mid = p1 + (p2 - p1) / 2.;
        verticesInteractionHandlePositions.append(QVector3D(p_mid, p_bot_hPa));
        verticesInteractionHandlePositions.append(QVector3D(p_mid, p_top_hPa));

        // Create vertices for a vertical line and interaction handle at this
        // waypoint.
        verticesVerticalWaypointLines.append(QVector3D(p, p_bot_hPa));
        verticesVerticalWaypointLines.append(QVector3D(p, p_top_hPa));
        verticesInteractionHandlePositions.append(QVector3D(p, p_bot_hPa));
        verticesInteractionHandlePositions.append(QVector3D(p, p_top_hPa));
    }

    // DEBUG: Print the coordinates of all points in this vertical section,
    // along with the model grid indices and coordinates i,j.
//    cout << "Vertical section path:\n";
//    for (int m = 0; m < path.size(); m++) {
//        QVector4D p = path.at(m);
//        int i = int(p.z());
//        int j = int(p.w());
//        float fractI = p.z() - i;
//        float fractJ = p.w() - j;
//        cout << "\tPoint("
//             << p.x() << "/" << p.y() << ") -- Grid(at "  << i << "/" << j
//             << " >> " << v0->grid->lons[i] << "/" << v0->grid->lats[j]
//             << "), Grid(at " << i+1 << "/" << j+1
//             << " >> " << v0->grid->lons[i+1] << "/" << v0->grid->lats[j+1]
//             << "), Fract(" << fractI << "/" << fractJ << ")\n" << flush;
//    }

//TODO: Register this texture with the glRM memory management?
    if (textureVerticalSectionPath)
    {
        delete textureVerticalSectionPath;
    }
    textureVerticalSectionPath = new GL::MTexture(QString("vpath_%1").arg(myID),
                                                 GL_TEXTURE_1D, GL_ALPHA32F_ARB,
                                                 2 * path.size());
    textureVerticalSectionPath->bindToTextureUnit(textureUnitVerticalSectionPath);

    // Set texture parameters: wrap mode and filtering.
    // NOTE: GL_NEAREST is required here to avoid interpolation.
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage1D(GL_TEXTURE_1D,             // target
                 0,                         // level of detail
                 GL_ALPHA32F_ARB,           // internal format
                 2 * path.size(),           // width, size * (lon/lat/i/j)
                 0,                         // border
                 GL_ALPHA,                  // format
                 GL_FLOAT,                  // data type of the pixel data
                 path.constData()); CHECK_GL_ERROR;

    // Update target grid texture for each variable. (no data is uploaded, only
    // the size is set).
    targetGridToBeUpdated = true;

    for (int vi = 0; vi < variables.size(); vi++)
    {
        MNWP2DVerticalActorVariable* var =
                static_cast<MNWP2DVerticalActorVariable*> (variables.at(vi));

        var->textureTargetGrid->bindToTextureUnit(var->textureUnitTargetGrid);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D,             // target
                     0,                         // level of detail
                     GL_RG32F,                  // internal format
                     path.size(),               // width
                     var->grid->nlevs,          // height
                     0,                         // border
                     GL_RED,                    // format
                     GL_FLOAT,                  // data type of the pixel data
                     NULL); CHECK_GL_ERROR;
    }

    // Send vertices of vertical waypoint lines to video memory.
    if (vbVerticalWaypointLines) delete vbVerticalWaypointLines;
    vbVerticalWaypointLines = new GL::MVector3DVertexBuffer(
                QString("vbwp_%1").arg(myID),
                verticesVerticalWaypointLines.size());
    vbVerticalWaypointLines->upload(verticesVerticalWaypointLines,
                                    currentGLContext);
    // Required for the glDrawArrays() call in renderToCurrentContext().
    numVerticesVerticalWaypointLines = verticesVerticalWaypointLines.size();

    // Send vertices of drag handle positions to video memory.
    if (vbInteractionHandlePositions) delete vbInteractionHandlePositions;
    vbInteractionHandlePositions = new GL::MVector3DVertexBuffer(
                QString("vbdhpos_%1").arg(myID),
                verticesInteractionHandlePositions.size());
    vbInteractionHandlePositions->upload(verticesInteractionHandlePositions,
                                         currentGLContext);
    numInteractionHandlePositions = verticesInteractionHandlePositions.size();

    //NOTE: generateLabels() switches to the MGLResourcesManager OpenGL context,
    // hence we need to switch back to the currentGLContext afterwards.
    generateLabels();
    if (currentGLContext) currentGLContext->makeCurrent();

    emitActorChangedSignal();
}


void MNWPVerticalSectionActor::actOnWaypointInsertDelete(
        const QModelIndex &parent, int start, int end)
{
    // Parameters are required so this function can act as a slot for
    // rowsInserted / rowsRemoved signals.
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

    generatePathFromWaypoints();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MNWPVerticalSectionActor::initializeActorResources()
{
    // Parent initialisation.
    MNWPMultiVarActor::initializeActorResources();

    if (textureUnitPressureLevels >=0)
        releaseTextureUnit(textureUnitPressureLevels);
    textureUnitPressureLevels = assignTextureUnit();
    generateIsoPressureLines();

    if (textureUnitVerticalSectionPath >=0)
        releaseTextureUnit(textureUnitVerticalSectionPath);
    textureUnitVerticalSectionPath = assignTextureUnit();

    updatePath = true;

    MGLResourcesManager *glRM = MGLResourcesManager::getInstance();
    bool loadShaders = false;

    loadShaders |= glRM->generateEffectProgram("vsec_sectiongrid",
                                                sectionGridShader);
    loadShaders |= glRM->generateEffectProgram("vsec_marchingsquares",
                                                marchingSquaresShader);
    loadShaders |= glRM->generateEffectProgram("vsec_pressurelines",
                                                pressureLinesShader);
    loadShaders |= glRM->generateEffectProgram("vsec_simplegeometry",
                                                simpleGeometryShader);
    loadShaders |= glRM->generateEffectProgram("vsec_positionsphere",
                                                positionSpheresShader);

    if (loadShaders) reloadShaderEffects();
}


void MNWPVerticalSectionActor::renderToCurrentContext(MSceneViewGLWidget *sceneView)
{
    // If there is no connected waypoints model, no connected bounding box or
    // no actor variable, nothing can be rendered.
    if (waypointsModel == nullptr || bBoxConnection->getBoundingBox() == nullptr
            || variables.size() == 0)
    {
        return;
    }

    if (updatePath)
    {
        // This method might already be called between initial data request and
        // all data fields being available. Return if not all variables
        // contain valid data yet.
        foreach (MNWPActorVariable *var, variables)
            if ( !var->hasData() ) return;

        // Prevent generatePathFromWaypoints() from emitting a signal.
        enableEmissionOfActorChangedSignal(false);
        updateVerticalLevelRange();
        generatePathFromWaypoints(QModelIndex(), QModelIndex(), sceneView);
        updatePath = false;
        enableEmissionOfActorChangedSignal(true);
    }

    // If major visualisation parameters of the view have changed (e.g.
    // vertical scaling), a recompuation of the target grid is necessary, as it
    // stores worldZ coordinates.
    targetGridToBeUpdated = targetGridToBeUpdated
            || sceneView->visualisationParametersHaveChanged();

    // 1D texture that stores the horizontal coordinates of the section points.
    textureVerticalSectionPath->bindToTextureUnit(
                textureUnitVerticalSectionPath);

    // Rendering for all data variables:
    for (int vi = 0; vi < variables.size(); vi++)
    {
        // Shortcuts to the variable's properties.
        MNWP2DVerticalActorVariable* var =
                static_cast<MNWP2DVerticalActorVariable*> (variables.at(vi));

        if ( !var->hasData() ) continue;

        // A) Compute the vertical section grid, store it to the "targetGrid"
        //    texture, render filled contours if requested.
        // ==================================================================

        bool renderFilledContours = (
                    (var->renderSettings.renderMode
                     == MNWP2DSectionActorVariable::RenderMode::FilledContours)
                    ||
                    (var->renderSettings.renderMode
                     == MNWP2DSectionActorVariable::RenderMode::FilledAndLineContours)
                    ) && ( var->transferFunction != nullptr );

        if (renderFilledContours || targetGridToBeUpdated)
        {
            if (renderFilledContours)
            {
                sectionGridShader->bindProgram("Standard");

                // Change the depth function to less and equal. This allows
                // OpenGL to overwrite fragments with the same depths and thus
                // allows the vsec actor to draw filled contours of more than
                // one variable.
                glDepthFunc(GL_LEQUAL);
            }
            else
            {
                sectionGridShader->bindProgram("OnlyUpdateTargetGrid");
            }

            // Reset optional textures (to avoid draw errors).
            // ===============================================
            var->textureDummy1D->bindToTextureUnit(var->textureUnitUnusedTextures);
            sectionGridShader->setUniformValue(
                        "hybridCoefficients", var->textureUnitUnusedTextures); CHECK_GL_ERROR;

            var->textureDummy2D->bindToTextureUnit(var->textureUnitUnusedTextures);
            sectionGridShader->setUniformValue(
                        "surfacePressure", var->textureUnitUnusedTextures); CHECK_GL_ERROR;

            // Model-view-projection matrix from the current scene view.
            sectionGridShader->setUniformValue(
                        "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

            sectionGridShader->setUniformValue(
                        "levelType", int(var->grid->getLevelType()));

            // Texture bindings for Lat/Lon axes (1D textures).
            var->textureLonLatLevAxes->bindToTextureUnit(var->textureUnitLonLatLevAxes);
            sectionGridShader->setUniformValue(
                        "lonLatLevAxes", var->textureUnitLonLatLevAxes);
            sectionGridShader->setUniformValue(
                        "latOffset", var->grid->nlons);

            // Texture bindings for data field (3D texture).
            var->textureDataField->bindToTextureUnit(var->textureUnitDataField);
            sectionGridShader->setUniformValue(
                        "dataField", var->textureUnitDataField);
            sectionGridShader->setUniformValue(
                        "auxPressureField_hPa", var->textureUnitDataField);

            // Texture bindings for transfer function for data field (1D texture from
            // transfer function class). Variables that are only rendered as
            // contour lines might not provide a valid transfer function.
            if (var->transferFunction != 0)
            {
                var->transferFunction->getTexture()->bindToTextureUnit(
                            var->textureUnitTransferFunction);
                sectionGridShader->setUniformValue(
                            "transferFunction", var->textureUnitTransferFunction);
                sectionGridShader->setUniformValue(
                            "scalarMinimum", var->transferFunction->getMinimumValue());
                sectionGridShader->setUniformValue(
                            "scalarMaximum", var->transferFunction->getMaximumValue());
            }

            if (var->grid->getLevelType() == HYBRID_SIGMA_PRESSURE_3D)
            {
                // Texture bindings for surface pressure (2D texture) and model level
                // coefficients (1D texture).
                var->textureSurfacePressure->bindToTextureUnit(var->textureUnitSurfacePressure);
                sectionGridShader->setUniformValue(
                            "surfacePressure", var->textureUnitSurfacePressure);
                var->textureHybridCoefficients->bindToTextureUnit(var->textureUnitHybridCoefficients);
                sectionGridShader->setUniformValue(
                            "hybridCoefficients", var->textureUnitHybridCoefficients);
            }

            if (var->grid->getLevelType() == AUXILIARY_PRESSURE_3D)
            {
                // Texture binding for pressure field (3D texture).
                var->textureAuxiliaryPressure->bindToTextureUnit(
                            var->textureUnitAuxiliaryPressure);
                sectionGridShader->setUniformValue(
                            "auxPressureField_hPa", var->textureUnitAuxiliaryPressure);
            }

            // Data volume info required to locate grid index for given lon/lat.
            sectionGridShader->setUniformValue(
                        "deltaLat", var->grid->getDeltaLat()); CHECK_GL_ERROR;
            sectionGridShader->setUniformValue(
                        "deltaLon", var->grid->getDeltaLon()); CHECK_GL_ERROR;
            QVector3D dataNWCrnr = var->grid->getNorthWestTopDataVolumeCorner_lonlatp();
            sectionGridShader->setUniformValue(
                        "dataNWCrnr", dataNWCrnr); CHECK_GL_ERROR;
            sectionGridShader->setUniformValue(
                        "gridIsCyclicInLongitude",
                        var->grid->gridIsCyclicInLongitude()); CHECK_GL_ERROR;

            // Scene view specific parameters to compute worldZ from pressure in
            // the vertex shader.
            sectionGridShader->setUniformValue(
                        "pToWorldZParams", sceneView->pressureToWorldZParameters());

            // (Texture object is already bound to this unit, see above).
            sectionGridShader->setUniformValue(
                        "path", textureUnitVerticalSectionPath);

            sectionGridShader->setUniformValue(
                        "targetGrid", var->imageUnitTargetGrid);
            glBindImageTexture(var->imageUnitTargetGrid,     // image unit
                               var->textureTargetGrid->getTextureObject(), // texture object
                               0,                             // level
                               GL_FALSE,                      // layered
                               0,                             // layer
                               GL_READ_WRITE,                 // shader access
                               GL_RG32F); CHECK_GL_ERROR;     // format
            sectionGridShader->setUniformValue(
                        "fetchFromTarget", !targetGridToBeUpdated);

            // Set the sections vertical limits (the fragment shader discards
            // elements outside this range).
            sectionGridShader->setUniformValue(
                        "verticalBounds",
                        QVector2D(sceneView->worldZfromPressure(p_bot_hPa),
                                  sceneView->worldZfromPressure(p_top_hPa))
                        ); CHECK_GL_ERROR;

            sectionGridShader->setUniformValue(
                        "opacity",
                        opacity); CHECK_GL_ERROR;

            // Use instanced rendering to avoid geometry upload (see notes 09Feb2012).
            // Offset depth buffer slightly to ensure correct rendering of isopressure
            // lines.
            glPolygonOffset(.8f, 1.0f);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonMode(GL_FRONT_AND_BACK,
                          renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
            glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                                  2 * var->gridVerticalLevelStart,
                                  2 * var->gridVerticalLevelCount,
                                  path.size() - 1); CHECK_GL_ERROR;
            glDisable(GL_POLYGON_OFFSET_FILL);
            // Change the depth function back to its default value.
            glDepthFunc(GL_LESS);
        } // sectionGridShader (interpolation, target grid, filled contours)

        // B) Contouring with the GPU Marching Squares implementation, if
        // enabled (the marching squares algorithm uses the "targetGrid" that
        // was written by the previous shader run as input).
        // ==================================================================

        if ( (var->renderSettings.renderMode
              == MNWP2DSectionActorVariable::RenderMode::LineContours) ||
             (var->renderSettings.renderMode
              == MNWP2DSectionActorVariable::RenderMode::FilledAndLineContours) )
        {
            marchingSquaresShader->bind();

            marchingSquaresShader->setUniformValue(
                        "mvpMatrix", *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;
            marchingSquaresShader->setUniformValue(
                        "pToWorldZParams", sceneView->pressureToWorldZParameters()); CHECK_GL_ERROR;

            // (Texture object is already bound to this unit, see above).
            marchingSquaresShader->setUniformValue(
                        "path", textureUnitVerticalSectionPath); CHECK_GL_ERROR;

            // The 2D data grid that the contouring algorithm processes.
            glBindImageTexture(var->imageUnitTargetGrid,     // image unit
                               var->textureTargetGrid->getTextureObject(), // texture object
                               0,                             // level
                               GL_FALSE,                      // layered
                               0,                             // layer
                               GL_READ_WRITE,                 // shader access
                               GL_RG32F); CHECK_GL_ERROR;     // format
            marchingSquaresShader->setUniformValue(
                        "sectionGrid", var->imageUnitTargetGrid); CHECK_GL_ERROR;

            // Set the sections vertical limits (the fragment shader discards
            // elements outside this range).
            marchingSquaresShader->setUniformValue(
                        "verticalBounds",
                        QVector2D(sceneView->worldZfromPressure(p_bot_hPa),
                                  sceneView->worldZfromPressure(p_top_hPa))
                        ); CHECK_GL_ERROR;

            // Draw individual line segments as output by the geometry shader (no
            // connected polygon is created).
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;

            // Loop over all contour sets.
            foreach (MNWP2DSectionActorVariable::ContourSettings
                     contourSet, var->contourSetList)
            {
                if (contourSet.enabled)
                {
                    glLineWidth(contourSet.thickness); CHECK_GL_ERROR;
                    marchingSquaresShader->setUniformValue(
                                "useTransferFunction",
                                GLboolean(var->renderSettings.contoursUseTF
                                          || contourSet.useTF)); CHECK_GL_ERROR;
                    if (!(var->renderSettings.contoursUseTF || contourSet.useTF))
                    {
                        marchingSquaresShader->setUniformValue(
                                    "colour", contourSet.colour); CHECK_GL_ERROR;
                    }
                    else
                    {
                        // Texture bindings for transfer function for data field
                        // (1D texture from transfer function class).
                        if (var->transferFunction != 0)
                        {
                            var->transferFunction->getTexture()->bindToTextureUnit(
                                        var->textureUnitTransferFunction);
                            marchingSquaresShader->setUniformValue(
                                        "transferFunction",
                                        var->textureUnitTransferFunction);
                            CHECK_GL_ERROR;
                            marchingSquaresShader->setUniformValue(
                                        "scalarMinimum",
                                        GLfloat(var->transferFunction
                                                ->getMinimumValue()));
                            CHECK_GL_ERROR;
                            marchingSquaresShader->setUniformValue(
                                        "scalarMaximum",
                                        GLfloat(var->transferFunction
                                                ->getMaximumValue()));
                            CHECK_GL_ERROR;
                        }
                        // Don't draw contour set if transfer function is not
                        // present.
                        else
                        {
                            continue;
                        }
                    }
                    // Loop over all iso values for which contour lines should
                    // be rendered -- one render pass per isovalue.
                    for (int i = contourSet.startIndex;
                         i < contourSet.stopIndex; i++)
                    {
                        marchingSquaresShader->setUniformValue(
                                    "isoValue",
                                    GLfloat(contourSet.levels.at(i)));
                        CHECK_GL_ERROR;
                        glDrawArraysInstanced(GL_POINTS,
                                              var->gridVerticalLevelStart,
                                              var->gridVerticalLevelCount - 1,
                                              path.size() - 1); CHECK_GL_ERROR;
                    }
                }
            }
        } // marching squares shader
    } // for (variables)

    // C) Render isopressure lines (vertical coordinate system).
    // =========================================================

    pressureLinesShader->bind();

    pressureLinesShader->setUniformValue(
                "mvpMatrix", *(sceneView->getModelViewProjectionMatrix()));

    pressureLinesShader->setUniformValue(
                "pToWorldZParams", sceneView->pressureToWorldZParameters());

    // (Texture object is already bound to this unit, see above).
    pressureLinesShader->setUniformValue(
                "path", textureUnitVerticalSectionPath);

    // 1D texture storing the pressure values at which lines should be drawn.
    texturePressureLevels->bindToTextureUnit(textureUnitPressureLevels);
    pressureLinesShader->setUniformValue(
                "pressureLevels", textureUnitPressureLevels);
    CHECK_GL_ERROR;

    glLineWidth(1); CHECK_GL_ERROR;
    glDrawArraysInstanced(GL_LINE_STRIP,
                          0,
                          path.size(),
                          pressureLineLevels.size()); CHECK_GL_ERROR;

    // D) Render vertical lines at waypoints.
    // ======================================

    if (vbVerticalWaypointLines != nullptr)
    {

        simpleGeometryShader->bindProgram("Pressure"); CHECK_GL_ERROR;

        simpleGeometryShader->setUniformValue(
                    "mvpMatrix",
                    *(sceneView->getModelViewProjectionMatrix())); CHECK_GL_ERROR;
        simpleGeometryShader->setUniformValue(
                    "pToWorldZParams",
                    sceneView->pressureToWorldZParameters()); CHECK_GL_ERROR;
        simpleGeometryShader->setUniformValue(
                    "colour",
                    QColor(0, 0, 0)); CHECK_GL_ERROR;

        vbVerticalWaypointLines->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); CHECK_GL_ERROR;
        glLineWidth(2); CHECK_GL_ERROR;

        glDrawArrays(GL_LINES, 0, numVerticesVerticalWaypointLines); CHECK_GL_ERROR;
        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    }

    // E) "Hover" effect for a waypoint in interaction mode: Highlight
    //    a specific waypoint (that at index "modifyWaypoint" in the vector
    //    "waypoints".
    // ====================================================================

    if (sceneView->interactionModeEnabled() &&
            (vbInteractionHandlePositions != nullptr))
    {
        positionSpheresShader->bindProgram("Normal");

        positionSpheresShader->setUniformValue(
                    "mvpMatrix",
                    *(sceneView->getModelViewProjectionMatrix()));
        positionSpheresShader->setUniformValue(
                    "pToWorldZParams",
                    sceneView->pressureToWorldZParameters());
        positionSpheresShader->setUniformValue(
                    "lightDirection",
                    sceneView->getLightDirection());
        positionSpheresShader->setUniformValue(
                    "cameraPosition",
                    sceneView->getCamera()->getOrigin());
        positionSpheresShader->setUniformValue(
                    "cameraUpDir",
                    sceneView->getCamera()->getYAxis());
        positionSpheresShader->setUniformValue(
                    "radius", GLfloat(MSystemManagerAndControl::getInstance()
                                      ->getHandleSize()));
        positionSpheresShader->setUniformValue(
                    "scaleRadius",
                    GLboolean(true));

        positionSpheresShader->setUniformValue(
                    "useTransferFunction", GLboolean(false));

        vbInteractionHandlePositions->attachToVertexAttribute(SHADER_VERTEX_ATTRIBUTE);

        glPolygonMode(GL_FRONT_AND_BACK,
                      renderAsWireFrame ? GL_LINE : GL_FILL); CHECK_GL_ERROR;
        glLineWidth(1); CHECK_GL_ERROR;

        if (modifyWaypoint >= 0)
        {
            positionSpheresShader->setUniformValue(
                        "constColour", QColor(Qt::red));
            glDrawArrays(GL_POINTS, 2*modifyWaypoint, 2); CHECK_GL_ERROR;
        }

        positionSpheresShader->setUniformValue(
                    "constColour", QColor(Qt::white));
        glDrawArrays(GL_POINTS, 0, numInteractionHandlePositions); CHECK_GL_ERROR;


        // Unbind VBO.
        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL_ERROR;
    }

    // Don't update the grid until the next update event occurs (see
    // actOnPropertyChange() and dataFieldChangedEvent().
    targetGridToBeUpdated = false;
}


void MNWPVerticalSectionActor::renderTransparencyToCurrentContext(
        MSceneViewGLWidget *sceneView)
{
    Q_UNUSED(sceneView);
}


void MNWPVerticalSectionActor::dataFieldChangedEvent()
{
    targetGridToBeUpdated = true;
    emitActorChangedSignal();
}


void MNWPVerticalSectionActor::generateIsoPressureLines()
{
    pressureLineLevels.clear();
    for (int i = 0; i < selectedPressureLineLevels.size(); i++)
    {
        if ((selectedPressureLineLevels[i] <= p_bot_hPa)
                && (selectedPressureLineLevels[i] >= p_top_hPa))
            pressureLineLevels.append(selectedPressureLineLevels[i]);
    }

    if (texturePressureLevels) delete texturePressureLevels;

//TODO: Register this texture with the glRM memory management?
    texturePressureLevels = new GL::MTexture(QString("prlevs_%1").arg(myID),
                                            GL_TEXTURE_1D, GL_ALPHA32F_ARB,
                                            pressureLineLevels.size());
    texturePressureLevels->bindToLastTextureUnit();

    // Set texture parameters: wrap mode and filtering.
    // NOTE: GL_NEAREST is required here to avoid interpolation.
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage1D(GL_TEXTURE_1D,             // target
                 0,                         // level of detail
                 GL_ALPHA32F_ARB,           // internal format
                 pressureLineLevels.size(), // width
                 0,                         // border
                 GL_ALPHA,                  // format
                 GL_FLOAT,                  // data type of the pixel data
                 pressureLineLevels.constData()); CHECK_GL_ERROR;
}


void MNWPVerticalSectionActor::updateVerticalLevelRange()
{
    LOG4CPLUS_DEBUG(mlog, "updating vertical level range.." << flush);

    // For each variables in the vertical section, determine the upper/lower
    // model levels that enclose the range pbot..ptop.
    for (int vi = 0; vi < variables.size(); vi++)
    {
        // Shortcuts to variable info structs.
        MNWP2DVerticalActorVariable* var =
                static_cast<MNWP2DVerticalActorVariable*> (variables.at(vi));

        var->updateVerticalLevelRange(p_bot_hPa, p_top_hPa);
    }
}


void MNWPVerticalSectionActor::generateLabels()
{
    // Remove all text labels of the old geometry (MActor method).
    removeAllLabels();

    if (path.size() == 0) return;

    MGLResourcesManager* glRM = MGLResourcesManager::getInstance();
    MTextManager* tm = glRM->getTextManager();

    // Label font size and colour.
    int labelsize = properties->mInt()->value(labelSizeProperty);
    QColor labelColour = properties->mColor()->value(labelColourProperty);

    // Label bounding box.
    bool labelbbox = properties->mBool()->value(labelBBoxProperty);
    QColor labelBBoxColour = properties->mColor()->value(labelBBoxColourProperty);

    // Draw labels at these horizontal positions.
    QList<QVector2D> labelPoints;
    // First and last point of the path.
    labelPoints.append(path[0]);
    labelPoints.append(path[path.size()-1]);

    int drawLabel = 0;
    for (int i = 0; i < pressureLineLevels.size(); i++)
    {
        // Label only every (labelDistance + 1)-th tick mark.
        if (drawLabel++ < 0) continue;
        if (drawLabel == 1) drawLabel = -labelDistance;

        for (int j = 0; j < labelPoints.size(); j++)
        {
            QVector3D position(labelPoints[j].x(), labelPoints[j].y(),
                               pressureLineLevels[i]);
            labels.append(tm->addText(
                              QString("%1").arg(pressureLineLevels[i]),
                              MTextManager::LONLATP,
                              position.x(), position.y(), position.z(),
                              labelsize, labelColour, MTextManager::MIDDLELEFT,
                              labelbbox, labelBBoxColour)
                          );
        }
    }
}


void MNWPVerticalSectionActor::onDeleteActorVariable(MNWPActorVariable *var)
{
    Q_UNUSED(var);

    // Remove labels if no variable is left. (Since variable is deleted
    // afterwards, current size() must be 1).
    if (variables.size() == 1)
    {
        removeAllLabels();
    }
}


void MNWPVerticalSectionActor::onAddActorVariable(MNWPActorVariable *var)
{
    Q_UNUSED(var);
    targetGridToBeUpdated = true;
    updatePath = true;
}


void MNWPVerticalSectionActor::onChangeActorVariable(MNWPActorVariable *var)
{
    Q_UNUSED(var);
    targetGridToBeUpdated = true;
    updatePath = true;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/


} // namespace Met3D
