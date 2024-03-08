/*
 * SPDX-FileCopyrightText: 2016-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// FreeModbus Slave Example ESP32
#include "mdb.h"


/************************************************************************************************************************************/

#define MB_TCP_PORT_NUMBER      (CONFIG_FMB_TCP_PORT_DEFAULT)

#define MB_REG_HOLDING_START_AREA0          (0)
#define MB_REG_INPUT_START_AREA0            (0)
#define MB_REG_COILS_START                  (0x0000)
#define MB_REG_DISCRETE_INPUT_START         (0x0000)

#define MB_PAR_INFO_GET_TOUT                (10)
#define MB_CHAN_DATA_MAX_VAL                (10)
#define MB_CHAN_DATA_OFFSET                 (1.1f)

#define MB_READ_MASK                        (MB_EVENT_INPUT_REG_RD | MB_EVENT_HOLDING_REG_RD | MB_EVENT_DISCRETE_RD | MB_EVENT_COILS_RD)
#define MB_WRITE_MASK                       (MB_EVENT_HOLDING_REG_WR | MB_EVENT_COILS_WR)
#define MB_READ_WRITE_MASK                  (MB_READ_MASK | MB_WRITE_MASK)

static const char *TAG = "MDB_WT32";
static esp_netif_t *eth_netif_global = NULL;

// Modbus register area descriptor structure
mb_register_area_descriptor_t reg_area;

holding_reg_area_t  holding_reg_area    = {0};
input_reg_area_t    input_reg_area      = {0};
discrete_reg_area_t discrete_reg_area   = {0};        // storage area for discreate registers
coil_reg_params_t   coil_reg_params     = {0};            // storage area for coil registers


/************************************************************************************************************************************/

esp_err_t mdb_init_services(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      result = nvs_flash_init();
    }
    MB_RETURN_ON_FALSE((result == ESP_OK), ESP_ERR_INVALID_STATE,
                            TAG,
                            "nvs_flash_init fail, returns(0x%x).",
                            (int)result);

    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create instance(s) of esp-netif for Ethernet(s)
    if (eth_port_cnt == 1) {
        // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
        // default esp-netif configuration parameters.
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        esp_netif_t *eth_netif = esp_netif_new(&cfg);
        // Disable DHCP and set static IP address
        ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif)); // Stop DHCP client
        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, 192, 168, 1, 123); // Set your desired static IP address here
        IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);    // Set your desired gateway address here
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0); // Set your desired netmask here
        ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &ip_info));
        // Attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));

        eth_netif_global = eth_netif;

    }
    
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }

    return ESP_OK;
}

esp_err_t mdb_destroy_services(void)
{
    esp_err_t err = ESP_OK;

    err = example_disconnect();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                   TAG,
                                   "example_disconnect fail, returns(0x%x).",
                                   (int)err);
    err = esp_event_loop_delete_default();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                       TAG,
                                       "esp_event_loop_delete_default fail, returns(0x%x).",
                                       (int)err);
    err = esp_netif_deinit();
    MB_RETURN_ON_FALSE((err == ESP_OK || err == ESP_ERR_NOT_SUPPORTED), ESP_ERR_INVALID_STATE,
                                        TAG,
                                        "esp_netif_deinit fail, returns(0x%x).",
                                        (int)err);
    err = nvs_flash_deinit();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                TAG,
                                "nvs_flash_deinit fail, returns(0x%x).",
                                (int)err);
#if CONFIG_MB_MDNS_IP_RESOLVER
    stop_mdns_service();
#endif
    return err;
}

// Modbus slave initialization
esp_err_t mdb_slave_init(void)
{
    void* slave_handler = NULL;

    // Initialization of Modbus controller
    esp_err_t err = mbc_slave_init_tcp(&slave_handler);
    MB_RETURN_ON_FALSE((err == ESP_OK && slave_handler != NULL), ESP_ERR_INVALID_STATE,
                                TAG,
                                "mb controller initialization fail.");


    if (eth_netif_global == NULL) {
        ESP_LOGE(TAG, "Ethernet interface not initialized!");
        return ESP_FAIL;
    }

    mb_communication_info_t comm_info = {
        .ip_addr_type = MB_IPV4,
        .ip_mode = MB_MODE_TCP,
        .ip_port = MB_TCP_PORT_NUMBER,
        .ip_addr = NULL,
        .ip_netif_ptr = (void*)eth_netif_global
    };

    // Setup communication parameters and start stack
    err = mbc_slave_setup((void*)&comm_info);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mbc_slave_setup fail, returns(0x%x).", (int)err);

    reg_area.type = MB_PARAM_HOLDING; // Set type of register area
    reg_area.start_offset = MB_REG_HOLDING_START_AREA0; // Offset of register area in Modbus protocol
    reg_area.address = (void*)&holding_reg_area; // Set pointer to storage instance
    reg_area.size = sizeof(holding_reg_area); // Set the size of register storage instance
    err = mbc_slave_set_descriptor(reg_area);

    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                    TAG,
                                    "mbc_slave_set_descriptor fail, returns(0x%x).",
                                    (int)err);

    reg_area.type = MB_PARAM_INPUT;
    reg_area.start_offset = MB_REG_INPUT_START_AREA0;
    reg_area.address = (void*)&input_reg_area;
    reg_area.size = sizeof(input_reg_area);
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                    TAG,
                                    "mbc_slave_set_descriptor fail, returns(0x%x).",
                                    (int)err);

    reg_area.type = MB_PARAM_DISCRETE;
    reg_area.start_offset = MB_REG_DISCRETE_INPUT_START;
    reg_area.address = (void*)&discrete_reg_area;
    reg_area.size = sizeof(discrete_reg_area);
    err = mbc_slave_set_descriptor(reg_area);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                    TAG,
                                    "mbc_slave_set_descriptor fail, returns(0x%x).",
                                    (int)err);

    reg_area.type = MB_PARAM_COIL;
    reg_area.start_offset = MB_REG_COILS_START;
    reg_area.address = (void*)&coil_reg_params;
    reg_area.size = sizeof(coil_reg_params);
    err = mbc_slave_set_descriptor(reg_area);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                    TAG,
                                    "mbc_slave_set_descriptor fail, returns(0x%x).",
                                    (int)err);

    // Starts of modbus controller and stack
    err = mbc_slave_start();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                        TAG,
                                        "mbc_slave_start fail, returns(0x%x).",
                                        (int)err);
    vTaskDelay(5);
    return err;
}

esp_err_t mdb_slave_destroy(void)
{
    esp_err_t err = mbc_slave_destroy();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                TAG,
                                "mbc_slave_destroy fail, returns(0x%x).",
                                (int)err);
    return err;
}

void mdb_init_registers(void)
{
    holding_reg_area.MAX_WELD_TIME = 0;
    holding_reg_area.CURR_RIS_TIME = 1;
    holding_reg_area.PLS_DUR = 2;
    holding_reg_area.INT_BTW_PULSES = 3;
    holding_reg_area.NUM_PUL_CYC = 4;
    holding_reg_area.reg5 = 5;
    holding_reg_area.reg6 = 6;
    holding_reg_area.reg7 = 7;
    holding_reg_area.reg8 = 8;
    holding_reg_area.reg9 = 9;

    input_reg_area.reg0 = 0;
    input_reg_area.reg1 = 100;
    input_reg_area.reg2 = 200;
    input_reg_area.reg3 = 300;
    input_reg_area.reg4 = 400;
    input_reg_area.reg5 = 500;
    input_reg_area.reg6 = 600;
    input_reg_area.reg7 = 700;
    input_reg_area.reg8 = 800;
    input_reg_area.reg9 = 900;

    discrete_reg_area.input0 = 1;
    discrete_reg_area.input1 = 1;
    discrete_reg_area.input2 = 1;
    discrete_reg_area.input3 = 0;
    discrete_reg_area.input4 = 0;
    discrete_reg_area.input5 = 0;
    discrete_reg_area.input6 = 0;
    discrete_reg_area.input7 = 1;
    discrete_reg_area.input8 = 1;
    discrete_reg_area.input9 = 1;

    coil_reg_params.coils_port0 = 0x55; //01010101
    coil_reg_params.coils_port1 = 0xFF; //11111111
}

void mdb_run(void)
{   
    mdb_init_registers();

    ESP_ERROR_CHECK(mdb_init_services());

    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_ERROR_CHECK(mdb_slave_init());

    //ESP_ERROR_CHECK(mdb_slave_destroy());
    //ESP_ERROR_CHECK(mdb_destroy_services());
}