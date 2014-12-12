#ifndef ERROR_SIMULATIONWIDGET_H
#define ERROR_SIMULATIONWIDGET_H

#ifndef QT_NO_DEBUG

#include "error/simulation.h"

#include <QWidget>

#include <vector>
#include <utility>

class QButtonGroup;

namespace Error {

class SimulationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SimulationWidget(QWidget *parent = 0);

signals:
    void sendSimulationChange(const QString& error, Simulation::SimulationState state);

private slots:
    void receiveSimulationChanged(const QString& error, Simulation::SimulationState state);
    void buttonToggled(int id);

private:
    std::vector<std::pair<QString, QButtonGroup*>> widgets;

};

} // namespace Error

#endif // QT_NO_DEBUG

#endif // ERROR_SIMULATIONWIDGET_H
