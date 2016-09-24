// gcc cap.c -o cap $(pkg-config --libs --cflags opencv) -lm

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

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#define V4L2_OK 0
#define V4L2_ERROR(s) printf("v4l2_error: %s\n", s); return -1;
#define CLIP(val, min, max) (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))

typedef struct {
   void * start;
   size_t length;
} v4l2_buffer_t;

static int k;
static int width;
static int height;
static v4l2_buffer_t *buffers = NULL;

double get_wall_time()
{
   struct timeval time;
   if (gettimeofday(&time, NULL))
      return 0.;
   return (double) time.tv_sec + (double) time.tv_usec * .000001;
}

int yuv420p_to_bgr(void *in, int length, unsigned char *out)
{
   uint8_t *yptr, *uptr, *vptr;
   uint32_t x, y, p;
   
   if (length < (width * height * 3) / 2)
      return -1;
   
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

         *(out++) += CLIP(b, 0x00, 0xFF);
         *(out++) += CLIP(g, 0x00, 0xFF);
         *(out++) += CLIP(r, 0x00, 0xFF);
         
         if (x & 1) p++;
      }

      if (!(y & 1)) p -= width / 2;
   }

   return 0;
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

int v4l2_init_camera(int fd, int width, int height)
{
   uint32_t i;
   uint32_t index;
   struct v4l2_format fmt = {0};
   struct v4l2_input input = {0};
   struct v4l2_capability caps = {0};

   if (xioctl(fd, VIDIOC_QUERYCAP, &caps) == -1) {
      V4L2_ERROR("unable to query capabilities.");
   }

   if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
      V4L2_ERROR("doesn't support video capturing.");
   }

   printf(" Driver Caps:\n"
          " Driver: \"%s\"\n"
          " Card: \"%s\"\n"
          " Bus: \"%s\"\n"
          " Version: %d.%d\n"
          " Capabilities: %08x\n",
          caps.driver,
          caps.card,
          caps.bus_info,
          (caps.version>>16) && 0xff,
          (caps.version>>24) && 0xff,
          caps.capabilities);

   input.index = 0;
   if (xioctl(fd, VIDIOC_ENUMINPUT, &input) == -1) {
      V4L2_ERROR("unable to enumerate input.");
   }

   if(xioctl(fd, VIDIOC_S_INPUT, &input.index) == -1) {
      V4L2_ERROR("unable to set input.");
   }

   fmt.fmt.pix.width = width;
   fmt.fmt.pix.height = height;
   fmt.fmt.pix.field = V4L2_FIELD_ANY;
   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;

   if (xioctl(fd, VIDIOC_TRY_FMT, &fmt) == -1) {
      V4L2_ERROR("failed trying to set pixel format.");   
   }

   if (fmt.fmt.pix.width != width)
      width = fmt.fmt.pix.width;
   if (fmt.fmt.pix.height != height)
      height = fmt.fmt.pix.height;

   if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
      V4L2_ERROR("failed to set pixel format.");
   }

   return V4L2_OK;
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
      V4L2_ERROR("failed requesting buffers");
   }

   if (req.count < 2) {
      V4L2_ERROR("insufficient buffer memory.");
   }

   buffers = (v4l2_buffer_t*) calloc(req.count, sizeof(v4l2_buffer_t));

   if (!buffers) {
      V4L2_ERROR("failed to allocated buffers memory.");
   }

   for (i = 0; i < req.count; i++) {
      struct v4l2_buffer buf = {0};

      buf.index = i;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;    

      if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
         V4L2_ERROR("failed to query buffer.");
      }

      buffers[i].length = buf.length;
      buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

      if (buffers[i].start == MAP_FAILED) {
         V4L2_ERROR("failed to mmap buffer.");
      }
   }

   for (i = 0; i < req.count; i++) {
      struct v4l2_buffer buf = {0};
      
      buf.index = i;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      
      if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
         V4L2_ERROR("failed to queue buffer.");
      }
   }

   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
      V4L2_ERROR("failed to stream on.");
   }

   *buffers_count = req.count;
   
   return V4L2_OK;
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
      V4L2_ERROR("failed to select frame.");
   }

   buf.memory = V4L2_MEMORY_MMAP;
   buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
      V4L2_ERROR("failed to retrieve frame.");
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
   cvSaveImage("frame.jpg", &frame_bgr, 0);
   free(frame_yuv);

   if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
      V4L2_ERROR("failed to queue buffer.");
   }

   return V4L2_OK;
}

int v4l2_close_camera(int fd, int buffers_count) {
   int i;
   enum v4l2_buf_type type;

   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
      V4L2_ERROR("failed to stream off.");
   }  

   for (i = 0; i < buffers_count; i++)
      munmap(buffers[i].start, buffers[i].length);

   close(fd);
}

int main(int argc, char *argv[])
{
   for (;;) {
      int i;
      int fd;
      double after;
      double before;
      int buffers_count;

	if (argc != 4) {
		V4L2_ERROR("./cap <frames> <width> <height>");
		}
      
      k = (int) atoi(argv[1]);
      width = (int) atoi(argv[2]);
      height = (int) atoi(argv[3]);

      fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
      if (fd == -1) {
         V4L2_ERROR("failed to open the camera.");
      }
      
      if (v4l2_init_camera(fd, width, height) == -1) {
         V4L2_ERROR("failed to init camera.");
      }

      if (v4l2_set_mmap(fd, &buffers_count) == -1) {
         V4L2_ERROR("failed to mmap.");
      }

      //cvNamedWindow("frame", CV_WINDOW_AUTOSIZE);

      for (i = 0; i < k; i++) {
         before = get_wall_time();
         if (v4l2_retrieve_frame(fd, buffers_count) == -1) {
            V4L2_ERROR("failed to retrieve frame.");
         }
         after = get_wall_time();
         printf("\nFPS: %f\n", 1./(after - before));
      }
      
      v4l2_close_camera(fd, buffers_count);
   }

   return V4L2_OK;
}