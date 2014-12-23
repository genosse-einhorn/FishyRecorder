#include "boolsignalor.h"

namespace Util {

BoolSignalOr::BoolSignalOr(QObject *parent) :
    QObject(parent)
{
}

void BoolSignalOr::inputA(bool value)
{
    m_a = value;
    output(m_a || m_b);
}

void BoolSignalOr::inputB(bool value)
{
    m_b = value;
    output(m_a || m_b);
}

} // namespace Util
