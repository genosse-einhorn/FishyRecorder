#ifndef PRESENTATION_PDFPRESENTER_H
#define PRESENTATION_PDFPRESENTER_H

#include <QWidget>
#include <QFutureWatcher>
#include <QPixmap>
#include <memory>

class QLabel;

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
    QWidget                            *m_presentationWindow   = nullptr;
    QLabel                             *m_presentationImageLbl = nullptr;

    int presentationWidth() { if (m_presentationWindow) return m_presentationWindow->width(); else return 10; }
    int presentationHeight() { if (m_presentationWindow) return m_presentationWindow->height(); else return 10; }

    QFutureWatcher<QPixmap> *m_thisPage = nullptr;
    QFutureWatcher<QPixmap> *m_nextPage = nullptr;
    QFutureWatcher<QPixmap> *m_prevPage = nullptr;

    int m_currentPageNo = 0;
};


} // namespace Presentation
#endif // PRESENTATION_PDFPRESENTER_H
