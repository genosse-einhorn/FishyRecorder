#ifndef MISC_H
#define MISC_H

#include <QString>

// credits to https://stackoverflow.com/a/16795664/1331519
template<typename... Args> struct SELECT_SIGNAL_OVERLOAD {
    template<typename C, typename R>
    static constexpr auto OF( R (C::*pmf)(Args...) ) -> decltype(pmf) {
        return pmf;
    }
};

namespace Util {
    inline QString formatTime(uint64_t samples) {
        uint64_t seconds = (samples/44100) % 60;
        uint64_t minutes = samples/44100/60 % 60;
        uint64_t hours   = samples/44100/60/60;

        return QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    }


    class SignalBlocker {
        QObject *m_object;
        bool     m_blockedBefore;
        bool     m_active;

    public:
        inline explicit SignalBlocker(QObject *o, bool active = true) : m_object(o), m_active(active) {
            if (active)
                m_blockedBefore = m_object->blockSignals(true);
        }

        inline ~SignalBlocker() {
            if (m_active)
                m_object->blockSignals(m_blockedBefore);
        }

    private:
        Q_DISABLE_COPY(SignalBlocker)
    };
}

#endif // MISC_H
