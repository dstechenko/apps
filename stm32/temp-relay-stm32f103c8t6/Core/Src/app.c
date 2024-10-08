#include "app.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static uint16_t hits;

#define UART_DBG huart1
#define UART_BT huart2
#define I2C_TMP hi2c1

static const uint8_t TMP102_ADDR_READ = 0x48 << 1;
static const uint8_t TMP102_ADDR_WRITE = TMP102_ADDR_READ | 0x01;
static const uint8_t TMP102_REG_TCUR = 0x00;
static const uint8_t TMP102_REG_CONF = 0x01;
static const uint8_t TMP102_REG_TLOW = 0x02;
static const uint8_t TMP102_REG_THIGH = 0x03;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == ALT_Pin) {
    hits++;
  }
}

static float ConvertToTemp(uint8_t msb, uint8_t lsb) {
  int16_t val = ((int16_t)msb << 4) | (lsb >> 4);
  if (val > 0x7FF) {
    val |= 0xF000;
  }
  return (val * 0.0625) * 100;
}

static uint16_t ConvertFromTemp(float temp) {
  int16_t val = (int16_t)(temp / 0.0625);
  if (val < 0) {
    val &= 0x0FFF;
  }
  return val;
}

static void ReadTemperatureFrom(const uint8_t reg, uint8_t buf[20]) {
  HAL_StatusTypeDef ret;
  float temp;

  buf[0] = reg;
  ret =
      HAL_I2C_Master_Transmit(&hi2c1, TMP102_ADDR_READ, buf, 1, HAL_MAX_DELAY);
  if (ret != HAL_OK) {
    sprintf((char *)buf, "Error Tx\r\n");
    return;
  }

  ret = HAL_I2C_Master_Receive(&hi2c1, TMP102_ADDR_READ, buf, 2, HAL_MAX_DELAY);
  if (ret != HAL_OK) {
    sprintf((char *)buf, "Error Rx\r\n");
    return;
  }

  temp = ConvertToTemp(buf[0], buf[1]);
  sprintf((char *)buf, "%u.%02u C\r\n", ((unsigned int)temp / 100),
          ((unsigned int)temp % 100));
}

static void WriteTemperatureTo(const uint8_t reg, const float temp,
                               uint8_t buf[20]) {
  HAL_StatusTypeDef ret;
  int16_t val = ConvertFromTemp(temp);

  buf[0] = reg;
  buf[1] = (uint8_t)(val >> 4);
  buf[2] = (uint8_t)(val << 4);

  ret =
      HAL_I2C_Master_Transmit(&hi2c1, TMP102_ADDR_WRITE, buf, 3, HAL_MAX_DELAY);
  if (ret != HAL_OK) {
    return;
  }
}

static uint16_t ReadFromConfig(void) {
  HAL_StatusTypeDef ret;
  uint8_t buf[2];

  buf[0] = TMP102_REG_CONF;
  ret =
      HAL_I2C_Master_Transmit(&hi2c1, TMP102_ADDR_READ, buf, 1, HAL_MAX_DELAY);
  if (ret != HAL_OK) {
    return 0;
  }

  ret = HAL_I2C_Master_Receive(&hi2c1, TMP102_ADDR_READ, buf, 2, HAL_MAX_DELAY);
  if (ret != HAL_OK) {
    return 0;
  }

  return (buf[0] << 8) | buf[1];
}

static void WriteToConfig(uint16_t val) {
  HAL_StatusTypeDef ret;
  uint8_t buf[3];

  buf[0] = TMP102_REG_CONF;
  buf[1] = (uint8_t)(val >> 8);
  buf[2] = (uint8_t)(val & 0xFF);

  ret =
      HAL_I2C_Master_Transmit(&hi2c1, TMP102_ADDR_WRITE, buf, 3, HAL_MAX_DELAY);
  if (ret != HAL_OK) {
    return;
  }
}

void App_StartDefaultTask(void) {
  uint8_t buf[50];

  osDelay(1000);

  WriteTemperatureTo(TMP102_REG_TLOW, 29.0, buf);
  WriteTemperatureTo(TMP102_REG_THIGH, 31.0, buf);

  strcpy((char *)buf, "Low temp: ");
  HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);
  ReadTemperatureFrom(TMP102_REG_TLOW, buf);
  HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);

  strcpy((char *)buf, "High temp: ");
  HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);
  ReadTemperatureFrom(TMP102_REG_THIGH, buf);
  HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);

  strcpy((char *)buf, "Curr temp: ");
  HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);
  ReadTemperatureFrom(TMP102_REG_TCUR, buf);
  HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);

  uint16_t conf = ReadFromConfig();

  strcpy((char *)buf, "Config: ");
  HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);
  sprintf((char *)buf, "0x%x\r\n", conf);
  HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);

  WriteToConfig(conf | (1 << 10));
  conf = ReadFromConfig();

  strcpy((char *)buf, "Config: ");
  HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);
  sprintf((char *)buf, "0x%x\r\n", conf);
  HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);

  while (true) {
    if (hits) {
      hits = 0;
      strcpy((char *)buf, "Alert: ");
    } else {
      strcpy((char *)buf, "Curr temp: ");
    }
    HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);
    ReadTemperatureFrom(TMP102_REG_TCUR, buf);
    HAL_UART_Transmit(&huart1, buf, strlen((char *)buf), HAL_MAX_DELAY);
    osDelay(500);
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
  }
}

void App_StartInitTask(void *) {
  while (true) {
    osThreadYield();
  }
}

void App_StartCommsTask(void *) {
  while (true) {
    osThreadYield();
  }
}

void App_StartInfoTask(void *) {
  osDelay(5000);
  while (true) {
    osDelay(500);
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
  }
}

void App_StartMonitorTask(void *) {
  while (true) {
    osThreadYield();
  }
}