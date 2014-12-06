#include "aboutpane.h"
#include "ui_aboutpane.h"

#include <lame/lame.h>
#include <portaudio.h>
#include <QtGlobal>

AboutPane::AboutPane(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AboutPane)
{
    ui->setupUi(this);

    auto text = ui->label->text();

    text.replace("__LAME_VERSION__", get_lame_version());
    text.replace("__QT_VERSION__", qVersion());
    text.replace("__PORTAUDIO_VERSION__", Pa_GetVersionText());

    ui->label->setText(text);
}

AboutPane::~AboutPane()
{
    delete ui;
}
