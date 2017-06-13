/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef _UAPI__GOOGLE_EASEL_COMM_H
#define _UAPI__GOOGLE_EASEL_COMM_H
#include <linux/compiler.h>
#include <linux/types.h>
#define EASELCOMM_MAX_MESSAGE_SIZE (12 * 1024)
#define EASELCOMM_SERVICE_COUNT 64
typedef __u64 easelcomm_msgid_t;
struct easelcomm_wait {
  __s32 timeout_ms;
};
struct easelcomm_kmsg_desc {
  easelcomm_msgid_t message_id;
  easelcomm_msgid_t in_reply_to;
  __u32 message_size;
  __u32 dma_buf_size;
  __u32 need_reply;
  __u32 replycode;
  struct easelcomm_wait wait;
};
enum easelcomm_dma_buffer_type {
  EASELCOMM_DMA_BUFFER_UNUSED = 0,
  EASELCOMM_DMA_BUFFER_USER,
  EASELCOMM_DMA_BUFFER_DMA_BUF
};
struct easelcomm_kbuf_desc {
  easelcomm_msgid_t message_id;
  void __user * buf;
  int dma_buf_fd;
  int buf_type;
  __u32 buf_size;
  struct easelcomm_wait wait;
};
#define EASELCOMM_IOC_MAGIC 0xEA
#define EASELCOMM_IOC_REGISTER _IOW(EASELCOMM_IOC_MAGIC, 0, int)
#define EASELCOMM_IOC_SENDMSG _IOWR(EASELCOMM_IOC_MAGIC, 1, struct easelcomm_kmsg_desc *)
#define EASELCOMM_IOC_READDATA _IOW(EASELCOMM_IOC_MAGIC, 2, struct easelcomm_kbuf_desc *)
#define EASELCOMM_IOC_WRITEDATA _IOW(EASELCOMM_IOC_MAGIC, 3, struct easelcomm_kbuf_desc *)
#define EASELCOMM_IOC_SENDDMA _IOW(EASELCOMM_IOC_MAGIC, 4, struct easelcomm_kbuf_desc *)
#define EASELCOMM_IOC_RECVDMA _IOW(EASELCOMM_IOC_MAGIC, 5, struct easelcomm_kbuf_desc *)
#define EASELCOMM_IOC_WAITREPLY _IOWR(EASELCOMM_IOC_MAGIC, 6, struct easelcomm_kmsg_desc *)
#define EASELCOMM_IOC_WAITMSG _IOWR(EASELCOMM_IOC_MAGIC, 7, struct easelcomm_kmsg_desc *)
#define EASELCOMM_IOC_SHUTDOWN _IO(EASELCOMM_IOC_MAGIC, 8)
#define EASELCOMM_IOC_FLUSH _IO(EASELCOMM_IOC_MAGIC, 9)
#endif
