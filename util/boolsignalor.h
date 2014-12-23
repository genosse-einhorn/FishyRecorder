#ifndef UTIL_BOOLSIGNALOR_H
#define UTIL_BOOLSIGNALOR_H

#include <QObject>

namespace Util {

class BoolSignalOr : public QObject
{
    Q_OBJECT
public:
    explicit BoolSignalOr(QObject *parent = 0);

signals:
    void output(bool value);

public slots:
    void inputA(bool value);
    void inputB(bool value);

private:
    bool m_a = false;
    bool m_b = false;
};

} // namespace Util

#endif // UTIL_BOOLSIGNALOR_H
