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
#ifndef _UAPI__MNH_SM_H
#define _UAPI__MNH_SM_H
#define MIPI_TX0 0
#define MIPI_TX1 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MIPI_TX_IPU 2
#define MIPI_RX0 0
#define MIPI_RX1 1
#define MIPI_RX2 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MIPI_RX_IPU 3
#define MIPI_MODE_BYPASS 0
#define MIPI_MODE_BYPASS_W_IPU 1
#define MIPI_MODE_FUNCTIONAL 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MNH_MIPI_VC0_EN_MASK 0x1
#define MNH_MIPI_VC1_EN_MASK 0x2
#define MNH_MIPI_VC2_EN_MASK 0x4
#define MNH_MIPI_VC3_EN_MASK 0x8
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MNH_MIPI_VC_ALL_EN_MASK 0xf
#define MNH_SM_IOC_MAGIC 'T'
#define MNH_SM_MAX 8
#define MNH_SM_IOC_GET_STATE _IOR(MNH_SM_IOC_MAGIC, 1, int *)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MNH_SM_IOC_SET_STATE _IOW(MNH_SM_IOC_MAGIC, 2, int)
#define MNH_SM_IOC_WAIT_FOR_STATE _IOW(MNH_SM_IOC_MAGIC, 3, int)
#define MNH_SM_IOC_CONFIG_MIPI _IOW(MNH_SM_IOC_MAGIC, 4, struct mnh_mipi_config *)
#define MNH_SM_IOC_STOP_MIPI _IOW(MNH_SM_IOC_MAGIC, 5, struct mnh_mipi_config *)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum mnh_sm_state {
  MNH_STATE_OFF,
  MNH_STATE_PENDING,
  MNH_STATE_ACTIVE,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  MNH_STATE_SUSPEND,
  MNH_STATE_MAX,
};
struct mnh_mipi_config {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int txdev;
  int rxdev;
  int rx_rate;
  int tx_rate;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int mode;
  int vc_en_mask;
};
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */

