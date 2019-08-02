/*
 * Copyright 2019 Florea Banus George <georgefb899@gmail.com>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KSharedConfig>
#include <KXmlGuiWindow>

#include "ui_settings.h"

class QProgressBar;
class QStandardItemModel;
class QTableView;
class QTreeView;
class View;
class Worker;
class QFileInfo;

class SettingsWidget: public QWidget, public Ui::SettingsWidget
{
    Q_OBJECT
public:
    explicit SettingsWidget(QWidget *parent) : QWidget(parent) {
        setupUi(this);
    }
};


class MainWindow : public KXmlGuiWindow
{
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    Qt::ToolBarArea mainToolBarArea();

    enum {
        IndexRole = Qt::UserRole,
        KeyRole,
        PathRole
    };

private:
    void init();
    void createMangaFoldersTree(QFileInfo mangaDirInfo);
    void setupActions();
    void addMangaFolder();
    void openMangaFolder();
    void openMangaArchive();
    void saveMangaFolders();
    void loadImages(QString path, bool recursive = false);
    void toggleFullScreen();
    void extractArchive(QString archivePath);
    void treeViewContextMenu(QPoint point);
    void hideDockWidgets(Qt::DockWidgetAreas area = Qt::AllDockWidgetAreas);
    void showDockWidgets(Qt::DockWidgetAreas area = Qt::AllDockWidgetAreas);
    void hideToolBars(Qt::ToolBarAreas area = Qt::AllToolBarAreas);
    void showToolBars(Qt::ToolBarAreas area = Qt::AllToolBarAreas);
    void onMouseMoved(QMouseEvent *event);
    void onAddBookmark(int pageNumber);
    void deleteBookmarks(QTableView *tableView, QString name);
    void openSettings();
    void toggleMenubar();
    bool isFullScreen();
    QMenu *populateMangaFoldersMenu();

    QMenu              *m_mangaFoldersMenu = nullptr;
    QProgressBar       *m_progressBar;
    KSharedConfig::Ptr  m_config;
    QStringList         m_images;
    QString             m_currentManga;
    View               *m_view;
    Worker             *m_worker;
    QThread            *m_thread;
    Qt::ToolBarArea     m_mainToolBarArea;
    QTreeView          *m_treeView;
    QString             m_tmpFolder;
    QStandardItemModel *m_bookmarksModel;
    QTableView         *m_bookmarksView;
    QAction            *m_selectMangaFolder;
    SettingsWidget     *m_settingsWidget = nullptr;
    int                 m_startPage = 0;
    bool                m_isLoadedRecursive = false;
    const QString       RECURSIVE_KEY_PREFIX = ":recursive:";
    void createBookmarksWidget();
};

#endif // MAINWINDOW_H
