#ifndef FANCYPROGRESSBAR_H
#define FANCYPROGRESSBAR_H

#include <QProgressBar>

namespace Recording {

class FancyProgressBar : public QWidget
{
    Q_OBJECT
public:
    explicit FancyProgressBar(QWidget *parent = 0);

signals:

public slots:
    void setValue(float value);


    // QWidget interface
public:
    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;

protected:
    virtual void paintEvent(QPaintEvent *);

private:
    float m_value = 0.0f;
};

} // namespace Recording

#endif // FANCYPROGRESSBAR_H
