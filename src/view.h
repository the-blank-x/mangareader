/*
 * SPDX-FileCopyrightText: 2019 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef VIEW_H
#define VIEW_H

#include <QGraphicsView>
#include <QObject>
#include <KXMLGUIClient>

class KArchive;
class Page;
class QGraphicsScene;
class MainWindow;

class View : public QGraphicsView, public KXMLGUIClient
{
    Q_OBJECT

public:
    View(MainWindow *parent);
    ~View() = default;
    void reset();
    void loadImages();
    void goToPage(int number);
    auto imageCount() -> int;
    void setStartPage(int number);
    const QString &manga() const;
    void setManga(const QString &manga);
    void setFiles(const QStringList &files);
    void setArchive(KArchive *newArchive);

    void setLoadFromMemory(bool newLoadFromMemory);

    bool event(QEvent *event) override;

Q_SIGNALS:
    void imagesLoaded(int number);
    void requestDriveImage(int number, const QString &path);
    void requestMemoryImage(int number, const QByteArray &data);
    void currentImageChanged(int number);
    void doubleClicked();
    void mouseMoved(QMouseEvent *event);
    void addBookmark(int number);
    void fileDropped(const QString &file);

public Q_SLOTS:
    void onImageReady(const QImage &image, int number);
    void onImageResized(const QImage &image, int number);
    void onScrollBarRangeChanged(int x, int y);
    void refreshPages();
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void togglePageZoom(Page *page);

private:
    void setupActions();
    void createPages();
    void calculatePageSizes();
    void setPagesVisibility();
    void addRequest(int number);
    void delRequest(int number);
    auto hasRequest(int number) const -> bool;
    void scrollContentsBy(int dx, int dy) override;
    auto isInView  (int imgTop, int imgBot) -> bool;
    void resizeEvent(QResizeEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

    QGraphicsScene  *m_scene{};
    QString          m_manga;
    QStringList      m_files;
    QVector<Page*>   m_pages;
    QVector<int>     m_start;
    QVector<int>     m_end;
    QVector<int>     m_requestedPages;
    int              m_startPage = 0;
    int              m_firstVisible = -1;
    float            m_firstVisibleOffset = 0.0f;
    double           m_globalZoom = 1.0;
    QTimer          *m_resizeTimer{};
    KArchive        *m_archive {};
    bool m_loadFromMemory {false};
};

#endif // VIEW_H
