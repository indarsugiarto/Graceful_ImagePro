#ifndef SPINNEDGE_H
#define SPINNEDGE_H

#include <spin1_api.h>

/* 3x3 GX and GY Sobel mask.  Ref: www.cee.hw.ac.uk/hipr/html/sobel.html */
static const short GX[3][3] = {{-1,0,1},
				 {-2,0,2},
				 {-1,0,1}};
static const short GY[3][3] = {{1,2,1},
				 {0,0,0},
				 {-1,-2,-1}};

/* Laplace operator: 5x5 Laplace mask.  Ref: Myler Handbook p. 135 */
static const short LAP[5][5] = {{-1,-1,-1,-1,-1},
				  {-1,-1,-1,-1,-1},
				  {-1,-1,24,-1,-1},
				  {-1,-1,-1,-1,-1},
				  {-1,-1,-1,-1,-1}};

/* Gaussian filter. Ref:  en.wikipedia.org/wiki/Canny_edge_detector */
static const short FILT[5][5] = {{2,4,5,4,2},
				   {4,9,12,9,4},
				   {5,12,15,12,5},
				   {4,9,12,9,4},
				   {2,4,5,4,2}};
static const short FILT_DENOM = 159;

#define TIMER_TICK_PERIOD_US 	1000000
#define PRIORITY_TIMER			3
#define PRIORITY_PROCESSING		2
#define PRIORITY_SDP			1
#define PRIORITY_MCPL			-1
#define PRIORITY_DMA			0

#define SDP_TAG_REPLY			1
#define SDP_UDP_REPLY_PORT		20000
#define SDP_HOST_IP				0x02F0A8C0	// 192.168.240.2, dibalik!

#define SDP_PORT_R_IMG_DATA		1
#define SDP_PORT_G_IMG_DATA		2
#define SDP_PORT_B_IMG_DATA		3
#define SDP_PORT_CONFIG			7
#define SDP_CMD_CONFIG			1	// will be sent via SDP_PORT_CONFIG
#define SDP_CMD_PROCESS			2	// will be sent via SDP_PORT_CONFIG
#define SDP_CMD_CLEAR			3	// will be sent via SDP_PORT_CONFIG
#define SDP_CMD_HOST_SEND_IMG	0x1234
#define SDP_CMD_REPLY_HOST_SEND_IMG	0x4321
#define IMG_OP_SOBEL_NO_FILT	1	// will be carried in arg2.low
#define IMG_OP_SOBEL_WITH_FILT	2	// will be carried in arg2.low
#define IMG_OP_LAP_NO_FILT		3	// will be carried in arg2.low
#define IMG_OP_LAP_WITH_FILT	4	// will be carried in arg2.low
#define IMG_NO_FILTERING		0
#define IMG_WITH_FILTERING		1
#define IMG_SOBEL				0
#define IMG_LAPLACE				1

#define IMG_R_BUFF0_BASE		0x61000000
#define IMG_G_BUFF0_BASE		0x62000000
#define IMG_B_BUFF0_BASE		0x63000000
#define IMG_R_BUFF1_BASE		0x64000000
#define IMG_G_BUFF1_BASE		0x65000000
#define IMG_B_BUFF1_BASE		0x66000000

//we also has direct key to each core (see initRouter())
#define MCPL_BCAST_INFO_KEY		0xbca50001	// for broadcasting ping and blkInfo
#define MCPL_BCAST_CMD_FILT		0xbca50002	// command for filtering only
#define MCPL_BCAST_CMD_DETECT	0xbca50003  // command for edge detection
#define MCPL_BCAST_GET_WLOAD	0xbca50004	// trigger workers to compute workload
#define MCPL_PING_REPLY			0x1ead0001
#define MCPL_FILT_DONE			0x1ead0002	// worker send signal to leader
#define MCPL_EDGE_DONE			0x1ead0003
//key with values

//some definitions
#define FLAG_FILTERING_DONE		0xf117
#define FLAG_DETECTION_DONE		0xde7c

typedef struct block_info {
	ushort wImg;
	ushort hImg;
	ushort isGrey;			// 0==color, 1==gray
	uchar opType;			// 0==sobel, 1==laplace
	uchar opFilter;			// 0==no filtering, 1==with filtering
	ushort nodeBlockID;		// will be send by host
	ushort maxBlock;		// will be send by host
	// then pointers to the image in SDRAM
	uchar *imgRIn;
	uchar *imgGIn;
	uchar *imgBIn;
	uchar *imgROut;
	uchar *imgGOut;
	uchar *imgBOut;
	// miscellaneous info
	uchar imageInfoRetrieved;
	uchar fullRImageRetrieved;
	uchar fullGImageRetrieved;
	uchar fullBImageRetrieved;
} block_info_t;

// worker info
typedef struct w_info {
	uchar wID[17];			// coreID of all workers (max is 17), hold by leadAp
	uchar subBlockID;		// this will be hold individually by each worker
	uchar tAvailable;		// total available workers, should be initialized to 1
	ushort startLine;
	ushort endLine;
	// helper pointers
	uchar *imgRIn;
	uchar *imgGIn;
	uchar *imgBIn;
	uchar *imgROut;
	uchar *imgGOut;
	uchar *imgBOut;
} w_info_t;


/* Due to filtering mechanism, the address of image might be changed.
 * Scenario:
 * 1. Without filtering:
 *		imgRIn will start from IMG_R_BUFF0_BASE and resulting in IMG_R_BUFF1_BASE
 *		imgGIn will start from IMG_G_BUFF0_BASE and resulting in IMG_G_BUFF1_BASE
 *		imgBIn will start from IMG_B_BUFF0_BASE and resulting in IMG_B_BUFF1_BASE
 * 2. Aftering filtering:
 *		imgRIn will start from IMG_R_BUFF1_BASE and resulting in IMG_R_BUFF0_BASE
 *		imgGIn will start from IMG_G_BUFF1_BASE and resulting in IMG_G_BUFF0_BASE
 *		imgBIn will start from IMG_B_BUFF1_BASE and resulting in IMG_B_BUFF0_BASE
 *
*/

// for dma operation
#define DMA_FETCH_IMG_TAG		0x14
#define DMA_STORE_IMG_TAG		0x41
uint szDMA;
#define DMA_MOVE_IMG_R			0x52
#define DMA_MOVE_IMG_G			0x47
#define DMA_MOVE_IMG_B			0x42

uint myCoreID;
w_info_t workers;
block_info_t *blkInfo;			// let's put in sysram, to be shared with workers
uchar nFiltJobDone;				// will be used to count how many workers have
uchar nEdgeJobDone;				// finished their job in either filtering or edge detection

sdp_msg_t *reportMsg;

// forward declaration
void triggerProcessing(uint arg0, uint arg1);	// after filterning, leadAp needs to copy
													// the content from FIL_IMG to ORG_IMG
void imgDetection(uint arg0, uint arg1);	// this might be the most intense function
void imgFiltering(uint arg0, uint arg1);	// this is separate operation from edge detection
void initSDP();
void initRouter();
void initImage();
void initIPTag();
void hDMADone(uint tid, uint tag);
void hTimer(uint tick, uint Unused);
void hMCPL(uint key, uint payload);
void imgFiltering(uint arg0, uint arg1);
void imgProcessing(uint arg0, uint arg1);
void cleanUp();
void computeWLoad(uint arg0, uint arg1);
void afterFiltDone(uint arg0, uint arg1);
void afterEdgeDone(uint arg0, uint arg1);

static uchar *dtcmImgBuf = NULL;
// for fetching/storing image
volatile uchar dmaImgFromSDRAMdone;
// also for copying image from sdp to sdram via dma
volatile uchar dmaImg2SDRAMdone;
ushort dLen;
uchar whichRGB;	//0=R, 1=G, 2=B

// helper/debugging functions
void printImgInfo(uint opType, uint None);
void printWID(uint None, uint Neno);

#endif // SPINNEDGE_H

