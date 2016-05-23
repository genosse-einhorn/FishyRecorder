#ifndef PRESENTATION_PDFPRESENTER_H
#define PRESENTATION_PDFPRESENTER_H

#include "presenterbase.h"

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

class PdfPresenter : public PresenterBase
{
    Q_OBJECT

public:
    static PdfPresenter* loadPdfFile(const QString& fileName, const QString &title = QString());
    ~PdfPresenter();

public:
    bool canNextPage() override { return m_canNextPage; }
    bool canPrevPage() override { return m_canPrevPage; }
    QString title() override { return m_title; }

public slots:
    void setScreen(const QRect& screen) override;
    void nextPage() override;
    void previousPage() override;
    void tabVisible() override;

private slots:
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
    QString m_title { "Dummy" };

    void setCanNextPage(bool can) {
        if (m_canNextPage != can) {
            m_canNextPage = can;
            emit controlsChanged();
        }
    }
    void setCanPrevPage(bool can) {
        if (m_canPrevPage != can) {
            m_canPrevPage = can;
            emit controlsChanged();
        }
    }
};


} // namespace Presentation
#endif // PRESENTATION_PDFPRESENTER_H
