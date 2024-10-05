#include "cmsis_os.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

void App_StartInitTask(void *);
void App_StartCommsTask(void *);
void App_StartInfoTask(void *);
void App_StartMonitorTask(void *);