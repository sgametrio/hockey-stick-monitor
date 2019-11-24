#include "LSM9DS1.h"

LSM9DS1::LSM9DS1(PinName sda, PinName scl, uint8_t xgAddr, uint8_t mAddr) : i2c(sda, scl)
{
    // xgAddress and mAddress will store the 7-bit I2C address, if using I2C.
    xgAddress = xgAddr;
    mAddress = mAddr;
}

bool LSM9DS1::begin(gyro_scale gScl, accel_scale aScl, mag_scale mScl,
                        gyro_odr gODR, accel_odr aODR, mag_odr mODR)
{
    // Store the given scales in class variables. These scale variables
    // are used throughout to calculate the actual g's, DPS,and Gs's.
    gScale = gScl;
    aScale = aScl;
    mScale = mScl;

    // Once we have the scale values, we can calculate the resolution
    // of each sensor. That's what these functions are for. One for each sensor
    calcgRes(); // Calculate DPS / ADC tick, stored in gRes variable
    calcmRes(); // Calculate Gs / ADC tick, stored in mRes variable
    calcaRes(); // Calculate g / ADC tick, stored in aRes variable


    // To verify communication, we can read from the WHO_AM_I register of
    // each device. Store those in a variable so we can return them.
    // The start of the addresses we want to read from
    char cmd[2] = {
        WHO_AM_I_XG,
        0
    };

    // Write the address we are going to read from and don't end the transaction
    i2c.write(xgAddress, cmd, 1, true);
    // Read in all the 8 bits of data
    i2c.read(xgAddress, cmd+1, 1);
    uint8_t xgTest = cmd[1];                    // Read the accel/gyro WHO_AM_I

    // Reset to the address of the mag who am i
    cmd[0] = WHO_AM_I_M;
    // Write the address we are going to read from and don't end the transaction
    i2c.write(mAddress, cmd, 1, true);
    // Read in all the 8 bits of data
    i2c.read(mAddress, cmd+1, 1);
    uint8_t mTest = cmd[1];      // Read the mag WHO_AM_I

    for(int ii = 0; ii < 3; ii++)
    {
        gBiasRaw[ii] = 0;
        aBiasRaw[ii] = 0;
        gBias[ii] = 0;
        aBias[ii] = 0;
    }
    autoCalib = false;

    // Accelerometer initialization stuff:
    initAccel(); // "Turn on" all axes of the accel. Set up interrupts, etc.
    setAccelODR(aODR); // Set the accel data rate.
    setAccelScale(aScale); // Set the accel range.

    // Gyro initialization stuff:
    initGyro(); // This will "turn on" the gyro. Setting up interrupts, etc.
    setGyroODR(gODR); // Set the gyro output data rate and bandwidth.
    setGyroScale(gScale); // Set the gyro range

    // Magnetometer initialization stuff:
    initMag(); // "Turn on" all axes of the mag. Set up interrupts, etc.
    setMagODR(mODR); // Set the magnetometer output data rate.
    setMagScale(mScale); // Set the magnetometer's range.

    // Interrupt initialization stuff
    initIntr();

    // Once everything is initialized, return the WHO_AM_I registers we read:
    return ( ((xgTest << 8) | mTest) == (WHO_AM_I_AG_RSP << 8 | WHO_AM_I_M_RSP) );
}

void LSM9DS1::initGyro()
{
    /*
    char cmd[4] = {
        CTRL_REG1_G,
        gScale | G_ODR_119_BW_14,
        0,          // Default data out and int out
        0           // Default power mode and high pass settings
    };
    */

    char cmd[4] = {
        CTRL_REG1_G,
        gScale | G_ODR_119_BW_14,
        0x03,          // Data pass througn HPF and LPF2, default int out
        0x80           // Low-power mode, disable high-pass filter, default cut-off frequency
    };

    // Write the data to the gyro control registers
    i2c.write(xgAddress, cmd, 4);
}

void LSM9DS1::initAccel()
{
    char cmd[4] = {
        CTRL_REG5_XL,
        0x38,       // Enable all axis and don't decimate data in out Registers
        (A_ODR_119 << 5) | (aScale << 3) | (A_BW_AUTO_SCALE),   // 119 Hz ODR, set scale, and auto BW
        0           // Default resolution mode and filtering settings
    };

    // Write the data to the accel control registers
    i2c.write(xgAddress, cmd, 4);
}

void LSM9DS1::initMag()
{
    char cmd[4] = {
        CTRL_REG1_M,
        0x10,       // Default data rate, xy axes mode, and temp comp
        mScale << 5, // Set mag scale
        0           // Enable I2C, write only SPI, not LP mode, Continuous conversion mode
    };

    // Write the data to the mag control registers
    i2c.write(mAddress, cmd, 4);
}

void LSM9DS1::initIntr()
{
    char cmd[2];
    uint16_t thresholdG = 500;
    uint8_t durationG = 10;
    uint8_t thresholdX = 20;
    uint8_t durationX = 1;
    uint16_t thresholdM = 10000;

    // 1. Configure the gyro interrupt generator
    cmd[0] = INT_GEN_CFG_G;
    cmd[1] = (1 << 5);
    i2c.write(xgAddress, cmd, 2);
    // 2. Configure the gyro threshold
    cmd[0] = INT_GEN_THS_ZH_G;
    cmd[1] = (thresholdG & 0x7F00) >> 8;
    i2c.write(xgAddress, cmd, 2);
    cmd[0] = INT_GEN_THS_ZL_G;
    cmd[1] = (thresholdG & 0x00FF);
    i2c.write(xgAddress, cmd, 2);
    cmd[0] = INT_GEN_DUR_G;
    cmd[1] = (durationG & 0x7F) | 0x80;
    i2c.write(xgAddress, cmd, 2);

    // 3. Configure accelerometer interrupt generator
    cmd[0] = INT_GEN_CFG_XL;
    cmd[1] = (1 << 1);
    i2c.write(xgAddress, cmd, 2);
    // 4. Configure accelerometer threshold
    cmd[0] = INT_GEN_THS_X_XL;
    cmd[1] = thresholdX;
    i2c.write(xgAddress, cmd, 2);
    cmd[0] = INT_GEN_DUR_XL;
    cmd[1] = (durationX & 0x7F);
    i2c.write(xgAddress, cmd, 2);

    // 5. Configure INT1 - assign it to gyro interrupt
    cmd[0] = INT1_CTRL;
//    cmd[1] = 0xC0;
    cmd[1] = (1 << 7) | (1 << 6);
    i2c.write(xgAddress, cmd, 2);
    cmd[0] = CTRL_REG8;
//    cmd[1] = 0x04;
    cmd[1] = (1 << 2) | (1 << 5) | (1 << 4);
    i2c.write(xgAddress, cmd, 2);

    // Configure interrupt 2 to fire whenever new accelerometer
    // or gyroscope data is available.
    cmd[0] = INT2_CTRL;
    cmd[1] = (1 << 0) | (1 << 1);
    i2c.write(xgAddress, cmd, 2);
    cmd[0] = CTRL_REG8;
    cmd[1] = (1 << 2) | (1 << 5) | (1 << 4);
    i2c.write(xgAddress, cmd, 2);

    // Configure magnetometer interrupt
    cmd[0] = INT_CFG_M;
    cmd[1] = (1 << 7) | (1 << 0);
    i2c.write(xgAddress, cmd, 2);

    // Configure magnetometer threshold
    cmd[0] = INT_THS_H_M;
    cmd[1] = uint8_t((thresholdM & 0x7F00) >> 8);
    i2c.write(xgAddress, cmd, 2);
    cmd[0] = INT_THS_L_M;
    cmd[1] = uint8_t(thresholdM & 0x00FF);
    i2c.write(xgAddress, cmd, 2);
}

void LSM9DS1::calibration()
{

    uint16_t samples = 0;
    int32_t aBiasRawTemp[3] = {0, 0, 0};
    int32_t gBiasRawTemp[3] = {0, 0, 0};
    int32_t gDeltaBiasRaw[3] = {0, 0, 0};

    // Turn off the autoCalib first to get the raw data
    autoCalib = false;

    while(samples < 200)
    {
        readGyro();
        gBiasRawTemp[0] += gx_raw;
        gBiasRawTemp[1] += gy_raw;
        gBiasRawTemp[2] += gz_raw;
        /*
        readAccel();
        aBiasRawTemp[0] += ax_raw;
        aBiasRawTemp[1] += ay_raw;
        aBiasRawTemp[2] += az_raw;
        */
        wait_ms(1); // 1 ms
        samples++;
    }

    //
    for(int ii = 0; ii < 3; ii++)
    {
        gBiasRaw[ii] = gBiasRawTemp[ii] / samples;
        // aBiasRaw[ii] = aBiasRawTemp[ii] / samples;
        aBiasRaw[ii] = 0;

        /*
        gBias[ii] = gBiasRaw[ii] * gRes;
        aBias[ii] = aBiasRaw[ii] * aRes;
        */
        gBiasRawTemp[ii] = 0;
    }

    // Re-calculate the bias
    int bound_bias = 10;
    int32_t samples_axis[3] = {0, 0, 0};
    for (size_t i = 0; i < 500; ++i){
        readGyro();
        gDeltaBiasRaw[0] = gx_raw - gBiasRaw[0];
        gDeltaBiasRaw[1] = gy_raw - gBiasRaw[1];
        gDeltaBiasRaw[2] = gz_raw - gBiasRaw[2];
        for (size_t k = 0; k < 3; ++k){
            if (gDeltaBiasRaw[k] >= -bound_bias && gDeltaBiasRaw[k] <= bound_bias){
                gBiasRawTemp[k] += gDeltaBiasRaw[k] + gBiasRaw[k];
                samples_axis[k]++;
            }
        }
        //
        wait_ms(1); // 1 ms
    }

    //
    for(int ii = 0; ii < 3; ii++)
    {
        gBiasRaw[ii] = gBiasRawTemp[ii] / samples_axis[ii];
        // aBiasRaw[ii] = aBiasRawTemp[ii] / samples;
        aBiasRaw[ii] = 0;

        gBias[ii] = gBiasRaw[ii] * gRes;
        aBias[ii] = aBiasRaw[ii] * aRes;
    }

    // Turn on the autoCalib
    autoCalib = true;
}

void LSM9DS1::readAccel()
{
    // The data we are going to read from the accel
    char data[6];

    // The start of the addresses we want to read from
    char subAddress = OUT_X_L_XL;

    // Write the address we are going to read from and don't end the transaction
    i2c.write(xgAddress, &subAddress, 1, true);
    // Read in all 8 bit registers containing the axes data
    i2c.read(xgAddress, data, 6);

    // Reassemble the data and convert to g
    ax_raw = data[0] | (data[1] << 8);
    ay_raw = data[2] | (data[3] << 8);
    az_raw = data[4] | (data[5] << 8);

    //
    if(autoCalib)
    {
        ax_raw -= aBiasRaw[0];
        ay_raw -= aBiasRaw[1];
        az_raw -= aBiasRaw[2];
    }
    //
    ax = ax_raw * aRes;
    ay = ay_raw * aRes;
    az = az_raw * aRes;
}

void LSM9DS1::readMag()
{
    // The data we are going to read from the mag
    char data[6];

    // The start of the addresses we want to read from
    char subAddress = OUT_X_L_M;

    // Write the address we are going to read from and don't end the transaction
    i2c.write(mAddress, &subAddress, 1, true);
    // Read in all 8 bit registers containing the axes data
    i2c.read(mAddress, data, 6);

    // Reassemble the data and convert to degrees
    mx_raw = data[0] | (data[1] << 8);
    my_raw = data[2] | (data[3] << 8);
    mz_raw = data[4] | (data[5] << 8);
    mx = mx_raw * mRes;
    my = my_raw * mRes;
    mz = mz_raw * mRes;
}

void LSM9DS1::readIntr()
{
    char data[1];
    char subAddress = INT_GEN_SRC_G;

    i2c.write(xgAddress, &subAddress, 1, true);
    i2c.read(xgAddress, data, 1);

    intr = (float)data[0];
}

void LSM9DS1::readTemp()
{
    // The data we are going to read from the temp
    char data[2];

    // The start of the addresses we want to read from
    char subAddress = OUT_TEMP_L;

    // Write the address we are going to read from and don't end the transaction
    i2c.write(xgAddress, &subAddress, 1, true);
    // Read in all 8 bit registers containing the axes data
    i2c.read(xgAddress, data, 2);

    // Temperature is a 12-bit signed integer
    temperature_raw = data[0] | (data[1] << 8);

    temperature_c = (float)temperature_raw / 8.0 + 25;
    temperature_f = temperature_c * 1.8 + 32;
}

void LSM9DS1::readGyro()
{
    // The data we are going to read from the gyro
    char data[6];

    // The start of the addresses we want to read from
    char subAddress = OUT_X_L_G;

    // Write the address we are going to read from and don't end the transaction
    i2c.write(xgAddress, &subAddress, 1, true);
    // i2c.write(xgAddress, &subAddress, 1);
    // Read in all 8 bit registers containing the axes data
    i2c.read(xgAddress, data, 6);

    // Reassemble the data and convert to degrees/sec
    gx_raw = data[0] | (data[1] << 8);
    gy_raw = data[2] | (data[3] << 8);
    gz_raw = data[4] | (data[5] << 8);

    //
    if(autoCalib)
    {
        gx_raw -= gBiasRaw[0];
        gy_raw -= gBiasRaw[1];
        gz_raw -= gBiasRaw[2];
    }
    //
    gx = gx_raw * gRes;
    gy = gy_raw * gRes;
    gz = gz_raw * gRes;

}

void LSM9DS1::setGyroScale(gyro_scale gScl)
{
    // The start of the addresses we want to read from
    char cmd[2] = {
        CTRL_REG1_G,
        0
    };

    // Write the address we are going to read from and don't end the transaction
    i2c.write(xgAddress, cmd, 1, true);
    // Read in all the 8 bits of data
    i2c.read(xgAddress, cmd+1, 1);

    // Then mask out the gyro scale bits:
    cmd[1] &= 0xFF^(0x3 << 3);
    // Then shift in our new scale bits:
    cmd[1] |= gScl << 3;

    // Write the gyroscale out to the gyro
    i2c.write(xgAddress, cmd, 2);

    // We've updated the sensor, but we also need to update our class variables
    // First update gScale:
    gScale = gScl;
    // Then calculate a new gRes, which relies on gScale being set correctly:
    calcgRes();
}

void LSM9DS1::setAccelScale(accel_scale aScl)
{
    // The start of the addresses we want to read from
    char cmd[2] = {
        CTRL_REG6_XL,
        0
    };

    // Write the address we are going to read from and don't end the transaction
    i2c.write(xgAddress, cmd, 1, true);
    // Read in all the 8 bits of data
    i2c.read(xgAddress, cmd+1, 1);

    // Then mask out the accel scale bits:
    cmd[1] &= 0xFF^(0x3 << 3);
    // Then shift in our new scale bits:
    cmd[1] |= aScl << 3;

    // Write the accelscale out to the accel
    i2c.write(xgAddress, cmd, 2);

    // We've updated the sensor, but we also need to update our class variables
    // First update aScale:
    aScale = aScl;
    // Then calculate a new aRes, which relies on aScale being set correctly:
    calcaRes();
}

void LSM9DS1::setMagScale(mag_scale mScl)
{
    // The start of the addresses we want to read from
    char cmd[2] = {
        CTRL_REG2_M,
        0
    };

    // Write the address we are going to read from and don't end the transaction
    i2c.write(mAddress, cmd, 1, true);
    // Read in all the 8 bits of data
    i2c.read(mAddress, cmd+1, 1);

    // Then mask out the mag scale bits:
    cmd[1] &= 0xFF^(0x3 << 5);
    // Then shift in our new scale bits:
    cmd[1] |= mScl << 5;

    // Write the magscale out to the mag
    i2c.write(mAddress, cmd, 2);

    // We've updated the sensor, but we also need to update our class variables
    // First update mScale:
    mScale = mScl;
    // Then calculate a new mRes, which relies on mScale being set correctly:
    calcmRes();
}

void LSM9DS1::setGyroODR(gyro_odr gRate)
{
    char cmd[2];
    char cmdLow[2];

    // Enable the low-power mode
    if(gRate == G_ODR_15_BW_0 | gRate == G_ODR_60_BW_16 | gRate == G_ODR_119_BW_14 | gRate == G_ODR_119_BW_31) {
        cmdLow[0] = CTRL_REG3_G;
        cmdLow[1] = 1<<7; // LP_mode

        i2c.write(xgAddress, cmdLow, 2);
    }

    // The start of the addresses we want to read from
    cmd[0] = CTRL_REG1_G;
    cmd[1] = 0;

    // Write the address we are going to read from and don't end the transaction
    i2c.write(xgAddress, cmd, 1, true);
    // Read in all the 8 bits of data
    i2c.read(xgAddress, cmd+1, 1);

    // Then mask out the gyro odr bits:
    cmd[1] &= (0x3 << 3);
    // Then shift in our new odr bits:
    cmd[1] |= gRate;

    // Write the gyroodr out to the gyro
    i2c.write(xgAddress, cmd, 2);
}

void LSM9DS1::setAccelODR(accel_odr aRate)
{
    // The start of the addresses we want to read from
    char cmd[2] = {
        CTRL_REG6_XL,
        0
    };

    // Write the address we are going to read from and don't end the transaction
    i2c.write(xgAddress, cmd, 1, true);
    // Read in all the 8 bits of data
    i2c.read(xgAddress, cmd+1, 1);

    // Then mask out the accel odr bits:
    cmd[1] &= 0xFF^(0x7 << 5);
    // Then shift in our new odr bits:
    cmd[1] |= aRate << 5;

    // Write the accelodr out to the accel
    i2c.write(xgAddress, cmd, 2);
}

void LSM9DS1::setMagODR(mag_odr mRate)
{
    // The start of the addresses we want to read from
    char cmd[2] = {
        CTRL_REG1_M,
        0
    };

    // Write the address we are going to read from and don't end the transaction
    i2c.write(mAddress, cmd, 1, true);
    // Read in all the 8 bits of data
    i2c.read(mAddress, cmd+1, 1);

    // Then mask out the mag odr bits:
    cmd[1] &= 0xFF^(0x7 << 2);
    // Then shift in our new odr bits:
    cmd[1] |= mRate << 2;

    // Write the magodr out to the mag
    i2c.write(mAddress, cmd, 2);
}

void LSM9DS1::calcgRes()
{
    // Possible gyro scales (and their register bit settings) are:
    // 245 DPS (00), 500 DPS (01), 2000 DPS (10).
    switch (gScale)
    {
        case G_SCALE_245DPS:
            // gRes = 245.0 / 32768.0;
            gRes = 8.75*0.001; // deg./sec.
            break;
        case G_SCALE_500DPS:
            // gRes = 500.0 / 32768.0;
            gRes = 17.50*0.001; // deg./sec.
            break;
        case G_SCALE_2000DPS:
            // gRes = 2000.0 / 32768.0;
            gRes = 70.0*0.001; // deg./sec.
            break;
    }
}

void LSM9DS1::calcaRes()
{
    // Possible accelerometer scales (and their register bit settings) are:
    // 2 g (000), 4g (001), 6g (010) 8g (011), 16g (100).
    switch (aScale)
    {
        case A_SCALE_2G:
            aRes = 2.0 / 32768.0;
            break;
        case A_SCALE_4G:
            aRes = 4.0 / 32768.0;
            break;
        case A_SCALE_8G:
            aRes = 8.0 / 32768.0;
            break;
        case A_SCALE_16G:
            // aRes = 16.0 / 32768.0;
            aRes = 0.732*0.001; // g (gravity)
            break;
    }
}

void LSM9DS1::calcmRes()
{
    // Possible magnetometer scales (and their register bit settings) are:
    // 2 Gs (00), 4 Gs (01), 8 Gs (10) 12 Gs (11).
    switch (mScale)
    {
        case M_SCALE_4GS:
            // mRes = 4.0 / 32768.0;
            mRes = 0.14*0.001; // gauss
            break;
        case M_SCALE_8GS:
            // mRes = 8.0 / 32768.0;
            mRes = 0.29*0.001; // gauss
            break;
        case M_SCALE_12GS:
            // mRes = 12.0 / 32768.0;
            mRes = 0.43*0.001; // gauss
            break;
        case M_SCALE_16GS:
            // mRes = 16.0 / 32768.0;
            mRes = 0.58*0.001; // gauss
            break;
    }
}


/*
void LSM9DS1::enableXgFIFO(bool enable)
{
    char cmd[2] = {CTRL_REG9, 0};

    i2c.write(xgAddress, cmd, 1);
    cmd[1] = i2c.read(CTRL_REG9);

    if (enable) cmd[1] |= (1<<1);
    else cmd[1] &= ~(1<<1);

    i2c.write(xgAddress, cmd, 2);
}

void LSM9DS1::setXgFIFO(uint8_t fifoMode, uint8_t fifoThs)
{
    // Limit threshold - 0x1F (31) is the maximum. If more than that was asked
    // limit it to the maximum.
    char cmd[2] = {FIFO_CTRL, 0};
    uint8_t threshold = fifoThs <= 0x1F ? fifoThs : 0x1F;
    cmd[1] = ((fifoMode & 0x7) << 5) | (threshold & 0x1F);
    i2c.write(xgAddress, cmd, 2);
}
*/
