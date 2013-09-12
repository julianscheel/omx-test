CC ?= cc

INCLUDES = -I$(SYSROOT)/opt/vc/include/IL -I$(SYSROOT)/opt/vc/include -I$(SYSROOT)/opt/vc/include/interface/vcos/pthreads \
		-I$(SYSROOT)/usr/include/IL -I$(SYSROOT)/usr/include/interface/vcos/pthreads
CFLAGS = -g --sysroot=$(SYSROOT)
LIBS = -L/opt/vc/lib -lopenmaxil -lbcm_host

test-video_render: test-video_render.c
	$(CC) $(CFLAGS) $(INCLUDES) -o test-video_render test-video_render.c $(LIBS)

test-image_fx: test-image_fx.c
	$(CC) $(CFLAGS) $(INCLUDES) -o test-image_fx test-image_fx.c $(LIBS)

test-audio_render: test-audio_render.c
	$(CC) $(CFLAGS) $(INCLUDES) -o test-audio_render test-audio_render.c $(LIBS)

all: test-video_render test-audio_render test-image_fx

clean:
	rm test-video_render test-audio_render test-image_fx
