#include "database.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

namespace {
    bool upgradeDatabase(sqlite3* db) {
        int result;

        result = sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr);
        if (result != SQLITE_OK) {
            qWarning() << "Begin of transaction failed:" << sqlite3_errmsg(db);
            goto rollback;
        }

        result = sqlite3_exec(db,
                              "CREATE TABLE IF NOT EXISTS config ("
                              "   key TEXT PRIMARY KEY,"
                              "   val INTEGER"  /* note: we may store text in here */
                              ")", nullptr, nullptr, nullptr);
        if (result != SQLITE_OK)
            goto display_warning;

        // version upgrades
        {
            // read the current version of the database
            int dbVersion = 0;
            sqlite3_stmt *stmt = nullptr;

            result = sqlite3_prepare_v2(db, "SELECT val FROM config WHERE key = 'db_version'", -1, &stmt, nullptr);
            if (result != SQLITE_OK)
                goto display_warning;

            result = sqlite3_step(stmt);

            if (result == SQLITE_ROW) {
                if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER)
                    dbVersion = sqlite3_column_int(stmt, 0);
            } else if (result == SQLITE_DONE) {
                // ignore
            } else {
                goto display_warning;
            }

            sqlite3_finalize(stmt);

            // do work according to the number we got
            if (dbVersion <= 0) {
                // our database is empty, let's create initial tables!
                result = sqlite3_exec(db,
                                      "CREATE TABLE tracks ("
                                      "   start  INTEGER PRIMARY KEY,"
                                      "   length INTEGER,"
                                      "   name   TEXT"
                                      ")", nullptr, nullptr, nullptr);
                if (result != SQLITE_OK)
                    goto display_warning;

                result = sqlite3_exec(db,
                                      "CREATE TABLE trackFiles ("
                                      "   start INTEGER PRIMARY KEY,"
                                      "   file  TEXT"
                                      ")", nullptr, nullptr, nullptr);
                if (result != SQLITE_OK)
                    goto display_warning;

                dbVersion = 1;
            }

            // write the db version back into the database
            result = sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO config VALUES('db_version', :version)", -1, &stmt, nullptr);
            if (result != SQLITE_OK)
                goto display_warning;

            result = sqlite3_bind_int(stmt, 1, dbVersion);
            if (result != SQLITE_OK)
                goto display_warning;

            result = sqlite3_step(stmt);
            if (result != SQLITE_DONE) {
                qWarning() << "SQL error:" << sqlite3_errmsg(db);
                sqlite3_finalize(stmt);
                goto rollback;
            }

            sqlite3_finalize(stmt);
        }

        result = sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);

        if (result == SQLITE_OK)
            return true;

    display_warning:
        qWarning() << "SQL error:" << sqlite3_errmsg(db);

    rollback:
        if (!sqlite3_get_autocommit(db)) {
            result = sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
            if (result != SQLITE_OK)
                qWarning() << "WTF: ROLLBACK failed." << sqlite3_errmsg(db);
        }

        return false;
    }
}

namespace Config {

Database::Database(QObject *parent) :
    QObject(parent)
{
    auto dbPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    int result;

    if (QDir().mkpath(dbPath)) {
        dbPath = QString("%1/recorder-config-db.sqlite3").arg(dbPath);

        QByteArray dbPathU8 = dbPath.toUtf8();

        result = sqlite3_open(dbPathU8.data(), &m_db);
        if (result == SQLITE_OK && upgradeDatabase(m_db))
            goto success;

        // try again with empty file
        qWarning() << "Can't use database" << dbPath << ": " << sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        m_db = nullptr;

        if (!QFile::remove(dbPath))
            qWarning() << "Removing" << dbPath << "failed, continuing anyways";

        result = sqlite3_open(dbPathU8.data(), &m_db);
        if (result == SQLITE_OK && upgradeDatabase(m_db))
            goto success; // thank got we have a sane database now

        qWarning() << "Can't use database" << dbPath << ": " << sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        m_db = nullptr;
    } else {
        qWarning() << "Couldn't create location" << dbPath << ", falling back to in-memory database";
    }

    qWarning() << "Falling back to in-memory database";

    result = sqlite3_open(":memory:", &m_db);
    if (result == SQLITE_OK && upgradeDatabase(m_db))
        goto success;

    qWarning() << "Even the in-memory database failed:" << sqlite3_errmsg(m_db);
    qWarning() << "We're lost now. Giving up.";

    sqlite3_close(m_db);
    m_db = nullptr;

    return;

success:
    repairLengthOfLastTrack();
}

Database::~Database()
{
    sqlite3_close_v2(m_db);
}

QString Database::readConfigString(const QString &key, const QString &fallbackValue)
{
    if (!m_db || !key.size())
        return fallbackValue;

    int result;
    sqlite3_stmt *stmt   = nullptr;
    QString       retval = fallbackValue;

    result = sqlite3_prepare_v2(m_db, "SELECT val FROM config WHERE key = :key", -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
        return fallbackValue;
    }

    result = sqlite3_bind_text16(stmt, 1, key.utf16(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);

        goto finished;
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        retval = QString::fromUtf16((const ushort*)sqlite3_column_text16(stmt, 0));
    } else if (result == SQLITE_DONE) {
        // value not found - default is already set
    } else {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
    }

finished:
    sqlite3_finalize(stmt);

    if (retval.size())
        return retval;
    else
        return fallbackValue;
}

double Database::readConfigDouble(const QString &key, double fallbackValue)
{
    if (!m_db || !key.size())
        return fallbackValue;

    int result;
    sqlite3_stmt *stmt   = nullptr;
    double        retval = fallbackValue;

    result = sqlite3_prepare_v2(m_db, "SELECT val FROM config WHERE key = :key", -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
        return fallbackValue;
    }

    result = sqlite3_bind_text16(stmt, 1, key.utf16(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);

        goto finished;
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        retval = sqlite3_column_double(stmt, 0);
    } else if (result == SQLITE_DONE) {
        // value not found - default is already set
    } else {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
    }

finished:
    sqlite3_finalize(stmt);

    return retval;
}

std::vector<Database::Track> Database::readAllTracks()
{
    if (!m_db)
        return {};

    std::vector<Database::Track> retval;

    int result;
    sqlite3_stmt *stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "SELECT start, length, name FROM tracks ORDER BY start ASC", -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error (readAllTracks prepare):" << sqlite3_errmsg(m_db);

        return {};
    }

    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        Database::Track t;

        t.start = (uint64_t)sqlite3_column_int64(stmt, 0);
        t.length = (uint64_t)sqlite3_column_int64(stmt, 1);
        t.name = QString::fromUtf16((const ushort*)sqlite3_column_text16(stmt, 2));

        retval.push_back(t);
    }

    if (result != SQLITE_DONE) {
        qWarning() << "SQL error (readAllTracks step):" << sqlite3_errmsg(m_db);
    }

    sqlite3_finalize(stmt);

    return retval;
}

std::map<uint64_t, QString> Database::readAllFiles()
{
    if (!m_db)
        return {};

    std::map<uint64_t, QString> retval;

    int result;
    sqlite3_stmt *stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "SELECT start,file FROM trackFiles ORDER BY start ASC", -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error (readAllFiles prepare):" << sqlite3_errmsg(m_db);

        return {};
    }

    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        uint64_t start = (uint64_t)sqlite3_column_int64(stmt, 0);
        QString  file  = QString::fromUtf16((const ushort*)sqlite3_column_text16(stmt, 1));

        retval[start] = file;
    }

    if (result != SQLITE_DONE)
        qWarning() << "SQL error (readAllFiles step):" << sqlite3_errmsg(m_db);

    sqlite3_finalize(stmt);

    return retval;
}

uint64_t Database::readTotalSamplesRecorded()
{
    if (!m_db)
        return 0;

    uint64_t retval = 0;

    int result;
    sqlite3_stmt *stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "SELECT start,length FROM tracks ORDER BY START desc LIMIT 1", -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error (readTotalSamplesRecorded prepare):" << sqlite3_errmsg(m_db);

        return 0;
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        uint64_t start = (uint64_t)sqlite3_column_int64(stmt, 0);
        uint64_t length = (uint64_t)sqlite3_column_int64(stmt, 1);

        retval = start+length;
    } else if (result != SQLITE_DONE) {
        qWarning() << "SQL error (readTotalSamplesRecorded step):" << sqlite3_errmsg(m_db);
    }

    sqlite3_finalize(stmt);

    return retval;
}

void Database::writeConfigString(const QString &key, const QString &value)
{
    if (!m_db || !key.size())
        return;

    int result;
    sqlite3_stmt *stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "INSERT OR REPLACE INTO config VALUES(:key, :val)", -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);

        return;
    }

    result = sqlite3_bind_text16(stmt, 1, key.utf16(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);

        goto finished;
    }

    result = sqlite3_bind_text16(stmt, 2, value.utf16(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);

        goto finished;
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
    }

finished:
    sqlite3_finalize(stmt);
}

void Database::writeConfigDouble(const QString &key, double value)
{
    if (!m_db || !key.size())
        return;

    int result;
    sqlite3_stmt *stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "INSERT OR REPLACE INTO config VALUES(:key, :val)", -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);

        return;
    }

    result = sqlite3_bind_text16(stmt, 1, key.utf16(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);

        goto finished;
    }

    result = sqlite3_bind_double(stmt, 2, value);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);

        goto finished;
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
    }

finished:
    sqlite3_finalize(stmt);
}

void Database::insertTrack(uint64_t start, uint64_t length, const QString &name)
{
    if (!m_db)
        return;

    int result;
    sqlite3_stmt *stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "INSERT OR REPLACE INTO tracks (start, length, name) VALUES (:start, :length, :name)", -1, &stmt, nullptr);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_bind_int64(stmt, 1, (int64_t)start);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_bind_int64(stmt, 2, (int64_t)length);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_bind_text16(stmt, 3, name.utf16(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE)
        goto complain;

    goto finished;

complain:
    qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
finished:
    sqlite3_finalize(stmt);
}

void Database::appendFile(uint64_t start, const QString &file)
{
    if (!m_db)
        return;

    int result;
    sqlite3_stmt *stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "INSERT OR REPLACE INTO trackFiles (start, file) VALUES (:start, :file)", -1, &stmt, nullptr);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_bind_int64(stmt, 1, (int64_t)start);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_bind_text16(stmt, 2, file.utf16(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE)
        goto complain;

    goto finished;

complain:
    qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
finished:
    sqlite3_finalize(stmt);
}

void Database::updateTrackLength(uint64_t trackStart, uint64_t newLength)
{
    if (!m_db)
        return;

    int result;
    sqlite3_stmt *stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "UPDATE tracks SET length=:length WHERE start=:start", -1, &stmt, nullptr);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":start"), (int64_t)trackStart);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":length"), (int64_t)newLength);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE)
        goto complain;

    goto finished;

complain:
    qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
finished:
    sqlite3_finalize(stmt);
}

void Database::updateTrackName(uint64_t trackStart, const QString &newName)
{
    if (!m_db)
        return;

    int result;
    sqlite3_stmt *stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "UPDATE tracks SET name=:name WHERE start=:start", -1, &stmt, nullptr);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":start"), (int64_t)trackStart);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_bind_text16(stmt, sqlite3_bind_parameter_index(stmt, ":name"), newName.utf16(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE)
        goto complain;

    qDebug() << "Updated name for track starting at"<<trackStart<<"new name is"<<newName<<"rows affected"<<sqlite3_changes(m_db);

    goto finished;

complain:
    qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
finished:
    sqlite3_finalize(stmt);
}

void Database::removeTrack(uint64_t trackStart)
{
    if (!m_db)
        return;

    int result;
    sqlite3_stmt *stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "DELETE FROM tracks WHERE start=:start", -1, &stmt, nullptr);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":start"), (int64_t)trackStart);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE)
        goto complain;

    goto finished;

complain:
    qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
finished:
    sqlite3_finalize(stmt);
}

void Database::clearTracksAndFiles()
{
    sqlite3_stmt *stmt = nullptr;
    int result;

    result = sqlite3_prepare_v2(m_db, "DELETE FROM tracks", -1, &stmt, nullptr);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE)
        goto complain;

    sqlite3_finalize(stmt);
    stmt = nullptr;

    result = sqlite3_prepare_v2(m_db, "DELETE FROM trackFiles", -1, &stmt, nullptr);
    if (result != SQLITE_OK)
        goto complain;

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE)
        goto complain;

    goto finished;

complain:
    qWarning() << "SQL error:" << sqlite3_errmsg(m_db);
finished:
    sqlite3_finalize(stmt);
}

void Database::repairLengthOfLastTrack()
{
    // the last track's length may be missing if we crashed mid-recording
    // hopefully we can improvise a meaningful value
    sqlite3_stmt *stmt = nullptr;
    int result;

    result = sqlite3_prepare_v2(m_db, "SELECT start,length FROM tracks ORDER BY start DESC limit 1", -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        qWarning() << "SQL error (selecting last track, prepare):" << sqlite3_errmsg(m_db);

        return;
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        // we have a last track
        // get length and start
        int64_t start = sqlite3_column_int64(stmt, 0);
        int64_t length = sqlite3_column_int64(stmt, 1);

        while (length == 0) {
            // find the last file we touched
            sqlite3_stmt *stmt = nullptr;

            result = sqlite3_prepare_v2(m_db, "SELECT start,file FROM trackFiles ORDER BY start DESC LIMIT 1", -1, &stmt, nullptr);
            if (result != SQLITE_OK) {
                qWarning() << "SQL error (finding last file, prepare):" << sqlite3_errmsg(m_db);

                break;
            }

            result = sqlite3_step(stmt);

            if (result == SQLITE_ROW) {
                // expected - we have a file we can use
                int64_t fileStart = sqlite3_column_int64(stmt, 0);
                QString  fileName = QString::fromUtf16((const ushort*)sqlite3_column_text16(stmt, 1));

                int64_t fileOffset = start - fileStart;

                int64_t fileSize = QFileInfo(fileName).size() / 4; // in samples

                if (fileSize != 0 && fileSize > fileOffset) {
                    qDebug() << "Successfully repaired length of last track, now" << fileSize - fileOffset << "samples";
                    this->updateTrackLength(start, (uint64_t)(fileSize - fileOffset));
                }
            } else if (result == SQLITE_DONE) {
                qWarning() << "LOGIC ERROR: expected to correct track length, but have no file!?";
            } else {
                qWarning() << "SQL error (finding last file, step):" << sqlite3_errmsg(m_db);
            }

            sqlite3_finalize(stmt);

            break;
        }
    } else if (result == SQLITE_DONE) {
        // we don't have any track - do nothing
    } else {
        qWarning() << "SQL error (selecting last track, step):" << sqlite3_errmsg(m_db);
    }

    sqlite3_finalize(stmt);
}

} // namespace Config
