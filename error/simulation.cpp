#include "simulation.h"

Error::Simulation::Simulation(QObject *parent) :
    QObject(parent),
    m_simu_map({
        { "openAudioStream",   SIMULATE_NEVER },
        { "openAudioDataFile", SIMULATE_NEVER },
        { "writeAudioFile",    SIMULATE_NEVER },
        { "writeAudioBuffer",  SIMULATE_NEVER },
        { "readAudioFile",     SIMULATE_NEVER },
        { "switchAudioFile",   SIMULATE_NEVER }
    }),
    m_threadSafetySaviour(QMutex::Recursive)
{
    qRegisterMetaType<Error::Simulation::SimulationState>("SimulationState");
}

QList<QString> Error::Simulation::simulatedErrors()
{
    QMutexLocker locker(&m_threadSafetySaviour);
    return m_simu_map.keys();
}

Error::Simulation::SimulationState Error::Simulation::simulationState(const QString &error)
{
    QMutexLocker locker(&m_threadSafetySaviour);

    if (m_simu_map.contains(error))
        return m_simu_map[error];
    else
        return SIMULATE_INVALID;
}

Error::Simulation *Error::Simulation::getInstance()
{
    static Error::Simulation *sim = nullptr;

    if (!sim)
        sim = new Error::Simulation;

    return sim;
}

void
Error::Simulation::changeSimulation(const QString &error, Error::Simulation::SimulationState simulation)
{
    QMutexLocker locker(&m_threadSafetySaviour);

    if (m_simu_map[error] == simulation)
        return;

    m_simu_map[error] = simulation;

    emit simulationChanged(error, simulation);
}
