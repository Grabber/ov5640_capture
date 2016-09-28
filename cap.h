#ifndef CAP_H
#define CAP_H

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