#include <stdio.h>

typedef struct {
	/* Source Options */
	char *source;
	uint8_t type;
	
	void *state;
	
	/* Last captured image */
	uint32_t length;
	void *img;
	
	/* Input Options */
	char    *input;
	uint8_t  tuner;
	uint32_t frequency;
	uint32_t delay;
	uint32_t timeout;
	char     use_read;

    /* Hack (@lex) */
    int exposure;
    int hflip;
    int vflip;
	
	/* List Options */
	uint8_t list;
	
	/* Image Options */
	int palette;
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	
	src_option_t **option;
	
	/* For calculating capture FPS */
	uint32_t captured_frames;
	struct timeval tv_first;
	struct timeval tv_last;
	
} src_t;

int main() {
	src_t src;
	src.source = "/dev/video0";

	src_v4l2_open(&src);
	
	return 0;
} 