#include "pdfpreviewwidget.h"

#include <poppler-qt5.h>

namespace {
    QPixmap createImage(std::shared_ptr<Poppler::Document> pdf, int pageno, int maxWidth, int maxHeight) {
        std::unique_ptr<Poppler::Page> page(pdf->page(pageno));
        if (!page)
            return QPixmap();

        //HACK: Poppler does floating point differently and somtimes ends up with interesting white
        // lines at the edges. Setting the page background color to black would fix the problem but
        // breaks most PDFs. The only solution is to crop a line of pixels. I'm sorry.

        // Okular has the same Problem. Evince gets it right, but I don't know how. Maybe it's because
        // Evince uses the Cairo backend, while the Qt5 binding can either use a custom Qt backend with
        // horrible results or the internal Splash backend, which creates those strange white lines.

        double xres = (maxWidth  + 1) / page->pageSizeF().width()  * 72.0;
        double yres = (maxHeight + 1) / page->pageSizeF().height() * 72.0;

        double res = std::min(xres, yres);


        int targetWidth  = int(res / 72.0 * page->pageSizeF().width()  - 0.5);
        int targetHeight = int(res / 72.0 * page->pageSizeF().height() - 0.5);

        QImage img = page->renderToImage(res, res, 0, 0, std::min(maxWidth, targetWidth), std::min(maxHeight, targetHeight));

        return QPixmap::fromImage(std::move(img));
    }
}

namespace Presentation {

PdfPreviewWidget::PdfPreviewWidget(QWidget *parent) : QLabel(parent)
{
    this->setAutoFillBackground(true);
    this->setBackgroundRole(QPalette::Dark);//TODO
    this->setAlignment(Qt::AlignCenter);
    this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    this->setPixmap(QPixmap());
    this->setMinimumSize(1, 1);

    this->m_watcher = new QFutureWatcher<QPixmap>(this);
    QObject::connect(m_watcher, &QFutureWatcher<QPixmap>::finished, this, &PdfPreviewWidget::imageFinished);
}

void PdfPreviewWidget::setDocument(std::shared_ptr<Poppler::Document> doc)
{
    this->m_doc = doc;

    this->kickoffRendering();
}

void PdfPreviewWidget::setPage(int no)
{
    this->m_page = no;

    this->kickoffRendering();
}

void PdfPreviewWidget::imageFinished()
{
    this->setPixmap(m_watcher->result());

    if (m_flagWaitForRendering)
        kickoffRendering();
}

void PdfPreviewWidget::kickoffRendering()
{
    m_flagWaitForRendering = m_watcher->isRunning();

    if (!m_flagWaitForRendering)
        m_watcher->setFuture(QtConcurrent::run(createImage, m_doc, m_page, this->width(), this->height()));
}

void PdfPreviewWidget::resizeEvent(QResizeEvent *)
{
    this->kickoffRendering();
}

} // namespace presentation
