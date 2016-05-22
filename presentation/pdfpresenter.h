#ifndef PRESENTATION_PDFPRESENTER_H
#define PRESENTATION_PDFPRESENTER_H

#include <QWidget>
#include <QFutureWatcher>
#include <QPixmap>
#include <QLabel>
#include <memory>

namespace Poppler {
class Document;
}

namespace Presentation {

namespace Ui {
class PdfPresenter;
}

class PdfPresenter : public QWidget
{
    Q_OBJECT

public:
    static PdfPresenter* loadPdfFile(const QString& fileName);

    ~PdfPresenter();

signals:
    void closeRequested();
    void canNextPageChanged(bool can);
    void canPrevPageChanged(bool can);

    // supposed to be connected to PresentationWindow::presentWidget
    void presentWidgetRequest(QWidget *widget);

public:
    bool canNextPage() { return m_canNextPage; }
    bool canPrevPage() { return m_canPrevPage; }

public slots:
    void setScreen(const QRect& screen);
    void nextPage();
    void previousPage();

private slots:
    void closeBtnClicked();
    void itemSelected();
    void savePreview();
    void imageFinished();

private:
    explicit PdfPresenter(QWidget *parent = 0);

    void kickoffPreviewList();
    void updatePresentedPage();
    void resetPresentationPixmaps();

    Ui::PdfPresenter *ui;

    std::shared_ptr<Poppler::Document>  m_pdf                  = nullptr;
    QLabel                             *m_presentationImageLbl = nullptr;

    int presentationWidth() { if (m_presentationImageLbl) return m_presentationImageLbl->width(); else return 10; }
    int presentationHeight() { if (m_presentationImageLbl) return m_presentationImageLbl->height(); else return 10; }

    QFutureWatcher<QPixmap> *m_thisPage = nullptr;
    QFutureWatcher<QPixmap> *m_nextPage = nullptr;
    QFutureWatcher<QPixmap> *m_prevPage = nullptr;

    int m_currentPageNo = 0;

    bool m_canNextPage = false;
    bool m_canPrevPage = false;
    void setCanNextPage(bool can) {
        if (m_canNextPage != can) {
            m_canNextPage = can;
            emit canNextPageChanged(can);
        }
    }
    void setCanPrevPage(bool can) {
        if (m_canPrevPage != can) {
            m_canPrevPage = can;
            emit canPrevPageChanged(can);
        }
    }
};


} // namespace Presentation
#endif // PRESENTATION_PDFPRESENTER_H
