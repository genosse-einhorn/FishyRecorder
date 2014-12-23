#ifndef FANCYPROGRESSBAR_H
#define FANCYPROGRESSBAR_H

#include <QProgressBar>

class FancyProgressBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit FancyProgressBar(QWidget *parent = 0);

signals:

public slots:


    // QWidget interface
public:
    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;

protected:
    virtual void paintEvent(QPaintEvent *);
};

#endif // FANCYPROGRESSBAR_H
