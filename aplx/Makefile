#DIRS = SpiNNEdge ImgDec
DIRS = SpiNNEdge

RM := \rm -rf
LS := \ls -l
CP := \cp

all: $(DIRS)
	@for d in $(DIRS); do (cd $$d; "$(MAKE)") || exit $$?; done
	@for d in $(DIRS); do (cp $$d/*.aplx binaries); done
	@$(LS) binaries/*

clean: $(DIRS)
	@for d in $(DIRS); do (cd $$d; "$(MAKE)" clean) || exit $$?; done
	@$(RM) binaries/*.aplx

