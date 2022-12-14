/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Bianca Tost
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
#ifndef SESSIONMANAGERDIALOG_H
#define SESSIONMANAGERDIALOG_H

// standard library imports

// related third party imports
#include <QDialog>
#include <QFileSystemModel>
#include <QStyledItemDelegate>

// local application imports


namespace Ui {
class MSessionManagerDialog;
}


namespace Met3D
{

class MActor;
class MSessionFileSystemModel;
class MSessionItemDelegate;

/**
  @brief MSessionManagerDialog creates a dialog to handle session management
  and implements session management.

  The dialog presents the sessions stored in the seleted directory in a list
  and offers the user the possibilities to switch between sessions, create a
  new session storing the current state, delete sessions, clone sessions,
  load the currently selected session and save the current state to the
  currently selected session. Saving to the current session will create a new
  file if the old file has been deleted meanwhile. Additionally, it is possible
  to change the folder containing the currently handled session files and
  to activate an auto save mode and selecting its interval from a set of
  predefined periods.
 */
class MSessionManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MSessionManagerDialog(QWidget *parent = 0);
    ~MSessionManagerDialog();

    void initialize(QString sessionName, QString path, int autoSaveInterval,
                    bool loadOnStart, bool saveOnApplicationExit,
                    int maximumNumberOfSavedRevisions);

    void loadSessionOnStart();

    void loadWindowLayout();

    /**
      Switch to session with name @p sessionName in the current directory
      @ref path if @p sessionName is not equal to @ref currentSession.
     */
    void switchToSession(QString sessionName);

    /**
      Reverts current session to its revision with number @p revisionNumber.

      Builds the filename of the revision file and tries to load it.
     */
    void revertCurrentSessionToRevision(QString revisionNumber);

    bool getAutoSaveSession();

    bool getLoadSessionOnStart() { return loadOnStart; }

    /** File extension of files storing Met.3D sessions. */
    static QString fileExtension;

public slots:
    /**
      Opens dialog to change directory from where to load and to where to save
      sessions.
     */
    void changeDirectory();

    /**
      createNewSession asks the user to enter a name for the session and saves
      the session to a file with the given name.

      Rejects names which already exits in the list view.
     */
    void createNewSession(bool createEmptySession=true,
                          QString sessionName="defaultSession");

    /**
      Opens current session.
     */
    void reloadSession();

    /**
      autoSaveSession is called by @ref MMainWindow to trigger an auto save
      event.
     */
    void autoSaveSession();

    /**
      saveSession saves the current active session.
     */
    void saveSession(bool autoSave = false);

    /**
      Saves the current active session and asks the user to enter a new name.
     */
    void saveSessionAs();

    /**
      cloneSession clones the selected session by asking the user to enter a
      name for the session file to create and copying the file of the selected
      session.

      Rejects names which already exits in the list view.
     */
    void cloneSession();

    /**
      switchToSession switches to the selected session.
     */
    void switchTo();

    /**
      renameItem renames @param item by asking the user to enter a new name.

      Entering the original name leads to asking the user again without warning.
      Rejects names which already exits in the list view.
     */
    void renameItem(QModelIndex item);

    /**
     Fills @ref sessionsList with sessions stored in the currently selected
     directory.

     Is called everytime the directory or the files inside the directory
     change.
     */
    void fillSessionsList();

    void fillCurrentSessionHistoryList();

    /**
      Propagates changes of auto save interval.
     */
    void changeAutoSaveInterval(int autoSaveInterval);

    /**
      Checks if a session exists to be saved automatically.
     */
    void onAutoSaveToggeled(bool checked);

    /**
      deleteSession deletes the session currently selected in the list view.

      Asks the user for confirmation before deletion.
     */
    void deleteSession();

    bool saveSessionOnAppExit();

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    /**
      Updates text and tooltip of the label showing the current session to the
      current session name.

      If the session name is too long to fit the label's width, it is chopped to
      fit and three dots are added as a sign that the name was shorten.
     */
    void updateSessionLabel();

    void setSessionToCurrent(QString session);

    void resetSession();

    void saveSessionToFile(QString sessionName, bool autoSave = false);

    void loadSessionFromFile(QString sessionName, bool appendFileExtension=true);

    MActor *createActor(QString actorType);

    void blockGUIElements();

    void unblockGUIElements();

    /**
      Checks if session name is valid by testing if it is not empty and no other
      session with the same name exists.
     */
    bool isValidSessionName(QString sessionName);

    /**
      Returns the appendix " (index)" with index being the smallest, not
      negative number needed to get a unique name when appending appendix to @p
      sessionName. If sessionName is already unique, an empty string will be
      returned.
     */
    QString getAppendixWithSmallestIndexForUniqueName(QString sessionName);

    /**
      Updates history of a session by coping the file given by @p filename
      (path + filename + file extension) to
      ".[currentSessionName].[sessionFileExtension].[revisionNumber]" and by
      removing files with smallest indices if too many revision files exist.
     */
    void updateSessionFileHistory(QString filename);

    /**
      Gets list of revision file names of session called @p sessionName and
      stores the list int @p fileNameList.
      */
    void getCurrentSessionFileHistoryFileNameList(QStringList &fileNameList,
                                                  QString &sessionName);

    Ui::MSessionManagerDialog *ui;

    /** Name of the current active session. */
    QString currentSession;
    QString path;
    bool loadOnStart;
    MSessionItemDelegate *sessionItemDelegate;
    MSessionFileSystemModel *sessionFileSystemModel;
    /** Revision number the current session would get as a revision file. */
    int currentRevisionNumber;
    int maximumNumberOfSavedRevisions;
}; // MSessionManagerDialog


/**
  @brief MSessionFileSystemModel reimplements parts of QFileSystemModel to get
  rid of the icon and file extensions shown per default by QFileSystemModel.

  MSessionFileSystemModel is used in @ref MSessionManagerDialog to show a plain
  list of the session file names stored in a directory.
 */
class MSessionFileSystemModel : public QFileSystemModel
{

public:
    MSessionFileSystemModel(QWidget *parent = 0);
    ~MSessionFileSystemModel() {}

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
}; // MSessionFileSystemModel


/**
  @brief MSessionItemDelegate reimplements parts of QStyledItemDelegate to
  change font of the item called @ref currentSessionName to italic and bold.

  MSessionItemDelegate is used in @ref MSessionManagerDialog to highlight the
  currently selected session.
 */
class MSessionItemDelegate : public QStyledItemDelegate
{

public:
    MSessionItemDelegate(QWidget *parent = 0);
    ~MSessionItemDelegate() {}

    void paint(QPainter * painter, const QStyleOptionViewItem & option,
               const QModelIndex & index) const override;

    void setCurrentSessionName(QString name)
    { currentSessionName = name; }

private:
    QString currentSessionName;

}; // MSessionItemDelegate

} // namespace Met3D

#endif // SESSIONMANAGERDIALOG_H
