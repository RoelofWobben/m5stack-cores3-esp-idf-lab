#include <stdio.h>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SYS_SDA GPIO_NUM_12
#define SYS_SCL GPIO_NUM_11

#define PORT_SDA GPIO_NUM_2
#define PORT_SCL GPIO_NUM_1

#define AW9523_ADDR 0x58
#define SHT30_ADDR 0x44
#define QMP6988_ADDR 0x70

uint8_t crc_check(uint8_t part1, uint8_t part2)
{
    uint8_t crc = 0xFF;

    crc ^= part1;

    for (size_t i = 0; i < 8; i++)
    {
        if (crc & 0x80)
        {
            crc = crc << 1;
            crc = crc ^ 0x31;
        }
        else
        {
            crc = crc << 1;
        }
    }

    crc ^= part2;

    for (size_t i = 0; i < 8; i++)
    {
        if (crc & 0x80)
        {
            crc = crc << 1;
            crc = crc ^ 0x31;
        }
        else
        {
            crc = crc << 1;
        }
    }

    return crc;
}

void app_main(void)
{
    printf("START app_main\n");

    // =========================================
    // 1. Internal I2C bus -> AW9523B
    // =========================================

    i2c_master_bus_config_t sys_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_1,
        .scl_io_num = SYS_SCL,
        .sda_io_num = SYS_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };

    i2c_master_bus_handle_t sys_bus;

    esp_err_t result =
        i2c_new_master_bus(&sys_cfg, &sys_bus);

    printf(
        "AW9523 bus: %s\n",
        esp_err_to_name(result));

    i2c_device_config_t aw_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AW9523_ADDR,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t aw_dev;

    result =
        i2c_master_bus_add_device(
            sys_bus,
            &aw_cfg,
            &aw_dev);

    printf(
        "AW9523 device: %s\n",
        esp_err_to_name(result));

    // P0 push-pull
    uint8_t data1[2] = {0x11, 0x10};

    result =
        i2c_master_transmit(
            aw_dev,
            data1,
            sizeof(data1),
            1000);

    printf(
        "P0 push-pull: %s\n",
        esp_err_to_name(result));

    // P1_7 = HIGH -> BOOST_EN
    uint8_t data2[2] = {0x03, 0x80};

    result =
        i2c_master_transmit(
            aw_dev,
            data2,
            sizeof(data2),
            1000);

    printf(
        "BOOST_EN: %s\n",
        esp_err_to_name(result));

    // P0_1 = HIGH -> BUS_OUT_EN
    uint8_t data3[2] = {0x02, 0x02};

    result =
        i2c_master_transmit(
            aw_dev,
            data3,
            sizeof(data3),
            1000);

    printf(
        "BUS_OUT_EN: %s\n",
        esp_err_to_name(result));

    // =========================================
    // 2. PORT.A I2C bus -> ENV III
    // =========================================

    i2c_master_bus_config_t port_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = PORT_SCL,
        .sda_io_num = PORT_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t port_bus;

    result =
        i2c_new_master_bus(&port_cfg, &port_bus);

    printf(
        "PORT.A bus: %s\n",
        esp_err_to_name(result));

    // =========================================
    // 3. Alleen probe van SHT30
    // =========================================

    result =
        i2c_master_probe(
            port_bus,
            SHT30_ADDR,
            1000);

    printf(
        "SHT30 probe: %s\n",
        esp_err_to_name(result));

    // =========================================
    // 4. SHT30 als I2C-device toevoegen
    // =========================================

    i2c_device_config_t sht30_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x44,
        .scl_speed_hz = 100000,
    };

    i2c_master_dev_handle_t sht30_handle;

    result = i2c_master_bus_add_device(
        port_bus,
        &sht30_cfg,
        &sht30_handle);

    printf(
        "SHT30 device: %s\n",
        esp_err_to_name(result));

    // =========================================
    // 5. SHT30 meting starten
    // =========================================

    uint8_t wr_data[] = {0x24, 0x00};

    result = i2c_master_transmit(
        sht30_handle,
        wr_data,
        sizeof(wr_data),
        1000);

    printf(
        "SHT30 transmit: %s\n",
        esp_err_to_name(result));

    // =========================================
    // 6. Meetdata van SHT30 ontvangen
    // =========================================

    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data_rd[6];

    result = i2c_master_receive(
        sht30_handle,
        data_rd,
        sizeof(data_rd),
        1000);

    printf(
        "SHT30 receive: %s\n",
        esp_err_to_name(result));

    if (result == ESP_OK)
    {
        printf(
            "RX: %02X %02X %02X %02X %02X %02X\n",
            data_rd[0],
            data_rd[1],
            data_rd[2],
            data_rd[3],
            data_rd[4],
            data_rd[5]);
    }

    // =========================================
    // 7. Temperatuur en luchtvochtigheid berekenen
    // =========================================

    uint16_t raw_temp =
        ((uint16_t)data_rd[0] << 8) | data_rd[1];

    uint16_t raw_humidity =
        ((uint16_t)data_rd[3] << 8) | data_rd[4];

    float temp =
        -45 + 175 * raw_temp / 65535.0;

    float humidity =
        100 * raw_humidity / 65535.0;

    printf(
        "T: %.3f C  RH: %.3f %%\n",
        temp,
        humidity);

    // =========================================
    // 8. CRC van de meetdata controleren
    // =========================================

    uint8_t temp_crc =
        crc_check(data_rd[0], data_rd[1]);

    uint8_t humidity_crc =
        crc_check(data_rd[3], data_rd[4]);

    printf(
        "Temp CRC: berekend=0x%02X ontvangen=0x%02X\n",
        temp_crc,
        data_rd[2]);

    printf(
        "Humidity CRC: berekend=0x%02X ontvangen=0x%02X\n",
        humidity_crc,
        data_rd[5]);

    // 9. Alleen probe van SHT30
    // =========================================

    result =
        i2c_master_probe(
            port_bus,
            QMP6988_ADDR,
            1000);

    printf(
        "QMP6988 probe: %s\n",
        esp_err_to_name(result));

    // =========================================
    // 10. SHT30 als I2C-device toevoegen
    // =========================================

    i2c_device_config_t qmp6988_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x70,
        .scl_speed_hz = 100000,
    };

    i2c_master_dev_handle_t qmp6988_handle;

    result = i2c_master_bus_add_device(
        port_bus,
        &qmp6988_cfg,
        &qmp6988_handle);

    printf(
        "QMP6988 device: %s\n",
        esp_err_to_name(result));

    // ========================================
    // 11. de 25 correctiecoëfficiënten van de QMP6988 uitlezen
    // =========================================

    uint8_t data_qm[25];

    uint8_t data_QM2[] = {0xA0};

    result = i2c_master_transmit_receive(
        qmp6988_handle,
        data_QM2,
        sizeof(data_QM2),
        data_qm,
        sizeof(data_qm),
        1000);

    printf(
        "QMP6988 calibration read: %s\n",
        esp_err_to_name(result));

        for (int i = 0; i < 25; i++)
        {
            printf("QMP6988 A%d: 0x%02X\n", i, data_qm[i]);
        }
        
    // =========================================
    // x. Opruimen
    // =========================================

    i2c_master_bus_rm_device(aw_dev);
    i2c_master_bus_rm_device(sht30_handle);
    i2c_master_bus_rm_device(qmp6988_handle);

    i2c_del_master_bus(port_bus);
    i2c_del_master_bus(sys_bus);

    printf("EINDE app_main\n");
}