#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/device.h>

#define BMP180_CHIP_ID_REG    0xD0
#define BMP180_CHIP_ID        0x55
#define BMP180_TEMP_REG       0xF6
#define BMP180_CTRL_MEAS_REG  0xF4
#define BMP180_TEMP_CMD       0x2E
#define BMP180_PRESS_CMD      0x34

#define BMP180_GET_TEMP_REAL   _IOR(0xB1, 2, int)
#define BMP180_GET_PRESS_REAL  _IOR(0xB1, 3, int)

static struct i2c_client *bmp180_client;

struct bmp180_calibration {
    s16 AC1, AC2, AC3;
    u16 AC4, AC5, AC6;
    s16 B1, B2, MB, MC, MD;
};

static struct bmp180_calibration bmp_cal;

static int bmp180_read_raw_temp(void)
{
    int ret;
    u8 buf[2];

    if (!bmp180_client) return -ENODEV;

    ret = i2c_smbus_write_byte_data(bmp180_client, BMP180_CTRL_MEAS_REG, BMP180_TEMP_CMD);
    if (ret < 0) return ret;

    msleep(5);

    ret = i2c_smbus_read_i2c_block_data(bmp180_client, BMP180_TEMP_REG, 2, buf);
    if (ret < 0) return ret;

    return (buf[0] << 8) | buf[1];
}

static int bmp180_read_raw_press(void)
{
    int ret;
    u8 buf[3];

    if (!bmp180_client) return -ENODEV;

    ret = i2c_smbus_write_byte_data(bmp180_client, BMP180_CTRL_MEAS_REG, BMP180_PRESS_CMD);
    if (ret < 0) return ret;

    msleep(8);

    ret = i2c_smbus_read_i2c_block_data(bmp180_client, BMP180_TEMP_REG, 3, buf);
    if (ret < 0) return ret;

    return ((buf[0] << 16) | (buf[1] << 8) | buf[2]) >> 8;
}

int bmp180_get_temperature_celsius(void)
{
    int ut, X1, X2, B5, temp;

    ut = bmp180_read_raw_temp();
    if (ut < 0) return ut;

    X1 = ((ut - bmp_cal.AC6) * bmp_cal.AC5) >> 15;
    X2 = (bmp_cal.MC << 11) / (X1 + bmp_cal.MD);
    B5 = X1 + X2;
    temp = (B5 + 8) >> 4;

    return temp;
}
EXPORT_SYMBOL(bmp180_get_temperature_celsius);

int bmp180_get_pressure_hpa(void)
{
    int ut, up, X1, X2, X3, B3, B5, B6, p;
    unsigned int B4, B7;

    ut = bmp180_read_raw_temp();
    up = bmp180_read_raw_press();
    if (ut < 0 || up < 0) return -EIO;

    X1 = ((ut - bmp_cal.AC6) * bmp_cal.AC5) >> 15;
    X2 = (bmp_cal.MC << 11) / (X1 + bmp_cal.MD);
    B5 = X1 + X2;

    B6 = B5 - 4000;
    X1 = (bmp_cal.B2 * ((B6 * B6) >> 12)) >> 11;
    X2 = (bmp_cal.AC2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = (((bmp_cal.AC1 * 4 + X3) + 2) / 4);
    X1 = (bmp_cal.AC3 * B6) >> 13;
    X2 = (bmp_cal.B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    B4 = (bmp_cal.AC4 * (unsigned int)(X3 + 32768)) >> 15;
    B7 = (unsigned int)(up - B3) * 50000;

    if (B7 < 0x80000000)
        p = (B7 << 1) / B4;
    else
        p = (B7 / B4) << 1;

    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    p = p + ((X1 + X2 + 3791) >> 4);

    return p;
}
EXPORT_SYMBOL(bmp180_get_pressure_hpa);

static long bmp180_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int data;

    switch (cmd) {
    case BMP180_GET_TEMP_REAL:
        data = bmp180_get_temperature_celsius();
        break;
    case BMP180_GET_PRESS_REAL:
        data = bmp180_get_pressure_hpa();
        break;
    default:
        return -EINVAL;
    }

    if (copy_to_user((int __user *)arg, &data, sizeof(int)))
        return -EFAULT;

    return 0;
}

static const struct file_operations bmp180_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = bmp180_ioctl,
    .compat_ioctl   = bmp180_ioctl,
};

static struct miscdevice bmp180_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "bmp180",
    .fops  = &bmp180_fops,
};

// ---------------- I2C Driver Bindings ----------------

static int bmp180_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    u8 calib[22];
    int chip_id, ret;
    struct device *dev = &client->dev;

    // Tự động unbind bmp280 nếu đang chiếm địa chỉ
    struct device *bmp280_dev = bus_find_device_by_name(&i2c_bus_type, NULL, "1-0077");
    if (bmp280_dev && bmp280_dev->driver &&
        strcmp(bmp280_dev->driver->name, "bmp280") == 0) {
        dev_warn(dev, "bmp280 detected at 0x77, trying to unbind it...\n");
        device_release_driver(bmp280_dev);
        put_device(bmp280_dev);
        msleep(100);  
    }

    bmp180_client = client;

    chip_id = i2c_smbus_read_byte_data(client, BMP180_CHIP_ID_REG);
    if (chip_id != BMP180_CHIP_ID) return -ENODEV;

    ret = i2c_smbus_read_i2c_block_data(client, 0xAA, 22, calib);
    if (ret < 0) return ret;

    bmp_cal.AC1 = (calib[0] << 8) | calib[1];
    bmp_cal.AC2 = (calib[2] << 8) | calib[3];
    bmp_cal.AC3 = (calib[4] << 8) | calib[5];
    bmp_cal.AC4 = (calib[6] << 8) | calib[7];
    bmp_cal.AC5 = (calib[8] << 8) | calib[9];
    bmp_cal.AC6 = (calib[10] << 8) | calib[11];
    bmp_cal.B1  = (calib[12] << 8) | calib[13];
    bmp_cal.B2  = (calib[14] << 8) | calib[15];
    bmp_cal.MB  = (calib[16] << 8) | calib[17];
    bmp_cal.MC  = (calib[18] << 8) | calib[19];
    bmp_cal.MD  = (calib[20] << 8) | calib[21];

    dev_info(dev, "BMP180 ready (chip id: 0x%x)\n", chip_id);

    return misc_register(&bmp180_misc_device);
}

static void bmp180_remove(struct i2c_client *client)
{
    misc_deregister(&bmp180_misc_device);
    dev_info(&client->dev, "BMP180 removed\n");
}

static const struct i2c_device_id bmp180_id[] = {
    { "bmp180", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, bmp180_id);

static const struct of_device_id bmp180_dt_ids[] = {
    { .compatible = "bosch,bmp180" },
    { }
};
MODULE_DEVICE_TABLE(of, bmp180_dt_ids);

static struct i2c_driver bmp180_driver = {
    .driver = {
        .name = "bmp180",
        .of_match_table = bmp180_dt_ids,
    },
    .probe  = bmp180_probe,
    .remove = bmp180_remove,
    .id_table = bmp180_id,
};

module_i2c_driver(bmp180_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hung+Kiet+Thuan");
MODULE_DESCRIPTION("BMP180 I2C Driver");

