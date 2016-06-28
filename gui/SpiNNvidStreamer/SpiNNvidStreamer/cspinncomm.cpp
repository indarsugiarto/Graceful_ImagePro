#include "cspinncomm.h"
#include <QHostAddress>
#include <QDebug>
#include <QApplication>

// TODO: the following X_CHIPS and Y_CHIPS should be configurable
// but now, let's make it convenient
uchar cSpiNNcomm::X_CHIPS[48] = {0,1,0,1,2,3,4,2,3,4,5,0,1,2,3,4,5,6,0,1,2,3,4,5,
                                 6,7,1,2,3,4,5,6,7,2,3,4,5,6,7,3,4,5,6,7,4,5,6,7};
uchar cSpiNNcomm::Y_CHIPS[48] = {0,0,1,1,0,0,0,1,1,1,1,2,2,2,2,2,2,2,3,3,3,3,3,3,
                                 3,3,4,4,4,4,4,4,4,5,5,5,5,5,5,6,6,6,6,6,7,7,7,7};

cSpiNNcomm::cSpiNNcomm(QObject *parent): QObject(parent)
{
    // initialize nodes
    for(ushort i=0; i<MAX_CHIPS; i++) {
        nodes[i].nodeID = i;
        nodes[i].chipX = X_CHIPS[i];
        nodes[i].chipY = Y_CHIPS[i];
    }
    // create sockets
    sdpReply = new QUdpSocket();
    sdpResult = new QUdpSocket();
    sdpDebug = new QUdpSocket();
    sender = new QUdpSocket();
	sdpReply->bind(SDP_REPLY_PORT);
	sdpResult->bind(SDP_RESULT_PORT);
	sdpDebug->bind(SDP_DEBUG_PORT);
    // handle/prepare callbacks
	connect(sdpReply, SIGNAL(readyRead()), this, SLOT(readReply()));
	connect(sdpResult, SIGNAL(readyRead()), this, SLOT(readResult()));
	connect(sdpDebug, SIGNAL(readyRead()), this, SLOT(readDebug()));

    // initialize SpiNNaker stuffs
    sdpImgRedPort = 1;
    sdpImgGreenPort = 2;
    sdpImgBluePort = 3;
	sdpImgReplyPort = 6;
    sdpImgConfigPort = 7;

	// TODO: ask spinnaker, who's the leadAp
	// right now, let's assume it is core-1
	leadAp = 1;

	// prepare headers for transmission
	hdrr.flags = 0x07;
	hdrr.tag = 0;
	hdrr.dest_port = (sdpImgRedPort << 5) + leadAp;
	hdrr.dest_addr = 0;
	hdrr.srce_port = ETH_PORT;
	hdrr.srce_addr = ETH_ADDR;

	hdrg = hdrr; hdrg.dest_port = (sdpImgGreenPort << 5) + leadAp;
	hdrb = hdrr; hdrb.dest_port = (sdpImgBluePort << 5) + leadAp;

	cont2Send = false;


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

	// during sending frame, spinnaker send reply as reportMsg via tag-1
	// that goes through port-2000 (this sdpReply port)
	// TODO: extract cmd_rc to make sure it is during image retrieval? No?
	cont2Send = true;
}

void cSpiNNcomm::readResult()
{
	QByteArray ba;
	ba.resize(sdpResult->pendingDatagramSize());
	sdpResult->readDatagram(ba.data(), ba.size());
	// then process it before emit the signal
    // TODO: getImage()
}

// configure SpiNNaker to calculate workloads
// it should be called when cDecoder first detect the resolution
void cSpiNNcomm::configSpin(uchar spinIDX, int imgW, int imgH)
// @slot
{
	hImg = imgH;
	wImg = imgW;

    sdp_hdr_t h;
    h.flags = 0x07;
    h.tag = 0;
    h.dest_port = (sdpImgConfigPort << 5) + leadAp;
    h.srce_port = ETH_PORT;
    h.dest_addr = 0;
    h.srce_addr = 0;

    cmd_hdr_t c;
    c.cmd_rc = SDP_CMD_CONFIG_CHAIN;
	c.seq = OP_SOBEL_NO_FILTER;    // edge-proc sobel, no filtering, not grey
    c.arg1 = (imgW << 16) + imgH;
    // ushort rootNode = 0;
    // ushort nNodes = spinIDX == 0 ? 4 : MAX_CHIPS;
    // c.arg2 = (rootNode << 16) + nNodes;
    c.arg2 = spinIDX==0?4:MAX_CHIPS;    // spin3 or spin5
    c.arg3 = BLOCK_REPORT_NO;

    // build the node list
    QByteArray nodeList;
    // here, we make chip<0,0> as the rootNode, hence we start from 1
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

	if(s.size()>0) ba.append(s);

    if(d.size()>0) ba.append(d);

	sender->writeDatagram(ba, ha, DEF_SEND_PORT);
	//sender->writeDatagram(ba, QHostAddress::Any, DEF_SEND_PORT);	// not working with Any
}

QByteArray cSpiNNcomm::scp(cmd_hdr_t cmd)
{
	QByteArray ba;

	QDataStream stream(&ba, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_4_8);
	stream.setByteOrder(QDataStream::LittleEndian);

	stream << cmd.cmd_rc << cmd.seq << cmd.arg1 << cmd.arg2 << cmd.arg3;
	return ba;
}

QByteArray cSpiNNcomm::hdr(sdp_hdr_t h)
{
	/* It doesn't work, because ::number creates more bytes :(
    QByteArray ba = QByteArray::number(h.flags);
    ba.append(QByteArray::number(h.tag));
    ba.append(QByteArray::number(h.dest_port));
    ba.append(QByteArray::number(h.srce_port));
    ba.append(QByteArray::number(h.dest_addr));
    ba.append(QByteArray::number(h.srce_addr));
    return ba;
	*/

	QByteArray ba;

	QDataStream stream(&ba, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_4_8);
	stream.setByteOrder(QDataStream::LittleEndian);

	stream << h.flags << h.tag << h.dest_port << h.srce_port << h.dest_addr << h.srce_addr;

	return ba;
}

void cSpiNNcomm::setHost(int spinIDX)
{
    if(spinIDX==SPIN3)
        ha = QHostAddress(QString(SPIN3_IP));
    else
        ha = QHostAddress(QString(SPIN5_IP));
}

void cSpiNNcomm::sendReply()
{
	sdp_hdr_t reply;
	reply.flags = 0x07;
	reply.tag = 0;
	reply.dest_port = (sdpImgReplyPort << 5) + leadAp;
	reply.srce_port = ETH_PORT;
	reply.dest_addr = 0;
	reply.srce_addr = 0;
	sendSDP(reply);
}

void cSpiNNcomm::sendImgLine(sdp_hdr_t h, const char *pixel, quint16 len)
{
    QByteArray sdp = QByteArray(2, '\0');    // pad first
    sdp.append(hdr(h));
    if(len > 0) {
        QByteArray data = QByteArray::fromRawData(pixel, len);
        sdp.append(data);
    }
    sender->writeDatagram(sdp, ha, DEF_SEND_PORT);
}

// The reply from SpiNNaker will be sent as reportMsg via tag-1 (SDP_TAG_REPLY). It
// contains cmd_rc SDP_CMD_REPLY_HOST_SEND_IMG
void cSpiNNcomm::frameIn(const QImage &frame)
{
	// at this point SpiNNaker should know the configuration (image size, nodes, etc)
	uchar rArray[1024];	//limited to 1024-wide frame
	uchar gArray[1024];
	uchar bArray[1024];

    const char *rPtr;
    const char *gPtr;
    const char *bPtr;

	quint16 remaining, sz;

	// for all lines in the image
	for(int i=0; i<hImg; i++) {
		memcpy(rArray, frame.scanLine(i), wImg);
		memcpy(gArray, frame.scanLine(i)+wImg, wImg);
		memcpy(bArray, frame.scanLine(i)+2*wImg, wImg);

        rPtr = (const char *)rArray;
        gPtr = (const char *)gArray;
        bPtr = (const char *)bArray;
		// assumming colorful frame (not greyscale)
		remaining = wImg;
		while(remaining > 0) {
			sz = remaining > 256+16 ? 256+16:remaining;
			cont2Send = false;
			sendImgLine(hdrr, rPtr, sz);
			while(cont2Send==false) {
				QApplication::processEvents();
			}
			cont2Send = false;
			sendImgLine(hdrg, gPtr, sz);
			while(cont2Send==false) {
				QApplication::processEvents();
			}
			cont2Send = false;
			sendImgLine(hdrb, bPtr, sz);
			while(cont2Send==false) {
				QApplication::processEvents();
			}
			// then adjust array position
			rPtr += sz;
			gPtr += sz;
			bPtr += sz;
			remaining -= sz;
		}
	}
    // then we have to send just headers to indicate end of image
    sendImgLine(hdrr, NULL, 0);
    sendImgLine(hdrg, NULL, 0);
    sendImgLine(hdrb, NULL, 0);
}

