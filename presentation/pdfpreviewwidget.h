#ifndef PDFPREVIEWWIDGET_H
#define PDFPREVIEWWIDGET_H

#include <QLabel>
#include <QtConcurrent>
#include <memory>

namespace Poppler { class Document; }

namespace Presentation {

class PdfPreviewWidget : public QLabel
{
    Q_OBJECT
public:
    PdfPreviewWidget(QWidget *parent = nullptr);

    std::shared_ptr<Poppler::Document> document() { return m_doc; }
    int pageNo() { return m_page; }

public slots:
    void setDocument(std::shared_ptr<Poppler::Document> m_doc);
    void setPage(int no);

private slots:
    void imageFinished();

private:
    std::shared_ptr<Poppler::Document> m_doc;
    int m_page = 0;
    bool m_flagWaitForRendering = false;

    QFutureWatcher<QPixmap> *m_watcher;

    void kickoffRendering();

    // QWidget interface
protected:
    void resizeEvent(QResizeEvent *);
};

} // namespace Presentation

#endif // PDFPREVIEWWIDGET_H
