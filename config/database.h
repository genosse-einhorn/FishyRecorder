#ifndef CONFIG_DATABASE_H
#define CONFIG_DATABASE_H

#include <QObject>
#include <QString>
#include <sqlite3.h>
#include <vector>
#include <map>

namespace Config {

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = 0);
    ~Database();

    struct Track {
        uint64_t start;
        uint64_t length;
        QString  name;
    };

    QString readConfigString(const QString& key, const QString& fallbackValue = QString());
    double  readConfigDouble(const QString& key, double fallbackValue = NAN);
    std::vector<Track> readAllTracks();
    std::map<uint64_t,QString> readAllFiles();
    uint64_t readTotalSamplesRecorded();

signals:

public slots:
    void writeConfigString(const QString& key, const QString& value);
    void writeConfigDouble(const QString& key, double value);
    void insertTrack(uint64_t start, uint64_t length, const QString& name);
    void appendFile(uint64_t start, const QString& file);
    void updateTrackLength(uint64_t trackStart, uint64_t newLength);
    void updateTrackName(uint64_t trackStart, const QString& newName);
    void removeTrack(uint64_t trackStart);
    void clearTracksAndFiles();

private:
    sqlite3* m_db = nullptr;

    void repairLengthOfLastTrack();

};

} // namespace Config

#endif // CONFIG_DATABASE_H
