#include "cspinncomm.h"
#include <QHostAddress>

cSpiNNcomm::cSpiNNcomm(QObject *parent): QObject(parent)
{
    // initialize nodes
    for(ushort i=0; i<MAX_CHIPS; i++) {
        nodes[i].nodeID = i;
        nodes[i].chipX = X_CHIPS[i];
        nodes[i].chipY = Y_CHIPS[i];
    }
    sdpReply = new QUdpSocket();
    sdpResult = new QUdpSocket();
    sdpDebug = new QUdpSocket();
    sender = new QUdpSocket();
	sdpReply->bind(SDP_REPLY_PORT);
	sdpResult->bind(SDP_RESULT_PORT);
	sdpDebug->bind(SDP_DEBUG_PORT);
	connect(sdpReply, SIGNAL(readyRead()), this, SLOT(readReply()));
	connect(sdpResult, SIGNAL(readyRead()), this, SLOT(readResult()));
	connect(sdpDebug, SIGNAL(readyRead()), this, SLOT(readDebug()));

    // initialize SpiNNaker stuffs
    sdpImgRedPort = 1;
    sdpImgGreenPort = 2;
    sdpImgBluePort = 3;
    sdpImgConfigPort = 7;

    // TODO: ask spinnaker, who's the leadAp
    // right now, let's assume it is core-1
    leadAp = 1;
}

cSpiNNcomm::~cSpiNNcomm()
{
    delete sdpReply;
    delete sdpResult;
    delete sdpDebug;
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

// configure SpiNNaker to calculate workloads
void cSpiNNcomm::configSpin(uchar spinIDX, int imgW, int imgH)
{
    sdp_hdr_t h;
    h.flags = 0x07;
    h.tag = 0;
    h.dest_port = (sdpImgConfigPort << 5) + leadAp;
    h.srce_port = ETH_PORT;
    h.dest_addr = 0;
    h.srce_addr = 0;

    cmd_hdr_t c;
    c.cmd_rc = SDP_CMD_CONFIG_CHAIN;
    c.seq = OP_SOBEL_NO_FILTER;    // edge-proc sobel, no filtering
    c.arg1 = (imgW << 16) + imgH;
    c.arg2 = spinIDX==0?4:MAX_CHIPS;    // spin3 or spin5
    c.arg3 = BLOCK_REPORT_NO;

    // build the node list
    QByteArray nodeList;
    // here, we make chip<0,0> as the root, hence we start from 1
    for(ushort i=1; i<c.arg2; i++) {
        nodeList.append((char)nodes[i].chipX);
        nodeList.append((char)nodes[i].chipY);
        nodeList.append((char)nodes[i].nodeID);
    }
    sendSDP(h, scp(c), nodeList);
}

void cSpiNNcomm::sendSDP(sdp_hdr_t h, QByteArray s, QByteArray d)
{
    QByteArray ba = QByteArray(2, '\0');    // pad first
    ba.append(hdr(h));
    if(s.size()>0)
        ba.append(s);
    if(d.size()>0)
        ba.append(s);
    sender->writeDatagram(ba, ha, DEF_SEND_PORT);
}

QByteArray cSpiNNcomm::scp(cmd_hdr_t cmd)
{
    QByteArray ba = QByteArray::number(cmd.cmd_rc);
    ba.append(QByteArray::number(cmd.seq));
    ba.append(QByteArray::number(cmd.arg1));
    ba.append(QByteArray::number(cmd.arg2));
    ba.append(QByteArray::number(cmd.arg3));
    return ba;
}

QByteArray cSpiNNcomm::hdr(sdp_hdr_t h)
{
    QByteArray ba = QByteArray::number(h.flags);
    ba.append(QByteArray::number(h.tag));
    ba.append(QByteArray::number(h.dest_port));
    ba.append(QByteArray::number(h.srce_port));
    ba.append(QByteArray::number(h.dest_addr));
    ba.append(QByteArray::number(h.srce_addr));
    return ba;
}

void cSpiNNcomm::setHost(int spinIDX)
{
    if(spinIDX==SPIN3)
        ha = QHostAddress(QString(SPIN3_IP));
    else
        ha = QHostAddress(QString(SPIN5_IP));
}
