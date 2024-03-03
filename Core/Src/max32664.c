#include "max32664.h"
#include "usart.h"

#include <stdlib.h>

void MAX32664_Init(MAX32664_Handle *handle, I2C_HandleTypeDef *hi2c, GPIO_Line *resetLine,
                   GPIO_Line *mfioLine, uint8_t address) {
    handle->hi2c = hi2c;
    handle->_resetLine = resetLine;
    handle->_mfioLine = mfioLine;
    handle->_address = address;
}

// Family Byte: READ_DEVICE_MODE (0x02) Index Byte: 0x00, Write Byte: 0x00
// The following function initializes the sensor. To place the MAX32664 into
// application mode, the MFIO pin must be pulled HIGH while the board is held
// in reset for 10ms. After 50 addtional ms have elapsed the board should be
// in application mode and will return two bytes, the first 0x00 is a
// successful communcation byte, followed by 0x00 which is the byte indicating
// which mode the IC is in.
uint8_t MAX32664_Begin(MAX32664_Handle *handle) {

    if ((handle->hi2c == NULL) || (handle->_resetLine == NULL) || (handle->_mfioLine == NULL)) {
        return 0xFF; // Bail if the pins have still not been defined
    }

    GPIO_InitTypeDef conf = {0};
    conf.Pin = handle->_resetLine->pin;
    conf.Mode = GPIO_MODE_OUTPUT_PP;
    conf.Pull = GPIO_NOPULL;
    conf.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(handle->_resetLine->port, &conf);

    conf.Pin = handle->_mfioLine->pin;
    conf.Mode = GPIO_MODE_OUTPUT_PP;
    conf.Pull = GPIO_NOPULL;
    conf.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(handle->_mfioLine->port, &conf);

    HAL_GPIO_WriteLine(handle->_mfioLine, GPIO_PIN_SET);
    HAL_GPIO_WriteLine(handle->_resetLine, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WriteLine(handle->_resetLine, GPIO_PIN_SET);

    HAL_Delay(1000);

    conf.Pin = handle->_mfioLine->pin;
    conf.Mode = GPIO_MODE_INPUT;
    conf.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(handle->_mfioLine->port, &conf);
    // To be used as an interrupt later

    uint8_t responseByte;
    (void)MAX32664_ReadByte(handle, READ_DEVICE_MODE, 0x00, &responseByte); // 0x00 only possible Index Byte.

    return responseByte;
}

// Family Byte: READ_DEVICE_MODE (0x02) Index Byte: 0x00, Write Byte: 0x00
// The following function puts the MAX32664 into bootloader mode. To place the MAX32664 into
// bootloader mode, the MFIO pin must be pulled LOW while the board is held
// in reset for 10ms. After 50 addtional ms have elapsed the board should be
// in bootloader mode and will return two bytes, the first 0x00 is a
// successful communcation byte, followed by 0x08 which is the byte indicating
// that the board is in bootloader mode.
uint8_t MAX32664_BeginBootloader(MAX32664_Handle *handle) {

    HAL_GPIO_WriteLine(handle->_mfioLine, GPIO_PIN_RESET);
    HAL_GPIO_WriteLine(handle->_resetLine, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WriteLine(handle->_resetLine, GPIO_PIN_SET);
    HAL_Delay(50); // Bootloader mode is enabled when this ends.

    GPIO_InitTypeDef conf = {0};
    conf.Pin = handle->_resetLine->pin;
    conf.Mode = GPIO_MODE_OUTPUT_PP;
    conf.Pull = GPIO_NOPULL;
    conf.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(handle->_resetLine->port, &conf);

    conf.Pin = handle->_mfioLine->pin;
    conf.Mode = GPIO_MODE_OUTPUT_PP;
    conf.Pull = GPIO_NOPULL;
    conf.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(handle->_mfioLine->port, &conf);

    // Let's check to see if the device made it into bootloader mode.
    uint8_t responseByte;
    (void)MAX32664_ReadByte(handle, READ_DEVICE_MODE, 0x00, &responseByte); // 0x00 only possible Index Byte
    return responseByte;
}

// Family Byte: HUB_STATUS (0x00), Index Byte: 0x00, No Write Byte.
// The following function checks the status of the FIFO.
uint8_t MAX32664_ReadSensorHubStatus(MAX32664_Handle *handle) {

    uint8_t status;
    (void)MAX32664_ReadByte(handle, 0x00, 0x00, &status); // Just family and index byte.
    return status;                                        // Will return 0x00
}

// This function sets very basic settings to get sensor and biometric data.
// The biometric data includes data about heartrate, the confidence
// level, SpO2 levels, and whether the sensor has detected a finger or not.
uint8_t MAX32664_ConfigBpm(MAX32664_Handle *handle, uint8_t mode) {

    uint8_t statusChauf = SB_ERR_UNKNOWN;
    if (mode == MODE_ONE || mode == MODE_TWO) {
    } else
        return INCORR_PARAM;

    statusChauf = MAX32664_SetOutputMode(handle, ALGO_DATA); // Just the data
    if (statusChauf != SB_SUCCESS) {
        return statusChauf;
    }

    statusChauf = MAX32664_SetFifoThreshold(handle, 0x01); // One sample before interrupt is fired.
    if (statusChauf != SB_SUCCESS) {
        return statusChauf;
    }

    statusChauf = MAX32664_AgcAlgoControl(handle, ENABLE); // One sample before interrupt is fired.
    if (statusChauf != SB_SUCCESS) {
        return statusChauf;
    }

    statusChauf = MAX32664_Max30101Control(handle, ENABLE);
    if (statusChauf != SB_SUCCESS) {
        return statusChauf;
    }

    statusChauf = MAX32664_MaximFastAlgoControl(handle, mode);
    if (statusChauf != SB_SUCCESS) {
        return statusChauf;
    }

    handle->_userSelectedMode = mode;
    handle->_sampleRate = MAX32664_ReadAlgoSamples(handle);

    HAL_Delay(1000);
    return SB_SUCCESS;
}

// This function sets very basic settings to get LED count values from the MAX30101.
// Sensor data includes 24 bit LED values for the three LED channels: Red, IR,
// and Green.
uint8_t MAX32664_ConfigSensor(MAX32664_Handle *handle) {

    uint8_t statusChauf; // Our status chauffeur

    statusChauf = MAX32664_SetOutputMode(handle, SENSOR_DATA); // Just the sensor data (LED)
    if (statusChauf != SB_SUCCESS)
        return statusChauf;

    statusChauf = MAX32664_SetFifoThreshold(handle, 0x01); // One sample before interrupt is fired to the MAX32664
    if (statusChauf != SB_SUCCESS)
        return statusChauf;

    statusChauf = MAX32664_Max30101Control(handle, ENABLE); // Enable Sensor.
    if (statusChauf != SB_SUCCESS)
        return statusChauf;

    statusChauf = MAX32664_MaximFastAlgoControl(handle, MODE_ONE); // Enable algorithm
    if (statusChauf != SB_SUCCESS)
        return statusChauf;

    HAL_Delay(1000);
    return SB_SUCCESS;
}

// This function sets very basic settings to get sensor and biometric data.
// Sensor data includes 24 bit LED values for the two LED channels: Red and IR.
// The biometric data includes data about heartrate, the confidence
// level, SpO2 levels, and whether the sensor has detected a finger or not.
// Of note, the number of samples is set to one.
uint8_t MAX32664_ConfigSensorBpm(MAX32664_Handle *handle, uint8_t mode) {

    uint8_t statusChauf; // Our status chauffeur
    if (mode == MODE_ONE || mode == MODE_TWO) {
    } else
        return INCORR_PARAM;

    statusChauf = MAX32664_SetOutputMode(handle, SENSOR_AND_ALGORITHM); // Data and sensor data
    if (statusChauf != SB_SUCCESS)
        return statusChauf;

    statusChauf = MAX32664_SetFifoThreshold(handle, 0x01); // One sample before interrupt is fired to the MAX32664
    if (statusChauf != SB_SUCCESS)
        return statusChauf;

    statusChauf = MAX32664_Max30101Control(handle, ENABLE); // Enable Sensor.
    if (statusChauf != SB_SUCCESS)
        return statusChauf;

    statusChauf = MAX32664_MaximFastAlgoControl(handle, mode); // Enable algorithm
    if (statusChauf != SB_SUCCESS)
        return statusChauf;

    handle->_userSelectedMode = mode;
    handle->_sampleRate = MAX32664_ReadAlgoSamples(handle);

    HAL_Delay(1000);
    return SB_SUCCESS;
}

// This function takes the 8 bytes from the FIFO buffer related to the wrist
// heart rate algortihm: heart rate (uint16_t), confidence (uint8_t) , SpO2 (uint16_t),
// and the finger detected status (uint8_t). Note that the the algorithm is stated as
// "wrist" though the sensor only works with the finger. The data is loaded
// into the whrmFifo and returned.
bioData MAX32664_ReadBpm(MAX32664_Handle *handle) {

    bioData libBpm;
    uint8_t statusChauf; // The status chauffeur captures return values.

    statusChauf = MAX32664_ReadSensorHubStatus(handle);

    if (statusChauf == 1) { // Communication Error
        libBpm.heartRate = 100;
        libBpm.confidence = 100;
        libBpm.oxygen = 100;
        return libBpm;
    }

    MAX32664_NumSamplesOutFifo(handle);

    if (handle->_userSelectedMode == MODE_ONE) {

        MAX32664_ReadFillArray(handle, READ_DATA_OUTPUT, READ_DATA,
                               MAXFAST_ARRAY_SIZE, handle->bpmArr);

        // Heart Rate formatting
        libBpm.heartRate = (uint16_t)(handle->bpmArr[0]) << 8;
        libBpm.heartRate |= (handle->bpmArr[1]);
        libBpm.heartRate /= 10;

        // Confidence formatting
        libBpm.confidence = handle->bpmArr[2];

        // Blood oxygen level formatting
        libBpm.oxygen = (uint16_t)(handle->bpmArr[3]) << 8;
        libBpm.oxygen |= handle->bpmArr[4];
        libBpm.oxygen /= 10;

        //"Machine State" - has a finger been detected?
        libBpm.status = handle->bpmArr[5];

        return libBpm;
    }

    else if (handle->_userSelectedMode == MODE_TWO) {
        MAX32664_ReadFillArray(handle, READ_DATA_OUTPUT, READ_DATA,
                               MAXFAST_ARRAY_SIZE + MAXFAST_EXTENDED_DATA, handle->bpmArrTwo);

        // Heart Rate formatting
        libBpm.heartRate = (uint16_t)(handle->bpmArrTwo[0]) << 8;
        libBpm.heartRate |= (handle->bpmArrTwo[1]);
        libBpm.heartRate /= 10;

        // Confidence formatting
        libBpm.confidence = handle->bpmArrTwo[2];

        // Blood oxygen level formatting
        libBpm.oxygen = (uint16_t)(handle->bpmArrTwo[3]) << 8;
        libBpm.oxygen |= handle->bpmArrTwo[4];
        libBpm.oxygen /= 10.0;

        //"Machine State" - has a finger been detected?
        libBpm.status = handle->bpmArrTwo[5];

        // Sp02 r Value formatting
        uint16_t tempVal = (uint16_t)(handle->bpmArrTwo[6]) << 8;
        tempVal |= handle->bpmArrTwo[7];
        libBpm.rValue = tempVal;
        libBpm.rValue /= 10.0;

        // Extended Machine State formatting
        libBpm.extStatus = handle->bpmArrTwo[8];

        // There are two additional bytes of data that were requested but that
        // have not been implemented in firmware 10.1 so will not be saved to
        // user's data.
        return libBpm;
    }

    else {
        libBpm.heartRate = 0;
        libBpm.confidence = 0;
        libBpm.oxygen = 0;
        return libBpm;
    }
}

// This function takes 9 bytes of LED values from the MAX30101 associated with
// the RED, IR, and GREEN LEDs. In addition it gets the 8 bytes from the FIFO buffer
// related to the wrist heart rate algortihm: heart rate (uint16_t), confidence (uint8_t),
// SpO2 (uint16_t), and the finger detected status (uint8_t). Note that the the algorithm
// is stated as "wrist" though the sensor only works with the finger. The data is loaded
// into the whrmFifo and returned.
bioData MAX32664_ReadSensor(MAX32664_Handle *handle) {

    bioData libLedFifo;
    MAX32664_ReadFillArray(handle, READ_DATA_OUTPUT, READ_DATA,
                           MAX30101_LED_ARRAY, handle->senArr);

    // Value of LED one....
    libLedFifo.irLed = (uint32_t)(handle->senArr[0]) << 16;
    libLedFifo.irLed |= (uint32_t)(handle->senArr[1]) << 8;
    libLedFifo.irLed |= handle->senArr[2];

    // Value of LED two...
    libLedFifo.redLed = (uint32_t)(handle->senArr[3]) << 16;
    libLedFifo.redLed |= (uint32_t)(handle->senArr[4]) << 8;
    libLedFifo.redLed |= handle->senArr[5];

    return libLedFifo;
}

// This function takes the information of both the LED value and the biometric
// data from the MAX32664's FIFO. In essence it combines the two functions
// above into a single function call.
bioData MAX32664_ReadSensorBpm(MAX32664_Handle *handle) {

    bioData libLedBpm;

    if (handle->_userSelectedMode == MODE_ONE) {

        MAX32664_ReadFillArray(handle, READ_DATA_OUTPUT, READ_DATA,
                               MAXFAST_ARRAY_SIZE + MAX30101_LED_ARRAY, handle->bpmSenArr);

        // Value of LED one....
        libLedBpm.irLed = (uint32_t)(handle->bpmSenArr[0]) << 16;
        libLedBpm.irLed |= (uint32_t)(handle->bpmSenArr[1]) << 8;
        libLedBpm.irLed |= handle->bpmSenArr[2];

        // Value of LED two...
        libLedBpm.redLed = (uint32_t)(handle->bpmSenArr[3]) << 16;
        libLedBpm.redLed |= (uint32_t)(handle->bpmSenArr[4]) << 8;
        libLedBpm.redLed |= handle->bpmSenArr[5];

        // -- What happened here? -- There are two uint32_t values that are given by
        // the sensor for LEDs that do not exists on the MAX30101. So we have to
        // request those empty values because they occupy the buffer:
        // handle->bpmSenArr[6-11].

        // Heart rate formatting
        libLedBpm.heartRate = ((uint16_t)(handle->bpmSenArr[12]) << 8);
        libLedBpm.heartRate |= (handle->bpmSenArr[13]);
        libLedBpm.heartRate /= 10;

        // Confidence formatting
        libLedBpm.confidence = handle->bpmSenArr[14];

        // Blood oxygen level formatting
        libLedBpm.oxygen = (uint16_t)(handle->bpmSenArr[15]) << 8;
        libLedBpm.oxygen |= handle->bpmSenArr[16];
        libLedBpm.oxygen /= 10;

        //"Machine State" - has a finger been detected?
        libLedBpm.status = handle->bpmSenArr[17];
        return libLedBpm;
    }

    else if (handle->_userSelectedMode == MODE_TWO) {

        MAX32664_ReadFillArray(handle, READ_DATA_OUTPUT, READ_DATA,
                               MAXFAST_ARRAY_SIZE + MAX30101_LED_ARRAY + MAXFAST_EXTENDED_DATA,
                               handle->bpmSenArrTwo);

        // Value of LED one....
        libLedBpm.irLed = (uint32_t)(handle->bpmSenArrTwo[0]) << 16;
        libLedBpm.irLed |= (uint32_t)(handle->bpmSenArrTwo[1]) << 8;
        libLedBpm.irLed |= handle->bpmSenArrTwo[2];

        // Value of LED two...
        libLedBpm.redLed = (uint32_t)(handle->bpmSenArrTwo[3]) << 16;
        libLedBpm.redLed |= (uint32_t)(handle->bpmSenArrTwo[4]) << 8;
        libLedBpm.redLed |= handle->bpmSenArrTwo[5];

        // -- What happened here? -- There are two uint32_t values that are given by
        // the sensor for LEDs that do not exists on the MAX30101. So we have to
        // request those empty values because they occupy the buffer:
        // handle->bpmSenArrTwo[6-11].

        // Heart rate formatting
        libLedBpm.heartRate = ((uint16_t)(handle->bpmSenArrTwo[12]) << 8);
        libLedBpm.heartRate |= (handle->bpmSenArrTwo[13]);
        libLedBpm.heartRate /= 10;

        // Confidence formatting
        libLedBpm.confidence = handle->bpmSenArrTwo[14];

        // Blood oxygen level formatting
        libLedBpm.oxygen = (uint16_t)(handle->bpmSenArrTwo[15]) << 8;
        libLedBpm.oxygen |= handle->bpmSenArrTwo[16];
        libLedBpm.oxygen /= 10;

        //"Machine State" - has a finger been detected?
        libLedBpm.status = handle->bpmSenArrTwo[17];

        // Sp02 r Value formatting
        uint16_t tempVal = (uint16_t)(handle->bpmSenArrTwo[18]) << 8;
        tempVal |= handle->bpmSenArrTwo[19];
        libLedBpm.rValue = tempVal;
        libLedBpm.rValue /= 10.0;

        // Extended Machine State formatting
        libLedBpm.extStatus = handle->bpmSenArrTwo[20];

        // There are two additional bytes of data that were requested but that
        // have not been implemented in firmware 10.1 so will not be saved to
        // user's data.
        //
        return libLedBpm;

    }

    else {
        libLedBpm.irLed = 0;
        libLedBpm.redLed = 0;
        libLedBpm.heartRate = 0;
        libLedBpm.confidence = 0;
        libLedBpm.oxygen = 0;
        libLedBpm.status = 0;
        libLedBpm.rValue = 0;
        libLedBpm.extStatus = 0;
        return libLedBpm;
    }
}
// This function modifies the pulse width of the MAX30101 LEDs. All of the LEDs
// are modified to the same width. This will affect the number of samples that
// can be collected and will also affect the ADC resolution.
// Default: 69us - 15 resolution - 50 samples per second.
// Register: 0x0A, bits [1:0]
// Width(us) - Resolution -  Sample Rate
//  69us     -    15      -   <= 3200 (fastest - least resolution)
//  118us    -    16      -   <= 1600
//  215us    -    17      -   <= 1600
//  411us    -    18      -   <= 1000 (slowest - highest resolution)
uint8_t MAX32664_SetPulseWidth(MAX32664_Handle *handle, uint16_t width) {

    uint8_t bits;
    uint8_t regVal;

    // Make sure the correct pulse width is selected.
    if (width == 69)
        bits = 0;
    else if (width == 118)
        bits = 1;
    else if (width == 215)
        bits = 2;
    else if (width == 411)
        bits = 3;
    else
        return INCORR_PARAM;

    // Get current register value so that nothing is overwritten.
    regVal = MAX32664_ReadRegisterMAX30101(handle, CONFIGURATION_REGISTER);
    regVal &= PULSE_MASK;                                                   // Mask bits to change.
    regVal |= bits;                                                         // Add bits
    MAX32664_WriteRegisterMAX30101(handle, CONFIGURATION_REGISTER, regVal); // Write Register

    return SB_SUCCESS;
}

// This function reads the CONFIGURATION_REGISTER (0x0A), bits [1:0] from the
// MAX30101 Sensor. It returns one of the four settings in microseconds.
uint16_t MAX32664_ReadPulseWidth(MAX32664_Handle *handle) {

    uint8_t regVal;

    regVal = MAX32664_ReadRegisterMAX30101(handle, CONFIGURATION_REGISTER);
    regVal &= READ_PULSE_MASK;

    if (regVal == 0)
        return 69;
    else if (regVal == 1)
        return 118;
    else if (regVal == 2)
        return 215;
    else if (regVal == 3)
        return 411;
    else
        return SB_ERR_UNKNOWN;
}

// This function changes the sample rate of the MAX30101 sensor. The sample
// rate is affected by the set pulse width of the MAX30101 LEDs.
// Default: 69us - 15 resolution - 50 samples per second.
// Register: 0x0A, bits [4:2]
// Width(us) - Resolution -  Sample Rate
//  69us     -    15      -   <= 3200 (fastest - least resolution)
//  118us    -    16      -   <= 1600
//  215us    -    17      -   <= 1600
//  411us    -    18      -   <= 1000 (slowest - highest resolution)
uint8_t MAX32664_SetSampleRate(MAX32664_Handle *handle, uint16_t sampRate) {

    uint8_t bits;
    uint8_t regVal;

    // Make sure the correct sample rate was picked
    if (sampRate == 50)
        bits = 0;
    else if (sampRate == 100)
        bits = 1;
    else if (sampRate == 200)
        bits = 2;
    else if (sampRate == 400)
        bits = 3;
    else if (sampRate == 800)
        bits = 4;
    else if (sampRate == 1000)
        bits = 5;
    else if (sampRate == 1600)
        bits = 6;
    else if (sampRate == 3200)
        bits = 7;
    else
        return INCORR_PARAM;

    // Get current register value so that nothing is overwritten.
    regVal = MAX32664_ReadRegisterMAX30101(handle, CONFIGURATION_REGISTER);
    regVal &= SAMP_MASK;                                                    // Mask bits to change.
    regVal |= (bits << 2);                                                  // Add bits but shift them first to correct position.
    MAX32664_WriteRegisterMAX30101(handle, CONFIGURATION_REGISTER, regVal); // Write Register

    return SB_SUCCESS;
}

// This function reads the CONFIGURATION_REGISTER (0x0A), bits [4:2] from the
// MAX30101 Sensor. It returns one of the 8 possible sample rates.
uint16_t MAX32664_ReadSampleRate(MAX32664_Handle *handle) {

    uint8_t regVal;

    regVal = MAX32664_ReadRegisterMAX30101(handle, CONFIGURATION_REGISTER);
    regVal &= READ_SAMP_MASK;
    regVal = (regVal >> 2);

    if (regVal == 0)
        return 50;
    else if (regVal == 1)
        return 100;
    else if (regVal == 2)
        return 200;
    else if (regVal == 3)
        return 400;
    else if (regVal == 4)
        return 800;
    else if (regVal == 5)
        return 1000;
    else if (regVal == 6)
        return 1600;
    else if (regVal == 7)
        return 3200;
    else
        return SB_ERR_UNKNOWN;
}

// MAX30101 Register: CONFIGURATION_REGISTER (0x0A), bits [6:5]
// This functions sets the dynamic range of the MAX30101's ADC. The function
// accepts the higher range as a parameter.
// Default Range: 7.81pA - 2048nA
// Possible Ranges:
// 7.81pA  - 2048nA
// 15.63pA - 4096nA
// 32.25pA - 8192nA
// 62.5pA  - 16384nA
uint8_t MAX32664_SetAdcRange(MAX32664_Handle *handle, uint16_t adcVal) {

    uint8_t regVal;
    uint8_t bits;

    if (adcVal <= 2048)
        bits = 0;
    else if (adcVal <= 4096)
        bits = 1;
    else if (adcVal <= 8192)
        bits = 2;
    else if (adcVal <= 16384)
        bits = 3;
    else
        return INCORR_PARAM;

    regVal = MAX32664_ReadRegisterMAX30101(handle, CONFIGURATION_REGISTER);
    regVal &= ADC_MASK;
    regVal |= bits << 5;

    MAX32664_WriteRegisterMAX30101(handle, CONFIGURATION_REGISTER, regVal);

    return SB_SUCCESS;
}

// MAX30101 Register: CONFIGURATION_REGISTER (0x0A), bits [6:5]
// This function returns the set ADC range of the MAX30101 sensor.
uint16_t MAX32664_ReadAdcRange(MAX32664_Handle *handle) {

    uint8_t regVal;
    regVal = MAX32664_ReadRegisterMAX30101(handle, CONFIGURATION_REGISTER);
    regVal &= READ_ADC_MASK;
    regVal = (regVal >> 5); // Shift our bits to the front of the line.

    if (regVal == 0)
        return 2048;
    else if (regVal == 1)
        return 4096;
    else if (regVal == 2)
        return 8192;
    else if (regVal == 3)
        return 16384;
    else
        return SB_ERR_UNKNOWN;
}

// Family Byte: SET_DEVICE_MODE (0x01), Index Byte: 0x01, Write Byte: 0x00
// The following function is an alternate way to set the mode of the of
// MAX32664. It can take three parameters: Enter and Exit Bootloader Mode, as
// well as reset.
// INCOMPLETE
uint8_t MAX32664_SetOperatingMode(MAX32664_Handle *handle, uint8_t selection) {

    // Must be one of the three....
    if (selection == EXIT_BOOTLOADER || selection == EXIT_RESET || selection == ENTER_BOOTLOADER) {
    } else
        return INCORR_PARAM;

    uint8_t statusByte = MAX32664_WriteByte(handle, SET_DEVICE_MODE, 0x00,
                                            selection);
    if (statusByte != SB_SUCCESS)
        return statusByte;

    // Here we'll check if the board made it into Bootloader mode...
    uint8_t responseByte;
    (void)MAX32664_ReadByte(handle, READ_DEVICE_MODE, 0x00, &responseByte); // 0x00 only possible Index Byte
    return responseByte;                                                    // This is in fact the status byte, need second returned byte - bootloader mode
}

// Family Byte: IDENTITY (0x01), Index Byte: READ_MCU_TYPE, Write Byte: NONE
// The following function returns a byte that signifies the microcontoller that
// is in communcation with your host microcontroller. Returns 0x00 for the
// MAX32625 and 0x01 for the MAX32660/MAX32664.
uint8_t MAX32664_GetMcuType(MAX32664_Handle *handle) {

    uint8_t returnByte;
    (void)MAX32664_ReadByteWrite(handle, IDENTITY, READ_MCU_TYPE,
                                 NO_WRITE, &returnByte);
    if (returnByte > 0x00)
        return SB_ERR_UNKNOWN;
    else
        return returnByte;
}

// Family Byte: BOOTLOADER_INFO (0x80), Index Byte: BOOTLOADER_VERS (0x00)
// This function checks the version number of the bootloader on the chip and
// returns a four bytes: Major version Byte, Minor version Byte, Space Byte,
// and the Revision Byte.
int32_t MAX32664_GetBootloaderInf(MAX32664_Handle *handle) {

    int32_t bootVers = 0;
    int32_t revNum[4] = {};
    uint8_t status = MAX32664_ReadMultipleBytes32(handle, BOOTLOADER_INFO,
                                                  BOOTLOADER_VERS, 0x00, 4, revNum);

    if (!status)
        return SB_ERR_UNKNOWN;
    else {
        bootVers |= ((int32_t)revNum[1] << 16);
        bootVers |= ((int32_t)revNum[2] << 8);
        bootVers |= revNum[3];
        return bootVers;
    }
}

// Family Byte: ENABLE_SENSOR (0x44), Index Byte: ENABLE_MAX30101 (0x03), Write
// Byte: senSwitch  (parameter - 0x00 or 0x01).
// This function enables the MAX30101.
uint8_t MAX32664_Max30101Control(MAX32664_Handle *handle, uint8_t senSwitch) {

    if (senSwitch == 0 || senSwitch == 1) {
    } else
        return INCORR_PARAM;

    // Check that communication was successful, not that the sensor is enabled.
    uint8_t statusByte = MAX32664_EnableWrite(handle, ENABLE_SENSOR,
                                              ENABLE_MAX30101, senSwitch);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: READ_SENSOR_MODE (0x45), Index Byte: READ_ENABLE_MAX30101 (0x03)
// This function checks if the MAX30101 is enabled or not.
uint8_t MAX32664_ReadMAX30101State(MAX32664_Handle *handle) {

    uint8_t state;
    MAX32664_ReadByte(handle, READ_SENSOR_MODE,
                      READ_ENABLE_MAX30101, &state);
    return state;
}

// Family Byte: ENABLE_SENSOR (0x44), Index Byte: ENABLE_ACCELEROMETER (0x04), Write
// Byte: accepts (parameter - 0x00 or 0x01).
// This function enables the Accelerometer.
uint8_t MAX32664_AccelControl(MAX32664_Handle *handle, uint8_t accelSwitch) {

    if ((accelSwitch != 0x00) || (accelSwitch != 0x00)) {
    } else
        return INCORR_PARAM;

    // Check that communication was successful, not that the sensor is enabled.
    uint8_t statusByte = MAX32664_EnableWrite(handle, ENABLE_SENSOR,
                                              ENABLE_ACCELEROMETER, accelSwitch);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: OUTPUT_MODE (0x10), Index Byte: SET_FORMAT (0x00),
// Write Byte : outputType (Parameter values in OUTPUT_MODE_WRITE_BYTE)
uint8_t MAX32664_SetOutputMode(MAX32664_Handle *handle, uint8_t outputType) {

    if (outputType > SENSOR_ALGO_COUNTER) // Bytes between 0x00 and 0x07
        return INCORR_PARAM;

    // Check that communication was successful, not that the IC is outputting
    // correct format.
    uint8_t statusByte = MAX32664_WriteByte(handle, OUTPUT_MODE, SET_FORMAT,
                                            outputType);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: OUTPUT_MODE(0x10), Index Byte: WRITE_SET_THRESHOLD (0x01), Write byte: intThres
// (parameter - value betwen 0 and 0xFF).
// This function changes the threshold for the FIFO interrupt bit/pin. The
// interrupt pin is the MFIO pin which is set to INPUT after IC initialization
// (begin).
uint8_t MAX32664_SetFifoThreshold(MAX32664_Handle *handle, uint8_t intThresh) {

    // Checks that there was succesful communcation, not that the threshold was
    // set correctly.
    uint8_t statusByte = MAX32664_WriteByte(handle, OUTPUT_MODE,
                                            WRITE_SET_THRESHOLD, intThresh);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: READ_DATA_OUTPUT (0x12), Index Byte: NUM_SAMPLES (0x00), Write
// Byte: NONE
// This function returns the number of samples available in the FIFO.
uint8_t MAX32664_NumSamplesOutFifo(MAX32664_Handle *handle) {

    uint8_t sampAvail;
    MAX32664_ReadByte(handle, READ_DATA_OUTPUT,
                      NUM_SAMPLES, &sampAvail);
    return sampAvail;
}

// Family Byte: READ_DATA_OUTPUT (0x12), Index Byte: READ_DATA (0x01), Write
// Byte: NONE
// This function returns the data in the FIFO.
uint8_t *MAX32664_GetDataOutFifo(MAX32664_Handle *handle, uint8_t data[]) {

    uint8_t samples = MAX32664_NumSamplesOutFifo(handle);
    MAX32664_ReadFillArray(handle, READ_DATA_OUTPUT, READ_DATA, samples, data);
    return data;
}

// Family Byte: READ_DATA_INPUT (0x13), Index Byte: FIFO_EXTERNAL_INDEX_BYTE (0x00), Write
// Byte: NONE
// This function adds support for the acceleromter that is NOT included on
// SparkFun's product, The Family Registery of 0x13 and 0x14 is skipped for now.
uint8_t MAX32664_NumSamplesExternalSensor(MAX32664_Handle *handle) {

    uint8_t sampAvail;
    (void)MAX32664_ReadByteWrite(handle, READ_DATA_INPUT,
                                 SAMPLE_SIZE, WRITE_ACCELEROMETER, &sampAvail);
    return sampAvail;
}

// Family Byte: WRITE_REGISTER (0x40), Index Byte: WRITE_MAX30101 (0x03), Write Bytes:
// Register Address and Register Value
// This function writes the given register value at the given register address
// for the MAX30101 sensor and returns a boolean indicating a successful or
// non-successful write.
void MAX32664_WriteRegisterMAX30101(MAX32664_Handle *handle, uint8_t regAddr,
                                    uint8_t regVal) {

    MAX32664_WriteByteParameter(handle, WRITE_REGISTER, WRITE_MAX30101, regAddr,
                                regVal);
}

// Family Byte: WRITE_REGISTER (0x40), Index Byte: WRITE_ACCELEROMETER (0x04), Write Bytes:
// Register Address and Register Value
// This function writes the given register value at the given register address
// for the Accelerometer and returns a boolean indicating a successful or
// non-successful write.
void MAX32664_WriteRegisterAccel(MAX32664_Handle *handle, uint8_t regAddr,
                                 uint8_t regVal) {

    MAX32664_WriteByteParameter(handle, WRITE_REGISTER, WRITE_ACCELEROMETER,
                                regAddr, regVal);
}

// Family Byte: READ_REGISTER (0x41), Index Byte: READ_MAX30101 (0x03), Write Byte:
// Register Address
// This function reads the given register address for the MAX30101 Sensor and
// returns the values at that register.
uint8_t MAX32664_ReadRegisterMAX30101(MAX32664_Handle *handle, uint8_t regAddr) {

    uint8_t regCont;
    MAX32664_ReadByteWrite(handle, READ_REGISTER,
                           READ_MAX30101, regAddr, &regCont);
    return regCont;
}

// Family Byte: READ_REGISTER (0x41), Index Byte: READ_ACCELEROMETER (0x04), Write Byte:
// Register Address
// This function reads the given register address for the MAX30101 Sensor and
// returns the values at that register.
uint8_t MAX32664_ReadRegisterAccel(MAX32664_Handle *handle, uint8_t regAddr) {

    uint8_t regCont;
    MAX32664_ReadByteWrite(handle, READ_REGISTER,
                           READ_ACCELEROMETER, regAddr, &regCont);
    return regCont;
}

// Family Byte: READ_ATTRIBUTES_AFE (0x42), Index Byte: RETRIEVE_AFE_MAX30101 (0x03)
// This function retrieves the attributes of the AFE (Analog Front End) of the
// MAX30101 sensor. It returns the number of bytes in a word for the sensor
// and the number of registers available.
sensorAttr MAX32664_GetAfeAttributesMAX30101(MAX32664_Handle *handle) {

    sensorAttr maxAttr;
    uint8_t tempArray[2];

    MAX32664_ReadFillArray(handle, READ_ATTRIBUTES_AFE, RETRIEVE_AFE_MAX30101,
                           2, tempArray);

    maxAttr.byteWord = tempArray[0];
    maxAttr.availRegisters = tempArray[1];

    return maxAttr;
}

// Family Byte: READ_ATTRIBUTES_AFE (0x42), Index Byte:
// RETRIEVE_AFE_ACCELEROMETER (0x04)
// This function retrieves the attributes of the AFE (Analog Front End) of the
// Accelerometer. It returns the number of bytes in a word for the sensor
// and the number of registers available.
sensorAttr MAX32664_GetAfeAttributesAccelerometer(MAX32664_Handle *handle) {

    sensorAttr maxAttr;
    uint8_t tempArray[2];

    MAX32664_ReadFillArray(handle, READ_ATTRIBUTES_AFE,
                           RETRIEVE_AFE_ACCELEROMETER, 2, tempArray);

    maxAttr.byteWord = tempArray[0];
    maxAttr.availRegisters = tempArray[1];

    return maxAttr;
}

// Family Byte: DUMP_REGISTERS (0x43), Index Byte: DUMP_REGISTER_MAX30101 (0x03)
// This function returns all registers and register values sequentially of the
// MAX30101 sensor: register zero and register value zero to register n and
// register value n. There are 36 registers in this case.
uint8_t MAX32664_DumpRegisterMAX30101(MAX32664_Handle *handle,
                                      uint8_t regArray[]) {

    uint8_t numOfBytes = 36;
    uint8_t status = MAX32664_ReadFillArray(handle, DUMP_REGISTERS,
                                            DUMP_REGISTER_MAX30101, numOfBytes, regArray);
    return status;
}

// Family Byte: DUMP_REGISTERS (0x43), Index Byte: DUMP_REGISTER_ACCELEROMETER (0x04)
// This function returns all registers and register values sequentially of the
// Accelerometer: register zero and register value zero to register n and
// register value n.
uint8_t MAX32664_DumpRegisterAccelerometer(MAX32664_Handle *handle,
                                           uint8_t numReg, uint8_t regArray[]) {

    uint8_t status = MAX32664_ReadFillArray(handle, DUMP_REGISTERS,
                                            DUMP_REGISTER_ACCELEROMETER, numReg, regArray); // Fake read amount
    return status;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte:
// SET_TARG_PERC (0x00), Write Byte: AGC_GAIN_ID (0x00)
// This function sets the target percentage of the full-scale ADC range that
// the automatic gain control algorithm uses. It takes a paramater of zero to
// 100 percent.
uint8_t MAX32664_SetAlgoRange(MAX32664_Handle *handle, uint8_t perc) {

    if (perc > 100)
        return INCORR_PARAM;

    // Successful communication or no?
    uint8_t statusByte = MAX32664_WriteByteParameter(handle,
                                                     CHANGE_ALGORITHM_CONFIG, SET_TARG_PERC, AGC_GAIN_ID, perc);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte:
// SET_STEP_SIZE (0x00), Write Byte: AGC_STEP_SIZE_ID (0x01)
// This function changes the step size toward the target for the AGC algorithm.
// It takes a paramater of zero to 100 percent.
uint8_t MAX32664_SetAlgoStepSize(MAX32664_Handle *handle, uint8_t step) {

    if (step > 100)
        return INCORR_PARAM;

    // Successful communication or no?
    uint8_t statusByte = MAX32664_WriteByteParameter(handle,
                                                     CHANGE_ALGORITHM_CONFIG, SET_STEP_SIZE, AGC_STEP_SIZE_ID, step);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte:
// SET_SENSITIVITY (0x00), Write Byte: AGC_SENSITIVITY_ID (0x02)
// This function changes the sensitivity of the AGC algorithm.
uint8_t MAX32664_SetAlgoSensitivity(MAX32664_Handle *handle, uint8_t sense) {

    if (sense > 100)
        return INCORR_PARAM;

    // Successful communication or no?
    uint8_t statusByte = MAX32664_WriteByteParameter(handle,
                                                     CHANGE_ALGORITHM_CONFIG, SET_SENSITIVITY, AGC_SENSITIVITY_ID,
                                                     sense);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte:
// SET_AVG_SAMPLES (0x00), Write Byte: AGC_NUM_SAMP_ID (0x03)
// This function changes the number of samples that are averaged.
// It takes a paramater of zero to 255.
uint8_t MAX32664_SetAlgoSamples(MAX32664_Handle *handle, uint8_t avg) {

    // Successful communication or no?
    uint8_t statusByte = MAX32664_WriteByteParameter(handle,
                                                     CHANGE_ALGORITHM_CONFIG, SET_AVG_SAMPLES, AGC_NUM_SAMP_ID, avg);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte:
// SET_PULSE_OX_COEF (0x02), Write Byte: MAXIMFAST_COEF_ID (0x0B)
// This function takes three values that are used as the Sp02 coefficients.
// These three values are multiplied by 100,000;
// default values are in order: 159584, -3465966, and 11268987.
uint8_t MAX32664_SetMaximFastCoef(MAX32664_Handle *handle, int32_t coef1,
                                  int32_t coef2, int32_t coef3) {

    const size_t numCoefVals = 3;
    int32_t coefArr[3] = {coef1, coef2, coef3};

    uint8_t statusByte = MAX32664_WriteLongBytes(handle,
                                                 CHANGE_ALGORITHM_CONFIG, SET_PULSE_OX_COEF, MAXIMFAST_COEF_ID,
                                                 coefArr, numCoefVals);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: READ_ALGORITHM_CONFIG (0x51), Index Byte:
// READ_AGC_PERCENTAGE (0x00), Write Byte: READ_AGC_PERC_ID (0x00)
// This function reads and returns the currently set target percentage
// of the full-scale ADC range that the Automatic Gain Control algorithm is using.
uint8_t MAX32664_ReadAlgoRange(MAX32664_Handle *handle) {

    uint8_t range;
    MAX32664_ReadByteWrite(handle, READ_ALGORITHM_CONFIG,
                           READ_AGC_PERCENTAGE, READ_AGC_PERC_ID, &range);
    return range;
}

// Family Byte: READ_ALGORITHM_CONFIG (0x51), Index Byte:
// READ_AGC_STEP_SIZE (0x00), Write Byte: READ_AGC_STEP_SIZE_ID (0x01)
// This function returns the step size toward the target for the AGC algorithm.
// It returns a value between zero and 100 percent.
uint8_t MAX32664_ReadAlgoStepSize(MAX32664_Handle *handle) {

    uint8_t stepSize;
    MAX32664_ReadByteWrite(handle, READ_ALGORITHM_CONFIG,
                           READ_AGC_STEP_SIZE, READ_AGC_STEP_SIZE_ID, &stepSize);
    return stepSize;
}

// Family Byte: READ_ALGORITHM_CONFIG (0x51), Index Byte:
// READ_AGC_SENSITIVITY_ID (0x00), Write Byte: READ_AGC_SENSITIVITY_ID (0x02)
// This function returns the sensitivity (percentage) of the automatic gain control.
uint8_t MAX32664_ReadAlgoSensitivity(MAX32664_Handle *handle) {

    uint8_t sensitivity;
    MAX32664_ReadByteWrite(handle, READ_ALGORITHM_CONFIG,
                           READ_AGC_SENSITIVITY, READ_AGC_SENSITIVITY_ID, &sensitivity);
    return sensitivity;
}

// Family Byte: READ_ALGORITHM_CONFIG (0x51), Index Byte:
// READ_AGC_NUM_SAMPLES (0x00), Write Byte: READ_AGC_NUM_SAMPLES_ID (0x03)
// This function changes the number of samples that are averaged.
// It takes a paramater of zero to 255.
uint8_t MAX32664_ReadAlgoSamples(MAX32664_Handle *handle) {

    uint8_t samples;
    MAX32664_ReadByteWrite(handle, READ_ALGORITHM_CONFIG,
                           READ_AGC_NUM_SAMPLES, READ_AGC_NUM_SAMPLES_ID, &samples);
    return samples;
}

// Family Byte: READ_ALGORITHM_CONFIG (0x51), Index Byte:
// READ_MAX_FAST_COEF (0x02), Write Byte: READ_MAX_FAST_COEF_ID (0x0B)
// This function reads the maximum age for the wrist heart rate monitor
// (WHRM) algorithm. It returns three uint32_t integers that are
// multiplied by 100,000.
uint8_t MAX32664_ReadMaximFastCoef(MAX32664_Handle *handle, int32_t coefArr[3]) {

    const size_t numOfReads = 3;
    uint8_t status = MAX32664_ReadMultipleBytes32(handle, READ_ALGORITHM_CONFIG,
                                                  READ_MAX_FAST_COEF, READ_MAX_FAST_COEF_ID, numOfReads, coefArr);
    coefArr[0] = coefArr[0] * 100000;
    coefArr[1] = coefArr[1] * 100000;
    coefArr[2] = coefArr[2] * 100000;
    return status;
}

// Family Byte: ENABLE_ALGORITHM (0x52), Index Byte:
// ENABLE_AGC_ALGO (0x00)
// This function enables (one) or disables (zero) the automatic gain control algorithm.
uint8_t MAX32664_AgcAlgoControl(MAX32664_Handle *handle, uint8_t enable) {

    if (enable == 0 || enable == 1) {
    } else
        return INCORR_PARAM;

    uint8_t statusByte = MAX32664_EnableWrite(handle, ENABLE_ALGORITHM,
                                              ENABLE_AGC_ALGO, enable);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: ENABLE_ALGORITHM (0x52), Index Byte:
// ENABLE_WHRM_ALGO (0x02)
// This function enables (one) or disables (zero) the wrist heart rate monitor
// algorithm.
uint8_t MAX32664_MaximFastAlgoControl(MAX32664_Handle *handle, uint8_t mode) {

    if (mode == 0 || mode == 1 || mode == 2) {
    } else
        return INCORR_PARAM;

    uint8_t statusByte = MAX32664_EnableWrite(handle, ENABLE_ALGORITHM,
                                              ENABLE_WHRM_ALGO, mode);
    if (statusByte != SB_SUCCESS)
        return statusByte;
    else
        return SB_SUCCESS;
}

// Family Byte: BOOTLOADER_FLASH (0x80), Index Byte: SET_INIT_VECTOR_BYTES (0x00)
// void MAX32664_SetInitBytes

// Family Byte: BOOTLOADER_FLASH (0x80), Index Byte: SET_AUTH_BYTES (0x01)

// Family Byte: BOOTLOADER_FLASH (0x80), Index Byte: SET_NUM_PAGES (0x02),
// Write Bytes: 0x00 - Number of pages at byte 0x44 from .msbl file.
uint8_t MAX32664_SetNumPages(MAX32664_Handle *handle, uint8_t totalPages) {

    uint8_t statusByte = MAX32664_WriteByteParameter(handle, BOOTLOADER_FLASH,
                                                     SET_NUM_PAGES, 0x00, totalPages);
    return statusByte;
}

// Family Byte: BOOTLOADER_FLASH (0x80), Index Byte: ERASE_FLASH (0x03)
// Returns true on successful communication.
uint8_t MAX32664_EraseFlash(MAX32664_Handle *handle) {

    // This is a unique write in that it does not have a relevant write byte.
    uint8_t buffer[2] = {BOOTLOADER_FLASH, ERASE_FLASH};
    HAL_I2C_Master_Transmit(handle->hi2c, handle->_address, buffer, 2,
                            HAL_MAX_DELAY);
    HAL_Delay(CMD_DELAY);

    /*
     _i2cPort->beginTransmission(_address);
     _i2cPort->write(BOOTLOADER_FLASH);
     _i2cPort->write(ERASE_FLASH);
     _i2cPort->endTransmission();
     delay(CMD_DELAY);
     */

    uint8_t statusByte[1] = {0xFF};
    HAL_I2C_Master_Receive(handle->hi2c, handle->_address, statusByte, 1,
                           HAL_MAX_DELAY);

    /*
     _i2cPort->requestFrom(_address, static_cast<uint8_t>(1));
     uint8_t statusByte = _i2cPort->read();
     */

    if (*statusByte == SB_SUCCESS)
        return 0x01;
    else
        return 0x00;
}

// Family Byte: BOOTLOADER_INFO (0x81), Index Byte: BOOTLOADER_VERS (0x00)
version MAX32664_ReadBootloaderVers(MAX32664_Handle *handle) {

    version booVers; // BOO!
    uint8_t wbuffer[2] = {BOOTLOADER_INFO, BOOTLOADER_VERS};
    HAL_I2C_Master_Transmit(handle->hi2c, WRITE_ADDRESS, wbuffer, 2,
                            HAL_MAX_DELAY);
    HAL_Delay(CMD_DELAY);

    /*
     _i2cPort->beginTransmission(_address);
     _i2cPort->write(BOOTLOADER_INFO);
     _i2cPort->write(BOOTLOADER_VERS);
     _i2cPort->endTransmission();
     delay(CMD_DELAY);
     */

    uint8_t buffer[4];
    HAL_I2C_Master_Receive(handle->hi2c, READ_ADDRESS, buffer, 4,
                           HAL_MAX_DELAY);

    uint8_t statusByte = buffer[0];

    /*
     _i2cPort->requestFrom(_address, static_cast<uint8_t>(4));
     uint8_t statusByte = _i2cPort->read();
     */
    if (statusByte != SB_SUCCESS) { // Pass through if SB_SUCCESS (0x00).
        booVers.major = 0;
        booVers.minor = 0;
        booVers.revision = 0;
        return booVers;
    }

    booVers.major = buffer[1];
    booVers.minor = buffer[2];
    booVers.revision = buffer[3];

    /*
     booVers.major = _i2cPort->read();
     booVers.minor = _i2cPort->read();
     booVers.revision = _i2cPort->read();
     */
    return booVers;
}

// Family Byte: IDENTITY (0xFF), Index Byte: READ_SENSOR_HUB_VERS (0x03)
version MAX32664_ReadSensorHubVersion(MAX32664_Handle *handle) {

    version bioHubVers;
    uint8_t wbuffer[2] = {IDENTITY, READ_SENSOR_HUB_VERS};
    HAL_I2C_Master_Transmit(handle->hi2c, handle->_address, wbuffer, 2,
                            HAL_MAX_DELAY);
    HAL_Delay(CMD_DELAY);

    /*
     _i2cPort->beginTransmission(_address);
     _i2cPort->write(IDENTITY);
     _i2cPort->write(READ_SENSOR_HUB_VERS);
     _i2cPort->endTransmission();
     delay(CMD_DELAY);
     */

    uint8_t buffer[4];
    HAL_I2C_Master_Receive(handle->hi2c, handle->_address, buffer, 4,
                           HAL_MAX_DELAY);
    uint8_t statusByte = buffer[0];

    /*
     _i2cPort->requestFrom(_address, static_cast<uint8_t>(4));
     uint8_t statusByte = _i2cPort->read();
     */
    if (statusByte) { // Pass through if SB_SUCCESS (0x00).
        bioHubVers.major = 0;
        bioHubVers.minor = 0;
        bioHubVers.revision = 0;
        return bioHubVers;
    }

    bioHubVers.major = buffer[1];
    bioHubVers.minor = buffer[2];
    bioHubVers.revision = buffer[3];
    /*
     bioHubVers.major = _i2cPort->read();
     bioHubVers.minor = _i2cPort->read();
     bioHubVers.revision = _i2cPort->read();
     */

    return bioHubVers;
}

// Family Byte: IDENTITY (0xFF), Index Byte: READ_ALGO_VERS (0x07)
version MAX32664_ReadAlgorithmVersion(MAX32664_Handle *handle) {

    version libAlgoVers;
    uint8_t wbuffer[2] = {IDENTITY, READ_ALGO_VERS};
    HAL_I2C_Master_Transmit(handle->hi2c, handle->_address, wbuffer, 2,
                            HAL_MAX_DELAY);
    HAL_Delay(CMD_DELAY);

    /*
     _i2cPort->beginTransmission(_address);
     _i2cPort->write(IDENTITY);
     _i2cPort->write(READ_ALGO_VERS);
     _i2cPort->endTransmission();
     delay(CMD_DELAY);
     */

    uint8_t buffer[4];
    HAL_I2C_Master_Receive(handle->hi2c, handle->_address, buffer, 4,
                           HAL_MAX_DELAY);
    uint8_t statusByte = buffer[0];

    /*
     _i2cPort->requestFrom(_address, static_cast<uint8_t>(4));
     uint8_t statusByte = _i2cPort->read();
     */
    if (statusByte) { // Pass through if SB_SUCCESS (0x00).
        libAlgoVers.major = 0;
        libAlgoVers.minor = 0;
        libAlgoVers.revision = 0;
        return libAlgoVers;
    }

    libAlgoVers.major = buffer[1];
    libAlgoVers.minor = buffer[2];
    libAlgoVers.revision = buffer[3];

    return libAlgoVers;
}

// ------------------Function Below for MAX32664 Version D (Blood Pressure) ----

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: BPT_MEDICATION (0x00)
uint8_t MAX32664_IsPatientBPMedicationValue(MAX32664_Handle *handle,
                                            uint8_t medication) {

    if ((medication != 0x00) || (medication != 0x01))
        return INCORR_PARAM;

    uint8_t status = MAX32664_WriteByteParameter(handle,
                                                 CHANGE_ALGORITHM_CONFIG, BPT_CONFIG, BPT_MEDICATION, medication);
    return status;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: BPT_MEDICATION (0x00)
uint8_t MAX32664_IsPatientBPMedication(MAX32664_Handle *handle) {

    uint8_t medication;
    MAX32664_ReadByteWrite(handle, CHANGE_ALGORITHM_CONFIG,
                           BPT_CONFIG, BPT_MEDICATION, &medication);
    return medication;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: SYSTOLIC_VALUE (0x01)
uint8_t MAX32664_WriteSystolicVals(MAX32664_Handle *handle, uint8_t sysVal1,
                                   uint8_t sysVal2, uint8_t sysVal3) {

    const size_t numSysVals = 3;
    uint8_t sysVals[3] = {sysVal1, sysVal2, sysVal3};
    uint8_t status = MAX32664_WriteBytes(handle, CHANGE_ALGORITHM_CONFIG,
                                         BPT_CONFIG, SYSTOLIC_VALUE, sysVals, numSysVals);

    return status;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: SYSTOLIC_VALUE (0x01)
uint8_t MAX32664_ReadSystolicVals(MAX32664_Handle *handle, uint8_t userArray[]) {

    const size_t numSysVals = 3;
    uint8_t status = MAX32664_ReadMultipleBytes(handle, CHANGE_ALGORITHM_CONFIG,
                                                BPT_CONFIG, SYSTOLIC_VALUE, numSysVals, userArray);

    return status;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: DIASTOLIC_VALUE (0x02)
uint8_t MAX32664_WriteDiastolicVals(MAX32664_Handle *handle, uint8_t diasVal1,
                                    uint8_t diasVal2, uint8_t diasVal3) {

    const size_t numDiasVals = 3;
    uint8_t diasVals[3] = {diasVal1, diasVal2, diasVal3};
    uint8_t status = MAX32664_WriteBytes(handle, CHANGE_ALGORITHM_CONFIG,
                                         BPT_CONFIG, DIASTOLIC_VALUE, diasVals, numDiasVals);

    return status;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: DIASTOLIC_VALUE (0x02)
uint8_t MAX32664_ReadDiastolicVals(MAX32664_Handle *handle, uint8_t userArray[]) {

    const size_t numDiasVals = 3;
    uint8_t status = MAX32664_ReadMultipleBytes(handle, CHANGE_ALGORITHM_CONFIG,
                                                BPT_CONFIG, DIASTOLIC_VALUE, numDiasVals, userArray);
    return status;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: BPT_CALIB_DATA (0x03)
uint8_t MAX32664_WriteBPTAlgoData(MAX32664_Handle *handle,
                                  uint8_t bptCalibData[]) {

    const size_t numCalibVals = 824;
    uint8_t status = MAX32664_WriteBytes(handle, CHANGE_ALGORITHM_CONFIG,
                                         BPT_CONFIG, BPT_CALIB_DATA, bptCalibData, numCalibVals);
    return status;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: BPT_CALIB_DATA (0x03)
uint8_t MAX32664_ReadBPTAlgoData(MAX32664_Handle *handle, uint8_t userArray[]) {

    const size_t numCalibVals = 824;
    uint8_t status = MAX32664_ReadMultipleBytes(handle, CHANGE_ALGORITHM_CONFIG,
                                                BPT_CONFIG, BPT_CALIB_DATA, numCalibVals, userArray);
    return status;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: PATIENT_RESTING (0x05)
uint8_t MAX32664_IsPatientRestingValue(MAX32664_Handle *handle, uint8_t resting) { //

    if ((resting != 0x00) || (resting != 0x01))
        return INCORR_PARAM;

    uint8_t status = MAX32664_WriteByteParameter(handle,
                                                 CHANGE_ALGORITHM_CONFIG, BPT_CONFIG, PATIENT_RESTING, resting);
    return status;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: PATIENT_RESTING (0x05)
uint8_t MAX32664_IsPatientResting(MAX32664_Handle *handle) {

    uint8_t resting = MAX32664_WriteByte(handle, CHANGE_ALGORITHM_CONFIG,
                                         BPT_CONFIG, PATIENT_RESTING);
    return resting;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: AGC_SP02_COEFS (0x0B)
uint8_t MAX32664_WriteSP02AlgoCoef(MAX32664_Handle *handle, int32_t intA,
                                   int32_t intB, int32_t intC) {

    const size_t numCoefVals = 3;
    int32_t coefVals[3] = {intA, intB, intC};
    uint8_t status = MAX32664_WriteLongBytes(handle, CHANGE_ALGORITHM_CONFIG,
                                             BPT_CONFIG, AGC_SP02_COEFS, coefVals, numCoefVals);
    return status;
}

// Family Byte: CHANGE_ALGORITHM_CONFIG (0x50), Index Byte: BPT_CONFIG (0x04),
// Write Byte: AGC_SP02_COEFS (0x0B)
uint8_t MAX32664_ReadSP02AlgoCoef(MAX32664_Handle *handle, int32_t userArray[]) { // Have the user provide their own array here and pass the pointer to it

    const size_t numOfReads = 3;
    uint8_t status = MAX32664_ReadMultipleBytes32(handle,
                                                  CHANGE_ALGORITHM_CONFIG, BPT_CONFIG, AGC_SP02_COEFS, numOfReads,
                                                  userArray);
    return status;
}

//-------------------Private Functions-----------------------

// This function uses the given family, index, and write byte to enable
// the given sensor.
uint8_t MAX32664_EnableWrite(MAX32664_Handle *handle, uint8_t _familyByte,
                             uint8_t _indexByte, uint8_t _enableByte) {
    uint8_t wbuffer[3] = {_familyByte, _indexByte, _enableByte};
    HAL_I2C_Master_Transmit(handle->hi2c, WRITE_ADDRESS, wbuffer, 3, HAL_MAX_DELAY);
    HAL_Delay(ENABLE_CMD_DELAY);

    /*
     _i2cPort->beginTransmission(_address);
     _i2cPort->write(_familyByte);
     _i2cPort->write(_indexByte);
     _i2cPort->write(_enableByte);
     _i2cPort->endTransmission();
     delay(ENABLE_CMD_DELAY);
     */

    // Status Byte, success or no? 0x00 is a successful transmit
    uint8_t buffer[1];
    HAL_I2C_Master_Receive(handle->hi2c, READ_ADDRESS, buffer, 1, HAL_MAX_DELAY);
    /*
     _i2cPort->requestFrom(_address, static_cast<uint8_t>(1));
     uint8_t statusByte = _i2cPort->read();
     */
    return buffer[0];
}

// This function uses the given family, index, and write byte to communicate
// with the MAX32664 which in turn communicates with downward sensors. There
// are two steps demonstrated in this function. First a write to the MCU
// indicating what you want to do, a delay, and then a read to confirm positive
// transmission.
uint8_t MAX32664_WriteByte(MAX32664_Handle *handle, uint8_t _familyByte,
                           uint8_t _indexByte, uint8_t _writeByte) {

    uint8_t wbuffer[3] = {_familyByte, _indexByte, _writeByte};
    HAL_I2C_Master_Transmit(handle->hi2c, WRITE_ADDRESS, wbuffer, 3, HAL_MAX_DELAY);
    HAL_Delay(CMD_DELAY);

    // Status Byte, success or no? 0x00 is a successful transmit
    uint8_t buffer[1];
    HAL_I2C_Master_Receive(handle->hi2c, READ_ADDRESS, buffer,
                           1, HAL_MAX_DELAY);
    return buffer[0];
}

// This function is the same as the function above and uses the given family,
// index, and write byte, but also takes a 16 bit integer as a paramter to communicate
// with the MAX32664 which in turn communicates with downward sensors. There
// are two steps demonstrated in this function. First a write to the MCU
// indicating what you want to do, a delay, and then a read to confirm positive
// transmission.
uint8_t MAX32664_WriteByteValue16(MAX32664_Handle *handle, uint8_t _familyByte,
                                  uint8_t _indexByte, uint8_t _writeByte, uint16_t _val) {
    uint8_t buffer[5] =
        {_familyByte, _indexByte, _writeByte, (_val >> 8), _val};
    HAL_I2C_Master_Transmit(handle->hi2c, handle->_address, buffer, 5,
                            HAL_MAX_DELAY);
    HAL_Delay(CMD_DELAY);

    uint8_t statusByte[1] = {0xFF};
    HAL_I2C_Master_Receive(handle->hi2c, handle->_address, statusByte, 1,
                           HAL_MAX_DELAY);
    return *statusByte;
}

// This function sends information to the MAX32664 to specifically write values
// to the registers of downward sensors and so also requires a
// register address and register value as parameters. Again there is the write
// of the specific bytes followed by a read to confirm positive transmission.
uint8_t MAX32664_WriteByteValue(MAX32664_Handle *handle, uint8_t _familyByte,
                                uint8_t _indexByte, uint8_t _writeByte, uint8_t _writeVal) {

    uint8_t buffer[4] = {_familyByte, _indexByte, _writeByte, _writeVal};
    HAL_I2C_Master_Transmit(handle->hi2c, handle->_address, buffer, 4,
                            HAL_MAX_DELAY);

    // Status Byte, 0x00 is a successful transmit.
    uint8_t statusByte[1] = {0xFF};
    HAL_I2C_Master_Receive(handle->hi2c, handle->_address, statusByte, 1,
                           HAL_MAX_DELAY);
    return *statusByte;
}

// This function sends information to the MAX32664 to specifically write values
// to the registers of downward sensors and so also requires a
// register address and register value as parameters. Again there is the write
// of the specific bytes followed by a read to confirm positive transmission.
uint8_t MAX32664_WriteLongBytes(MAX32664_Handle *handle, uint8_t _familyByte,
                                uint8_t _indexByte, uint8_t _writeByte, int32_t _writeVal[],
                                const size_t _size) {
    size_t bufSize = 3 + sizeof(int32_t) * _size;
    uint8_t *buffer = (uint8_t *)malloc(bufSize);
    buffer[0] = _familyByte;
    buffer[1] = _indexByte;
    buffer[2] = _writeByte;

    for (size_t i = 0; i < _size; i++) {
        buffer[sizeof(int32_t) * i + 3] = (_writeVal[i] >> 24);
        buffer[sizeof(int32_t) * i + 4] = (_writeVal[i] >> 16);
        buffer[sizeof(int32_t) * i + 5] = (_writeVal[i] >> 8);
        buffer[sizeof(int32_t) * i + 6] = _writeVal[i];
    }

    HAL_I2C_Master_Transmit(handle->hi2c, handle->_address, buffer, bufSize,
                            HAL_MAX_DELAY);
    HAL_Delay(CMD_DELAY);

    // Status Byte, 0x00 is a successful transmit.
    free(buffer);
    uint8_t statusByte[1] = {0xFF};
    HAL_I2C_Master_Receive(handle->hi2c, handle->_address, statusByte, 1,
                           HAL_MAX_DELAY);
    return *statusByte;
}

// This function sends information to the MAX32664 to specifically write values
// to the registers of downward sensors and so also requires a
// register address and register value as parameters. Again there is the write
// of the specific bytes followed by a read to confirm positive transmission.
uint8_t MAX32664_WriteBytes(MAX32664_Handle *handle, uint8_t _familyByte,
                            uint8_t _indexByte, uint8_t _writeByte, uint8_t _writeVal[],
                            size_t _size) {

    size_t bufSize = 3 + _size;
    uint8_t *buffer = (uint8_t *)malloc(bufSize);
    buffer[0] = _familyByte;
    buffer[1] = _indexByte;
    buffer[2] = _writeByte;

    for (size_t i = 0; i < _size; i++) {
        buffer[3 + i] = _writeVal[i];
    }

    HAL_I2C_Master_Transmit(handle->hi2c, handle->_address, buffer, 3 + _size,
                            HAL_MAX_DELAY);
    HAL_Delay(CMD_DELAY);

    // Status Byte, 0x00 is a successful transmit.
    free(buffer);
    uint8_t statusByte[1] = {0xFF};
    HAL_I2C_Master_Receive(handle->hi2c, handle->_address, statusByte, 1,
                           HAL_MAX_DELAY);
    return *statusByte;
}
// This function handles all read commands or stated another way, all information
// requests. It starts a request by writing the family byte an index byte, and
// then delays 60 microseconds, during which the MAX32664 retrieves the requested
// information. An I-squared-C request is then issued, and the information is read.
uint8_t MAX32664_ReadByte(MAX32664_Handle *handle, uint8_t _familyByte, uint8_t _indexByte, uint8_t *dest) {

    uint8_t returnByte;
    uint8_t statusByte;

    uint8_t wbuffer[2] = {_familyByte, _indexByte};
    HAL_I2C_Master_Transmit(handle->hi2c, WRITE_ADDRESS, wbuffer, 2, HAL_MAX_DELAY);

    HAL_Delay(CMD_DELAY);

    uint8_t buffer[2] = {0x0A, 0x0B};
    HAL_I2C_Master_Receive(handle->hi2c, READ_ADDRESS, buffer, 2, HAL_MAX_DELAY);
    statusByte = buffer[0];

    *dest = buffer[1];
    return statusByte;
}

// This function is exactly as the one above except it accepts also receives a
// Write Byte as a parameter. It starts a request by writing the family byte, index byte, and
// write byte to the MAX32664 and then delays 60 microseconds, during which
// the MAX32664 retrieves the requested information. A I-squared-C request is
// then issued, and the information is read.
uint8_t MAX32664_ReadByteWrite(MAX32664_Handle *handle, uint8_t _familyByte,
                               uint8_t _indexByte, uint8_t _writeByte, uint8_t *dest) {

    uint8_t returnByte;
    uint8_t statusByte;

    uint8_t wbuffer[3] = {_familyByte, _indexByte, _writeByte};
    HAL_I2C_Master_Transmit(handle->hi2c, WRITE_ADDRESS, wbuffer, 3,
                            HAL_MAX_DELAY);

    HAL_Delay(CMD_DELAY);

    uint8_t buffer[2];
    HAL_I2C_Master_Receive(handle->hi2c, READ_ADDRESS, buffer, 2,
                           HAL_MAX_DELAY);
    statusByte = buffer[0];
    *dest = buffer[1];
    return statusByte;
}

uint8_t MAX32664_ReadFillArray(MAX32664_Handle *handle, uint8_t _familyByte,
                               uint8_t _indexByte, uint8_t _numOfReads, uint8_t array[]) {

    uint8_t statusByte;

    uint8_t wbuffer[2] = {_familyByte, _indexByte};
    HAL_I2C_Master_Transmit(handle->hi2c, WRITE_ADDRESS, wbuffer, 2,
                            HAL_MAX_DELAY);
    HAL_Delay(CMD_DELAY);

    HAL_I2C_Master_Receive(handle->hi2c, READ_ADDRESS, handle->readsBuffer,
                           _numOfReads + 1, HAL_MAX_DELAY);
    statusByte = handle->readsBuffer[0];
    if (statusByte == SB_SUCCESS) {
        for (size_t i = 0; i < _numOfReads; i++) {
            array[i] = 0;
        }
        return statusByte;
    }

    for (size_t i = 0; i < _numOfReads; i++) {
        array[i] = handle->readsBuffer[i + 1];
    }

    return statusByte;
}
// This function handles all read commands or stated another way, all information
// requests. It starts a request by writing the family byte, an index byte, and
// a write byte and then then delays 60 microseconds, during which the MAX32664
// retrieves the requested information. An I-squared-C request is then issued,
// and the information is read. This differs from the above read commands in
// that it returns a 16 bit integer instead of a single byte.
uint16_t MAX32664_ReadIntByte(MAX32664_Handle *handle, uint8_t _familyByte,
                              uint8_t _indexByte, uint8_t _writeByte, uint8_t *dest) {

    uint16_t returnByte;
    uint8_t statusByte;

    uint8_t wbuffer[3] = {_familyByte, _indexByte, _writeByte};
    HAL_I2C_Master_Transmit(handle->hi2c, handle->_address, wbuffer, 3,
                            HAL_MAX_DELAY);
    HAL_Delay(CMD_DELAY);

    uint8_t buffer[3];
    HAL_I2C_Master_Receive(handle->hi2c, handle->_address, buffer, 3,
                           HAL_MAX_DELAY);
    statusByte = buffer[0];
    returnByte = (buffer[1] << 8);
    returnByte |= buffer[2];
    *dest = returnByte;
    return statusByte;
}

// This function handles all read commands or stated another way, all information
// requests. It starts a request by writing the family byte, an index byte, and
// a write byte and then then delays 60 microseconds, during which the MAX32664
// retrieves the requested information. An I-squared-C request is then issued,
// and the information is read. This function is very similar to the one above
// except it returns three uint32_t bytes instead of one.
uint8_t MAX32664_ReadMultipleBytes32(MAX32664_Handle *handle,
                                     uint8_t _familyByte, uint8_t _indexByte, uint8_t _writeByte,
                                     const size_t _numOfReads, int32_t userArray[]) {

    uint8_t statusByte;

    uint8_t wbuffer[3] = {_familyByte, _indexByte, _writeByte};
    HAL_I2C_Master_Transmit(handle->hi2c, handle->_address, wbuffer, 3,
                            HAL_MAX_DELAY);

    HAL_Delay(CMD_DELAY);

    HAL_I2C_Master_Receive(handle->hi2c, handle->_address, handle->readsBuffer,
                           sizeof(int32_t) * _numOfReads + 1, HAL_MAX_DELAY);
    statusByte = handle->readsBuffer[0];
    if (statusByte == SB_SUCCESS) {
        for (size_t i = 0; i < _numOfReads; i++) {
            userArray[i] = handle->readsBuffer[sizeof(int32_t) * i + 1] << 24;
            userArray[i] |= handle->readsBuffer[sizeof(int32_t) * i + 2] << 16;
            userArray[i] |= handle->readsBuffer[sizeof(int32_t) * i + 3] << 8;
            userArray[i] |= handle->readsBuffer[sizeof(int32_t) * i + 4];
        }
    }
    return statusByte;
}

// This function handles all read commands or stated another way, all information
// requests. It starts a request by writing the family byte, an index byte, and
// a write byte and then then delays 60 microseconds, during which the MAX32664
// retrieves the requested information. An I-squared-C request is then issued,
// and the information is read. This function is very similar to the one above
// except it returns three uint32_t bytes instead of one.
uint8_t MAX32664_ReadMultipleBytes(MAX32664_Handle *handle, uint8_t _familyByte,
                                   uint8_t _indexByte, uint8_t _writeByte, const size_t _numOfReads,
                                   uint8_t userArray[]) {

    uint8_t statusByte;

    uint8_t wbuffer[3] = {_familyByte, _indexByte, _writeByte};
    HAL_I2C_Master_Transmit(handle->hi2c, handle->_address, wbuffer, 3,
                            HAL_MAX_DELAY);

    HAL_Delay(CMD_DELAY);
    HAL_I2C_Master_Receive(handle->hi2c, handle->_address, handle->readsBuffer,
                           _numOfReads + 1, HAL_MAX_DELAY);
    statusByte = handle->readsBuffer[0];
    if (statusByte == SB_SUCCESS) {
        for (size_t i = 0; i < _numOfReads; i++) {
            userArray[i] = handle->readsBuffer[i + 1];
        }
    }
    return statusByte;
}
