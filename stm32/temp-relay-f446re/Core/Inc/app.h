#include "main.h"
#include "cmsis_os.h"

extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;

void App_StartDefaultTask(void);