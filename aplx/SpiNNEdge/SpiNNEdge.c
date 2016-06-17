#include <spin1_api.h>
#include "SpiNNEdge.h"

void hDMADone(uint tid, uint tag)
{

	if(tag == DMA_STORE_IMG_TAG) {
		dmaImg2SDRAMdone = 1;
		switch(whichRGB) {
		case SDP_PORT_R_IMG_DATA: blkInfo->imgRIn += dLen; break;
		case SDP_PORT_G_IMG_DATA: blkInfo->imgGIn += dLen; break;
		case SDP_PORT_B_IMG_DATA: blkInfo->imgBIn += dLen; break;
		}
	}
	else if((tag & 0xFFFF) == DMA_FETCH_IMG_TAG) {
		//io_printf(IO_BUF, "dma tag = 0x%x\n", tag);
		if((tag >> 16) == myCoreID) {
			dmaImgFromSDRAMdone = 1;	// sor the image processing can continue
			//io_printf(IO_BUF, "for me!\n");
		}
	}
}

// only core <0,0,leadAp> will do
void notifyHostDone(uint arg0, uint arg1)
{
	resultMsg.length = sizeof(sdp_hdr_t);
	spin1_send_sdp_msg(&resultMsg, 10);
}

void hMCPL(uint key, uint payload)
{
	if(key==MCPL_BCAST_INFO_KEY) {
		// leadAp sends "0" for ping, worker replys with its core
		if(payload==0)
			spin1_send_mc_packet(MCPL_PING_REPLY, myCoreID, WITH_PAYLOAD);
		else
			blkInfo = (block_info_t *)payload;
	}
	else if(key==myCoreID) {
		workers.tAvailable = payload >> 16;
		workers.subBlockID = payload & 0xFFFF;
	}
	else if(key==MCPL_BCAST_CMD_FILT) {
		spin1_schedule_callback(imgFiltering, 0, 0, PRIORITY_PROCESSING);
	}
	else if(key==MCPL_BCAST_CMD_DETECT) {
		spin1_schedule_callback(imgDetection, 0, 0, PRIORITY_PROCESSING);
	}
	else if(key==MCPL_BCAST_GET_WLOAD) {
		spin1_schedule_callback(computeWLoad, 0, 0, PRIORITY_PROCESSING);
	}
	else if(key==MCPL_BCAST_HOST_ACK) {
		if(payload==blkInfo->nodeBlockID)
			hostAck = 1;
	}
	//------------------------ this is leadAp only part --------------------------
	else if(key==MCPL_PING_REPLY) {
		workers.wID[workers.tAvailable] = payload;	// this will be automatically mapped to workers.subBlockID[workers.tAvailable]
		workers.tAvailable++;
	}
	else if(key==MCPL_FILT_DONE) {
		nFiltJobDone++;
		if(nFiltJobDone==workers.tAvailable)
			spin1_schedule_callback(afterFiltDone, 0, 0, PRIORITY_PROCESSING);
	}
	else if(key==MCPL_EDGE_DONE) {
		nEdgeJobDone++;
		if(nEdgeJobDone==workers.tAvailable)
			spin1_schedule_callback(afterEdgeDone, 0, 0, PRIORITY_PROCESSING);
	}
	else if(key==MCPL_BLOCK_DONE) {
		nBlockDone++;
		if(nBlockDone==blkInfo->maxBlock)
			spin1_schedule_callback(notifyHostDone, 0, 0, PRIORITY_PROCESSING);
	}
}

void c_main()
{
	myCoreID = sark_core_id();
	spin1_callback_on(MCPL_PACKET_RECEIVED, hMCPL, PRIORITY_MCPL);
	spin1_callback_on(DMA_TRANSFER_DONE, hDMADone, PRIORITY_DMA);
	initSDP();	// sebelumnya hanya leadAp, tapi masalahnya semua core butuh reportMsg dan debugMsg

	// only leadAp has access to dma and sdp
	if(leadAp) {

		// the danger of sdp_msg_t * without initialization is...
		/*
		reportMsg = sark_alloc(1, sizeof(sdp_msg_t));
		resultMsg = sark_alloc(1, sizeof(sdp_msg_t));
		io_printf(IO_BUF, "reportMsg @ 0x%x, resultMsg @ 0x%x\n", reportMsg, resultMsg);
		*/

		io_printf(IO_STD, "[SpiNNEdge] leadAp running @ core-%d\n", sark_core_id());
		io_printf(IO_BUF, "Will allocate %d in sysram\n", sizeof(block_info_t));
		// prepare chip-level image block information
		//blkInfo = sark_xalloc(sv->sysram_heap, sizeof(block_info_t),
		//					  sark_app_id(), ALLOC_LOCK);
		blkInfo = sark_xalloc(sv->sdram_heap, sizeof(block_info_t),
									  sark_app_id(), ALLOC_LOCK);
		if(blkInfo==NULL) {
			io_printf(IO_BUF, "blkInfo alloc error!\n");
			rt_error(RTE_ABORT);
		}
		else
			initImage();	// some of blkInfo are initialized there
		spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
		spin1_callback_on(TIMER_TICK, hTimer, PRIORITY_TIMER);
		spin1_callback_on(SDP_PACKET_RX, hSDP, PRIORITY_SDP);

		workers.wID[0] = myCoreID;
		workers.tAvailable = 1;
		workers.subBlockID = 0;	// leadAp has task ID-0
		chips = NULL;
		initRouter();
		initIPTag();
	}
	else {
		io_printf(IO_BUF, "SpiNNEdge running @ core-%d id-%d\n",
				  sark_core_id(), sark_app_id());
	}

	// let's test blkInfo

	spin1_start(SYNC_NOWAIT);
}
