#include "vidstreamer.h"
#include "ui_vidstreamer.h"

#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>

vidStreamer::vidStreamer(QWidget *parent) :
    QWidget(parent),
	isPaused(false),
    ui(new Ui::vidStreamer)
{
    ui->setupUi(this);
    refresh = new QTimer(this);
    screen = new cScreen();
    edge = new cScreen();
    spinn = new cSpiNNcomm();
    spinn->setHost(ui->cbSpiNN->currentIndex());

    connect(ui->pbLoad, SIGNAL(pressed()), this, SLOT(pbLoadClicked()));
	connect(ui->pbPause, SIGNAL(pressed()), this, SLOT(pbPauseClicked()));
    connect(ui->cbSpiNN, SIGNAL(currentIndexChanged(int)), spinn,
            SLOT(setHost(int)));

	refresh->setInterval(40);   // which produces roughly 25fps
	//refresh->setInterval(1000);   // which produces roughly 25fps
	refresh->start();
	//connect(refresh, SIGNAL(timeout()), this, SLOT(refreshUpdate()));
}

void vidStreamer::pbPauseClicked()
{
	if(isPaused==false) {
		ui->pbPause->setText("Run");
		isPaused = true;
		refresh->stop();
	}
	else {
		ui->pbPause->setText("Pause");
		isPaused = false;
		refresh->start();
	}
}

vidStreamer::~vidStreamer()
{
    delete spinn;
	delete edge;
	delete screen;
    delete ui;
}

void vidStreamer::errorString(QString err)
{
    qDebug() << err;
}

void vidStreamer::pbLoadClicked()
{
	QString fName = QFileDialog::getOpenFileName(this, "Open Video File", "./", "*");
    if(fName.isEmpty())
        return;

	worker = new QThread();
	decoder = new cDecoder();

	decoder->moveToThread(worker);
	connect(decoder, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
	connect(worker, SIGNAL(started()), decoder, SLOT(started()));
	connect(decoder, SIGNAL(finished()), worker, SLOT(quit()));
	connect(decoder, SIGNAL(finished()), decoder, SLOT(deleteLater()));
	connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
	connect(decoder, SIGNAL(newFrame(const QImage &)), screen, SLOT(putFrame(const QImage &)));
	connect(decoder, SIGNAL(gotPicSz(int,int)), this, SLOT(setSize(int,int)));
	connect(decoder, SIGNAL(finished()), this, SLOT(videoFinish()));
	connect(refresh, SIGNAL(timeout()), decoder, SLOT(refresh()));

	decoder->filename = fName;
	worker->start();
	//refresh->start();
}

void vidStreamer::refreshUpdate()
{
	screen->drawFrame();
	edge->drawFrame();
}

void vidStreamer::setSize(int w, int h)
{
	screen->setSize(w,h);
	edge->setGeometry(edge->x()+w,edge->y(),w,h);
	edge->setSize(w,h);
}

void vidStreamer::closeEvent(QCloseEvent *event)
{
	refresh->stop();
	worker->quit();
	screen->close();
	edge->close();
	event->accept();
}

void vidStreamer::videoFinish()
{
	worker->quit();
	refresh->stop();
	screen->hide();
	edge->hide();
}

// tell SpiNNaker to compute workload
void vidStreamer::configSpin(int w, int h)
{

}
