#include <stdio.h>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>

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

    printf("B8 / data_qm[24]: 0x%02X\n", data_qm[24]);
    printf("B3 / data_qm[19]: 0x%02X\n", data_qm[19]);
    printf("B2 / data_qm[18]: 0x%02X\n", data_qm[18]);

    printf("B4 / data_qm[20]: 0x%02X\n", data_qm[20]);
    printf("B5 / data_qm[21]: 0x%02X\n", data_qm[21]);

    printf("B6 / data_qm[22]: 0x%02X\n", data_qm[22]);
    printf("B7 / data_qm[23]: 0x%02X\n", data_qm[23]);

    uint32_t coe_a0 = ((uint32_t)data_qm[18] << 12) | ((uint32_t)data_qm[19] << 4) | (data_qm[24] & 0x0F);
    uint16_t coe_a1 = ((uint16_t)data_qm[20] << 8) | data_qm[21];
    uint16_t coe_a2 = ((uint16_t)data_qm[22] << 8) | data_qm[23];

    uint32_t coe_b00 = ((uint32_t)data_qm[0] << 12) | ((uint32_t)data_qm[1] << 4) | (data_qm[24] >> 4);
    uint16_t coe_bp3 = data_qm[17] | (data_qm[16] << 8);
    uint16_t coe_b21 = data_qm[15] | (data_qm[14] << 8);
    uint16_t coe_b12 = data_qm[13] | (data_qm[12] << 8);
    uint16_t coe_bp2 = data_qm[11] | (data_qm[10] << 8);
    uint16_t coe_b11 = data_qm[9] | (data_qm[8] << 8);
    uint16_t coe_bp1 = data_qm[7] | (data_qm[6] << 8);
    uint16_t coe_bt2 = data_qm[5] | (data_qm[4] << 8);
    uint16_t coe_bt1 = data_qm[3] | (data_qm[2] << 8);

    printf("coe_a0: 0x%04lX\n", coe_a0);
    printf("coe_a1: 0x%04X\n", coe_a1);
    printf("coe_a2: 0x%04X\n", coe_a2);
    printf("coe_b00: 0x%04" PRIX32 "\n", coe_b00);

    printf("coe_bp3: 0x%04X\n", coe_bp3);
    printf("coe_b21: 0x%04X\n", coe_b21);
    printf("coe_b12: 0x%04X\n", coe_b12);
    printf("coe_bp2: 0x%04X\n", coe_bp2);
    printf("coe_b11: 0x%04X\n", coe_b11);
    printf("coe_bp1: 0x%04X\n", coe_bp1);
    printf("coe_bt2: 0x%04X\n", coe_bt2);
    printf("coe_bt1: 0x%04X\n", coe_bt1);

    // =========================================
    // 12. Configure averaging + Power Mode
    // =========================================

    uint8_t mq_data3[] = {0xF4, 0x8B};

    result = i2c_master_transmit(
        qmp6988_handle,
        mq_data3,
        sizeof(mq_data3),
        1000);

    uint8_t ctrl_meas_reg[] = {0xF4};
    uint8_t ctrl_meas_data[1];

    result = i2c_master_transmit_receive(
        qmp6988_handle,
        ctrl_meas_reg,
        sizeof(ctrl_meas_reg),
        ctrl_meas_data,
        sizeof(ctrl_meas_data),
        1000);

    printf("I2C result: %s\n", esp_err_to_name(result));
    printf("CTRL_MEAS: 0x%02X\n", ctrl_meas_data[0]);

    // =========================================
    // 13 Lezen van de raw temperature
    // =========================================

    uint8_t register_address[] = {0xFC};
    uint8_t register_data[1];

    result = i2c_master_transmit_receive(
        qmp6988_handle,
        register_address,
        sizeof(register_address),
        register_data,
        sizeof(register_data),
        1000);

    printf("TEMP_TXD0: 0x%02X\n", register_data[0]);

    uint8_t register_address1[] = {0xFB};
    uint8_t register_data1[1];

    result = i2c_master_transmit_receive(
        qmp6988_handle,
        register_address1,
        sizeof(register_address1),
        register_data1,
        sizeof(register_data1),
        1000);

    printf("TEMP_TXD1: 0x%02X\n", register_data1[0]);

    uint8_t register_address2[] = {0xFA};
    uint8_t register_data2[1];

    result = i2c_master_transmit_receive(
        qmp6988_handle,
        register_address2,
        sizeof(register_address2),
        register_data2,
        sizeof(register_data2),
        1000);

    printf("TEMP_TXD2: 0x%02X\n", register_data2[0]);

    uint8_t register_address4[] = {0xF3};
    uint8_t register_data4[1];

    result = i2c_master_transmit_receive(
        qmp6988_handle,
        register_address4,
        sizeof(register_address4),
        register_data4,
        sizeof(register_data4),
        1000);

    printf("DEVICE_STAT: 0x%02X\n", register_data4[0]);

    uint32_t qmp_raw_temp =
        ((uint32_t)register_data2[0] << 16) |
        ((uint32_t)register_data1[0] << 8) |
        register_data[0];

    printf("qmp_raw_temp: 0x%06" PRIX32 "\n", qmp_raw_temp);

    int32_t Dt = qmp_raw_temp - (1 << 23);

    // printf("Dt: %" PRId32 "\n", Dt);

    printf("qmp_raw_temp: 0x%06" PRIX32 "\n", qmp_raw_temp);
    printf("Dt: %ld\n", (long)Dt);

    // =========================================
    // 14 Lezen van de raw luchtdruk
    // =========================================

    uint8_t pressure_register_address[] = {0xF9};
    uint8_t pressure_TXD0[1];

    result = i2c_master_transmit_receive(
        qmp6988_handle,
        pressure_register_address,
        sizeof(pressure_register_address),
        pressure_TXD0,
        sizeof(pressure_TXD0),
        1000);

    printf("PRESS_TXD0: 0x%02X\n", pressure_TXD0[0]);

    uint8_t pressure_register_address1[] = {0xF8};
    uint8_t pressure_TXD1[1];

    result = i2c_master_transmit_receive(
        qmp6988_handle,
        pressure_register_address1,
        sizeof(pressure_register_address1),
        pressure_TXD1,
        sizeof(pressure_TXD1),
        1000);

    printf("PRESS_TXD1: 0x%02X\n", pressure_TXD1[0]);

    uint8_t pressure_register_address2[] = {0xF7};
    uint8_t pressure_TXD2[1];

    result = i2c_master_transmit_receive(
        qmp6988_handle,
        pressure_register_address2,
        sizeof(pressure_register_address2),
        pressure_TXD2,
        sizeof(pressure_TXD2),
        1000);

    printf("PRESS_TXD2: 0x%02X\n", pressure_TXD2[0]);

    uint32_t qmp_raw_pressure = ((uint32_t)pressure_TXD2[0] << 16) | ((uint32_t)pressure_TXD1[0] << 8) | pressure_TXD0[0];

    printf("qmp_raw_pressure: 0x%06" PRIX32 "\n", qmp_raw_pressure);
    printf("qmp_raw_pressure decimal: %" PRIu32 "\n", qmp_raw_pressure);

    int32_t Dp = qmp_raw_pressure - (1 << 23);

    printf("Dp: %" PRId32 "\n", Dp);

    // =========================================
    // 14 Lezen van de raw luchtdruk
    // =========================================

    // 20-bit signed
    int32_t a0_otp;

    if (coe_a0 & 0x80000)
    {
        a0_otp = (int32_t)coe_a0 - 0x100000;
    }
    else
    {
        a0_otp = (int32_t)coe_a0;
    }

    double a0 = a0_otp / 16.0;

    printf("a0: %.4f\n", a0);

    // A1
    double A1 = -6.30e-3;
    double S1 = 4.30e-4;
    int OTP_a1 = 5797;

    double a1 = A1 + (S1 * OTP_a1) / 32767.0;

    printf("a1: %.9e\n", a1);

    // A2
    double A2 = -1.90e-11;
    double S2 = 1.20e-10;
    int OTP_a2 = 0x08CE;

    double a2 = A2 + (S2 * OTP_a2) / 32767.0;

    printf("a2: %.9e\n", a2);

    // Temperature compensation
    double Tr = a0 + (a1 * Dt) + (a2 * (double)Dt * (double)Dt);

    double temperature = Tr / 256.0;

    printf("Tr: %.4f\n", Tr);
    printf("QMP6988 temperature: %.3f C\n", temperature);

    // =========================================
    // Drukcoëfficiënten
    // =========================================

    // b00
    // signed 20-bit
    int32_t b00_otp;

    if (coe_b00 & 0x80000)
    {
        b00_otp = (int32_t)coe_b00 - 0x100000;
    }
    else
    {
        b00_otp = (int32_t)coe_b00;
    }

    double b00 = b00_otp / 16.0;

    printf("b00: %.4f\n", b00);

    // bt1
    double bt1 = 1.00e-01 + (9.10e-02 * coe_bt1) / 32767.0;
    printf("bt1: %.9e\n", bt1);

    printf("coe_bt1: 0x%04X\n", coe_bt1);
    printf("coe_bt1 decimal: %u\n", coe_bt1);

    // bt2
    double bt2 = 1.20e-08 + (1.20e-06 * coe_bt2) / 32767.0;
    printf("bt2: %.9e\n", bt2);

    // bp1
    double bp1 = 3.30e-02 + (1.90e-02 * coe_bp1) / 32767.0;

    printf("bp1: %.9e\n", bp1);

    // b11
    double b11 = 2.10e-07 + (1.40e-07 * coe_b11) / 32767.0;

    printf("b11: %.9e\n", b11);

    // bp2
    double bp2 = -6.30e-10 + (3.50e-10 * coe_bp2) / 32767.0;

    printf("bp2: %.9e\n", bp2);

    // b12
    double b12 = 2.90e-13 + (7.60e-13 * coe_b12) / 32767.0;

    printf("b12: %.9e\n", b12);

    // b21
    double b21 = 2.10e-15 + (1.20e-14 * coe_b21) / 32767.0;

    printf("b21: %.9e\n", b21);

    // bp3
    double bp3 = 1.30e-16 + (7.90e-17 * coe_bp3) / 32767.0;

    printf("bp3: %.9e\n", bp3);

    double Tr2 = Tr * Tr;
    double Dp2 = (double)Dp * Dp;
    double Dp3 = Dp2 * Dp;

    printf("Tr^2: %.6f\n", Tr2);
    printf("Dp^2: %.6f\n", Dp2);
    printf("Dp^3: %.6f\n", Dp3);

    double term_b00 = b00;
    double term_bt1 = bt1 * Tr;
    double term_bp1 = bp1 * Dp;

    printf("term b00: %.9f\n", term_b00);
    printf("term bt1: %.9f\n", term_bt1);
    printf("term bp1: %.9f\n", term_bp1);

    double term_b11 = b11 * Tr * Dp;
    double term_bt2 = bt2 * Tr2;
    double term_bp2 = bp2 * Dp2;

    printf("term b11: %.9f\n", term_b11);
    printf("term bt2: %.9f\n", term_bt2);
    printf("term bp2: %.9f\n", term_bp2);

    double term_b12 = b12 * Dp * Tr2;
    double term_b21 = b21 * Dp2 * Tr;
    double term_bp3 = bp3 * Dp3;

    printf("term b12: %.9f\n", term_b12);
    printf("term b21: %.9f\n", term_b21);
    printf("term bp3: %.9f\n", term_bp3);

    double Pr = term_b00 + term_bt1 + term_bp1 + term_b11 + term_bt2 + term_bp2 + term_b12 + term_b21 + term_bp3;

    printf("Pr: %.6f Pa\n", Pr);

    printf("qmp_raw_pressure: %" PRIu32 "\n", qmp_raw_pressure);
    printf("Dp: %" PRId32 "\n", Dp);
    printf("Pr: %.6f Pa\n", Pr);



    printf("B6 / data_qm[0]: 0x%02X\n", data_qm[0]);
        printf("B7 / data_qm[1]: 0x%02X\n", data_qm[1]);

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