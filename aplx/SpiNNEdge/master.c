#include "SpiNNEdge.h"


/* TODO:
 * - We can use hTimer for pooling workers from time to time (to find out,
 *   which one is alive).
 * */
void hTimer(uint tick, uint Unused)
{
	// first-tick: populate workers
	if(tick==1) {
		sark_delay_us(1000*sv->p2p_addr);
		io_printf(IO_BUF, "Collecting worker IDs...\n");
		/* Initial process: broadcast info:
		 * payload == 0 : ping
		 * payload != 0 : location of block_info_t variable
		 * */
		spin1_send_mc_packet(MCPL_BCAST_INFO_KEY, 0, WITH_PAYLOAD);
		spin1_send_mc_packet(MCPL_BCAST_INFO_KEY, (uint)blkInfo, WITH_PAYLOAD);
	}
	// second tick: broadcast info to workers, assuming 1-sec is enough for ID collection
	else if(tick==2) {
		sark_delay_us(1000*sv->p2p_addr);
		io_printf(IO_BUF, "Distributing wIDs...\n");
		// payload.high = tAvailable, payload.low = wID
		for(uint i= 1; i<workers.tAvailable; i++)
			spin1_send_mc_packet(workers.wID[i], (workers.tAvailable << 16) + i, WITH_PAYLOAD);
	}
	else if(tick==3) {
		// debugging
		printWID(0,0);
		io_printf(IO_STD, "Chip-%d ready!\n", sv->p2p_addr);
	}
	else if(tick==4) {
		/*
		ushort x = (sv->p2p_addr >> 8) * 2;
		ushort y = (sv->p2p_addr) & 0xFF;
		sark_delay_us(1000*sv->p2p_addr);
		io_printf(IO_STD, "Simulating 1000x1007 pixels!\n");
		blkInfo->isGrey = 0; //1=Grey, 0=color
		blkInfo->wImg = 1000;
		blkInfo->hImg = 1007;
		blkInfo->nodeBlockID = x+y;
		blkInfo->maxBlock = 4;
		blkInfo->imageInfoRetrieved = 1;
		spin1_send_mc_packet(MCPL_BCAST_GET_WLOAD, 0, WITH_PAYLOAD);
		computeWLoad(0,0);
		*/
	}
	else {

	}
}

// TODO: initIPTag()
void initIPTag()
{
	// only chip <0,0>
	if(sv->p2p_addr==0) {
		sdp_msg_t iptag;
		iptag.flags = 0x07;	// no replay
		iptag.tag = 0;		// internal
		iptag.srce_addr = sv->p2p_addr;
		iptag.srce_port = 0xE0 + myCoreID;	// use port-7
		iptag.dest_addr = sv->p2p_addr;
		iptag.dest_port = 0;				// send to "root"
		iptag.cmd_rc = 26;
		iptag.arg1 = (1 << 16) + SDP_TAG_REPLY;
		iptag.arg2 = SDP_UDP_REPLY_PORT;
		iptag.arg3 = SDP_HOST_IP;
		iptag.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
		spin1_send_sdp_msg(&iptag, 10);
	}
}

/*
// Let's use:
// port-7 for receiving configuration and command:
//		cmd_rc == SDP_CMD_CONFIG -->
//			arg1.high == img_width, arg1.low == img_height
//			arg2.high : 0==gray, 1==colour(rgb)
//			seq.high == isGray: 1==gray, 0==colour(rgb)
//			seq.low == type_of_operation:
//						1 = sobel_no_filtering
//						2 = sobel_with_filtering
//						3 = laplace_no_filtering
//						4 = laplace_with_filtering
//		cmd_rc == SDP_CMD_PROCESS
//		cmd_rc == SDP_CMD_CLEAR
If leadAp detects filtering, first it ask workers to do filtering only.
Then each worker will start filtering. Once finished, each worker
will report to leadAp about this filtering.
Once leadAp received filtering report from all workers, leadAp
trigger the edge detection
*/
void hSDP(uint mBox, uint port)
{
	sdp_msg_t *msg = (sdp_msg_t *)mBox;
	uint checkDMA;
	// if host send SDP_CONFIG, means the image has been
	// loaded into sdram via ybug operation
	if(port==SDP_PORT_CONFIG) {
		if(msg->cmd_rc == SDP_CMD_CONFIG || msg->cmd_rc == SDP_CMD_CONFIG_CHAIN) {
			blkInfo->isGrey = msg->seq >> 8; //1=Grey, 0=color
			blkInfo->wImg = msg->arg1 >> 16;
			blkInfo->hImg = msg->arg1 & 0xFFFF;
			blkInfo->nodeBlockID = msg->arg2 >> 16;
			blkInfo->maxBlock = msg->arg2 & 0xFFFF;
			switch(msg->seq & 0xFF) {
			case IMG_OP_SOBEL_NO_FILT:
				blkInfo->opFilter = IMG_NO_FILTERING; blkInfo->opType = IMG_SOBEL;
				break;
			case IMG_OP_SOBEL_WITH_FILT:
				blkInfo->opFilter = IMG_WITH_FILTERING; blkInfo->opType = IMG_SOBEL;
				break;
			case IMG_OP_LAP_NO_FILT:
				blkInfo->opFilter = IMG_NO_FILTERING; blkInfo->opType = IMG_LAPLACE;
				break;
			case IMG_OP_LAP_WITH_FILT:
				blkInfo->opFilter = IMG_WITH_FILTERING; blkInfo->opType = IMG_LAPLACE;
				break;
			}

			// should be propagated?
			if(sark_chip_id() == 0 && msg->cmd_rc == SDP_CMD_CONFIG_CHAIN) {
				ushort i, x, y, id, maxBlock = blkInfo->maxBlock;
				chainMode = 1;
				if(chips != NULL)
					sark_free(chips);
				chips = sark_alloc(maxBlock-1, sizeof(chain_t));
				for(i=0; i<blkInfo->maxBlock-1; i++) {	// don't include me!
					x = msg->data[i*3]; chips[i].x = x;
					y = msg->data[i*3 + 1]; chips[i].y = y;
					id = msg->data[i*3 + 2]; chips[i].id = id;
					msg->dest_addr = (x << 8) + y;
					msg->arg2 = (id << 16) + maxBlock;
					spin1_send_sdp_msg(msg, 10);
				}
			}
			else {
				chainMode = 0;
			}

			blkInfo->imageInfoRetrieved = 1;

			// just debugging:
			spin1_schedule_callback(printImgInfo, msg->seq, 0, PRIORITY_PROCESSING);
			// then inform workers to compute workload
			spin1_send_mc_packet(MCPL_BCAST_GET_WLOAD, 0, WITH_PAYLOAD);
			spin1_schedule_callback(computeWLoad,0,0, PRIORITY_PROCESSING);	// only for leadAp

		}
		// TODO: don't forget to give a "kick" from python?
		else if(msg->cmd_rc == SDP_CMD_CLEAR) {
			initImage();
		}
	}

	// if host send images
	// NOTE: what if we use arg part of SCP for image data? OK let's try, because see HOW.DO...
	else if(port==SDP_PORT_R_IMG_DATA) {
		io_printf(IO_BUF, "Got in SDP_PORT_R_IMG_DATA with chainMode = %d...\n", chainMode);
		// forward?
		if(chainMode == 1) {
			for(ushort i=0; i<blkInfo->maxBlock-1; i++) {
				io_printf(IO_BUF, "Forwarding to <%d,%d>:%d\n",
						  chips[i].x, chips[i].y, chips[i].id);
				msg->dest_addr = (chips[i].x << 8) + chips[i].y;
				msg->arg2 = (chips[i].id << 16) + blkInfo->maxBlock;
				spin1_send_sdp_msg(msg, 10);
			}
		}

		// get the image data from SCP+data_part
		dLen = msg->length - sizeof(sdp_hdr_t);	// dLen is used by hDMADone()
		//io_printf(IO_BUF, "Receiving %d-bytes\n", dLen);

		if(dLen > 0) {
			//io_printf(IO_BUF, "blkInfo->imgRIn = 0x%x\n", blkInfo->imgRIn);
			// in the beginning, dtcmImgBuf is NULL
			if(dtcmImgBuf==NULL) {
				dtcmImgBuf = sark_alloc(sizeof(cmd_hdr_t) + SDP_BUF_SIZE, sizeof(uchar));
				if(dtcmImgBuf==NULL) {
					io_printf(IO_STD, "dtcmImgBuf alloc fail!\n");
					rt_error(RTE_ABORT);
				}
				dmaImg2SDRAMdone = 1;	// will be set in hDMA()
				whichRGB = SDP_PORT_R_IMG_DATA;
			}

			// if the previous dma transfer is still in progress...
			while(dmaImg2SDRAMdone==0) {
			}
			// copy to dtcm buffer, if the previous transfer is complete
			// spin1_memcpy((void *)imgRIn, (void *)&msg->cmd_rc, dLen);
			dmaImg2SDRAMdone = 0;	// reset the dma done flag
			spin1_memcpy((void *)dtcmImgBuf, (void *)&msg->cmd_rc, dLen);
			checkDMA = spin1_dma_transfer(DMA_STORE_IMG_TAG, (void *)blkInfo->imgRIn, (void *)dtcmImgBuf,
							   DMA_WRITE, dLen);
			if(checkDMA==0)
				io_printf(IO_STD, "DMA full!\n");
			// imgRIn += dLen; -> moved to hDMADone

			// send reply immediately only if the sender is host-PC
			if(msg->srce_port == PORT_ETH) {
				io_printf(IO_BUF, "Sending reply...\n");
				reportMsg->cmd_rc = SDP_CMD_REPLY_HOST_SEND_IMG;
				reportMsg->seq = dLen;
				reportMsg->length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
				spin1_send_sdp_msg(reportMsg, 10);
			}
			io_printf(IO_BUF, "just receives R-channel\n");
		}
		// if zero data is sent, means the end of message transfer!
		else {
			io_printf(IO_STD, "layer-R is complete!\n");
			sark_free(dtcmImgBuf);
			dtcmImgBuf = NULL;	// reset ImgBuffer in DTCM
			blkInfo->imgRIn = (char *)IMG_R_BUFF0_BASE;	// reset to initial base position
			blkInfo->fullRImageRetrieved = 1;
			if(blkInfo->isGrey==1)
				spin1_schedule_callback(triggerProcessing, 0, 0, PRIORITY_PROCESSING);
		}
	}
	else if(port==SDP_PORT_G_IMG_DATA) {
		// forward?
		if(chainMode == 1) {
			for(ushort i=0; i<blkInfo->maxBlock-1; i++) {
				io_printf(IO_BUF, "Forwarding to <%d,%d>:%d\n",
						  chips[i].x, chips[i].y, chips[i].id);
				msg->dest_addr = (chips[i].x << 8) + chips[i].y;
				msg->arg2 = (chips[i].id << 16) + blkInfo->maxBlock;
				spin1_send_sdp_msg(msg, 10);
			}
		}
		// get the image data from SCP+data_part
		dLen = msg->length - sizeof(sdp_hdr_t);
		//io_printf(IO_BUF, "Receiving %d-bytes\n", dLen);

		if(dLen > 0) {
			//io_printf(IO_BUF, "blkInfo->imgGIn = 0x%x\n", blkInfo->imgGIn);
			if(dtcmImgBuf==NULL) {
				dtcmImgBuf = sark_alloc(sizeof(cmd_hdr_t) + SDP_BUF_SIZE, sizeof(uchar));
				if(dtcmImgBuf==NULL) {
					io_printf(IO_STD, "dtcmImgBuf alloc fail!\n");
					rt_error(RTE_ABORT);
				}
				dmaImg2SDRAMdone = 1;
				whichRGB = SDP_PORT_G_IMG_DATA;
			}

			// wait the previous dma transfer
			while(dmaImg2SDRAMdone==0) {
			}
			// spin1_memcpy((void *)imgGIn, (void *)&msg->cmd_rc, dLen);
			dmaImg2SDRAMdone = 0;	// reset the dma done flag
			spin1_memcpy((void *)dtcmImgBuf, (void *)&msg->cmd_rc, dLen);
			checkDMA = spin1_dma_transfer(DMA_STORE_IMG_TAG, (void *)blkInfo->imgGIn, (void *)dtcmImgBuf,
							   DMA_WRITE, dLen);
			if(checkDMA==0)
				io_printf(IO_STD, "DMA full!\n");

			// send reply immediately only if the sender is host-PC
			if(msg->srce_port == PORT_ETH) {
				io_printf(IO_BUF, "Sending reply...\n");
				reportMsg->cmd_rc = SDP_CMD_REPLY_HOST_SEND_IMG;
				reportMsg->seq = dLen;
				reportMsg->length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
				spin1_send_sdp_msg(reportMsg, 10);
			}
		}
		// if zero data is sent, means the end of message transfer!
		else {
			io_printf(IO_STD, "layer-G is complete!\n");
			sark_free(dtcmImgBuf);
			dtcmImgBuf = NULL;	// reset ImgBuffer in DTCM
			blkInfo->imgGIn = (char *)IMG_G_BUFF0_BASE;	// reset to initial base position
			blkInfo->fullGImageRetrieved = 1;
		}
	}
	else if(port==SDP_PORT_B_IMG_DATA) {
		// forward?
		if(chainMode == 1) {
			for(ushort i=0; i<blkInfo->maxBlock-1; i++) {
				io_printf(IO_BUF, "Forwarding to <%d,%d>:%d\n",
						  chips[i].x, chips[i].y, chips[i].id);
				msg->dest_addr = (chips[i].x << 8) + chips[i].y;
				msg->arg2 = (chips[i].id << 16) + blkInfo->maxBlock;
				spin1_send_sdp_msg(msg, 10);
			}
		}
		// get the image data from SCP+data_part
		dLen = msg->length - sizeof(sdp_hdr_t);
		//io_printf(IO_BUF, "Receiving %d-bytes\n", dLen);

		if(dLen > 0) {
			//io_printf(IO_BUF, "blkInfo->imgBIn = 0x%x\n", blkInfo->imgBIn);
			if(dtcmImgBuf==NULL) {
				dtcmImgBuf = sark_alloc(sizeof(cmd_hdr_t) + SDP_BUF_SIZE, sizeof(uchar));
				if(dtcmImgBuf==NULL) {
					io_printf(IO_STD, "dtcmImgBuf alloc fail!\n");
					rt_error(RTE_ABORT);
				}
				dmaImg2SDRAMdone = 1;
				whichRGB = SDP_PORT_B_IMG_DATA;
			}
			// wait for the previous dma transfer
			while(dmaImg2SDRAMdone==0) {
			}
			dmaImg2SDRAMdone = 0;	// reset the dma done flag
			spin1_memcpy((void *)dtcmImgBuf, (void *)&msg->cmd_rc, dLen);
			checkDMA = spin1_dma_transfer(DMA_STORE_IMG_TAG, (void *)blkInfo->imgBIn, (void *)dtcmImgBuf,
							   DMA_WRITE, dLen);
			if(checkDMA==0)
				io_printf(IO_STD, "DMA full!\n");

			// send reply immediately only if the sender is host-PC
			if(msg->srce_port == PORT_ETH) {
				io_printf(IO_BUF, "Sending reply...\n");
				reportMsg->cmd_rc = SDP_CMD_REPLY_HOST_SEND_IMG;
				reportMsg->seq = dLen;
				reportMsg->length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
				spin1_send_sdp_msg(reportMsg, 10);
			}
		}
		// if zero data is sent, means the end of message transfer!
		else {
			io_printf(IO_STD, "layer-B is complete!\n");
			sark_free(dtcmImgBuf);
			dtcmImgBuf = NULL;	// reset ImgBuffer in DTCM
			blkInfo->imgBIn = (char *)IMG_B_BUFF0_BASE;	// reset to initial base position
			blkInfo->fullBImageRetrieved = 1;
			// at the end of B image transmission, it should trigger processing
			spin1_schedule_callback(triggerProcessing, 0, 0, PRIORITY_PROCESSING);
		}
	}
	spin1_msg_free(msg);
}

void triggerProcessing(uint arg0, uint arg1)
{
	/*
	if(blkInfo->opFilter==1) {
		// broadcast command for filtering
		nFiltJobDone = 0;
		spin1_send_mc_packet(MCPL_BCAST_CMD_FILT, 0, WITH_PAYLOAD);
	}
	else {
		nEdgeJobDone = 0;
		// broadcast command for detection
		spin1_send_mc_packet(MCPL_BCAST_CMD_DETECT, 0, WITH_PAYLOAD);
	}
	*/

	// debugging: see, how many chips are able to collect the images:
	// myDelay();
	// io_printf(IO_STD, "Image is completely received. Ready for processing!\n");
	// return;

	// Let's skip filtering
	spin1_send_mc_packet(MCPL_BCAST_CMD_DETECT, 0, WITH_PAYLOAD);
}

// afterFiltDone() will swap BUFF1_BASE to BUFF0_BASE
void afterFiltDone(uint arg0, uint arg1)
{

}

// afterEdgeDone() send the result to host?
void afterEdgeDone(uint arg0, uint arg1)
{
	io_printf(IO_STD, "Edge detection done!\n");
}

void initSDP()
{
	spin1_callback_on(SDP_PACKET_RX, hSDP, PRIORITY_SDP);
	reportMsg->flags = 0x07;	//no reply
	reportMsg->tag = SDP_TAG_REPLY;
	reportMsg->srce_port = SDP_PORT_CONFIG;
	reportMsg->srce_addr = sv->p2p_addr;
	reportMsg->dest_port = PORT_ETH;
	reportMsg->dest_addr = sv->eth_addr;
	//reportMsg->length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
	// prepare iptag?
	initIPTag();
}

/* initRouter() initialize MCPL routing table by leadAp. Use two keys:
 * MCPL_BCAST_KEY and MCPL_TO_LEADER
 * */
void initRouter()
{
	uint allRoute = 0xFFFF80;	// excluding core-0 and external links
	uint leader = (1 << (myCoreID+6));
	uint workers = allRoute & ~leader;

	// first, set individual destination
	uint e = rtr_alloc(17);
	if(e==0) {
		rt_error(RTE_ABORT);
	} else {
		// each destination core might have its own key association
		// so that leadAp can tell each worker, which region is their part
		for(uint i=0; i<17; i++)
			// starting from core-1 up to core-17
			rtr_mc_set(e+i, i+1, 0xFFFFFFFF, (MC_CORE_ROUTE(i+1)));
	}
	// then add another keys
	e = rtr_alloc(7);
	if(e==0) {
		rt_error(RTE_ABORT);
	} else {
		rtr_mc_set(e, MCPL_BCAST_INFO_KEY, 0xFFFFFFFF, workers); e++;
		rtr_mc_set(e, MCPL_BCAST_CMD_FILT, 0xFFFFFFFF, allRoute); e++;
		rtr_mc_set(e, MCPL_BCAST_CMD_DETECT, 0xFFFFFFFF, allRoute); e++;
		rtr_mc_set(e, MCPL_BCAST_GET_WLOAD, 0xFFFFFFFF, workers); e++;
		rtr_mc_set(e, MCPL_PING_REPLY, 0xFFFFFFFF, leader); e++;
		rtr_mc_set(e, MCPL_FILT_DONE, 0xFFFFFFFF, leader); e++;
		rtr_mc_set(e, MCPL_EDGE_DONE, 0xFFFFFFFF, leader); e++;
	}
}

void initImage()
{
	blkInfo->imageInfoRetrieved = 0;
	blkInfo->fullRImageRetrieved = 0;
	blkInfo->fullGImageRetrieved = 0;
	blkInfo->fullBImageRetrieved = 0;
	blkInfo->imgRIn = (uchar *)IMG_R_BUFF0_BASE;
	blkInfo->imgGIn = (uchar *)IMG_G_BUFF0_BASE;
	blkInfo->imgBIn = (uchar *)IMG_B_BUFF0_BASE;
	blkInfo->imgROut = (uchar *)IMG_R_BUFF1_BASE;
	blkInfo->imgGOut = (uchar *)IMG_G_BUFF1_BASE;
	blkInfo->imgBOut = (uchar *)IMG_B_BUFF1_BASE;
}


// terminate gracefully
void cleanUp()
{
	sark_xfree(sv->sysram_heap, blkInfo, ALLOC_LOCK);
	// in this app, we "fix" the address of image, no need for xfree
}

/*_____________________________ Helper/debugging functions _________________________*/

void printImgInfo(uint opType, uint None)
{
	io_printf(IO_BUF, "Image w = %d, h = %d, ", blkInfo->wImg, blkInfo->hImg);
	if(blkInfo->isGrey==1)
		io_printf(IO_BUF, "grascale, ");
	else
		io_printf(IO_BUF, "color, ");
	switch(opType & 0xFF) {
	case IMG_OP_SOBEL_NO_FILT:
		io_printf(IO_BUF, "for sobel without filtering\n");
		break;
	case IMG_OP_SOBEL_WITH_FILT:
		io_printf(IO_BUF, "for sobel with filtering\n");
		break;
	case IMG_OP_LAP_NO_FILT:
		io_printf(IO_BUF, "for laplace without filtering\n");
		break;
	case IMG_OP_LAP_WITH_FILT:
		io_printf(IO_BUF, "for laplace with filtering\n");
		break;
	}
	io_printf(IO_BUF, "nodeBlockID = %d with maxBlock = %d\n",
			  blkInfo->nodeBlockID, blkInfo->maxBlock);
}

void printWID(uint None, uint Neno)
{
	for(uint i=0; i<workers.tAvailable; i++)
		io_printf(IO_BUF, "wID-%d is core-%d\n", i, workers.wID[i]);
}

void myDelay()
{
	ushort x = CHIP_X(sv->board_addr);
	ushort y = CHIP_Y(sv->board_addr);
	spin1_delay_us((x*2+y)*1000);
}

/*____________________________________________________ Helper/debugging functions __*/
