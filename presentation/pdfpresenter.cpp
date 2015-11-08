// Conflicts with glib
#define QT_NO_KEYWORDS

#include "pdfpresenter.h"
#include "ui_pdfpresenter.h"

#include "external/waitingspinnerwidget.h"

#include <poppler.h>
#include <cairo.h>
#include <QGridLayout>
#include <QLabel>
#include <QDebug>
#include <QIcon>
#include <QPixmap>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QtWinExtras>
#include <windows.h>
#include <cmath>
#include <memory>
#include <vector>

namespace {
    const int ICON_SIZE = 200;

    QPixmap createImage(PopplerDocument *pdf, int pageno, int maxWidth, int maxHeight) {
        if ((pageno < 0) || (pageno >= poppler_document_get_n_pages(pdf))) {
            g_object_unref(pdf);
            return QPixmap();
        }

        PopplerPage *page = poppler_document_get_page(pdf, pageno);

        double size_x, size_y;
        poppler_page_get_size(page, &size_x, &size_y);

        double xscale = (maxWidth+1) / size_x;
        double yscale = (maxHeight+1) / size_y;
        double scale = std::min(xscale, yscale);

        QImage img(std::min(int(size_x * scale), maxWidth),
                   std::min(int(size_y * scale), maxHeight),
                   QImage::Format_ARGB32_Premultiplied);
        img.fill(0);

        cairo_surface_t *sf = cairo_image_surface_create_for_data(img.bits(), CAIRO_FORMAT_ARGB32, img.width(), img.height(), img.bytesPerLine());
        cairo_t *cr = cairo_create(sf);

        cairo_scale(cr, scale, scale);
        poppler_page_render(page, cr);

        cairo_destroy(cr);
        cairo_surface_destroy(sf);

        g_object_unref(page);
        g_object_unref(pdf);
        return QPixmap::fromImage(std::move(img));

    }

    std::pair<int /*pageno */, QPixmap> createThumbnail(PopplerDocument *pdf, int pageno) {
        return std::pair<int, QPixmap>(pageno, createImage(pdf, pageno, ICON_SIZE, ICON_SIZE));
    }
}

namespace Presentation {

PdfPresenter::PdfPresenter(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PdfPresenter)
{
    ui->setupUi(this);
    ui->spinner->setInnerRadius(5);
    ui->spinner->setLineLength(5);

    QObject::connect(ui->deleteBtn, &QAbstractButton::clicked, this, &PdfPresenter::closeBtnClicked);
    QObject::connect(ui->slideList, &QListWidget::itemSelectionChanged, this, &PdfPresenter::itemSelected);

    m_thisPage = new QFutureWatcher<QPixmap>(this);
    m_nextPage = new QFutureWatcher<QPixmap>(this);
    m_prevPage = new QFutureWatcher<QPixmap>(this);

    QObject::connect(m_thisPage, &QFutureWatcher<QPixmap>::finished, this, &PdfPresenter::imageFinished);
    QObject::connect(m_nextPage, &QFutureWatcher<QPixmap>::finished, this, &PdfPresenter::imageFinished);
    QObject::connect(m_prevPage, &QFutureWatcher<QPixmap>::finished, this, &PdfPresenter::imageFinished);
}

void PdfPresenter::kickoffPreviewList()
{
    if (!m_pdf)
        return;

    ui->slideList->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    ui->slideList->setFlow(QListView::TopToBottom);
    ui->slideList->setWrapping(false);
    ui->slideList->setMaximumWidth(ICON_SIZE + 50);

    for (int i = 0; i < poppler_document_get_n_pages(m_pdf); ++i) {
        QListWidgetItem *item = new QListWidgetItem(QString("%1").arg(i+1));

        ui->slideList->addItem(item);

        auto *watcher = new QFutureWatcher<std::pair<int, QPixmap>>(this);
        QObject::connect(watcher, &QFutureWatcher<std::pair<int, QPixmap>>::finished, this, &PdfPresenter::savePreview);

        g_object_ref(m_pdf);
        watcher->setFuture(QtConcurrent::run(createThumbnail, m_pdf, i));
    }
}


void PdfPresenter::savePreview()
{
    auto *watcher = static_cast<QFutureWatcher<std::pair<int, QPixmap>>*>(QObject::sender());

    const std::pair<int, QPixmap> &thumbnail = watcher->result();

    ui->slideList->setRowHidden(thumbnail.first, true);

    auto *item = ui->slideList->item(thumbnail.first);
    if (item)
        item->setIcon(QIcon(thumbnail.second));

    ui->slideList->setRowHidden(thumbnail.first, false);

    watcher->deleteLater();
}

void PdfPresenter::imageFinished()
{
    auto *watcher = static_cast<QFutureWatcher<QPixmap>*>(QObject::sender());

    if (watcher == m_thisPage) {
        m_presentationImageLbl->setPixmap(watcher->result());
        ui->spinner->stop();
    }
}

void PdfPresenter::updatePresentedPage()
{
    setCanPrevPage(m_currentPageNo > 0);
    setCanNextPage(m_currentPageNo < poppler_document_get_n_pages(m_pdf)-1);

    if (!m_presentationWindow || !m_presentationImageLbl)
        return;

    if (ui->slideList->currentRow() != m_currentPageNo) {
        ui->slideList->setCurrentRow(m_currentPageNo);
        ui->slideList->scrollToItem(ui->slideList->item(m_currentPageNo), QAbstractItemView::PositionAtCenter);
    }

    if (m_thisPage->isFinished()) {
        m_presentationImageLbl->setPixmap(m_thisPage->result());
        ui->spinner->stop();
    } else {
        m_presentationImageLbl->setPixmap(QPixmap());
        ui->spinner->start();
    }
}

PdfPresenter *PdfPresenter::loadPdfFile(const QString &fileName)
{
    QByteArray utf8_uri = QUrl::fromLocalFile(fileName).url().toUtf8();
    PopplerDocument *doc = poppler_document_new_from_file(utf8_uri.data(), nullptr, nullptr);
    if (!doc)
        return nullptr;

    PdfPresenter *presenter = new PdfPresenter();
    presenter->m_pdf = doc;

    presenter->kickoffPreviewList();

    return presenter;
}

PdfPresenter::~PdfPresenter()
{
    delete ui;
    g_object_unref(m_pdf);

    delete m_presentationWindow;
}

void PdfPresenter::setScreen(const QRect &screen)
{
    delete m_presentationWindow;

    m_presentationWindow = new QWidget();
    m_presentationImageLbl = new QLabel();

    m_presentationImageLbl->setStyleSheet("background-color:black;");
    m_presentationImageLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QGridLayout *gridLayout = new QGridLayout(m_presentationWindow);
    gridLayout->setMargin(0);
    gridLayout->addWidget(m_presentationImageLbl, 0, 0, 1, 1, Qt::AlignCenter);

    m_presentationWindow->setLayout(gridLayout);

    QtWin::setWindowExcludedFromPeek(m_presentationWindow, true);

    // black background
    QPalette pal(m_presentationWindow->palette());
    pal.setColor(QPalette::Background, Qt::black);
    m_presentationWindow->setAutoFillBackground(true);
    m_presentationWindow->setPalette(pal);

    m_presentationWindow->show();

    // now that we have the window, position it
    m_presentationWindow->setWindowFlags(m_presentationWindow->windowFlags() | Qt::FramelessWindowHint | Qt::Tool);
    m_presentationWindow->setWindowState(Qt::WindowFullScreen);
    m_presentationWindow->setGeometry(screen.left(), screen.top(), screen.width(), screen.height());
    m_presentationWindow->setVisible(true);

    resetPresentationPixmaps();
    updatePresentedPage();

    this->focusWidget();
    this->topLevelWidget()->raise();
}

void PdfPresenter::nextPage()
{
    if (m_currentPageNo + 1 >= poppler_document_get_n_pages(m_pdf))
        return;

    m_currentPageNo += 1;
    auto *oldPrev = m_prevPage;
    m_prevPage = m_thisPage;
    m_thisPage = m_nextPage;
    m_nextPage = oldPrev;
    g_object_ref(m_pdf);
    m_nextPage->setFuture(QtConcurrent::run(createImage, m_pdf, m_currentPageNo+1, presentationWidth(), presentationHeight()));

    updatePresentedPage();
}

void PdfPresenter::previousPage()
{
    if (m_currentPageNo - 1 < 0)
        return;

    m_currentPageNo -= 1;
    auto *oldNext = m_nextPage;
    m_nextPage = m_thisPage;
    m_thisPage = m_prevPage;
    m_prevPage = oldNext;
    g_object_ref(m_pdf);
    m_prevPage->setFuture(QtConcurrent::run(createImage, m_pdf, m_currentPageNo-1, presentationWidth(), presentationHeight()));

    updatePresentedPage();
}


void PdfPresenter::resetPresentationPixmaps()
{
    for (int i = 0; i < 3; ++i) g_object_ref(m_pdf);
    m_thisPage->setFuture(QtConcurrent::run(createImage, m_pdf, m_currentPageNo  , presentationWidth(), presentationHeight()));
    m_prevPage->setFuture(QtConcurrent::run(createImage, m_pdf, m_currentPageNo-1, presentationWidth(), presentationHeight()));
    m_nextPage->setFuture(QtConcurrent::run(createImage, m_pdf, m_currentPageNo+1, presentationWidth(), presentationHeight()));
}

void PdfPresenter::closeBtnClicked()
{
    Q_EMIT closeRequested();
}

void PdfPresenter::itemSelected()
{
    auto items = ui->slideList->selectedItems();
    if (items.size() < 1)
        return;

    int pageNo = items.first()->text().toInt() - 1;
    if (pageNo < 0 || pageNo >= poppler_document_get_n_pages(m_pdf) || m_currentPageNo == pageNo)
        return;

    m_currentPageNo = pageNo;
    resetPresentationPixmaps();

    updatePresentedPage();
}

} // namespace Presentation
