#include "cspinncomm.h"
#include <QHostAddress>

cSpiNNcomm::cSpiNNcomm()
{
	sdpReply = new QUdpSocket(this);
	sdpResult = new QUdpSocket(this);
	sdpDebug = new QUdpSocket(this);
	sdpReply->bind(SDP_REPLY_PORT);
	sdpResult->bind(SDP_RESULT_PORT);
	sdpDebug->bind(SDP_DEBUG_PORT);
	connect(sdpReply, SIGNAL(readyRead()), this, SLOT(readReply()));
	connect(sdpResult, SIGNAL(readyRead()), this, SLOT(readResult()));
	connect(sdpDebug, SIGNAL(readyRead()), this, SLOT(readDebug()));
}

void cSpiNNcomm::readDebug()
{
	QByteArray ba;
	ba.resize(sdpDebug->pendingDatagramSize());
	sdpDebug->readDatagram(ba.data(), ba.size());
	// then process it before emit the signal
}

void cSpiNNcomm::readReply()
{
	QByteArray ba;
	ba.resize(sdpReply->pendingDatagramSize());
	sdpReply->readDatagram(ba.data(), ba.size());
	// then process it before emit the signal
}

void cSpiNNcomm::readResult()
{
	QByteArray ba;
	ba.resize(sdpResult->pendingDatagramSize());
	sdpResult->readDatagram(ba.data(), ba.size());
	// then process it before emit the signal
}

