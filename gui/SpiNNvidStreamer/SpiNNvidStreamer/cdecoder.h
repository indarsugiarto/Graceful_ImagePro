/* Sepertinya bagian decoder ini bertanggung jawab me-load file, kemudian
 * bikin semacam streamer!
 *
 * */

#ifndef CDECODER_H
#define CDECODER_H

#include <QObject>
#include <QImage>

#include "viddef.h"

class cDecoder : public QObject
{
    Q_OBJECT
public:
    explicit cDecoder(QObject *parent = 0);
    ~cDecoder();
    VideoState *is;
    void stop() {isStopped = true;}
	QString filename;
	QImage frame;
	int w;
	int h;

signals:
    void finished();
    void error(QString err);
	void newFrame(const QImage &frame);
	void gotPicSz(int w, int h);

public slots:
    void started(); // thread loop is in here
private:
    int cntr;
	int testCntr;
    AVFrame             *pFrame;
    AVFrame             *pFrameRGB;
    uint8_t             *buffer;
    AVCodec             *pCodec;
    AVCodecContext      *pCodecCtx;
    AVDictionary        *optionsDict;
	QImage getFrame();
    volatile bool       isStopped;
};

#endif // CDECODER_H
