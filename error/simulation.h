#ifndef SIMULATION_H
#define SIMULATION_H

#ifdef QT_NO_DEBUG

#include <QString>

namespace Error {
    namespace Simulation {
        static bool SIMULATE(const QString&) {
            return false;
        }
    }
}

#else

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>

namespace Error {

class Simulation : public QObject
{
    Q_OBJECT
public:
    enum SimulationState {
        SIMULATE_INVALID = -1,
        SIMULATE_NEVER   = 0,
        SIMULATE_ONCE    = 1,
        SIMULATE_ALWAYS  = 2
    };

    QList<QString>  simulatedErrors();
    SimulationState simulationState(const QString& error);

    static Simulation *getInstance();

    static bool SIMULATE(const QString& error) {
        auto *sim = Simulation::getInstance();

        QMutexLocker(&sim->m_threadSafetySaviour);

        switch (sim->simulationState(error)) {
        case SIMULATE_INVALID:
        case SIMULATE_NEVER:
            return false;
        case SIMULATE_ONCE:
            sim->changeSimulation(error, SIMULATE_NEVER);
            return true;
        case SIMULATE_ALWAYS:
            return true;
        default:
            return false;
        }
    }

signals:
    void simulationChanged(const QString& error, SimulationState simulation);

public slots:
    void changeSimulation(const QString& error, SimulationState simulation);

private:
    explicit Simulation(QObject *parent = 0);

    QMap<QString, SimulationState> m_simu_map;
    QMutex                         m_threadSafetySaviour;
};

} // namespace Error

#endif // QT_NO_DEBUG

#endif // SIMULATION_H
