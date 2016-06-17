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
    else if(tick==5) {

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
		// set the reply tag
		iptag.arg1 = (1 << 16) + SDP_TAG_REPLY;
		iptag.arg2 = SDP_UDP_REPLY_PORT;
		iptag.arg3 = SDP_HOST_IP;
		iptag.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
		spin1_send_sdp_msg(&iptag, 10);
		// set the result tag
		iptag.arg1 = (1 << 16) + SDP_TAG_RESULT;
		iptag.arg2 = SDP_UDP_RESULT_PORT;
		spin1_send_sdp_msg(&iptag, 10);
        // set the debug tag
        iptag.arg1 = (1 << 16) + SDP_TAG_DEBUG;
        iptag.arg2 = SDP_UDP_DEBUG_PORT;
        spin1_send_sdp_msg(&iptag, 10);
    }
}

void sendReply(uint arg0, uint arg1)
{
	// io_printf(IO_BUF, "Sending reply...\n");
	reportMsg.cmd_rc = SDP_CMD_REPLY_HOST_SEND_IMG;
	reportMsg.seq = arg0;
	reportMsg.dest_addr = sv->eth_addr;
	reportMsg.dest_port = PORT_ETH;
	//reportMsg.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
	uint checkSDP = spin1_send_sdp_msg(&reportMsg, 10);
	if(checkSDP == 0)
		io_printf(IO_STD, "SDP fail!\n");
}

// since R,G and B channel processing are similar
void getImage(sdp_msg_t *msg, uint port)
{
	uint checkDMA = 0;
	uchar *imgIn;
	switch(port) {
	case SDP_PORT_R_IMG_DATA: imgIn = blkInfo->imgRIn; break;	// blkInfo->imgRIn is altered in hDMA!!!
	case SDP_PORT_G_IMG_DATA: imgIn = blkInfo->imgGIn; break;
	case SDP_PORT_B_IMG_DATA: imgIn = blkInfo->imgBIn; break;
	}

	// forward?
	if(chainMode == 1 && sark_chip_id()==0) {
		for(ushort i=0; i<blkInfo->maxBlock-1; i++) {	// don't include me!
			/*
			io_printf(IO_BUF, "Forwarding to <%d,%d>:%d\n",
					  chips[i].x, chips[i].y, chips[i].id);
			*/
			msg->dest_addr = (chips[i].x << 8) + chips[i].y;
			msg->srce_addr = sv->p2p_addr;	// replace with my ID instead of ETH
			msg->srce_port = myCoreID;
			spin1_send_sdp_msg(msg, 10);
		}
	}

	// get the image data from SCP+data_part
	dLen = msg->length - sizeof(sdp_hdr_t);	// dLen is used by hDMADone()
	// io_printf(IO_BUF, "Receiving %d-bytes\n", dLen);

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
			whichRGB = port;
		}

		// if the previous dma transfer is still in progress...
		while(dmaImg2SDRAMdone==0) {
		}
		// copy to dtcm buffer, if the previous transfer is complete
		// spin1_memcpy((void *)imgRIn, (void *)&msg->cmd_rc, dLen);
		dmaImg2SDRAMdone = 0;	// reset the dma done flag
		spin1_memcpy((void *)dtcmImgBuf, (void *)&msg->cmd_rc, dLen);

		/*
		// debugging, VERY SLOW...!!!!
		io_printf(IO_BUF, "Receiving %d pixels!\n", dLen);
		for(ushort i=0; i<dLen; i++)
			io_printf(IO_BUF, "%02x ", dtcmImgBuf[i]);
		io_printf(IO_BUF, "\n");
		*/

		do {
			// checkDMA = spin1_dma_transfer(DMA_STORE_IMG_TAG, (void *)blkInfo->imgRIn, (void *)dtcmImgBuf,
			//				   DMA_WRITE, dLen);
			//checkDMA = spin1_dma_transfer(DMA_STORE_IMG_TAG + (myCoreID << 16), (void *)imgIn,
			//							  (void *)dtcmImgBuf, DMA_WRITE, dLen);
			// no need to include core ID, because only leadAp will do this!!!
			checkDMA = spin1_dma_transfer(DMA_STORE_IMG_TAG , (void *)imgIn,
										  (void *)dtcmImgBuf, DMA_WRITE, dLen);
			if(checkDMA==0)
				io_printf(IO_BUF, "[Retrieving] DMA full! Retry!\n");
		} while(checkDMA==0);
		// imgRIn += dLen; -> moved to hDMADone

		// send reply immediately only if the sender is host-PC
		// if(msg->srce_port == PORT_ETH)	// no, because we have modified srce_port in chainMode 1
		if(chainMode == 0 || (chainMode == 1 && sv->p2p_addr == 0))
			sendReply(dLen, 0);
	}
	// if zero data is sent, means the end of message transfer!
	else {
		sark_free(dtcmImgBuf);
		dtcmImgBuf = NULL;	// reset ImgBuffer in DTCM
		switch(port) {
		case SDP_PORT_R_IMG_DATA:
			io_printf(IO_STD, "layer-R is complete!\n");
			blkInfo->imgRIn = (char *)IMG_R_BUFF0_BASE;	// reset to initial base position
			blkInfo->fullRImageRetrieved = 1;
			break;
		case SDP_PORT_G_IMG_DATA:
			io_printf(IO_STD, "layer-G is complete!\n");
			blkInfo->imgGIn = (char *)IMG_G_BUFF0_BASE;	// reset to initial base position
			blkInfo->fullGImageRetrieved = 1;
			break;
		case SDP_PORT_B_IMG_DATA:
			io_printf(IO_STD, "layer-B is complete!\n");
			blkInfo->imgBIn = (char *)IMG_B_BUFF0_BASE;	// reset to initial base position
			blkInfo->fullBImageRetrieved = 1;
			break;
		}

		// check: if the image in memory of all chips are similar
		// io_printf(IO_STD, "Please check my content!\n"); // --> OK!

		// if grey or at the end of B image transmission, it should trigger processing
		if(blkInfo->isGrey==1 || port==SDP_PORT_B_IMG_DATA)
			spin1_schedule_callback(triggerProcessing, 0, 0, PRIORITY_PROCESSING);

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

			if(msg->cmd_rc == SDP_CMD_CONFIG_CHAIN)
				chainMode = 1;
			else
				chainMode = 0;
			// should be propagated?
			if(sark_chip_id() == 0 && chainMode==1) {
				ushort i, x, y, id, maxBlock = blkInfo->maxBlock;
				if(chips != NULL)
					sark_free(chips);
				chips = sark_alloc(maxBlock-1, sizeof(chain_t));

				for(i=0; i<blkInfo->maxBlock-1; i++) {	// don't include me!
					x = msg->data[i*3];			chips[i].x = x;
					y = msg->data[i*3 + 1];		chips[i].y = y;
					id = msg->data[i*3 + 2];	chips[i].id = id;
					msg->dest_addr = (x << 8) + y;
					msg->arg2 = (id << 16) + maxBlock;
					msg->srce_addr = sv->p2p_addr;
					msg->srce_port = myCoreID;
					spin1_send_sdp_msg(msg, 10);
				}

			}

			blkInfo->imageInfoRetrieved = 1;
			initImage();

			// just debugging:
			spin1_schedule_callback(printImgInfo, msg->seq, 0, PRIORITY_PROCESSING);
			// then inform workers to compute workload
			spin1_send_mc_packet(MCPL_BCAST_GET_WLOAD, 0, WITH_PAYLOAD);
			spin1_schedule_callback(computeWLoad,0,0, PRIORITY_PROCESSING);	// only for leadAp

		}
		// TODO: don't forget to give a "kick" from python?
		else if(msg->cmd_rc == SDP_CMD_CLEAR) {
			io_printf(IO_STD, "Clearing...\n");
			initImage();

			// should be propagated?
			if(sark_chip_id() == 0 && chainMode==1) {
				for(ushort i=0; i<blkInfo->maxBlock-1; i++) {	// don't include me!
					msg->dest_addr = (chips[i].x << 8) + chips[i].y;
					msg->srce_addr = sv->p2p_addr;	// replace with my ID instead of ETH
					msg->srce_port = myCoreID;
					spin1_send_sdp_msg(msg, 10);
				}

			}
		}
	}

	// if host send images
	// NOTE: what if we use arg part of SCP for image data? OK let's try, because see HOW.DO...
	else if(port==SDP_PORT_R_IMG_DATA) {
		getImage(msg, SDP_PORT_R_IMG_DATA);
	}
	else if(port==SDP_PORT_G_IMG_DATA) {
		getImage(msg, SDP_PORT_G_IMG_DATA);
	}
	else if(port==SDP_PORT_B_IMG_DATA) {
		getImage(msg, SDP_PORT_B_IMG_DATA);
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
	nEdgeJobDone = 0;
	nBlockDone = 0;
	spin1_send_mc_packet(MCPL_BCAST_CMD_DETECT, 0, WITH_PAYLOAD);
}

// afterFiltDone() will swap BUFF1_BASE to BUFF0_BASE
void afterFiltDone(uint arg0, uint arg1)
{

}

// check: sendResult() will be executed only by leadAp
// we cannot use workers.imgROut, because workers.imgROut differs from core to core
// use workers.blkImgROut instead!
void sendResult(uint arg0, uint arg1)
{
	// format sdp (scp_segment + data_segment):
	// cmd_rc = line number
	// seq = sequence of data
	// arg1 = number of data in the data_segment
	// arg2 = channel (R=0, G=1, B=2), NOTE: rgb info isn't in the srce_port!!!
	uchar *imgOut;
	ushort rem, sz;
	uint checkDMA;
	ushort l,c;

	// dtcmImgBuf should be NULL at this point
	if(dtcmImgBuf != NULL)
		io_printf(IO_BUF, "[Sending] Warning, dtcmImgBuf is not free!\n");
	dtcmImgBuf == sark_alloc(workers.wImg, sizeof(uchar));
	dLen = SDP_BUF_SIZE + sizeof(cmd_hdr_t);

	uint total[3] = {0};
	for(uchar rgb=0; rgb<3; rgb++) {
		if(rgb > 0 && blkInfo->isGrey==1) break;
		resultMsg.arg2 = rgb;

		io_printf(IO_STD, "[Sending] result channel-%d...\n", rgb);

		l = 0;	// for the imgXOut pointer
		for(ushort lines=workers.blkStart; lines<=workers.blkEnd; lines++) {

			// get the line from sdram
			switch(rgb) {
			case 0: imgOut = workers.blkImgROut + l*workers.wImg; break;
			case 1: imgOut = workers.blkImgGOut + l*workers.wImg; break;
			case 2: imgOut = workers.blkImgBOut + l*workers.wImg; break;
			}

			dmaImgFromSDRAMdone = 0;	// will be altered in hDMA
			do {
				checkDMA = spin1_dma_transfer(DMA_FETCH_IMG_TAG + (myCoreID << 16), (void *)imgOut,
											  (void *)dtcmImgBuf, DMA_READ, workers.wImg);
				if(checkDMA==0)
					io_printf(IO_BUF, "[Sending] DMA full! Retry!\n");

			} while(checkDMA==0);
			// wait until dma is completed
			while(dmaImgFromSDRAMdone==0) {
			}
			// then sequentially copy & send via sdp
			c = 0;
			rem = workers.wImg;
			do {
				//resultMsg.cmd_rc = lines;
				//resultMsg.seq = c+1;
				sz = rem>dLen?dLen:rem;
				//resultMsg.arg1 = sz;
				resultMsg.srce_port = c+1;
				resultMsg.srce_addr = (lines << 2) + rgb;
				spin1_memcpy((void *)&resultMsg.cmd_rc, (void *)(dtcmImgBuf + c*dLen), sz);
				resultMsg.length = sizeof(sdp_hdr_t) + sz;

				total[rgb] += sz;

				// send via sdp
				spin1_send_sdp_msg(&resultMsg, 10);
				io_printf(IO_BUF, "[Sending] rgbCh-%d, line-%d, chunk-%d via tag-%d\n", rgb,
						  lines, c+1, resultMsg.tag);

				spin1_delay_us((1 + blkInfo->nodeBlockID)*1000);

				// send debugging via debugMsg

				c++;		// for the dtcmImgBuf pointer
				rem -= sz;
			} while(rem > 0);
			l++;
		}
	} // end loop channel (rgb)

	io_printf(IO_STD, "Transfer done!\n");
	io_printf(IO_BUF, "[Sending] pixels [%d,%d,%d] done!\n", total[0], total[1], total[2]);

	// then send notification to chip<0,0> that my part is complete
	spin1_send_mc_packet(MCPL_BLOCK_DONE, 0, WITH_PAYLOAD);

	// release dtcm
	sark_free(dtcmImgBuf);
}

// afterEdgeDone() send the result to host?
void afterEdgeDone(uint arg0, uint arg1)
{
	io_printf(IO_STD, "Edge detection done!\n");

	// since each chip holds a part of image data, it needs to send individually to host
	sendResult(0, 0);
}

void initSDP()
{
	// prepare the reply message
	reportMsg.flags = 0x07;	//no reply
	reportMsg.tag = SDP_TAG_REPLY;
	reportMsg.srce_port = (SDP_PORT_CONFIG << 5) + myCoreID;
	reportMsg.srce_addr = sv->p2p_addr;
	reportMsg.dest_port = PORT_ETH;
	reportMsg.dest_addr = sv->eth_addr;
	reportMsg.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);	// it's fix!

	// prepare the result data
	resultMsg.flags = 0x07;
	resultMsg.tag = SDP_TAG_RESULT;
	// what if:
	// - srce_addr contains image line number + rgb info
	// - srce_port contains the data sequence
	//resultMsg.srce_port = myCoreID;		// during sending, this must be modified
	//resultMsg.srce_addr = sv->p2p_addr;
	resultMsg.dest_port = PORT_ETH;
	resultMsg.dest_addr = sv->eth_addr;
	//resultMsg.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);	// need to be modified later

    // and the debug data
    debugMsg.flags = 0x07;
    debugMsg.tag = SDP_TAG_DEBUG;
    debugMsg.srce_port = (SDP_PORT_CONFIG << 5) + myCoreID;
    debugMsg.srce_addr = sv->p2p_addr;
    debugMsg.dest_port = PORT_ETH;
    debugMsg.dest_addr = sv->eth_addr;
    debugMsg.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);

	// prepare iptag?
	io_printf(IO_BUF, "reportMsg.tag = %d, resultMsg.tag = %d, debugMsg.tag = %d\n",
			  reportMsg.tag, resultMsg.tag, debugMsg.tag);
}

/* initRouter() initialize MCPL routing table by leadAp. Use two keys:
 * MCPL_BCAST_KEY and MCPL_TO_LEADER
 * */
void initRouter()
{
	uint allRoute = 0xFFFF80;	// excluding core-0 and external links
	uint leader = (1 << (myCoreID+6));
	uint workers = allRoute & ~leader;
	uint dest;

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
	e = rtr_alloc(8);
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

		// special for MCPL_BLOCK_DONE
		ushort x = CHIP_X(sv->p2p_addr);
		ushort y = CHIP_Y(sv->p2p_addr);
		if (x>0 && y>0)			dest = (1 << 4);	// south-west
		else if(x>0 && y==0)	dest = (1 << 3);	// west
		else if(x==0 && y>0)	dest = (1 << 5);	// south
		else					dest = leader;
		rtr_mc_set(e, MCPL_BLOCK_DONE, 0xFFFFFFFF, dest); e++;
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
	dtcmImgBuf = NULL;
}


// terminate gracefully
void cleanUp()
{
	//sark_xfree(sv->sysram_heap, blkInfo, ALLOC_LOCK);
	sark_xfree(sv->sdram_heap, blkInfo, ALLOC_LOCK);
	// in this app, we "fix" the address of image, no need for xfree

	/*
	if(leadAp) {
		sark_free(reportMsg);
		sark_free(resultMsg);
	}
	*/
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
	io_printf(IO_BUF, "Running in mode-%d\n", chainMode);
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
