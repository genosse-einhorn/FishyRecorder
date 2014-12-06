#include "error/simulationwidget.h"
#include "error/simulation.h"
#include "util/misc.h"

#include <QLabel>
#include <QButtonGroup>
#include <QPushButton>
#include <QGridLayout>
#include <QDebug>

namespace Error {

SimulationWidget::SimulationWidget(QWidget *parent) :
    QWidget(parent)
{
    Simulation *sim = Simulation::getInstance();

    QObject::connect(sim, &Simulation::simulationChanged, this, &SimulationWidget::receiveSimulationChanged);
    QObject::connect(this, &SimulationWidget::sendSimulationChange, sim, &Simulation::changeSimulation);

    QGridLayout *layout = new QGridLayout(this);

    int row = 0;
    for (auto const& it : sim->simulatedErrors()) {
        QLabel *lbl = new QLabel(this);
        lbl->setText(it);

        QPushButton *never_btn = new QPushButton(this);
        never_btn->setCheckable(true);
        never_btn->setText("Never");

        QPushButton *once_btn = new QPushButton(this);
        once_btn->setCheckable(true);
        once_btn->setText("Once");

        QPushButton *always_btn = new QPushButton(this);
        always_btn->setCheckable(true);
        always_btn->setText("Always");

        layout->addWidget(lbl, row, 0);
        layout->addWidget(never_btn, row, 1);
        layout->addWidget(once_btn, row, 2);
        layout->addWidget(always_btn, row, 3);

        QButtonGroup *group = new QButtonGroup(this);
        group->addButton(never_btn, Simulation::SIMULATE_NEVER);
        group->addButton(once_btn, Simulation::SIMULATE_ONCE);
        group->addButton(always_btn, Simulation::SIMULATE_ALWAYS);

        auto currentBtn = group->button(sim->simulationState(it));
        if (currentBtn)
            currentBtn->setChecked(true);

        widgets.emplace_back(it, group);

        QObject::connect(group, SELECT_SIGNAL_OVERLOAD<int>::OF(&QButtonGroup::buttonClicked), this, &SimulationWidget::buttonToggled);

        row++;
    }
}

void SimulationWidget::receiveSimulationChanged(const QString &error, Simulation::SimulationState state)
{
    for (auto const& it : widgets) {
        if (it.first == error) {
            auto button = it.second->button(state);

            if (button)
                button->setChecked(true);

            return;
        }
    }

    qWarning() << "Received change event for unknown error: " << error;
}

void SimulationWidget::buttonToggled(int id)
{
    auto sender = QObject::sender();

    for (auto const& it : widgets) {
        if (it.second == sender) {
            emit sendSimulationChange(it.first, (Simulation::SimulationState)id);

            return;
        }
    }

    qWarning() << "Received change event for unknown button group!?";
}



} // namespace Error
