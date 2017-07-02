#include <fcntl.h>
#include <getopt.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SPIDEV_FILE "/dev/spidev10.0"

int fd;

#define MNH_SPI_CMD_WR 0x02
#define MNH_SPI_CMD_RD 0x03

int spi_read(uint32_t addr, uint32_t *data)
{
    struct spi_ioc_transfer xfer;
    char buf[12], buf2[12];
    int ret;

    memset(&xfer, 0, sizeof(xfer));
    memset(buf, 0, sizeof(buf));
    memset(buf2, 0, sizeof(buf2));

    // Add the command and reg address to the buf
    buf[3] = MNH_SPI_CMD_RD;
    buf[4] = (addr >> 24) & 0xFF;
    buf[5] = (addr >> 16) & 0xFF;
    buf[6] = (addr >> 8) & 0xFF;
    buf[7] = addr & 0xFF;

    xfer.tx_buf = (__u64)buf;
    xfer.len = 12;

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), xfer);
    if (ret < 0) {
        printf("ERROR: failed to send spidev message (%d)\n", -errno);
        return -errno;
    }

    // Give the PBL some time to fetch the data
    usleep(1000);

    // Send it dummy data for 96 bits
    memset(buf, 0, sizeof(buf));

    // Set the receive buffer
    xfer.rx_buf = (__u64)buf2;

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), xfer);
    if (ret < 0) {
        printf("ERROR: failed to send spidev message (%d)\n", -errno);
        return -errno;
    }

    *data = (buf2[8] << 24) | (buf2[9] << 16) | (buf2[10] << 8) | buf2[11];

    return 0;
}

int spi_write(uint32_t addr, uint32_t data)
{
    struct spi_ioc_transfer xfer;
    char buf[12], buf2[12];
    int ret;

    memset(&xfer, 0, sizeof(xfer));
    memset(buf, 0, sizeof(buf));
    memset(buf2, 0, sizeof(buf2));

    // Add the command, reg address, and data to the buf
    buf[3]  = MNH_SPI_CMD_WR;
    buf[4]  = (addr >> 24) & 0xFF;
    buf[5]  = (addr >> 16) & 0xFF;
    buf[6]  = (addr >> 8) & 0xFF;
    buf[7]  = addr & 0xFF;
    buf[8]  = (data >> 24) & 0xFF;
    buf[9]  = (data >> 16) & 0xFF;
    buf[10] = (data >> 8) & 0xFF;
    buf[11] = data & 0xFF;

    xfer.tx_buf = (__u64)buf;
    xfer.len = 12;

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), xfer);
    if (ret < 0) {
        printf("ERROR: failed to send spidev message (%d)\n", -errno);
        return -errno;
    }

    // Give the PBL some time to fetch the data
    usleep(1000);

    // Send it dummy data for 96 bits
    memset(buf, 0, sizeof(buf));

    // Set the receive buffer
    xfer.rx_buf = (__u64)buf2;

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), xfer);
    if (ret < 0) {
        printf("ERROR: failed to send spidev message (%d)\n", -errno);
        return -errno;
    }

    uint32_t echo_data = (buf2[8] << 24) | (buf2[9] << 16) | (buf2[10] << 8) | buf2[11];
    if (data != echo_data) {
        printf("ERROR: failed write, 0x%08x, expected 0x%08x\n", echo_data, data);
        return -EIO;
    }

    return 0;
}

int spi_init()
{
    __u8 mode = SPI_MODE_0;
    __u32 speed = 10000000;
    __u8 bits = 8;
    int ret;

    fd = open(SPIDEV_FILE, O_RDWR);
    if (fd < 0) {
        printf("ERROR: failed to open spidev file (%d)\n", -errno);
        return -errno;
    }

    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret < 0) {
        printf("ERROR: failed to write spidev mode (%d)\n", -errno);
        close(fd);
        return -errno;
    }

    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret < 0) {
        printf("ERROR: failed to write spidev max speed (%d)\n", -errno);
        close(fd);
        return -errno;
    }

    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret < 0) {
        printf("ERROR: failed to write spidev bits per word (%d)\n", -errno);
        close(fd);
        return -errno;
    }

    return 0;
}

void spi_deinit()
{
    if (fd >= 0) {
        close(fd);
    }
}

int main(int argc, char **argv) {
    int ret;
    uint32_t addr, data;

    // usage ezlspi {addr} [write data]
    if ((argc < 2) || (argc > 3)) {
        printf("ERROR: usage:\n");
        printf("           For reads,  ezlspi {addr}\n");
        printf("           For writes, ezlspi {addr} [data]\n");
    }

    addr = (uint32_t)strtol(argv[1], NULL, 0);
    if (argc == 3) {
        data = (uint32_t)strtol(argv[2], NULL, 0);
    }

    ret = spi_init();
    if (ret < 0) {
        printf("ERROR: failed to initialize spi device (%d)\n", ret);
        return ret;
    }

    if (argc == 2) {
        ret = spi_read(addr, &data);
        if (ret < 0) {
            printf("ERROR: failed read address 0x%08x (%d)\n", addr, ret);
        }
        printf("ADDR[0x%08x] -> 0x%08x\n", addr, data);
    } else {
        ret = spi_write(addr, data);
        if (ret < 0) {
            printf("ERROR: failed write address 0x%08x (%d)\n", addr, ret);
        }
        printf("ADDR[0x%08x] <- 0x%08x\n", addr, data);
    }

    spi_deinit();

    return 0;
}

