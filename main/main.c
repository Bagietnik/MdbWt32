#include "main.h"

SemaphoreHandle_t modbusSemaphore;

void PLS_DUR_Task(void *params)
{
    const TickType_t xDelay = pdMS_TO_TICKS(1000); // 1 sec delay

    while (1) {
        if (xSemaphoreTake(modbusSemaphore, portMAX_DELAY) == pdTRUE) {

            holding_reg_area.CURR_RIS_TIME += 1;
            xSemaphoreGive(modbusSemaphore);
            vTaskDelay(xDelay);
        }
    }
}

void MAX_WELD_TIME_Task(void *params)
{

    const TickType_t xDelay = pdMS_TO_TICKS(1000); // 1 sec delay

    while (1) {
        if (xSemaphoreTake(modbusSemaphore, portMAX_DELAY) == pdTRUE) {

            holding_reg_area.PLS_DUR += 1;
            printf("BAGIETA: %d", holding_reg_area.PLS_DUR);
            xSemaphoreGive(modbusSemaphore);
            vTaskDelay(xDelay);
        }
    }
}

void app_main(void)
{   
    
    mdb_run();

    modbusSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(modbusSemaphore);

    if (modbusSemaphore != NULL) { 
        
        xTaskCreate(MAX_WELD_TIME_Task, "Modbus Task", 2048, NULL, 1, NULL); 

        xTaskCreate(PLS_DUR_Task, "MaxWeldTask", 2048, NULL, 1, NULL);
    }
}