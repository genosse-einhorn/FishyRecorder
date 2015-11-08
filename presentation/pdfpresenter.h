#ifndef PRESENTATION_PDFPRESENTER_H
#define PRESENTATION_PDFPRESENTER_H

#include <QWidget>
#include <QFutureWatcher>
#include <QPixmap>
#include <memory>

typedef struct _PopplerDocument PopplerDocument;
class QLabel;

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

Q_SIGNALS:
    void closeRequested();
    void canNextPageChanged(bool can);
    void canPrevPageChanged(bool can);

public:
    bool canNextPage() { return m_canNextPage; }
    bool canPrevPage() { return m_canPrevPage; }

public Q_SLOTS:
    void setScreen(const QRect& screen);
    void nextPage();
    void previousPage();

private Q_SLOTS:
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

    PopplerDocument *m_pdf                  = nullptr;
    QWidget         *m_presentationWindow   = nullptr;
    QLabel          *m_presentationImageLbl = nullptr;

    int presentationWidth() { if (m_presentationWindow) return m_presentationWindow->width(); else return 10; }
    int presentationHeight() { if (m_presentationWindow) return m_presentationWindow->height(); else return 10; }

    QFutureWatcher<QPixmap> *m_thisPage = nullptr;
    QFutureWatcher<QPixmap> *m_nextPage = nullptr;
    QFutureWatcher<QPixmap> *m_prevPage = nullptr;

    int m_currentPageNo = 0;

    bool m_canNextPage = false;
    bool m_canPrevPage = false;
    void setCanNextPage(bool can) {
        if (m_canNextPage != can) {
            m_canNextPage = can;
            Q_EMIT canNextPageChanged(can);
        }
    }
    void setCanPrevPage(bool can) {
        if (m_canPrevPage != can) {
            m_canPrevPage = can;
            Q_EMIT canPrevPageChanged(can);
        }
    }
};


} // namespace Presentation
#endif // PRESENTATION_PDFPRESENTER_H
