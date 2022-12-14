/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2017-2018 Bianca Tost
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
/******************************************************************************
**
** NOTE: This file is based on code from the QtPropertyBrowser framework. Parts
** have been copied and partly modified from the "decoration" example, from
** qtpropertymanager.h and .cpp, qtpropertybrowserutils_p.h and .cpp,
** qteditorfactory.h and .cpp.
**
** This file incorporates work covered by the following copyright and
** permission notice:
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Solutions component.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
*******************************************************************************/
#ifndef QTROPERTYMANAGER_EXTENSIONS_H
#define QTROPERTYMANAGER_EXTENSIONS_H

// standard library imports

// related third party imports
#include <QApplication>
#include <QtCore/QMap>
#include <QDoubleSpinBox>
#include <QToolButton>
#include "qtpropertybrowser.h"
#include "qteditorfactory.h"
#include "qttreepropertybrowser.h"
#include "qtpropertymanager.h"
#include "qtpropertybrowserutils_p.h"

// local application imports
#include "scientificdoublespinbox.h"

#ifndef Q_TYPENAME
#define Q_TYPENAME typename
#endif

namespace QtExtensions
{

/******************************************************************************
***             Property manager for "decorated" double values              ***
*******************************************************************************/

/**
  @brief QtDecoratedDoublePropertyManager provides a QtDoublePropertyManager
  whose editor spinboxed can be augmented by a prefix and a suffix. See
  the qt-solutions QtPropertyBrowser "decoration" example for more details.
 */
class QtDecoratedDoublePropertyManager : public QtDoublePropertyManager
{
    Q_OBJECT
public:
    QtDecoratedDoublePropertyManager(QObject *parent = 0);
    ~QtDecoratedDoublePropertyManager();

    QString prefix(const QtProperty *property) const;

    QString suffix(const QtProperty *property) const;

public Q_SLOTS:
    void setPrefix(QtProperty *property, const QString &prefix);

    void setSuffix(QtProperty *property, const QString &suffix);

Q_SIGNALS:
    void prefixChanged(QtProperty *property, const QString &prefix);

    void suffixChanged(QtProperty *property, const QString &suffix);

protected:
    QString valueText(const QtProperty *property) const;

    virtual void initializeProperty(QtProperty *property);

    virtual void uninitializeProperty(QtProperty *property);

private:
    struct Data {
        QString prefix;
        QString suffix;
    };

    QMap<const QtProperty *, Data> propertyToData;
};


/**
  @brief Factory for the @ref QtDecoratedDoublePropertyManager class.
 */
class QtDecoratedDoubleSpinBoxFactory
        : public QtAbstractEditorFactory<QtDecoratedDoublePropertyManager>
{
    Q_OBJECT
public:
    QtDecoratedDoubleSpinBoxFactory(QObject *parent = 0);
    ~QtDecoratedDoubleSpinBoxFactory();

protected:
    void connectPropertyManager(QtDecoratedDoublePropertyManager *manager);

    QWidget *createEditor(QtDecoratedDoublePropertyManager *manager, QtProperty *property,
                QWidget *parent);

    void disconnectPropertyManager(QtDecoratedDoublePropertyManager *manager);

private slots:

    void slotPrefixChanged(QtProperty *property, const QString &prefix);

    void slotSuffixChanged(QtProperty *property, const QString &prefix);

    void slotEditorDestroyed(QObject *object);

private:    
    // We delegate responsibilities for QtDoublePropertyManager, which is a
    // base class of DecoratedDoublePropertyManager to appropriate
    // QtDoubleSpinBoxFactory
    QtDoubleSpinBoxFactory *originalFactory;

    QMap<QtProperty *, QList<QDoubleSpinBox *> > createdEditors;

    QMap<QDoubleSpinBox *, QtProperty *> editorToProperty;
};


/******************************************************************************
***           Property manager for "click" events (tool buttons)            ***
*******************************************************************************/

class QtClickPropertyManagerPrivate;

/**
  QtClickPropertyManager implements a property manager for handling "click
  events". It allows to insert QToolButtons into the property browser and
  processes "clicked()" signals.
  */
class QT_QTPROPERTYBROWSER_EXPORT QtClickPropertyManager :
        public QtAbstractPropertyManager
{
// The following code is based on on a simplified version of
// QtBoolPropertyManager in "qtpropertymanager.h". No value can be set for a
// tool button, so the "setValue()" slot has been removed, as well as the
// "valueChanged()" signal. Instead, the "emitClicked()" slot is called
// whenever the button has been clicked; it simply emit a "propertyChanged()"
// signal.
    Q_OBJECT
public:
    QtClickPropertyManager(QObject *parent = 0);
    ~QtClickPropertyManager();

public Q_SLOTS:
    void emitClicked(QtProperty *property);

protected:
    QString valueText(const QtProperty *property) const;

    virtual void initializeProperty(QtProperty *property);

    virtual void uninitializeProperty(QtProperty *property);

private:
    QtClickPropertyManagerPrivate *d_ptr;

    Q_DECLARE_PRIVATE(QtClickPropertyManager)

    Q_DISABLE_COPY(QtClickPropertyManager)
};


/******************************************************************************
***                        Tool button "editor"                             ***
*******************************************************************************/

/**
  QtToolButtonBoolEdit is the "editor" for click signals. At least necessary
  for the tree browser -- a tree widget displays data items, that is no GUI
  widgets. If the items are editable, editor widgets are required (the same as
  for table widgets). In the property browser framework, these editors include
  line edits, spin edits etc. For a "click property" (which actually isn't a
  property but rather a mechanism to create click signals) the "editor" is a
  tool button.
  */
class QtToolButtonBoolEdit : public QWidget
{
// Code based on QtBoolEdit in "qtqtpropertybrowserutils_p.h". Instead of the
// "toggled()" signal of the check box of QtBoolEdit, here only the "clicked()"
// signal is emitted.
    Q_OBJECT
public:
    QtToolButtonBoolEdit(QWidget *parent = 0);

Q_SIGNALS:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent * event);

    void paintEvent(QPaintEvent *);

private:
    QToolButton *m_toolButton;
};


/******************************************************************************
***                        Tool button "factory"                            ***
*******************************************************************************/

// ************
// The following code has been copied from "qteditorfactory.cpp" and renamed to
// "EditorFactoryPrivate2" to avoid a duplicate implementation.

// ---------- EditorFactoryPrivate :
// Base class for editor factory private classes. Manages mapping of properties
// to editors and vice versa.

template <class Editor>
class EditorFactoryPrivate2
{
public:

    typedef QList<Editor *> EditorList;
    typedef QMap<QtProperty *, EditorList> PropertyToEditorListMap;
    typedef QMap<Editor *, QtProperty *> EditorToPropertyMap;

    Editor *createEditor(QtProperty *property, QWidget *parent);
    void initializeEditor(QtProperty *property, Editor *e);
    void slotEditorDestroyed(QObject *object);

    PropertyToEditorListMap  m_createdEditors;
    EditorToPropertyMap m_editorToProperty;
};

template <class Editor>
Editor *EditorFactoryPrivate2<Editor>::createEditor(QtProperty *property,
                                                    QWidget *parent)
{
    Editor *editor = new Editor(parent);
    initializeEditor(property, editor);
    return editor;
}

template <class Editor>
void EditorFactoryPrivate2<Editor>::initializeEditor(QtProperty *property,
                                                     Editor *editor)
{
    Q_TYPENAME PropertyToEditorListMap::iterator it = m_createdEditors.find(property);
    if (it == m_createdEditors.end())
        it = m_createdEditors.insert(property, EditorList());
    it.value().append(editor);
    m_editorToProperty.insert(editor, property);
}

template <class Editor>
void EditorFactoryPrivate2<Editor>::slotEditorDestroyed(QObject *object)
{
    const Q_TYPENAME EditorToPropertyMap::iterator ecend =
            m_editorToProperty.end();
    for (Q_TYPENAME EditorToPropertyMap::iterator itEditor =
         m_editorToProperty.begin(); itEditor !=  ecend; ++itEditor) {
        if (itEditor.key() == object) {
            Editor *editor = itEditor.key();
            QtProperty *property = itEditor.value();
            const Q_TYPENAME PropertyToEditorListMap::iterator pit =
                    m_createdEditors.find(property);
            if (pit != m_createdEditors.end()) {
                pit.value().removeAll(editor);
                if (pit.value().empty())
                    m_createdEditors.erase(pit);
            }
            m_editorToProperty.erase(itEditor);
            return;
        }
    }
}

// ************
// ************ end copy.


class QtToolButtonFactory;

class QtToolButtonFactoryPrivate :
        public EditorFactoryPrivate2<QtToolButtonBoolEdit>
{
    QtToolButtonFactory *q_ptr;
    Q_DECLARE_PUBLIC(QtToolButtonFactory)

public:
    void forwardClickedSignal();
};


/**
  Factory for instantiating QtToolButtonBoolEdits.

  Based on QtCheckBoxFactory in "qteditorfactory.h".
  */
class QT_QTPROPERTYBROWSER_EXPORT QtToolButtonFactory :
        public QtAbstractEditorFactory<QtClickPropertyManager>
{
    Q_OBJECT

public:
    QtToolButtonFactory(QObject *parent = 0);
    ~QtToolButtonFactory();

protected:
    /** Needs to be reimplemented, but not used here. */
    void connectPropertyManager(QtClickPropertyManager *manager)
    {
        Q_UNUSED(manager);
    }

    /**
      Creates a QtToolButtonBoolEdit editor and connects its clicked() signal
      with @ref forwardClickedSignal().
      */
    QWidget *createEditor(QtClickPropertyManager *manager, QtProperty *property,
                          QWidget *parent);

    /** Needs to be reimplemented, but not used here. */
    void disconnectPropertyManager(QtClickPropertyManager *manager)
    {
        Q_UNUSED(manager);
    }

private:
    QtToolButtonFactoryPrivate *d_ptr;

    Q_DECLARE_PRIVATE(QtToolButtonFactory)

    Q_DISABLE_COPY(QtToolButtonFactory)

    /**
      Finds the property that has been clicked and emits calls the
      QtClickPropertyManagers @ref emitClicked() slot. The manager will then
      emit a "propertyChanged()" signal to which the application can react.
      */
    Q_PRIVATE_SLOT(d_func(), void forwardClickedSignal())

    Q_PRIVATE_SLOT(d_func(), void slotEditorDestroyed(QObject *))
};


/******************************************************************************
***        Property manager for double values in scientific notation        ***
*******************************************************************************/

/**
  @brief QtScientificDoublePropertyManager provides a
  @ref QtDecoratedDoublePropertyManager whose editor spinbox can handle
  double values in scientific notation as input as well as as displayed text.

  Additionally, its spinbox can be augmented by a prefix and a suffix, the
  switch notation exponent (absolute exponent from which on the scientific
  notation is considered), the minimum exponent and the significant digits
  can be set.
 */
class QT_QTPROPERTYBROWSER_EXPORT QtScientificDoublePropertyManager
        : public QtDecoratedDoublePropertyManager
{
    Q_OBJECT
public:
    QtScientificDoublePropertyManager(QObject *parent = 0);
    ~QtScientificDoublePropertyManager();

    int significantDigits(const QtProperty *property) const;
    int switchNotationExponent(const QtProperty *property) const;
    int minimumExponent(const QtProperty *property) const;

    QString valueAsPropertyFormatedText(const QtProperty *property,
                                        const double value) const;

public Q_SLOTS:
    void setSignificantDigits(QtProperty *property,
                              const int &significantDigits);
    void setSwitchNotationExponent(QtProperty *property,
                                   const int &switchNotationExponent);
    void setMinimumExponent(QtProperty *property, const int &minExponent);

Q_SIGNALS:
    void switchNotationExponentChanged(QtProperty *property,
                                       const int &switchNotationExponent);
    void minimumExponentChanged(QtProperty *property,
                                const int &minimumExponent);
    void significantDigitsChanged(QtProperty *property,
                                  const int &significantDigits);

protected:
    QString valueText(const QtProperty *property) const;
    QString getTextFromValue(const QtProperty *property, double value) const;
    virtual void initializeProperty(QtProperty *property);
    virtual void uninitializeProperty(QtProperty *property);

    struct Data
    {
        Data()
            : switchNotationExponent(1),
              minimumExponent(120),
              significantDigits(2) {}
        int switchNotationExponent;
        int minimumExponent;
        int significantDigits;
    };

    QMap<const QtProperty *, Data> propertyToData;
};


/******************************************************************************
***          QtScientificDoublePropertyManager with Subproperties           ***
*******************************************************************************/

/**
  @brief QtConfigurableScientificDoublePropertyManager extends
  @ref QtScientificDoublePropertyManager by adding Subproperties to change
  single step, switch notation exponent and significant digits.
 */
class QT_QTPROPERTYBROWSER_EXPORT QtConfigurableScientificDoublePropertyManager
        : public QtScientificDoublePropertyManager
{
    Q_OBJECT
public:
    QtConfigurableScientificDoublePropertyManager(QObject *parent = 0);
    ~QtConfigurableScientificDoublePropertyManager();

    QtIntPropertyManager *subIntPropertyManager() const;
    QtScientificDoublePropertyManager *subSciDoublePropertyManager() const;

    QString configuration(const QtProperty *property) const;

public Q_SLOTS:
    void setConfiguration(QtProperty *property, const QString &config);
    void setSingleStep(QtProperty *property, const double &singleStep);
    void setSignificantDigits(QtProperty *property, const int &significantDigits);
    void setSwitchNotationExponent(QtProperty *property,
                                   const int &switchNotationExponent);
    void slotIntChanged(QtProperty *property, int value);
    void slotScientificDoubleChanged(QtProperty *property, double value);
    void slotPropertyDestroyed(QtProperty *property);

protected:
    virtual void initializeProperty(QtProperty *property);
    virtual void uninitializeProperty(QtProperty *property);

private:
    QtIntPropertyManager *intPropertyManager;
    QtScientificDoublePropertyManager *sciDoublePropertyManager;

    QMap<const QtProperty *, QtProperty *> propertyToSingleStep;
    QMap<const QtProperty *, QtProperty *> propertyToSignificantDigits;
    QMap<const QtProperty *, QtProperty *> propertyToSwitchNotationExponent;

    QMap<const QtProperty *, QtProperty *> singleStepToProperty;
    QMap<const QtProperty *, QtProperty *> significantDigitsToProperty;
    QMap<const QtProperty *, QtProperty *> switchNotationExponentToProperty;
};

// ************
// The following code has been copied partially from "qteditorfactory.cpp"
// and "qteditorfactory.h" and adapted to handle spin boxes with scientific
// notation.

/******************************************************************************
***        Property factory for double values in scientific notation        ***
*******************************************************************************/

class QtScientificDoubleSpinBoxFactory;

/**
  @brief Private class of @ref QtScientificDoubleSpinBoxFactory.

  Most of the slot-methods are copied from @ref QtDoubleSpinBoxFactoryPrivate.
 */
class QtScientificDoubleSpinBoxFactoryPrivate :
        public EditorFactoryPrivate2<MScientificDoubleSpinBox>
{
    QtScientificDoubleSpinBoxFactory *q_ptr;
    Q_DECLARE_PUBLIC(QtScientificDoubleSpinBoxFactory)

public:
    void slotSetValue(double value);
    void slotPropertyChanged(QtProperty *property, double value);
    void slotRangeChanged(QtProperty *property, double min, double max);
    void slotSingleStepChanged(QtProperty *property, double step);
    void slotMinimumExponentChanged(QtProperty *property, int prec);
    void slotSwitchNotationExponentChanged(QtProperty *property, int exponent);
    void slotSignificantDigitsChanged(QtProperty *property,
                                      int significantDigits);
    void slotPrefixChanged(QtProperty *property, const QString &prefix);
    void slotSuffixChanged(QtProperty *property, const QString &suffix);
    void slotReadOnlyChanged(QtProperty *property, bool readOnly);
};


/**
  @brief Factory for the @ref QtScientificDoublePropertyManager class.
 */
class QT_QTPROPERTYBROWSER_EXPORT QtScientificDoubleSpinBoxFactory
        : public QtAbstractEditorFactory<QtScientificDoublePropertyManager>
{
    Q_OBJECT
public:
    QtScientificDoubleSpinBoxFactory(QObject *parent = 0);
    ~QtScientificDoubleSpinBoxFactory();

protected:
    void connectPropertyManager(QtScientificDoublePropertyManager *manager);
    QWidget *createEditor(QtScientificDoublePropertyManager *manager,
                          QtProperty *property, QWidget *parent);
    void disconnectPropertyManager(QtScientificDoublePropertyManager *manager);

private:
    QtScientificDoubleSpinBoxFactoryPrivate *d_ptr;

    Q_DECLARE_PRIVATE(QtScientificDoubleSpinBoxFactory)

    Q_DISABLE_COPY(QtScientificDoubleSpinBoxFactory)

    Q_PRIVATE_SLOT(d_func(), void slotSetValue(double))
    Q_PRIVATE_SLOT(d_func(), void slotPropertyChanged(QtProperty *, double))
    Q_PRIVATE_SLOT(d_func(), void slotRangeChanged(QtProperty *, double, double))
    Q_PRIVATE_SLOT(d_func(), void slotSingleStepChanged(QtProperty *, double))
    Q_PRIVATE_SLOT(d_func(), void slotMinimumExponentChanged(QtProperty *, int))
    Q_PRIVATE_SLOT(d_func(), void slotSwitchNotationExponentChanged(QtProperty *, int))
    Q_PRIVATE_SLOT(d_func(), void slotSignificantDigitsChanged(QtProperty *, int))
    Q_PRIVATE_SLOT(d_func(), void slotReadOnlyChanged(QtProperty *, bool))
    Q_PRIVATE_SLOT(d_func(), void slotPrefixChanged(QtProperty *, const QString &))
    Q_PRIVATE_SLOT(d_func(), void slotSuffixChanged(QtProperty *, const QString &))
    Q_PRIVATE_SLOT(d_func(), void slotEditorDestroyed(QObject *))
};
// ************
// ************ end copy.

// TODO (bt, 19Jan2018): Change code to get a factory that can handle configurable SDManager.
//   Idea: cf. system/qtproperties.h
///**
//  @brief Factory for the @ref QtScientificDoublePropertyManager class.
// */
//class QT_QTPROPERTYBROWSER_EXPORT QtScientificDoubleSpinBoxFactory
//        : public QtAbstractEditorFactory<QtConfigurableScientificDoublePropertyManager>
//{
//    Q_OBJECT
//public:
//    QtScientificDoubleSpinBoxFactory(QObject *parent = 0);
//    ~QtScientificDoubleSpinBoxFactory();

//protected:
//    void connectPropertyManager(QtConfigurableScientificDoublePropertyManager *manager);
//    QWidget *createEditor(QtScientificDoublePropertyManager *manager,
//                          QtProperty *property, QWidget *parent);
//    void disconnectPropertyManager(QtScientificDoublePropertyManager *manager);

//private:
//    QtScientificDoubleSpinBoxFactoryPrivate *d_ptr;

//    Q_DECLARE_PRIVATE(QtScientificDoubleSpinBoxFactory)

//    Q_DISABLE_COPY(QtScientificDoubleSpinBoxFactory)

//    Q_PRIVATE_SLOT(d_func(), void slotSetValue(double))
//    Q_PRIVATE_SLOT(d_func(), void slotPropertyChanged(QtProperty *, double))
//    Q_PRIVATE_SLOT(d_func(), void slotRangeChanged(QtProperty *, double, double))
//    Q_PRIVATE_SLOT(d_func(), void slotSingleStepChanged(QtProperty *, double))
//    Q_PRIVATE_SLOT(d_func(), void slotDecimalsChanged(QtProperty *, int))
//    Q_PRIVATE_SLOT(d_func(), void slotSwitchNotationExponentChanged(QtProperty *, int))
//    Q_PRIVATE_SLOT(d_func(), void slotSignificantDigitsChanged(QtProperty *, int))
//    Q_PRIVATE_SLOT(d_func(), void slotReadOnlyChanged(QtProperty *, bool))
//    Q_PRIVATE_SLOT(d_func(), void slotPrefixChanged(QtProperty *, const QString &))
//    Q_PRIVATE_SLOT(d_func(), void slotSuffixChanged(QtProperty *, const QString &))
//    Q_PRIVATE_SLOT(d_func(), void slotEditorDestroyed(QObject *))
//};

} // namespace QtExtensions

#endif // QTROPERTYMANAGER_EXTENSIONS_H
