all: js8pitx

CFLAGS	= -Wall -g -O3 -Wno-unused-variable -I /opt/vc/include 
LDFLAGS	= -lm -lpthread
# Raspberry Pi legacy VideoCore userland is optional.
ifneq ("$(wildcard /opt/vc/lib)","")
  LDFLAGS += -L/opt/vc/lib -lbcm_host
endif
ifeq ("$(shell uname -s)","Linux")
  LDFLAGS += -lrt
endif
CCP = g++ 
CC = gcc 


js8pitx: js8tx.cpp js8_js8call_encoder.h js8_varicode.h js8_huffman.h js8_frames.h js8_phy_ft8.h ft8_ldpc_174_91.h librpitx/src/librpitx.h librpitx/src/librpitx.a
	$(CCP) $(CFLAGS) -std=c++17 -o js8pitx js8tx.cpp librpitx/src/librpitx.a $(LDFLAGS)

clean:
	rm -f js8tx
