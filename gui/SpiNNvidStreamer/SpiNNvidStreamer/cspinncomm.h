#ifndef CSPINNCOMM_H
#define CSPINNCOMM_H

#include <QObject>
#include <QUdpSocket>

#define SDP_REPLY_PORT  20000		// with tag 1
#define SDP_RESULT_PORT	20001		// with tag 2
#define SDP_DEBUG_PORT  20002		// with tag 3
#define DEF_SEND_PORT	17893		// tidak bisa diganti dengan yang lain

class cSpiNNcomm
{
	Q_OBJECT

public:
	cSpiNNcomm();
public slots:
	void readResult();
	void readReply();
	void readDebug();
signals:
	void gotResult(const QByteArray &data);
	void gotReply(const QByteArray &data);
	void gotDebug(const QByteArray &data);
private:
	QUdpSocket *sdpResult;
	QUdpSocket *sdpReply;
	QUdpSocket *sdpDebug;
};

#endif // CSPINNCOMM_H
