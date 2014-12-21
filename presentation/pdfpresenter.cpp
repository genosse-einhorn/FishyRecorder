#include "pdfpresenter.h"
#include "ui_pdfpresenter.h"

#include <poppler-qt5.h>
#include <QGridLayout>
#include <QLabel>
#include <QDebug>
#include <QIcon>
#include <QPixmap>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QtWinExtras>
#include <windows.h>
#include <math.h>
#include <memory>
#include <vector>

namespace {
    const int ICON_SIZE = 200;

    std::pair<int /*pageno */, QPixmap> createThumbnail(std::shared_ptr<Poppler::Document> pdf, int pageno) {
        if (!pdf || pageno < 0 || pageno >= pdf->numPages())
            return std::pair<int, QPixmap>(-1, QPixmap());

        std::unique_ptr<Poppler::Page> page(pdf->page(pageno));

        if (!page)
            return std::pair<int, QPixmap>(-1, QPixmap());

        double xres = ICON_SIZE/page->pageSizeF().width()*72;
        double yres = ICON_SIZE/page->pageSizeF().height()*72;

        double res = qMin(xres, yres);

        return std::pair<int, QPixmap>(pageno, QPixmap::fromImage(page->renderToImage(res, res)));
    }

    QPixmap createImage(std::shared_ptr<Poppler::Document> pdf, int pageno, int width, int height) {
        std::unique_ptr<Poppler::Page> page(pdf->page(pageno));
        if (!page)
            return QPixmap();

        double xres = width/page->pageSizeF().width()*72;
        double yres = height/page->pageSizeF().height()*72;

        double res = qMin(xres, yres);

        QImage img = page->renderToImage(res, res);

        //HACK: Poppler will round the coordinates, which might lead to a
        // single row or column filled with the page background color.
        // This might be unfortunate if we wanted to display a completely
        // black page (since the page background is white and can not be
        // changed since a lot of PDFs expect it to be white). Worst case scenario
        // is chopping off a completely fine line of pixels.
        int expected_width = floor(page->pageSizeF().width()*res/72);
        int expected_height = floor(page->pageSizeF().height()*res/72);

        QPixmap pixmap(expected_width, expected_height);
        QPainter(&pixmap).drawImage(0, 0, img);

        return pixmap;
    }
}

namespace Presentation {

PdfPresenter::PdfPresenter(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PdfPresenter)
{
    ui->setupUi(this);

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

    for (int i = 0; i < m_pdf->numPages(); ++i) {
        QListWidgetItem *item = new QListWidgetItem(QString("%1").arg(i+1));

        ui->slideList->addItem(item);

        auto *watcher = new QFutureWatcher<std::pair<int, QPixmap>>(this);
        QObject::connect(watcher, &QFutureWatcher<std::pair<int, QPixmap>>::finished, this, &PdfPresenter::savePreview);

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

    if (watcher == m_thisPage)
        m_presentationImageLbl->setPixmap(watcher->result());
}

void PdfPresenter::updatePresentedPage()
{
    if (!m_presentationWindow || !m_presentationImageLbl)
        return;

    Poppler::Page *page = m_pdf->page(m_currentPageNo);
    if (!page)
        return;

    if (ui->slideList->currentRow() != m_currentPageNo) {
        ui->slideList->setCurrentRow(m_currentPageNo);
        ui->slideList->scrollToItem(ui->slideList->item(m_currentPageNo), QAbstractItemView::PositionAtCenter);
    }

    if (m_thisPage->isFinished())
        m_presentationImageLbl->setPixmap(m_thisPage->result());
    else
        m_presentationImageLbl->setPixmap(QPixmap());
}

PdfPresenter *PdfPresenter::loadPdfFile(const QString &fileName)
{
    Poppler::Document *doc = Poppler::Document::load(fileName);

    doc->setRenderHint(Poppler::Document::Antialiasing, true);
    doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    doc->setRenderHint(Poppler::Document::TextHinting, true);
    doc->setRenderHint(Poppler::Document::TextSlightHinting, true);

    if (!doc)
        return nullptr;

    PdfPresenter *presenter = new PdfPresenter();
    presenter->m_pdf.reset(doc);

    presenter->kickoffPreviewList();
    presenter->updatePresentedPage();

    return presenter;
}

PdfPresenter::~PdfPresenter()
{
    delete ui;

    qDebug() << "Deleting the presentation window";
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
    if (m_currentPageNo + 1 >= m_pdf->numPages())
        return;

    m_currentPageNo += 1;
    auto *oldPrev = m_prevPage;
    m_prevPage = m_thisPage;
    m_thisPage = m_nextPage;
    m_nextPage = oldPrev;
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
    m_prevPage->setFuture(QtConcurrent::run(createImage, m_pdf, m_currentPageNo-1, presentationWidth(), presentationHeight()));

    updatePresentedPage();
}


void PdfPresenter::resetPresentationPixmaps()
{
    m_prevPage->setFuture(QtConcurrent::run(createImage, m_pdf, m_currentPageNo-1, presentationWidth(), presentationHeight()));
    m_thisPage->setFuture(QtConcurrent::run(createImage, m_pdf, m_currentPageNo  , presentationWidth(), presentationHeight()));
    m_nextPage->setFuture(QtConcurrent::run(createImage, m_pdf, m_currentPageNo+1, presentationWidth(), presentationHeight()));
}

void PdfPresenter::closeBtnClicked()
{
    emit closeRequested();
}

void PdfPresenter::itemSelected()
{
    auto items = ui->slideList->selectedItems();
    if (items.size() < 1)
        return;

    int pageNo = items.first()->text().toInt() - 1;
    if (pageNo < 0 || pageNo >= m_pdf->numPages() || m_currentPageNo == pageNo)
        return;

    m_currentPageNo = pageNo;
    resetPresentationPixmaps();

    updatePresentedPage();
}

} // namespace Presentation
