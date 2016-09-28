#ifndef CAP_H
#define CAP_H

#define CAP_OK 0
#define CAP_ERROR -1
#define CAP_ERROR_RET(s) { \
							printf("v4l2: %s\n", s); \
							return CAP_ERROR; \
						 }
#define CAP_CLIP(val, min, max) (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))

typedef struct {
	void * start;
	size_t length;
} v4l2_buffer_t;

static int width;
static int height;
static v4l2_buffer_t *buffers;

inline int yuv420p_to_bgr(void *in, int length, unsigned char *out)
{
	uint8_t *yptr, *uptr, *vptr;
	uint32_t x, y, p;

	if (length < (width * height * 3) / 2)
		return CAP_ERROR;

	yptr = (uint8_t *) in;
	uptr = yptr + (width * height);
	vptr = uptr + (width * height / 4);
	p = 0;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int r, g, b;
			int y, u, v;

			y = *(yptr++) << 8;
			u = uptr[p] - 128;
			v = vptr[p] - 128;

			r = (y + (359 * v)) >> 8;
			g = (y - (88 * u) - (183 * v)) >> 8;
			b = (y + (454 * u)) >> 8;

			*(out++) += CAP_CLIP(b, 0x00, 0xFF);
			*(out++) += CAP_CLIP(g, 0x00, 0xFF);
			*(out++) += CAP_CLIP(r, 0x00, 0xFF);

			if (x & 1) p++;
		}

		if (!(y & 1)) p -= width / 2;
	}

	return CAP_ERROR;
}

#endif // CAP_H