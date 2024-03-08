#pragma once 

#include "nvs_flash.h"
#include "eth.h"
#include "lwip/ip4_addr.h"
#include "protocol_examples_common.h"
#include "mbcontroller.h"

esp_err_t mdb_init_services(void);

esp_err_t mdb_slave_init();

esp_err_t mdb_slave_destroy(void);

esp_err_t mdb_destroy_services(void);

typedef struct {
    uint16_t MAX_WELD_TIME;
    uint16_t CURR_RIS_TIME;
    uint16_t PLS_DUR;
    uint16_t INT_BTW_PULSES;
    uint16_t NUM_PUL_CYC;
    uint16_t reg5;
    uint16_t reg6;
    uint16_t reg7;
    uint16_t reg8;
    uint16_t reg9;
} holding_reg_area_t;

extern holding_reg_area_t holding_reg_area;

typedef struct {
    uint16_t reg0;
    uint16_t reg1;
    uint16_t reg2;
    uint16_t reg3;
    uint16_t reg4;
    uint16_t reg5;
    uint16_t reg6;
    uint16_t reg7;
    uint16_t reg8;
    uint16_t reg9;
} input_reg_area_t;

typedef struct {
    uint8_t input0:1;
    uint8_t input1:1;
    uint8_t input2:1;
    uint8_t input3:1;
    uint8_t input4:1;
    uint8_t input5:1;
    uint8_t input6:1;
    uint8_t input7:1;
    uint8_t input8:1;
    uint8_t input9:1;
} discrete_reg_area_t;

typedef struct
{
    uint8_t coils_port0;
    uint8_t coils_port1;
} coil_reg_params_t;

void mdb_run(void);

void mdb_init_registers(void);