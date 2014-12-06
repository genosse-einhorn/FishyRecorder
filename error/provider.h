#ifndef ERRORPROVIDER_H
#define ERRORPROVIDER_H

#include <QObject>

namespace Error {

class Provider : public QObject
{
    Q_OBJECT
public:
    explicit Provider(QObject *parent = 0);

    enum class ErrorType {
        NoError,
        Notice,
        TemporaryWarning, //! should automatically disappear after a few seconds
        Warning,
        Error
    };

signals:
    /*!
     * \brief Signals a potential consumer that there is an error condition.
     * \param severity The type of error
     * \param message1 The "title" message, which should contain a short description of the error (rich text)
     * \param message2 The "subtitle" message, which may contain a detailed explanation (richt text)
     *
     * A consumer of an error provider should connect to this signal only.
     */
    void error(ErrorType severity, const QString& message1, const QString& message2);

public slots:
    void setError(ErrorType severity, const QString& message1, const QString& message2);
    void simulate(ErrorType severity, const QString& error);
    void clearError();
    void emitErrorAgain();

private:
    QString   message1;
    QString   message2;
    ErrorType severity = ErrorType::NoError;
};

} // namespace Error

Q_DECLARE_METATYPE(Error::Provider::ErrorType)

#endif // ERRORPROVIDER_H
