#include "vidstreamer.h"
#include "ui_vidstreamer.h"

#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>

vidStreamer::vidStreamer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vidStreamer)
{
    ui->setupUi(this);
    connect(ui->pbLoad, SIGNAL(clicked()), this, SLOT(pbLoadClicked()));
    refresh = new QTimer(this);
	screen = new cScreen();

    refresh->setInterval(40);   // which produces roughly 25fps
    connect(refresh, SIGNAL(timeout()), this, SLOT(refreshUpdate()));
}



vidStreamer::~vidStreamer()
{
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
	connect(decoder, SIGNAL(gotPicSz(int,int)), screen, SLOT(setSize(int,int)));
	connect(decoder, SIGNAL(finished()), this, SLOT(videoFinish()));

	decoder->filename = fName;
	worker->start();
    refresh->start();
}

void vidStreamer::refreshUpdate()
{

}

void vidStreamer::closeEvent(QCloseEvent *event)
{
	refresh->stop();
	worker->quit();
	screen->close();
	event->accept();
}

void vidStreamer::videoFinish()
{
	worker->quit();
	refresh->stop();
	screen->hide();
}
