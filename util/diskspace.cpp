#include "diskspace.h"

#include <QDebug>
#include <QFile>
#include <QByteArray>

#ifdef Q_OS_WIN32

#include <windows.h>

namespace Util {

uint64_t DiskSpace::availableBytesForPath(const QString &path)
{
    uint64_t freeBytes;
    if (GetDiskFreeSpaceEx((const wchar_t*)path.utf16(), nullptr, nullptr, (PULARGE_INTEGER)&freeBytes)) {
        return freeBytes;
    } else {
        qWarning() << "Error while receiving free disk space, programmer was too lazy";

        return 0;
    }
}

} // namespace Util

#else

#include <sys/statvfs.h>

namespace Util {

uint64_t DiskSpace::availableBytesForPath(const QString &path)
{
    QByteArray localFilename = QFile::encodeName(path);
    struct statvfs result;

    if (statvfs(localFilename.data(), &result) == 0) {
        return (uint64_t)result.f_bavail * (uint64_t)result.f_frsize;
    } else {
        qWarning() << "statvfs(3) failed:" << strerror(errno);

        return 0;
    }
}


} // namespace Util

#endif

namespace Util {

QString DiskSpace::humanReadableForPath(const QString &path, bool withRecordingTime)
{
    return humanReadable(availableBytesForPath(path), withRecordingTime);
}

QString DiskSpace::humanReadable(uint64_t space, bool withRecordingTime)
{
    if (space == 0)
        return tr("Unknown");

    double bytes = space;
    int i = 0;
    const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    while (bytes > 1024) {
        bytes /= 1024;
        i++;
    }

    QString sizeStr = QString("%1%2").arg(bytes, 0, 'f', 2).arg(units[i]);

    if (!withRecordingTime)
        return sizeStr;

    uint64_t samples = space/4;
    uint64_t minutes = samples/48000/60 % 60;
    uint64_t hours   = samples/48000/60/60;

    QString timeStr;

    if (hours > 10)
        timeStr = QString("%1h").arg(hours);
    else
        timeStr = QString("%1:%2h").arg(hours).arg(minutes);

    return QString("%1 (%2)").arg(sizeStr).arg(timeStr);
}

} // namespace Util


