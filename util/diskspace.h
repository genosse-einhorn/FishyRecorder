#ifndef UTIL_DISKSPACE_H
#define UTIL_DISKSPACE_H

#include <QString>
#include <QObject>

namespace Util {

/*!
 * Fixme: Get rid of this when we move to Qt5.4
 */
class DiskSpace : QObject
{
    Q_OBJECT

    // disable instantiation
    DiskSpace() { Q_UNREACHABLE(); }

public:
    static uint64_t availableBytesForPath(const QString& path);

    static QString humanReadableForPath(const QString& path, bool withRecordingTime=false);
    static QString humanReadable(uint64_t space, bool withRecordingTime=false);
};

} // namespace Util

#endif // UTIL_DISKSPACE_H
