#include "powerpointpresenter.h"
#include "ui_powerpointpresenter.h"

#include <QDebug>
#include <QAxScriptManager>
#include <QTemporaryFile>
#include <QDir>
#include <QTimer>
#include <qt_windows.h>

static const int ICON_SIZE = 200;

namespace Presentation {

PowerpointPresenter::PowerpointPresenter(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PowerpointPresenter)
{
    ui->setupUi(this);

    //NOTE: it is super important to create the script manager as child object
    // of a QWidget. Otherwise, it will mysteriously fail to run any script.
    m_scripts = new QAxScriptManager(this);

    QObject::connect(ui->endBtn, &QAbstractButton::clicked, this, &PowerpointPresenter::closeBtnClicked);
    QObject::connect(ui->thumbnailList, &QListWidget::itemSelectionChanged, this, &PowerpointPresenter::itemSelected);
    QObject::connect(m_scripts, &QAxScriptManager::error, this, &PowerpointPresenter::scriptError);
}

void PowerpointPresenter::fillPreviewList()
{
    if (!m_script)
        return;

    ui->thumbnailList->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    ui->thumbnailList->setFlow(QListView::TopToBottom);
    ui->thumbnailList->setWrapping(false);
    ui->thumbnailList->setMaximumWidth(ICON_SIZE + 50);

    for (int i = 0; i < m_script->call("getNumSlides()").toInt(); ++i) {
        QListWidgetItem *item = new QListWidgetItem(QString("%1").arg(i+1));
        ui->thumbnailList->addItem(item);

        QString fileName;

        {
            QTemporaryFile file(QString("%1/slide_%2_XXXXXX.png").arg(QDir::tempPath()).arg(i+1));
            file.setAutoRemove(false);
            file.open();
            fileName = file.fileName();
        }

        m_script->call("exportSlide(const QVariant&, const QVariant&)",
                       QVariant::fromValue(QVariant::fromValue(i+1)),
                       QVariant::fromValue(QVariant::fromValue(QDir::toNativeSeparators(fileName))));

        item->setIcon(QIcon(QPixmap(fileName)));

        QFile::remove(fileName);
    }
}

void PowerpointPresenter::updateCurrentSlide()
{
    if (!m_script)
        return;

    int slideNo = m_script->call("getCurrentSlideNo()").toInt();
    if (!slideNo) // slide numbers start at one
        return;

    if (ui->thumbnailList->currentRow() != (slideNo-1)) {
        ui->thumbnailList->setCurrentRow(slideNo-1);
        ui->thumbnailList->scrollToItem(ui->thumbnailList->item(slideNo-1), QAbstractItemView::PositionAtCenter);
    }
}

PowerpointPresenter *PowerpointPresenter::loadPowerpointFile(const QString &fileName)
{
    std::unique_ptr<PowerpointPresenter> presenter(new PowerpointPresenter);

    presenter->m_script = presenter->m_scripts->load(R"script(
        var app          = null;
        var presentation = null;
        var view         = null;
        var window       = null;

        function getHwnd() {
            return window.HWND;
        }

        function open(filename) {
            app = new ActiveXObject('PowerPoint.Application');

            // At least PowerPoint 2013 refuses to open a presentation
            // with spaces in the file name, unless you add extra quotes.
            // Older versions (like Office 2000) choke on the quotes and
            // demand the real, unqoted string.
            // Don't ask why.

            try {
                presentation = app.Presentations.Open(filename, true, true, false);
            } catch (e) {
                presentation = app.Presentations.Open('"' + filename + '"', true, true, false);
            }

            return presentation;
        }

        function present() {
            var settings = presentation.SlideShowSettings;

            settings.ShowType = 3; // Kiosk
            settings.LoopUntilStopped = true;

            window = settings.Run();
            view = window.View;

            window.Activate();

            return window;
        }

        function next() {
            view.Next();
        }

        function previous() {
            view.Previous();
        }

        function close() {
            try {
               view.Exit();
           } catch (e) {}
           try {
               presentation.Close();
           } catch (e) {}
        }

        function getNumSlides() {
            return presentation.Slides.Count;
        }

        function goToSlide(slideno) {
            return view.GotoSlide(slideno);
        }

        function copySlide(slideno) {
            presentation.Slides.Item(slideno).Copy();
        }

        function exportSlide(slideno, filename) {
            presentation.Slides.Item(slideno).Export(filename, 'PNG');
        }

        function getCurrentSlideNo() {
           return view.Slide.SlideIndex;
        }

    )script", "ppcontrol", "JScript");

    if (!presenter->m_script) {
        qDebug() << "Loading the script failed :(";
        return nullptr;
    }

    qDebug() << "Open: " << presenter->m_script->call("open(const QVariant&)", QVariant::fromValue(QVariant::fromValue(fileName)));
    qDebug() << "Present: " << presenter->m_script->call("present()");

    if (presenter->m_scriptError)
        return nullptr;

    presenter->fillPreviewList();
    presenter->updateCurrentSlide();

    return presenter.release();
}

PowerpointPresenter::~PowerpointPresenter()
{
    if (m_script)
        m_script->call("close()");

    delete ui;
}

void PowerpointPresenter::setScreen(const QRect &screen)
{
    qDebug() << "Positioning powerpoint window";

    HWND powerPoint = (HWND)m_script->call("getHwnd()").toInt();
    if (!powerPoint)
        powerPoint = FindWindowW(L"screenClass", nullptr);

    qDebug() << "PowerPoint HWND=" << powerPoint;

    // We have been told we could just set the Left, Top, Width, Height properties
    // but that's bullshit because the window is misplaced every time.
    // So we'll just move the window we found earlier.
    SetWindowPos(powerPoint, NULL, screen.x(), screen.y(), screen.width(), screen.height(), SWP_NOZORDER);
    SetForegroundWindow(powerPoint);

    // We have to give PowerPoint some time to adjust its window, before we grab the focus again
    HWND self = (HWND)this->topLevelWidget()->winId();
    QTimer::singleShot(500, [=](){ SetForegroundWindow(self); });
}


void PowerpointPresenter::nextPage()
{
    m_script->call("next()");
    updateCurrentSlide();
}

void PowerpointPresenter::previousPage()
{
    m_script->call("previous()");
    updateCurrentSlide();
}

void PowerpointPresenter::closeBtnClicked()
{
    m_script->call("close()");

    emit closeRequested();
}

void PowerpointPresenter::itemSelected()
{
    auto items = ui->thumbnailList->selectedItems();
    if (items.size() < 1)
        return;

    int pageNo = items.first()->text().toInt();

    m_script->call("goToSlide(const QVariant&)", QVariant::fromValue(QVariant::fromValue(pageNo)));
}

void PowerpointPresenter::scriptError(QAxScript *, int code, const QString &desc, int pos, const QString &source)
{
    m_scriptError = true;

    qDebug() << "SCRIPT ERROR:";
    qDebug() << "Code" << code << ": " << desc << " at " << pos << ": " << source;
}

} // namespace Presentation
