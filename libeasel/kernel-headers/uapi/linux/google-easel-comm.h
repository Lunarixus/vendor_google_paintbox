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
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define EASELCOMM_MAX_MESSAGE_SIZE (12 * 1024)
enum easelcomm_service_id {
  EASELCOMM_SERVICE_SYSCTRL = 0,
  EASELCOMM_SERVICE_SHELL,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  EASELCOMM_SERVICE_TEST,
  EASELCOMM_SERVICE_HDRPLUS,
  EASELCOMM_SERVICE_COUNT,
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef uint64_t easelcomm_msgid_t;
struct easelcomm_kmsg_desc {
  easelcomm_msgid_t message_id;
  easelcomm_msgid_t in_reply_to;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t message_size;
  uint32_t dma_buf_size;
  uint32_t need_reply;
  uint32_t replycode;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum easelcomm_dma_buffer_type {
  EASELCOMM_DMA_BUFFER_UNUSED = 0,
  EASELCOMM_DMA_BUFFER_USER,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  EASELCOMM_DMA_BUFFER_DMA_BUF
};
struct easelcomm_kbuf_desc {
  easelcomm_msgid_t message_id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  void __user * buf;
  int dma_buf_fd;
  int buf_type;
  uint32_t buf_size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define EASELCOMM_IOC_MAGIC 0xEA
#define EASELCOMM_IOC_REGISTER _IOW(EASELCOMM_IOC_MAGIC, 0, int)
#define EASELCOMM_IOC_SENDMSG _IOWR(EASELCOMM_IOC_MAGIC, 1, struct easelcomm_kmsg_desc *)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define EASELCOMM_IOC_READDATA _IOW(EASELCOMM_IOC_MAGIC, 2, struct easelcomm_kbuf_desc *)
#define EASELCOMM_IOC_WRITEDATA _IOW(EASELCOMM_IOC_MAGIC, 3, struct easelcomm_kbuf_desc *)
#define EASELCOMM_IOC_SENDDMA _IOW(EASELCOMM_IOC_MAGIC, 4, struct easelcomm_kbuf_desc *)
#define EASELCOMM_IOC_RECVDMA _IOW(EASELCOMM_IOC_MAGIC, 5, struct easelcomm_kbuf_desc *)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define EASELCOMM_IOC_WAITREPLY _IOWR(EASELCOMM_IOC_MAGIC, 6, struct easelcomm_kmsg_desc *)
#define EASELCOMM_IOC_WAITMSG _IOR(EASELCOMM_IOC_MAGIC, 7, struct easelcomm_kmsg_desc *)
#define EASELCOMM_IOC_SHUTDOWN _IO(EASELCOMM_IOC_MAGIC, 8)
#define EASELCOMM_IOC_FLUSH _IO(EASELCOMM_IOC_MAGIC, 9)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif

