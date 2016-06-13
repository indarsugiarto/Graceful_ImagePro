
#include "SpiNNEdge.h"

// compute individual working block
void computeWLoad(uint arg0, uint arg1)
{


	// block-wide
	ushort nLinesPerBlock = blkInfo->hImg / blkInfo->maxBlock;
	ushort nRemInBlock = blkInfo->hImg % blkInfo->maxBlock;
	ushort blkStart = blkInfo->nodeBlockID * nLinesPerBlock;

	// core-wide
	if(blkInfo->nodeBlockID==blkInfo->maxBlock-1)
		nLinesPerBlock += nRemInBlock;
	ushort nLinesPerCore = nLinesPerBlock / workers.tAvailable;
	ushort nRemInCore = nLinesPerBlock % workers.tAvailable;
	ushort wl[17], sp[17], ep[17];	// assuming 17 cores at max
	ushort i,j;

	// get the basic information
	io_printf(IO_BUF, "myWID = %d, tAvailable = %d\n", workers.subBlockID, workers.tAvailable);
	io_printf(IO_BUF, "wImg = %d, hImg = %d\n", blkInfo->wImg, blkInfo->hImg);
	io_printf(IO_BUF, "nodeBlockID = %d, maxBlock = %d\n", blkInfo->nodeBlockID, blkInfo->maxBlock);
	io_printf(IO_BUF, "nLinesPerBlock = %d\n", nLinesPerBlock);
	io_printf(IO_BUF, "blkStart = %d\n", blkStart);


	// initialize starting point with respect to blkStart
	for(i=0; i<17; i++)
		sp[i] = blkStart;

	for(i=0; i<workers.tAvailable; i++) {
		wl[i] = nLinesPerCore;
		if(nRemInCore > 0) {
			wl[i]++;
			for(j=i+1; j<workers.tAvailable; j++) {
				sp[j]++;
			}
			nRemInCore--;
		}
		sp[i] += i*nLinesPerCore;
		ep[i] = sp[i]+wl[i]-1;
	}
	workers.startLine = sp[workers.subBlockID];
	workers.endLine = ep[workers.subBlockID];
	// let's print the resulting workload
	io_printf(IO_BUF, "sp = %d, ep = %d\n", workers.startLine, workers.endLine);
}

void imgFiltering(uint arg0, uint arg1)
{

}

void imgDetection(uint arg0, uint arg1)
{

}
