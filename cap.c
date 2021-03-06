// gcc cap.c -o cap $(pkg-config --libs --cflags opencv) -lm -O3

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include "opencv2/highgui/highgui_c.h"
#include "opencv2/imgproc/imgproc_c.h"

#include "cap.h"

#define CAP_DEVICE "/dev/video0"

double get_wall_time()
{
	struct timeval time;
	if (gettimeofday(&time, NULL))
		return 0.;
	return (double) time.tv_sec + (double) time.tv_usec * .000001;
}

static int xioctl(int fd, int request, void *arg)
{
	uint32_t r;
	uint32_t tries = 3;

	do {
		r = ioctl(fd, request, arg);
	} while (--tries > 0 && -1 == r && EINTR == errno);

	return r;
}

int v4l2_init_camera(int fd)
{
	uint32_t i;
	uint32_t index;
	struct v4l2_format fmt = {0};
	struct v4l2_input input = {0};
	struct v4l2_capability caps = {0};

	if (xioctl(fd, VIDIOC_QUERYCAP, &caps) == -1) {
		CAP_ERROR_RET("unable to query capabilities.");
	}

	if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		CAP_ERROR_RET("doesn't support video capturing.");
	}

	printf("Driver: \"%s\"\n", caps.driver);
	printf("Card: \"%s\"\n", caps.card);
	printf("Bus: \"%s\"\n", caps.bus_info);
	printf("Version: %d.%d\n", (caps.version>>16) && 0xff, (caps.version>>24) && 0xff);
	printf("Capabilities: %08x\n", caps.capabilities);

	input.index = 0;

	if (xioctl(fd, VIDIOC_ENUMINPUT, &input) == -1) {
		CAP_ERROR_RET("unable to enumerate input.");
	}

	if (xioctl(fd, VIDIOC_S_INPUT, &input.index) == -1) {
		CAP_ERROR_RET("unable to set input.");
	}

	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	//fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;

	if (xioctl(fd, VIDIOC_TRY_FMT, &fmt) == -1) {
		CAP_ERROR_RET("failed trying to set pixel format.");
	}

	if (fmt.fmt.pix.width != width) 
		width = fmt.fmt.pix.width;
	if (fmt.fmt.pix.height != height)
		height = fmt.fmt.pix.height;

	if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
		CAP_ERROR_RET("failed to set pixel format.");
	}

	printf("Frame Width: %d\n", width);
	printf("Frame Height: %d\n", height);

	switch (fmt.fmt.pix.pixelformat) {
		case V4L2_PIX_FMT_RGB24: {
			printf("Pixel Format: V4L2_PIX_FMT_RGB24\n");
			break;
		}
		case V4L2_PIX_FMT_YUV420: {
			printf("Pixel Format: V4L2_PIX_FMT_YUV420\n");
			break;
		}		
	}

	return CAP_OK;
}

int v4l2_set_mmap(int fd, int *buffers_count)
{
	uint32_t i;
	uint32_t j;
	enum v4l2_buf_type type;
	struct v4l2_requestbuffers req = {0};

	req.count = 4;
	req.memory = V4L2_MEMORY_MMAP;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
		CAP_ERROR_RET("failed requesting buffers.");
	}

	if (req.count < 2) {
		CAP_ERROR_RET("insufficient buffer memory.");
	}

	buffers = (v4l2_buffer_t*) calloc(req.count, sizeof(v4l2_buffer_t));
	if (!buffers) {
		CAP_ERROR_RET("failed to allocated buffers memory.");
	}

	for (i = 0; i < req.count; i++) {
		struct v4l2_buffer buf = {0};

		buf.index = i;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
			CAP_ERROR_RET("failed to query buffer.");
		}

		buffers[i].length = buf.length;
		buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

		if (buffers[i].start == MAP_FAILED) {
			CAP_ERROR_RET("failed to mmap buffer.");
		}
	}

	for (i = 0; i < req.count; i++) {
		struct v4l2_buffer buf = {0};

		buf.index = i;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
			CAP_ERROR_RET("failed to queue buffer.");
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		CAP_ERROR_RET("failed to stream on.");
	}

	*buffers_count = req.count;

	return CAP_OK;
}

int v4l2_retrieve_frame(int fd, int buffers_count)
{
	uint32_t i;
	fd_set fds;
	IplImage *frame_ipl;
	struct timeval tv = {0};
	struct v4l2_buffer buf = {0};

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	if (select(fd+1, &fds, NULL, NULL, &tv) == -1) {
		CAP_ERROR_RET("failed to select frame.");
	}

	buf.memory = V4L2_MEMORY_MMAP;
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
		CAP_ERROR_RET("failed to retrieve frame.");
	}

	printf("Length: %d\n", buf.length);
	printf("Bytesused: %d\n", buf.bytesused);
	printf("Address: %p\n", &buffers[buf.index]);

   // FILE * f = fopen("frame.yuv420p", "wb");
   // if (frame != NULL)  {
   //    fwrite(buffers[buf.index].start, buf.length, 1, f);
   //    fclose(f);
   // }

	unsigned char *frame_yuv = calloc(width * height * 3, sizeof(unsigned char));
	yuv420p_to_bgr(buffers[buf.index].start, buf.length, frame_yuv);
	CvMat frame_bgr = cvMat(height, width, CV_8UC3, (void *) frame_yuv);
	// cvSaveImage("frame.jpg", &frame_bgr, 0);
	free(frame_yuv);

	if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
		CAP_ERROR_RET("failed to queue buffer.");
	}

	return CAP_OK;
}

int v4l2_close_camera(int fd, int buffers_count) {
	int i;
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
		CAP_ERROR_RET("failed to stream off.");
	}

	for (i = 0; i < buffers_count; i++) {
		munmap(buffers[i].start, buffers[i].length);
	}

	close(fd);
}

int main(int argc, char *argv[])
{
	int i;
	int fd;
	double after;
	double before;
	int buffers_count;

	if (argc != 3) {
		CAP_ERROR_RET("./cap <width> <height>")
	}

	width = (int) atoi(argv[1]);
	height = (int) atoi(argv[2]);

	fd = open(CAP_DEVICE, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		CAP_ERROR_RET("failed to open the camera.");
	}

	if (v4l2_init_camera(fd) == -1) {
		CAP_ERROR_RET("failed to init camera.");
	}

	if (v4l2_set_mmap(fd, &buffers_count) == -1) {
		CAP_ERROR_RET("failed to mmap.");
	}

	// cvNamedWindow("frame", CV_WINDOW_AUTOSIZE);

	for (i = 0; i < 100; i++) {
		before = get_wall_time();
		if (v4l2_retrieve_frame(fd, buffers_count) == -1) {
			CAP_ERROR_RET("failed to retrieve frame.");
		}
		after = get_wall_time();
		printf("\nFPS: %f\n", 1./(after - before));
	}

	v4l2_close_camera(fd, buffers_count);

	return CAP_OK;
}