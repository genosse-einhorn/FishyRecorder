#include "trackviewpane.h"

#include "recording/trackcontroller.h"
#include "recording/tracklistmodel.h"
#include "export/exportbuttongroup.h"
#include "recording/playbackcontrol.h"

#include <QSplitter>
#include <QTableView>
#include <QLabel>
#include <QGridLayout>
#include <QScrollArea>
#include <QHeaderView>

namespace Recording {

TrackViewPane::TrackViewPane(TrackController *controller, Config::Database *config, QWidget *parent) :
    QWidget(parent),
    m_controller(controller)
{
    m_trackView = new QTableView(this);
    m_trackView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_trackView->setModel(new TrackListModel(controller, this));
    m_trackView->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_trackView->setColumnHidden(0, true);
    m_trackView->horizontalHeader()->moveSection(1, 3);
    m_trackView->horizontalHeader()->setStretchLastSection(true);
    m_trackView->setFrameStyle(QFrame::NoFrame);

    QObject::connect(m_trackView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TrackViewPane::selectedRowChanged);

    // construct the right hand pane
    QWidget *rightPane = new QWidget(this);
    QGridLayout *paneLayout = new QGridLayout(this);

    auto exportPane = new Export::ExportButtonGroup(controller, config, this);
    exportPane->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_playback = new PlaybackControl(controller, this);
    m_playback->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    paneLayout->addWidget(m_playback, 0, 0, 1, 1, Qt::AlignTop);
    paneLayout->addWidget(exportPane, 1, 0, 1, 1, Qt::AlignBottom);

    rightPane->setLayout(paneLayout);

    QScrollArea *rightPaneScroll = new QScrollArea(this);
    rightPaneScroll->setWidget(rightPane);
    rightPaneScroll->setWidgetResizable(true);
    rightPaneScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightPaneScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    rightPaneScroll->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    rightPaneScroll->setFrameStyle(QFrame::NoFrame);
    rightPaneScroll->setBackgroundRole(QPalette::Window);

    // create the final layout
    QGridLayout *layout = new QGridLayout(this);
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(m_trackView, 0, 0, 1, 1);
    layout->addWidget(rightPaneScroll, 0, 1, 1, 1);

    this->setLayout(layout);
}

void TrackViewPane::setMonitorDevice(PaDeviceIndex index)
{
    m_playback->monitorDeviceChanged(index);
}

void TrackViewPane::selectedRowChanged()
{
    if (m_trackView->selectionModel()->hasSelection()) {
        //FIXME: is this a layering violation?
        int index = m_trackView->selectionModel()->selectedRows()[0].row();

        m_playback->trackSelected(index);
    } else {
        m_playback->trackDeselected();
    }
}

} // namespace Recording
