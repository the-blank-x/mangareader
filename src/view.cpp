/*
 * Copyright 2019 George Florea Banus
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "_debug.h"
#include "mainwindow.h"
#include "page.h"
#include "view.h"
#include "worker.h"
#include "settings.h"

#include <KToolBar>

#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>

View::View(QWidget *parent)
    : QGraphicsView(parent)
{
    setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);

    setBackgroundBrush(QColor(MangaReaderSettings::backgroundColor()));
    setCacheMode(QGraphicsView::CacheBackground);

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    connect(this, &View::requestPage,
            Worker::instance(), &Worker::processImageRequest);

    connect(Worker::instance(), &Worker::imageReady,
            this, &View::onImageReady);

    connect(Worker::instance(), &Worker::imageResized,
            this, &View::onImageResized);

    connect(verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &View::onScrollBarRangeChanged);
}

void View::reset()
{
    m_requestedPages.clear();
    verticalScrollBar()->setValue(0);
}

void View::loadImages()
{
    createPages();
    calculatePageSizes();
    setPagesVisibility();
}

void View::goToPage(int number)
{
    verticalScrollBar()->setValue(m_start[number]);
}

void View::setStartPage(int number)
{
    m_startPage = number;
}

void View::createPages()
{
    for (Page *page : m_pages) {
        delete page;
    }
    m_pages.clear();
    m_start.clear();
    m_end.clear();
    m_start.resize(m_images.count());
    m_end.resize(m_images.count());
    int w = viewport()->width() - 10;
    int h = viewport()->height() - 10;
    // create page for each image
    if (m_images.count() > 0) {
        int y = 0;
        for (int i = 0; i < m_images.count(); i++) {
            Page *p = new Page(w, h, i);
            p->setView(this);
            m_pages.append(p);
            m_scene->addItem(p);
        }
    }
}

void View::calculatePageSizes()
{
    if (m_pages.count() > 0) {
        int avgw = 0;
        int avgh = 0;
        int n = 0;
        for (int i = 0; i < m_pages.count(); i++) {
            // find loaded pages
            Page *p = m_pages.at(i);
            p->setMaxWidth(MangaReaderSettings::maxWidth());
            if (p->scaledSize().width() > 0) {
                const QSize s(p->scaledSize());
                avgw += s.width();
                avgh += s.height();
                ++n;
            }
        }
        if (n > 0) {
            avgw /= n;
            avgh /= n;
        }
        int y = 0;

        for (int i = 0; i < m_pages.count(); i++) {
            Page *p = m_pages.at(i);
            if (n > 0 && !p->scaledSize().width() > 0) {
                p->setEstimatedSize(QSize(avgw, avgh));
                p->redrawImage();
            }
            p->setPos(0, y);

            const int x = (viewport()->width() - p->scaledSize().width()) / 2;
            p->setPos(x, p->y());

            int height = p->scaledSize().height() > 0 ? p->scaledSize().height() : viewport()->height() - 20;
            m_start[i] = y;
            m_end[i] = y + height;
            y += height + MangaReaderSettings::pageSpacing();
        }
    }
    m_scene->setSceneRect(m_scene->itemsBoundingRect());
}

void View::setPagesVisibility()
{
    const int vy1 = verticalScrollBar()->value();
    const int vy2 = vy1 + viewport()->height();

    m_firstVisible = -1;
    m_firstVisibleOffset = 0.0f;

    for (Page *page : m_pages) {
        // page is visible on the screen but its image not loaded
        int pageNumber = page->number();
        if (isInView(m_start[pageNumber], m_end[pageNumber])) {
            if (page->isImageDeleted()) {
                addRequest(pageNumber);
            }
            if (m_firstVisible < 0) {
                m_firstVisible = pageNumber;
                // hidden portion (%) of page
                m_firstVisibleOffset = static_cast<double>(vy1 - m_start[pageNumber]) / page->scaledSize().height();
            }
        } else {
            // page is not visible but its image is loaded
            bool isPrevPageInView = isInView(m_start[pageNumber - 1], m_end[pageNumber - 1]);
            bool isNextPageInView = isInView(m_start[pageNumber + 1], m_end[pageNumber + 1]);
            if (!page->isImageDeleted()) {
                // delete page image unless it is before or after a page that is in view
                if (!(isPrevPageInView || isNextPageInView)) {
                    page->deleteImage();
                    delRequest(pageNumber);
                }
            } else { // page is not visible and its image not loaded
                // if previous page is visible load current page image too
                if (isPrevPageInView) {
                    addRequest(pageNumber);
                } else {
                    delRequest(pageNumber);
                }
            }
        }
    }
}

void View::addRequest(int number)
{
    if (hasRequest(number)) {
        return;
    }
    m_requestedPages.append(number);
    emit requestPage(number);
}

bool View::hasRequest(int number) const
{
    return m_requestedPages.indexOf(number) >= 0;
}

void View::delRequest(int number)
{
    int idx = m_requestedPages.indexOf(number);
    if (idx >= 0) {
        m_requestedPages.removeAt(idx);
    }
}

void View::onImageReady(QImage image, int number)
{
    m_pages.at(number)->setImage(image);
    calculatePageSizes();
    if (m_startPage > 0) {
        goToPage(m_startPage);
        m_startPage = 0;
    }
    setPagesVisibility();
}

void View::onImageResized(QImage image, int number)
{
    m_pages.at(number)->redraw(image);
    m_scene->setSceneRect(m_scene->itemsBoundingRect());
}

void View::onScrollBarRangeChanged(int x, int y)
{
    if (m_firstVisible >= 0)
    {
        int pageHeight = (m_end[m_firstVisible] - m_start[m_firstVisible]);
        int offset = m_start[m_firstVisible] + static_cast<int>(m_firstVisibleOffset * pageHeight);
        verticalScrollBar()->setValue(offset);
    }
}

void View::onSettingsChanged()
{
    // clear requested pages so they are resized too
    m_requestedPages.clear();
    setBackgroundBrush(QColor(MangaReaderSettings::backgroundColor()));

    if (maximumWidth() != MangaReaderSettings::maxWidth()) {
        for (Page *page: m_pages) {
            if (!page->isImageDeleted()) {
                page->deleteImage();
            }
        }
    }
    calculatePageSizes();
    setPagesVisibility();
}

bool View::isInView(int imgTop, int imgBot)
{
    const int vy1 = verticalScrollBar()->value();
    const int vy2 = vy1 + viewport()->height();
    return std::min(imgBot, vy2) > std::max(imgTop, vy1);
}

void View::resizeEvent(QResizeEvent *e)
{
    for (Page *p : m_pages) {
        p->redrawImage();
    }
    calculatePageSizes();
    QGraphicsView::resizeEvent(e);
}

void View::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    emit doubleClicked();
}

void View::mouseMoveEvent(QMouseEvent *event)
{
    emit mouseMoved(event);
}

void View::contextMenuEvent(QContextMenuEvent *event)
{
    QPoint position = mapFromGlobal(event->globalPos());
    Page *page;
    if (QGraphicsItem *item = itemAt(position)) {
        page = qgraphicsitem_cast<Page *>(item);
        QMenu *menu = new QMenu();
        menu->addAction(QIcon::fromTheme("folder-bookmark"), i18n("Set Bookmark"), this, [ = ] {
            emit addBookmark(page->number());
        });
        menu->popup(event->globalPos());
    }
}

void View::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    setPagesVisibility();
}

void View::setManga(QString manga)
{
    m_manga = manga;
}

void View::setImages(QStringList images)
{
    m_images = images;
}
