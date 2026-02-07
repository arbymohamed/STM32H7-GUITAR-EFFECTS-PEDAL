/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "GT911.h"
#include "main.h"

/* Private variables ---------------------------------------------------------*/
static uint8_t GT911_Config[] = {
		0x61,0x40,0x01,0xE0,0x01,0x05,0x35,0x00,0x02,0x08,0x1E,0x08,0x50,0x3C,0x0F,0x05,0x00,0x00,0x00,0x00,0x50,0x00,0x00,0x18,0x1A,0x1E,0x14,0x87,0x27,0x0A,0x4B,0x4D,0xD3,0x07,0x00,0x00,0x00,0x02,0x32,0x1D,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x32,0x00,0x00,0x2A,0x32,0x64,0x94,0xD5,0x02,0x07,0x00,0x00,0x04,0xA5,0x35,0x00,0x91,0x3D,0x00,0x80,0x46,0x00,0x70,0x51,0x00,0x63,0x5D,0x00,0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x14,0x12,0x10,0x0E,0x0C,0x0A,0x08,0x06,0x04,0x02,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x24,0x22,0x21,0x20,0x1F,0x1E,0x1D,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x21,0x01
};

static GT911_Status_t CommunicationResult;
static uint8_t TxBuffer[300];
static uint8_t RxBuffer[300];

/* Private function prototypes -----------------------------------------------*/
static void GT911_Reset(void);
static void GT911_ResetAlt(void);
static void GT911_CalculateCheckSum(void);
static GT911_Status_t GT911_SetCommandRegister(uint8_t command);
static GT911_Status_t GT911_GetProductID(uint32_t* id);
static GT911_Status_t GT911_SendConfig(void);
static GT911_Status_t GT911_GetStatus(uint8_t* status);
static GT911_Status_t GT911_SetStatus(uint8_t status);

/* API Implementation --------------------------------------------------------*/
GT911_Status_t GT911_Init(GT911_Config_t config){
    GT911_Reset();
    GT911_Delay(100);

    uint32_t productID = 0;
    CommunicationResult = GT911_GetProductID(&productID);
    if(CommunicationResult != GT911_OK){
        return CommunicationResult;
    }
    if(productID == 0){
        return GT911_NotResponse;
    }

    GT911_Reset();
    GT911_Delay(100);

    // Send config with retry logic
    CommunicationResult = GT911_SendConfig();
    if(CommunicationResult != GT911_OK){
        GT911_Delay(50);
        CommunicationResult = GT911_SendConfig();
        if(CommunicationResult != GT911_OK){
            GT911_ResetAlt();
            GT911_Delay(100);
            CommunicationResult = GT911_SendConfig();
            if(CommunicationResult != GT911_OK){
                return CommunicationResult;
            }
        }
    }

    GT911_Delay(50);

    GT911_SetCommandRegister(0x00);

    GT911_SetStatus(0);

    return GT911_OK;
}

GT911_Status_t GT911_ReadTouch(TouchCordinate_t *cordinate, uint8_t *number_of_cordinate) {
    uint8_t StatusRegister;
    GT911_Status_t Result = GT911_NotResponse;

    Result = GT911_GetStatus(&StatusRegister);
    if (Result != GT911_OK) {
        return Result;
    }

    if ((StatusRegister & 0x80) != 0) {
        *number_of_cordinate = StatusRegister & 0x0F;
        if (*number_of_cordinate != 0) {
            for (uint8_t i = 0; i < *number_of_cordinate; i++) {
                 TxBuffer[0] = ((GOODIX_POINT1_X_ADDR + (i* 8)) & 0xFF00) >> 8;
                 TxBuffer[1] = (GOODIX_POINT1_X_ADDR + (i* 8)) & 0xFF;
                 GT911_I2C_Write(GOODIX_ADDRESS, TxBuffer, 2);
                 GT911_I2C_Read(GOODIX_ADDRESS, RxBuffer, 6);
                 cordinate[i].x = RxBuffer[0];
                 cordinate[i].x = (RxBuffer[1] << 8) + cordinate[i].x;
                 cordinate[i].y = RxBuffer[2];
                 cordinate[i].y = (RxBuffer[3] << 8) + cordinate[i].y;
             }
         }
         GT911_SetStatus(0);
     }
     return GT911_OK;
}

//Private functions Implementation
static void GT911_Reset(void){
	GT911_INT_Output();
	GT911_INT_Control(0);
	GT911_Delay(10);

	GT911_RST_Control(0);
	GT911_Delay(20);

	GT911_RST_Control(1);
	GT911_Delay(50);

	GT911_INT_Input();
	GT911_Delay(50);
}

static void GT911_ResetAlt(void){
	GT911_INT_Output();
	GT911_INT_Control(1);
	GT911_Delay(5);

	GT911_RST_Control(0);
	GT911_Delay(20);

	GT911_RST_Control(1);
	GT911_Delay(50);

	GT911_INT_Input();
	GT911_Delay(50);
}

static void GT911_CalculateCheckSum(void){
	GT911_Config[184] = 0;
	for(uint8_t i = 0 ; i < 184 ; i++){
		GT911_Config[184] += GT911_Config[i];
	}
	GT911_Config[184] = (~GT911_Config[184]) + 1;
}

static GT911_Status_t GT911_SetCommandRegister(uint8_t command){
	TxBuffer[0] = (GOODIX_REG_COMMAND & 0xFF00) >> 8;
	TxBuffer[1] = GOODIX_REG_COMMAND & 0xFF;
	TxBuffer[2] = command;
	return GT911_I2C_Write(GOODIX_ADDRESS, TxBuffer, 3);
}

static GT911_Status_t GT911_GetProductID(uint32_t* id){
    TxBuffer[0] = (GOODIX_REG_ID & 0xFF00) >> 8;
    TxBuffer[1] = GOODIX_REG_ID & 0xFF;
    GT911_Status_t Result = GT911_NotResponse;
    Result = GT911_I2C_Write(GOODIX_ADDRESS, TxBuffer, 2);
    if(Result == GT911_OK){
        Result = GT911_I2C_Read(GOODIX_ADDRESS, RxBuffer, 4);
        if(Result == GT911_OK){
            memcpy(id, RxBuffer, 4);
        }
    }
    return Result;
}

static GT911_Status_t GT911_SendConfig(void){
    GT911_CalculateCheckSum();

    GT911_INT_Output();
    GT911_INT_Control(0);
    GT911_Delay(5);

    const size_t cfg_len = sizeof(GT911_Config);
    const size_t chunk = 32;
    for(size_t offset = 0; offset < cfg_len; offset += chunk){
        size_t to_write = (cfg_len - offset) > chunk ? chunk : (cfg_len - offset);
        uint16_t reg = GOODIX_REG_CONFIG_DATA + (uint16_t)offset;
        TxBuffer[0] = (reg & 0xFF00) >> 8;
        TxBuffer[1] = reg & 0xFF;
        memcpy(&TxBuffer[2], &GT911_Config[offset], to_write);
        GT911_Status_t Result = GT911_I2C_Write(GOODIX_ADDRESS, TxBuffer, (uint16_t)(to_write + 2));
        if(Result != GT911_OK){
            GT911_INT_Input();
            return Result;
        }
        GT911_Delay(5);
    }

    TxBuffer[0] = (GOODIX_REG_CONFIG_FRESH & 0xFF00) >> 8;
    TxBuffer[1] = GOODIX_REG_CONFIG_FRESH & 0xFF;
    TxBuffer[2] = 0x01;
    if(GT911_I2C_Write(GOODIX_ADDRESS, TxBuffer, 3) != GT911_OK){
        GT911_INT_Input();
        return GT911_NotResponse;
    }
    GT911_Delay(10);

    GT911_INT_Input();
    GT911_Delay(10);

    GT911_SetCommandRegister(GOODIX_CMD_SOFTRESET);
    GT911_Delay(100);

    GT911_INT_Input();
    GT911_Delay(100);

    TxBuffer[0] = (GOODIX_REG_CONFIG_DATA & 0xFF00) >> 8;
    TxBuffer[1] = GOODIX_REG_CONFIG_DATA & 0xFF;
    if(GT911_I2C_Write(GOODIX_ADDRESS, TxBuffer, 2) != GT911_OK) return GT911_NotResponse;
    if(GT911_I2C_Read(GOODIX_ADDRESS, RxBuffer, cfg_len) != GT911_OK) return GT911_NotResponse;
    for(size_t i = 0; i < cfg_len; ++i){
        if(RxBuffer[i] != GT911_Config[i]) return GT911_NotResponse;
    }
    return GT911_OK;
}

static GT911_Status_t GT911_GetStatus(uint8_t* status){
	TxBuffer[0] = (GOODIX_READ_COORD_ADDR & 0xFF00) >> 8;
	TxBuffer[1] = GOODIX_READ_COORD_ADDR & 0xFF;
	GT911_Status_t Result = GT911_NotResponse;
	Result = GT911_I2C_Write(GOODIX_ADDRESS, TxBuffer, 2);
	if(Result == GT911_OK){
		Result = GT911_I2C_Read(GOODIX_ADDRESS, RxBuffer, 1);
		if( Result == GT911_OK){
			*status = RxBuffer[0];
		}
	}
	return Result;
}

static GT911_Status_t GT911_SetStatus(uint8_t status){
	TxBuffer[0] = (GOODIX_READ_COORD_ADDR & 0xFF00) >> 8;
	TxBuffer[1] = GOODIX_READ_COORD_ADDR & 0xFF;
	TxBuffer[2] = status;
	return GT911_I2C_Write(GOODIX_ADDRESS, TxBuffer, 3);
}
