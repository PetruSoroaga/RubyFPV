#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include "base.h"
#include "wiringPiI2C_radxa.h"

// Initialize I2C device (compatible with wiringPiI2CSetup)
int wiringPiI2CSetup(const int devId)
{
    int bus_id = 3;
    char filename[20];
    snprintf(filename, sizeof(filename), "/dev/i2c-%d", bus_id); // Bus number specified by user
    int fd = open(filename, O_RDWR);
    if (fd < 0)
    {
        log_error_and_alarm("Failed to open I2C device, bus number: %d", bus_id);
        return -1;
    }

    if (ioctl(fd, 0x0703, devId) < 0)
    {
        log_error_and_alarm("Can't set I2c slave address,bus number %d,address %d", bus_id, devId);
        close(fd);
        return -1;
    }
    log_line("Opened I2C device at address 0x%02X,fd %d", devId, fd);
    return fd;
}

int wiringPiI2CRead(int fd)
{
    return i2c_smbus_read_byte(fd);
}

int wiringPiI2CReadReg8(int fd, int reg)
{
    return i2c_smbus_read_byte_data(fd, reg);
}

int wiringPiI2CReadReg16(int fd, int reg)
{
    return i2c_smbus_read_word_data(fd, reg);
}

int wiringPiI2CWrite(int fd, int data)
{
    return i2c_smbus_write_byte(fd, data);
}

int wiringPiI2CWriteReg8(int fd, int reg, int data)
{
    return i2c_smbus_write_byte_data(fd, reg, data);
}

int wiringPiI2CWriteReg16(int fd, int reg, int data)
{
    return i2c_smbus_write_word_data(fd, reg, data);
}

int wiringPiI2CWriteBlockData(int fd, uint8_t reg, uint8_t length, uint8_t *values)
{
    //Beacuse of the hardware limitation,write 32 bytes every time
    uint8_t max_length = 32;
    int ret = -1;
    if (length <= max_length)
    {
        ret = i2c_smbus_write_i2c_block_data(fd, reg, length, values);
    }
    else
    {
        uint16_t offset = 0;
        uint16_t remaining = length;
        ret = 0;

        while (remaining > 0)
        {
            uint16_t chunk_len = (remaining > max_length) ? max_length : remaining;

            int result = i2c_smbus_write_i2c_block_data(fd, reg, chunk_len, values + offset);

            if (result < 0)
            {
                ret = result;
                break;
            }

            offset += chunk_len;
            remaining -= chunk_len;
        }
    }
    return ret;
}

int wiringPiI2CWriteBlockDataIoctl(int fd, int addr, uint8_t reg, uint8_t length, uint8_t *values)
{
    struct i2c_msg msg[2];
    uint8_t *buffer = malloc(length + 1);
    buffer[0] = reg;
    memcpy(buffer + 1, values, length);

    msg[0].addr = addr;
    msg[0].flags = 0;
    msg[0].len = length + 1;
    msg[0].buf = buffer;

    struct i2c_rdwr_ioctl_data ioctl_data;
    ioctl_data.msgs = msg;
    ioctl_data.nmsgs = 1;

    int ret = ioctl(fd, I2C_RDWR, &ioctl_data);
    free(buffer);
    return ret;
}