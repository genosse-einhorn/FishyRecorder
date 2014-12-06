#include "error/provider.h"

namespace {
    void ensureErrorTypeEnumRegistered() {
        static int errorTypeId = qRegisterMetaType<Error::Provider::ErrorType>("ErrorType");
        Q_UNUSED(errorTypeId);
    }
}

Error::Provider::Provider(QObject *parent) :
    QObject(parent)
{
    ensureErrorTypeEnumRegistered();
}

void
Error::Provider::setError(ErrorType severity, const QString &message1, const QString &message2)
{
    this->severity = severity;
    this->message1 = message1;
    this->message2 = message2;

    emit error(severity, message1, message2);
}

void Error::Provider::simulate(Error::Provider::ErrorType severity, const QString &error)
{
    QString message1 = QString("Simulated Error: %1").arg(error);
    QString message2 = QString("An error of type `%1' has been simulated for debugging purposes.").arg(error);

    setError(severity, message1, message2);
}

void
Error::Provider::clearError()
{
    this->severity = ErrorType::NoError;
    this->message1.clear();
    this->message2.clear();

    emit error(severity, message1, message2);
}

void
Error::Provider::emitErrorAgain()
{
    emit error(severity, message1, message2);
}
