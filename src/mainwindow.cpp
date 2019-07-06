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

#include "_debug.h"
#include "mainwindow.h"
#include "view.h"
#include "worker.h"
#include "settings.h"

#include <KActionCollection>
#include <KConfigDialog>
#include <KConfigGroup>
#include <KToolBar>
#include <KLocalizedString>

#include <QtWidgets>
#include <QThread>

#include <QArchive>

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent)
{
    // setup central widget
    auto centralWidget = new QWidget(this);
    auto centralWidgetLayout = new QVBoxLayout(centralWidget);
    centralWidgetLayout->setContentsMargins(0, 0, 0, 0);
    setCentralWidget(centralWidget);

    m_config = KSharedConfig::openConfig("mangareader/mangareader.conf");

    init();
    setupActions();
    setupGUI(QSize(1280, 720), Default, "mangareaderui.rc");
    if (MangaReaderSettings::mangaFolders().count() < 2) {
        m_selectMangaFolder->setVisible(false);
    }

    connect(m_view, &View::mouseMoved,
            this, &MainWindow::onMouseMoved);

    showToolBars();
    showDockWidgets();
    menuBar()->show();
    statusBar()->hide();
}

MainWindow::~MainWindow()
{
    m_thread->quit();
    m_thread->wait();
}

Qt::ToolBarArea MainWindow::mainToolBarArea()
{
    return m_mainToolBarArea;
}

void MainWindow::init()
{
    // ==================================================
    // setup progress bar
    // ==================================================
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setVisible(false);
    centralWidget()->layout()->addWidget(m_progressBar);

    // ==================================================
    // setup view
    // ==================================================
    m_view = new View(this);
    connect(m_view, &View::addBookmark,
            this, &MainWindow::onAddBookmark);
    centralWidget()->layout()->addWidget(m_view);

    // ==================================================
    // setup thread and worker
    // ==================================================
    m_worker = Worker::instance();
    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished,
            m_worker, &Worker::deleteLater);
    connect(m_thread, &QThread::finished,
            m_thread, &QThread::deleteLater);
    m_thread->start();

    // ==================================================
    // setup dock widgets
    // ==================================================

    // ==================================================
    // tree dock widget
    // ==================================================
    KConfigGroup rootGroup = m_config->group("");
    QFileInfo mangaDirInfo(rootGroup.readEntry("Manga Folder"));
    if (!mangaDirInfo.absoluteFilePath().isEmpty()) {
        createMangaFoldersTree(mangaDirInfo);
    }

    // ==================================================
    // bookmarks dock widget
    // ==================================================
    KConfigGroup bookmarksGroup = m_config->group("Bookmarks");
    if (bookmarksGroup.keyList().count() > 0) {
        createBookmarksWidget();
    }
}

void MainWindow::createMangaFoldersTree(QFileInfo mangaDirInfo)
{
    auto treeDock = new QDockWidget(mangaDirInfo.baseName(), this);
    auto treeModel = new QFileSystemModel(this);
    m_treeView = new QTreeView(this);

    treeDock->setObjectName("treeDock");

    treeModel->setObjectName("mangaTree");
    treeModel->setRootPath(mangaDirInfo.absoluteFilePath());
    treeModel->setFilter(QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot);
    treeModel->setNameFilters(QStringList() << "*.zip" << "*.7z" << "*.cbz");
    treeModel->setNameFilterDisables(false);

    m_treeView->setModel(treeModel);
    m_treeView->setRootIndex(treeModel->index(mangaDirInfo.absoluteFilePath()));
    m_treeView->setColumnHidden(1, true);
    m_treeView->setColumnHidden(2, true);
    m_treeView->setColumnHidden(3, true);
    m_treeView->header()->hide();
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_treeView, &QTreeView::doubleClicked,
            this, [ = ](const QModelIndex &index) {
                // get path from index
                const QFileSystemModel *model = static_cast<const QFileSystemModel *>(index.model());
                QString path = model->filePath(index);
                m_currentManga = path;
                loadImages(path);
            });
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &MainWindow::treeViewContextMenu);

    treeDock->setWidget(m_treeView);
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);
    resizeDocks({treeDock}, {400}, Qt::Horizontal);
}

void MainWindow::createBookmarksWidget()
{
    auto bookmarksLayout = new QVBoxLayout();
    auto bookmarksWidget = new QWidget(this);
    auto dockWidget = new QDockWidget();
    dockWidget->setWindowTitle(i18n("Bookmarks"));
    dockWidget->setObjectName("bookmarksDockWidget");
    dockWidget->setMinimumHeight(300);

    m_bookmarksModel = new QStandardItemModel(0, 2, this);
    m_bookmarksModel->setHorizontalHeaderItem(0, new QStandardItem(i18n("Manga")));
    m_bookmarksModel->setHorizontalHeaderItem(1, new QStandardItem(i18n("Page")));

    KConfigGroup bookmarks = m_config->group("Bookmarks");
    for (const QString key : bookmarks.keyList()) {
        QString value = bookmarks.readEntry(key);
        QString path = key;
        if (path.startsWith(RECURSIVE_KEY_PREFIX)) {
            path = path.remove(0, RECURSIVE_KEY_PREFIX.length());
        }
        QFileInfo pathInfo(path);
        QList<QStandardItem *> rowData;
        QMimeDatabase db;
        QMimeType type = db.mimeTypeForFile(pathInfo.absoluteFilePath());
        QIcon icon = QIcon::fromTheme("folder");
        if (type.name().startsWith("application/")) {
            icon = QIcon::fromTheme("application-zip");
        }
        QString displayPrefix = (key.startsWith(RECURSIVE_KEY_PREFIX)) ? "(r) " : QStringLiteral();
        auto col1 = new QStandardItem(displayPrefix + pathInfo.fileName());
        col1->setData(icon, Qt::DecorationRole);
        col1->setData(key, Qt::UserRole);
        col1->setData(pathInfo.absoluteFilePath(), Qt::ToolTipRole);
        col1->setEditable(false);
        auto col2 = new QStandardItem(value);
        col2->setEditable(false);
        m_bookmarksModel->appendRow(rowData << col1 << col2);
    }
    m_bookmarksView = new QTableView();
    m_bookmarksView->setObjectName("bookmarksTableView");
    m_bookmarksView->setModel(m_bookmarksModel);
    m_bookmarksView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_bookmarksView->setWordWrap(false);
    m_bookmarksView->setTextElideMode(Qt::ElideRight);
    m_bookmarksView->verticalHeader()->hide();

    auto tableHeader = m_bookmarksView->horizontalHeader();
    tableHeader->setSectionResizeMode(0, QHeaderView::Stretch);
    tableHeader->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    connect(m_bookmarksView, &QTableView::doubleClicked, this, [ = ](const QModelIndex &index) {
        // get path from index
        QModelIndex pathIndex = m_bookmarksModel->index(index.row(), 0);
        QModelIndex pageNumberIndex = m_bookmarksModel->index(index.row(), 1);
        m_startPage  = m_bookmarksModel->data(pageNumberIndex, Qt::DisplayRole).toInt();
        QString path = m_bookmarksModel->data(pathIndex, Qt::ToolTipRole).toString();
        QString key  = m_bookmarksModel->data(pathIndex, Qt::UserRole).toString();
        m_currentManga = path;
        if (key.startsWith(RECURSIVE_KEY_PREFIX)) {
            loadImages(path, true);
        } else {
            loadImages(path);
        }
    });

    auto deleteButton = new QPushButton();
    deleteButton->setText(i18n("Delete Selected Bookmarks"));
    connect(deleteButton, &QPushButton::clicked, this, [ = ]() {
        deleteBookmarks(m_bookmarksView, "Bookmarks");
    });

    bookmarksLayout->addWidget(m_bookmarksView);
    bookmarksLayout->addWidget(deleteButton);
    bookmarksWidget->setLayout(bookmarksLayout);
    dockWidget->setWidget(bookmarksWidget);
    addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
}

void MainWindow::addMangaFolder()
{
    openSettings();
    m_settingsWidget->addMangaFolder->click();
}

void MainWindow::openMangaArchive()
{
    QString file = QFileDialog::getOpenFileName(
                this,
                i18n("Open Archive"),
                QDir::homePath(),
                i18n("Archives (*.zip *.rar *.7z *.cbz *.cbt *.cbr)"));
    if (file.isEmpty()) {
        return;
    }
    loadImages(file, true);
}

void MainWindow::openMangaFolder()
{
    QString path = QFileDialog::getExistingDirectory(this, i18n("Open folder"), QDir::homePath());
    if (path.isEmpty()) {
        return;
    }
    loadImages(path, true);
}

void MainWindow::loadImages(QString path, bool recursive)
{
    m_isLoadedRecursive = recursive;
    const QFileInfo fileInfo(path);
    QString mangaPath = fileInfo.absoluteFilePath();
    if (fileInfo.isFile()) {
        // extract files to a temporary location
        // when finished call this function with the temporary location and recursive = true
        extractArchive(fileInfo.absoluteFilePath());
        return;
    }

    m_images.clear();

    // get images from path
    auto it = new QDirIterator(mangaPath, QDir::Files, QDirIterator::NoIteratorFlags);
    if (recursive) {
        it = new QDirIterator(mangaPath, QDir::Files, QDirIterator::Subdirectories);
    }
    while (it->hasNext()) {
        QString file = it->next();
        QMimeDatabase db;
        QMimeType type = db.mimeTypeForFile(file);
        // only get images
        if (type.name().startsWith("image/")) {
            m_images.append(file);
        }
    }
    // natural sort images
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(m_images.begin(), m_images.end(), collator);

    if (m_images.count() < 1) {
        return;
    }
    m_worker->setImages(m_images);
    m_view->reset();
    m_view->setStartPage(m_startPage);
    m_view->setManga(mangaPath);
    m_view->setImages(m_images);
    m_view->loadImages();
    m_startPage = 0;
}

void MainWindow::setupActions()
{
    auto addMangaFolder = new QAction(this);
    addMangaFolder->setText(i18n("&Add Manga Folder"));
    addMangaFolder->setIcon(QIcon::fromTheme("folder-add"));
    actionCollection()->addAction("addMangaFolder", addMangaFolder);
    actionCollection()->setDefaultShortcut(addMangaFolder, Qt::CTRL + Qt::Key_A);
    connect(addMangaFolder, &QAction::triggered,
            this, &MainWindow::addMangaFolder);

    auto openMangaFolder = new QAction(this);
    openMangaFolder->setText(i18n("&Open Manga Folder"));
    openMangaFolder->setIcon(QIcon::fromTheme("folder-open"));
    actionCollection()->addAction("openMangaFolder", openMangaFolder);
    actionCollection()->setDefaultShortcut(openMangaFolder, Qt::CTRL + Qt::Key_O);
    connect(openMangaFolder, &QAction::triggered,
            this, &MainWindow::openMangaFolder);

    auto openMangaArchive = new QAction(this);
    openMangaArchive->setText(i18n("&Open Manga Archive"));
    openMangaArchive->setIcon(QIcon::fromTheme("application-zip"));
    actionCollection()->addAction("openMangaArchive", openMangaArchive);
    actionCollection()->setDefaultShortcut(openMangaArchive, Qt::CTRL + Qt::SHIFT + Qt::Key_O);
    connect(openMangaArchive, &QAction::triggered,
            this, &MainWindow::openMangaArchive);

    m_mangaFoldersMenu = new QMenu();
    populateMangaFoldersMenu();
    m_selectMangaFolder = new QAction(this);
    m_selectMangaFolder->setMenu(m_mangaFoldersMenu);
    m_selectMangaFolder->setText(i18n("Select Manga Folder"));
    actionCollection()->addAction("selectMangaFolder", m_selectMangaFolder);
    connect(m_selectMangaFolder, &QAction::triggered, this, [ = ]() {
        QWidget *widget = toolBar("mainToolBar")->widgetForAction(m_selectMangaFolder);
        QToolButton *button = qobject_cast<QToolButton*>(widget);
        button->showMenu();
    });

    auto spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    spacer->setVisible(true);
    auto spacerAction = new QWidgetAction(this);
    spacerAction->setDefaultWidget(spacer);
    spacerAction->setText(i18n("Spacer"));
    actionCollection()->addAction("spacer", spacerAction);

    KStandardAction::showMenubar(this, &MainWindow::toggleMenubar, actionCollection());
    KStandardAction::preferences(this, &MainWindow::openSettings, actionCollection());
    KStandardAction::quit(qApp, &QCoreApplication::quit, actionCollection());

    QAction *action = KStandardAction::fullScreen(this, [ = ]() {toggleFullScreen();}, this, actionCollection());
    connect(m_view, &View::doubleClicked,
            action, &QAction::trigger);
}

void MainWindow::toggleMenubar()
{
    menuBar()->isHidden() ? menuBar()->show() : menuBar()->hide();
}

bool MainWindow::isFullScreen()
{
    return (windowState() == (Qt::WindowFullScreen | Qt::WindowMaximized))
            || (windowState() == Qt::WindowFullScreen);
}

QMenu *MainWindow::populateMangaFoldersMenu()
{
    m_mangaFoldersMenu->clear();
    for (QString mangaFolder : MangaReaderSettings::mangaFolders()) {
       QAction *action = m_mangaFoldersMenu->addAction(mangaFolder);
       connect(action, &QAction::triggered, this, [ = ]() {
           QFileSystemModel *treeModel = static_cast<QFileSystemModel*>(m_treeView->model());
           treeModel->setRootPath(mangaFolder);
           m_treeView->setRootIndex(treeModel->index(mangaFolder));
           findChild<QDockWidget*>("treeDock")->setWindowTitle(QFileInfo(mangaFolder).baseName());
           m_config->group("").writeEntry("Manga Folder", mangaFolder);
           m_config->sync();
        });
    }
    return m_mangaFoldersMenu;
}

void MainWindow::extractArchive(QString archivePath)
{
    QFileInfo extractionFolder(MangaReaderSettings::extractionFolder());
    QFileInfo archivePathInfo(archivePath);
    if (!extractionFolder.exists() || !extractionFolder.isWritable()) {
        return;
    }
    // delete previous extracted folder
    QFileInfo file(m_tmpFolder);
    if (file.exists() && file.isDir() && file.isWritable()) {
        QDir dir(m_tmpFolder);
        dir.removeRecursively();
    }
    m_tmpFolder = extractionFolder.absoluteFilePath() + "/" + archivePathInfo.baseName().toLatin1();
    QDir dir(m_tmpFolder);
    if (!dir.exists()) {
        dir.mkdir(m_tmpFolder);
    }

    auto extractor = new QArchive::DiskExtractor(this);
    extractor->setArchive(archivePathInfo.absoluteFilePath());
    extractor->setOutputDirectory(m_tmpFolder);
    extractor->start();

    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    connect(extractor, &QArchive::DiskExtractor::finished,
            this, [ = ]() {
                m_progressBar->setVisible(false);
                loadImages(m_tmpFolder, true);
            });

    connect(extractor, &QArchive::DiskExtractor::progress,
            this, [ = ](QString file, int processedFiles, int totalFiles, int percent) {
                m_progressBar->setValue(percent);
            });
}

void MainWindow::treeViewContextMenu(QPoint point)
{
    QModelIndex index = m_treeView->indexAt(point);
    QFileSystemModel *model = static_cast<QFileSystemModel *>(m_treeView->model());
    QString path = model->filePath(index);
    QFileInfo pathInfo(path);

    auto menu = new QMenu();
    menu->setMinimumWidth(200);

    auto load = new QAction(QIcon::fromTheme("arrow-down"), i18n("Load"));
    m_treeView->addAction(load);

    auto loadRecursive = new QAction(QIcon::fromTheme("arrow-down-double"), i18n("Load recursive"));
    m_treeView->addAction(loadRecursive);

    auto openPath = new QAction(QIcon::fromTheme("unknown"), i18n("Open"));
    m_treeView->addAction(openPath);

    auto openContainingFolder = new QAction(QIcon::fromTheme("folder-open"), i18n("Open containing folder"));
    m_treeView->addAction(openContainingFolder);

    menu->addAction(load);
    menu->addAction(loadRecursive);
    menu->addSeparator();
    menu->addAction(openPath);
    menu->addAction(openContainingFolder);

    connect(load, &QAction::triggered,
            this, [ = ]() {
                // get path from index
                const QFileSystemModel *model = static_cast<const QFileSystemModel *>(index.model());
                QString path = model->filePath(index);
                m_currentManga = path;
                loadImages(path);
            });
    connect(loadRecursive, &QAction::triggered,
            this, [ = ]() {
                // get path from index
                const QFileSystemModel *model = static_cast<const QFileSystemModel *>(index.model());
                QString path = model->filePath(index);
                m_currentManga = path;
                loadImages(path, true);
            });

    connect(openPath, &QAction::triggered,
            this, [ = ]() {
                QString nativePath = QDir::toNativeSeparators(pathInfo.absoluteFilePath());
                QDesktopServices::openUrl(QUrl::fromLocalFile(nativePath));
            });

    connect(openContainingFolder, &QAction::triggered,
            this, [ = ]() {
                QString nativePath = QDir::toNativeSeparators(pathInfo.absolutePath());
                QDesktopServices::openUrl(QUrl::fromLocalFile(nativePath));
            });
    menu->exec(QCursor::pos());
}

void MainWindow::hideDockWidgets(Qt::DockWidgetAreas area)
{
    QList<QDockWidget *> dockWidgets = findChildren<QDockWidget *>();
    for (QDockWidget *dockWidget : dockWidgets) {
        if ((dockWidgetArea(dockWidget) == area || area == Qt::AllDockWidgetAreas) && !dockWidget->isFloating()) {
            dockWidget->hide();
        }
    }
}

void MainWindow::showDockWidgets(Qt::DockWidgetAreas area)
{
    QList<QDockWidget *> dockWidgets = findChildren<QDockWidget *>();
    for (int i = dockWidgets.size(); i > 0; i--) {
        if ((dockWidgetArea(dockWidgets.at(i-1)) == area || area == Qt::AllDockWidgetAreas)
                && !dockWidgets.at(i-1)->isFloating()) {
            dockWidgets.at(i-1)->show();
        }
    }
}

void MainWindow::hideToolBars(Qt::ToolBarAreas area)
{
    QList<QToolBar *> toolBars = findChildren<QToolBar *>();
    for (QToolBar *toolBar : toolBars) {
        if ((toolBarArea(toolBar) == area || area == Qt::AllToolBarAreas) && !toolBar->isFloating()) {
            toolBar->hide();
        }
    }
}

void MainWindow::showToolBars(Qt::ToolBarAreas area)
{
    QList<QToolBar *> toolBars = findChildren<QToolBar *>();
    for (QToolBar *toolBar : toolBars) {
        if ((toolBarArea(toolBar) == area || area == Qt::AllToolBarAreas) && !toolBar->isFloating()) {
            toolBar->show();
        }
    }
}

void MainWindow::onMouseMoved(QMouseEvent *event)
{
    if(!isFullScreen()) {
        return;
    }
    if (event->y() < 50) {
        showDockWidgets(Qt::TopDockWidgetArea);
        showToolBars(Qt::TopToolBarArea);
    } else if (event->y() > m_view->height() - 50) {
        showDockWidgets(Qt::BottomDockWidgetArea);
        showToolBars(Qt::BottomToolBarArea);
    } else if (event->x() < 50) {
        showDockWidgets(Qt::LeftDockWidgetArea);
        showToolBars(Qt::LeftToolBarArea);
    } else if (event->x() > m_view->width() - 50) {
        showDockWidgets(Qt::RightDockWidgetArea);
        showToolBars(Qt::RightToolBarArea);
    } else {
        hideDockWidgets();
        hideToolBars();
    }
}

void MainWindow::onAddBookmark(int pageNumber)
{
    QDockWidget *bookmarksWidget = findChild<QDockWidget *>("bookmarksDockWidget");
    if (!bookmarksWidget) {
        createBookmarksWidget();
    }
    QFileInfo mangaInfo(m_currentManga);
    QString keyPrefix = (m_isLoadedRecursive) ? RECURSIVE_KEY_PREFIX : QStringLiteral();
    QString key = keyPrefix + mangaInfo.absoluteFilePath();
    // get the bookmark from the config file
    m_config->reparseConfiguration();
    KConfigGroup bookmarksGroup = m_config->group("Bookmarks");
    QString bookmark = bookmarksGroup.readEntry(key);
    // if the bookmark from the config is the same
    // as the one to be saved return
    if (QString::number(pageNumber) == bookmark) {
        return;
    }

    bookmarksGroup.writeEntry(key, QString::number(pageNumber));
    bookmarksGroup.config()->sync();

    // check if there is a bookmark for this manga in the bookmarks tableView
    // if found update the page number
    for (int i = 0; i < m_bookmarksModel->rowCount(); i++) {
        QStandardItem *itemPath = m_bookmarksModel->item(i);
        if (key == itemPath->data(Qt::UserRole)) {
            QStandardItem *itemNumber = m_bookmarksModel->item(i, 1);
            m_bookmarksView->model()->setData(itemNumber->index(), pageNumber);
            return;
        }
    }

    // determine icon for bookmark (folder or archive)
    QMimeDatabase db;
    QMimeType type = db.mimeTypeForFile(mangaInfo.absoluteFilePath());
    QIcon icon = QIcon::fromTheme("folder");
    if (type.name().startsWith("application/")) {
        icon = QIcon::fromTheme("application-zip");
    }

    // add bookmark to tableView
    QList<QStandardItem *> rowData;
    QString displayPrefix = (m_isLoadedRecursive) ? "(r) " : "";
    auto *col1 = new QStandardItem(displayPrefix + mangaInfo.fileName());
    col1->setData(icon, Qt::DecorationRole);
    col1->setData(key, Qt::UserRole);
    col1->setData(mangaInfo.absoluteFilePath(), Qt::ToolTipRole);
    col1->setEditable(false);
    auto *col2 = new QStandardItem(QString::number(pageNumber));
    col2->setEditable(false);
    m_bookmarksModel->appendRow(rowData << col1 << col2);
}

void MainWindow::deleteBookmarks(QTableView *tableView, QString name)
{
    QItemSelection selection(tableView->selectionModel()->selection());
    QVector<int> rows;
    int prev = -1;
    // get the rows to be deleted
    for (const QModelIndex &index : selection.indexes()) {
        int current = index.row();
        if (prev != current) {
            rows.append(index.row());
            prev = current;
        }
    }

    // starting from beginning causes the index of the following items to change
    // removing item at index 0 will cause item at index 1 to become item at index 0
    // starting from the end prevents this
    // removing item at index 3 will cause an index change for indexes bigger than 3
    // but since we go in the opposite direction we don't care about those items
    for (int i = rows.count() - 1; i >= 0; i--) {
        // first delete from config file
        // deleting from table view first causes problems with index change
        QModelIndex cell_0 = tableView->model()->index(rows.at(i), 0);
        QModelIndex cell_1 = tableView->model()->index(rows.at(i), 1);
        QString key   = tableView->model()->data(cell_0, Qt::ToolTipRole).toString();
        QString value = tableView->model()->data(cell_1).toString();
        m_config->reparseConfiguration();
        KConfigGroup bookmarksGroup = m_config->group("Bookmarks");
        QString bookmarks = bookmarksGroup.readEntry(key);
        bookmarksGroup.deleteEntry(key);
        bookmarksGroup.config()->sync();
        tableView->model()->removeRow(rows.at(i));
    }
}

void MainWindow::openSettings()
{
    if (m_settingsWidget == nullptr) {
        m_settingsWidget = new SettingsWidget(nullptr);
    }
    m_settingsWidget->mangaFolders->clear();
    m_settingsWidget->mangaFolders->addItems(MangaReaderSettings::mangaFolders());

    if (KConfigDialog::showDialog("settings")) {
        return;
    }
    auto dialog = new KConfigDialog(
             this, "settings", MangaReaderSettings::self());
    dialog->setMinimumSize(700, 600);
    dialog->setFaceType(KPageDialog::Plain);
    dialog->addPage(m_settingsWidget, i18n("Settings"));
    dialog->show();

    connect(dialog, &KConfigDialog::settingsChanged,
            m_view, &View::onSettingsChanged);

    connect(m_settingsWidget->selectExtractionFolder, &QPushButton::clicked, this, [ = ]() {
        QString path = QFileDialog::getExistingDirectory(
                    this,
                    i18n("Select extraction folder"),
                    MangaReaderSettings::extractionFolder());
        if (path.isEmpty()) {
            return;
        }
        m_settingsWidget->kcfg_ExtractionFolder->setText(path);
    });

    // add manga folder
    connect(m_settingsWidget->addMangaFolder, &QPushButton::clicked, this, [ = ]() {
        QString path = QFileDialog::getExistingDirectory(this, i18n("Select manga folder"), QDir::homePath());
        if (path.isEmpty() || MangaReaderSettings::mangaFolders().contains(path)) {
            return;
        }
        m_settingsWidget->mangaFolders->addItem(path);
        dialog->button(QDialogButtonBox::Apply)->setEnabled(true);
        if (MangaReaderSettings::mangaFolders().count() > 1) {
            m_selectMangaFolder->setVisible(true);
        }
    });

    // delete manga folder
    connect(m_settingsWidget->deleteMangaFolder, &QPushButton::clicked, this, [ = ]() {
        // delete selected item from list widget
        QList<QListWidgetItem*> selectedItems = m_settingsWidget->mangaFolders->selectedItems();
        for (QListWidgetItem *item : selectedItems) {
            delete item;
        }
        dialog->button(QDialogButtonBox::Apply)->setEnabled(true);
    });

    connect(dialog->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &MainWindow::saveMangaFolders);
    connect(dialog->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, [ = ]() {
        saveMangaFolders();
        dialog->button(QDialogButtonBox::Apply)->setDisabled(true);
    });
}

void MainWindow::saveMangaFolders()
{
    QStringList mangaFolders;
    for(int i = 0; i < m_settingsWidget->mangaFolders->count(); ++i) {
        mangaFolders << m_settingsWidget->mangaFolders->item(i)->text();
    }
    MangaReaderSettings::setMangaFolders(mangaFolders);
    MangaReaderSettings::self()->save();

    // update the manga folder selection menu
    m_selectMangaFolder->setMenu(populateMangaFoldersMenu());
    QDockWidget *treeWidget = findChild<QDockWidget *>("treeDock");
    if (MangaReaderSettings::mangaFolders().count() == 0) {
        m_config->group(QStringLiteral()).writeEntry("Manga Folder", QStringLiteral());
        m_config->sync();
        delete treeWidget;
    } else if (MangaReaderSettings::mangaFolders().count() > 0) {
        delete treeWidget;
        createMangaFoldersTree(MangaReaderSettings::mangaFolders().at(0));
        m_config->group("").writeEntry("Manga Folder", MangaReaderSettings::mangaFolders().at(0));
        m_config->sync();

        m_selectMangaFolder->setVisible(false);
        if (MangaReaderSettings::mangaFolders().count() > 1) {
            m_selectMangaFolder->setVisible(true);
        }
    }
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        setWindowState(windowState() & ~Qt::WindowFullScreen);
        showToolBars();
        showDockWidgets();
        menuBar()->show();
    } else {
        setWindowState(windowState() | Qt::WindowFullScreen);
        hideToolBars();
        hideDockWidgets();
        menuBar()->hide();
    }
}
