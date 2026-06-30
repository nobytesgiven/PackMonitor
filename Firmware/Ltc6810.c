#include "Ltc6810.h"

#include <string.h>

#ifndef LTC6810_WEAK
#if defined(__GNUC__) || defined(__clang__)
#define LTC6810_WEAK __attribute__((weak))
#elif defined(__ICCARM__)
#define LTC6810_WEAK __weak
#else
#define LTC6810_WEAK
#endif
#endif

#define LTC6810_RESPONSE_BYTES 8U

#define LTC6810_CMD_WRCFG 0x001U
#define LTC6810_CMD_RDCFG 0x002U
#define LTC6810_CMD_RDCVA 0x004U
#define LTC6810_CMD_RDCVB 0x006U
#define LTC6810_CMD_RDAUXA 0x00CU
#define LTC6810_CMD_RDAUXB 0x00EU
#define LTC6810_CMD_RDSTATB 0x012U
#define LTC6810_CMD_ADCV_NORMAL 0x360U
#define LTC6810_CMD_ADAX_NORMAL 0x560U
#define LTC6810_CMD_ADOW_PD_NORMAL 0x328U
#define LTC6810_CMD_ADOW_PU_NORMAL 0x368U
#define LTC6810_CMD_DIAGN 0x715U

#ifndef LTC6810_NO_DEFAULT_PLATFORM
LTC6810_WEAK void ltc6810_platform_init(uint32_t spi_frequency_hz) {
  (void)spi_frequency_hz;
}

LTC6810_WEAK void ltc6810_platform_begin_transaction(
    uint32_t spi_frequency_hz) {
  (void)spi_frequency_hz;
}

LTC6810_WEAK uint8_t ltc6810_platform_spi_transfer(uint8_t value) {
  (void)value;
  return 0xFFU;
}

LTC6810_WEAK void ltc6810_platform_end_transaction(void) {}

LTC6810_WEAK void ltc6810_platform_chip_select(bool high) {
  (void)high;
}

LTC6810_WEAK void ltc6810_platform_delay_us(uint32_t microseconds) {
  (void)microseconds;
}
#endif

static bool ltc6810_valid_driver(const ltc6810_t *driver) {
  return driver != NULL && driver->device_count > 0U &&
         driver->device_count <= LTC6810_MAX_DEVICES;
}

static bool ltc6810_valid_address(const ltc6810_t *driver, uint8_t address) {
  return ltc6810_valid_driver(driver) && address < driver->device_count;
}

static void ltc6810_wake_from_sleep(void) {
  /*
   * A long CS pulse wakes both the LTC6820 link and an LTC6810 core in SLEEP.
   */
  ltc6810_platform_chip_select(false);
  ltc6810_platform_delay_us(2500U);
  ltc6810_platform_chip_select(true);
  ltc6810_platform_delay_us(300U);
}

static void ltc6810_wake_from_idle(void) {
  /* REFON is normally kept set, so only the isoSPI interface needs waking. */
  ltc6810_platform_chip_select(false);
  ltc6810_platform_delay_us(20U);
  ltc6810_platform_chip_select(true);
  ltc6810_platform_delay_us(20U);
}

uint16_t ltc6810_calculate_pec15(const uint8_t *data, size_t length) {
  size_t byte_index;
  uint16_t remainder = 16U;

  if (data == NULL && length != 0U) {
    return 0U;
  }

  for (byte_index = 0U; byte_index < length; ++byte_index) {
    uint8_t bit;
    for (bit = 0U; bit < 8U; ++bit) {
      const bool input_bit =
          ((data[byte_index] >> (uint8_t)(7U - bit)) & 1U) != 0U;
      const bool top_bit = ((remainder >> 14U) & 1U) != 0U;
      remainder = (uint16_t)((remainder << 1U) & 0x7FFFU);
      if (input_bit != top_bit) {
        remainder ^= 0x4599U;
      }
    }
  }
  return (uint16_t)(remainder << 1U);
}

static void ltc6810_make_command(uint16_t command_code, int8_t address,
                                 uint8_t command[4]) {
  uint16_t pec;

  if (address < 0) {
    command[0] = (uint8_t)((command_code >> 8U) & 0x07U);
  } else {
    command[0] =
        (uint8_t)(0x80U | (((uint8_t)address & 0x0FU) << 3U) |
                  ((command_code >> 8U) & 0x07U));
  }
  command[1] = (uint8_t)(command_code & 0xFFU);
  pec = ltc6810_calculate_pec15(command, 2U);
  command[2] = (uint8_t)(pec >> 8U);
  command[3] = (uint8_t)pec;
}

static void ltc6810_transfer_bytes(const uint8_t *data, size_t length) {
  size_t index;
  for (index = 0U; index < length; ++index) {
    (void)ltc6810_platform_spi_transfer(data[index]);
  }
}

static void ltc6810_send_command(const ltc6810_t *driver,
                                 uint16_t command_code,
                                 int8_t address) {
  uint8_t command[4];

  if (!ltc6810_valid_driver(driver)) {
    return;
  }

  ltc6810_make_command(command_code, address, command);
  ltc6810_wake_from_idle();
  ltc6810_platform_begin_transaction(driver->spi_frequency_hz);
  ltc6810_platform_chip_select(false);
  ltc6810_transfer_bytes(command, sizeof(command));
  ltc6810_platform_chip_select(true);
  ltc6810_platform_end_transaction();
}

static bool ltc6810_write_register(
    const ltc6810_t *driver, uint8_t address, uint16_t command_code,
    const uint8_t data[LTC6810_REGISTER_BYTES]) {
  uint8_t command[4];
  uint16_t data_pec;
  uint8_t index;

  if (!ltc6810_valid_address(driver, address) || data == NULL) {
    return false;
  }

  ltc6810_make_command(command_code, (int8_t)address, command);
  data_pec = ltc6810_calculate_pec15(data, LTC6810_REGISTER_BYTES);

  ltc6810_wake_from_idle();
  ltc6810_platform_begin_transaction(driver->spi_frequency_hz);
  ltc6810_platform_chip_select(false);
  ltc6810_transfer_bytes(command, sizeof(command));
  for (index = 0U; index < LTC6810_REGISTER_BYTES; ++index) {
    (void)ltc6810_platform_spi_transfer(data[index]);
  }
  (void)ltc6810_platform_spi_transfer((uint8_t)(data_pec >> 8U));
  (void)ltc6810_platform_spi_transfer((uint8_t)data_pec);
  ltc6810_platform_chip_select(true);
  ltc6810_platform_end_transaction();
  return true;
}

static bool ltc6810_read_register_once(
    ltc6810_t *driver, uint8_t address, uint16_t command_code,
    uint8_t data[LTC6810_REGISTER_BYTES]) {
  uint8_t command[4];
  uint8_t response[LTC6810_RESPONSE_BYTES];
  uint16_t received_pec;
  uint16_t calculated_pec;
  uint8_t index;

  if (!ltc6810_valid_address(driver, address) || data == NULL) {
    return false;
  }

  ltc6810_make_command(command_code, (int8_t)address, command);
  ltc6810_wake_from_idle();
  ltc6810_platform_begin_transaction(driver->spi_frequency_hz);
  ltc6810_platform_chip_select(false);
  ltc6810_transfer_bytes(command, sizeof(command));
  for (index = 0U; index < LTC6810_RESPONSE_BYTES; ++index) {
    response[index] = ltc6810_platform_spi_transfer(0xFFU);
  }
  ltc6810_platform_chip_select(true);
  ltc6810_platform_end_transaction();

  received_pec =
      (uint16_t)(((uint16_t)response[6] << 8U) | (uint16_t)response[7]);
  calculated_pec =
      ltc6810_calculate_pec15(response, LTC6810_REGISTER_BYTES);
  if (received_pec != calculated_pec) {
    ++driver->pec_errors;
    return false;
  }

  memcpy(data, response, LTC6810_REGISTER_BYTES);
  return true;
}

static bool ltc6810_read_register(
    ltc6810_t *driver, uint8_t address, uint16_t command_code,
    uint8_t data[LTC6810_REGISTER_BYTES]) {
  uint16_t attempt;

  if (!ltc6810_valid_address(driver, address) || data == NULL) {
    return false;
  }

  for (attempt = 0U; attempt <= (uint16_t)driver->read_retries; ++attempt) {
    if (ltc6810_read_register_once(driver, address, command_code, data)) {
      return true;
    }
  }
  ++driver->transaction_errors;
  return false;
}

static ltc6810_config_status_t ltc6810_verify_configuration(
    ltc6810_t *driver, uint8_t address,
    const uint8_t expected[LTC6810_REGISTER_BYTES]) {
  static const uint8_t config_0_verify_mask = 0x05U;
  uint8_t actual[LTC6810_REGISTER_BYTES];

  if (!ltc6810_read_register(driver, address, LTC6810_CMD_RDCFG, actual)) {
    memset(driver->config_readback[address], 0, LTC6810_REGISTER_BYTES);
    return LTC6810_CONFIG_READ_FAILED;
  }

  memcpy(driver->config_readback[address], actual, LTC6810_REGISTER_BYTES);
  /*
   * CFGR0 GPIO bits read pin logic levels. DTEN also follows the DTEN pin
   * state on readback, so only REFON and ADCOPT are stable config bits here.
   */
  if ((actual[0] & config_0_verify_mask) !=
      (expected[0] & config_0_verify_mask)) {
    return LTC6810_CONFIG_MISMATCH;
  }
  return memcmp(&actual[1], &expected[1], 5U) == 0
             ? LTC6810_CONFIG_OK
             : LTC6810_CONFIG_MISMATCH;
}

void ltc6810_init(ltc6810_t *driver, uint8_t device_count,
                  uint8_t read_retries, uint32_t spi_frequency_hz) {
  if (driver == NULL) {
    return;
  }

  memset(driver, 0, sizeof(*driver));
  driver->device_count =
      device_count <= LTC6810_MAX_DEVICES ? device_count : 0U;
  driver->read_retries = read_retries;
  driver->spi_frequency_hz = spi_frequency_hz;
}

void ltc6810_begin(const ltc6810_t *driver) {
  if (!ltc6810_valid_driver(driver)) {
    return;
  }

  ltc6810_platform_init(driver->spi_frequency_hz);
  ltc6810_platform_chip_select(true);
  ltc6810_wake_from_sleep();
}

bool ltc6810_configure_all(
    ltc6810_t *driver,
    const uint8_t config[LTC6810_REGISTER_BYTES]) {
  uint8_t address;
  bool all_good = true;

  if (!ltc6810_valid_driver(driver) || config == NULL) {
    return false;
  }

  memcpy(driver->config_expected, config, LTC6810_REGISTER_BYTES);
  for (address = 0U; address < driver->device_count; ++address) {
    uint16_t attempt;
    bool configured = false;

    driver->config_status[address] = LTC6810_CONFIG_NOT_RUN;
    memset(driver->config_readback[address], 0, LTC6810_REGISTER_BYTES);
    for (attempt = 0U; attempt <= (uint16_t)driver->read_retries; ++attempt) {
      (void)ltc6810_write_register(driver, address, LTC6810_CMD_WRCFG,
                                   config);
      driver->config_status[address] =
          ltc6810_verify_configuration(driver, address, config);
      if (driver->config_status[address] == LTC6810_CONFIG_OK) {
        configured = true;
        break;
      }
    }
    if (!configured) {
      ++driver->transaction_errors;
      all_good = false;
    }
  }
  return all_good;
}

void ltc6810_start_cell_conversion(const ltc6810_t *driver) {
  ltc6810_send_command(driver, LTC6810_CMD_ADCV_NORMAL, -1);
}

void ltc6810_start_aux_conversion(const ltc6810_t *driver) {
  ltc6810_send_command(driver, LTC6810_CMD_ADAX_NORMAL, -1);
}

void ltc6810_start_open_wire_conversion(const ltc6810_t *driver,
                                        bool pull_up) {
  ltc6810_send_command(
      driver,
      pull_up ? LTC6810_CMD_ADOW_PU_NORMAL : LTC6810_CMD_ADOW_PD_NORMAL, -1);
}

void ltc6810_start_mux_diagnostic(const ltc6810_t *driver) {
  ltc6810_send_command(driver, LTC6810_CMD_DIAGN, -1);
}

static uint16_t ltc6810_unpack16(const uint8_t *bytes) {
  return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

bool ltc6810_read_cells(ltc6810_t *driver, uint8_t address,
                        uint16_t out[6]) {
  uint8_t group_a[LTC6810_REGISTER_BYTES];
  uint8_t group_b[LTC6810_REGISTER_BYTES];
  uint8_t index;

  if (out == NULL ||
      !ltc6810_read_register(driver, address, LTC6810_CMD_RDCVA, group_a) ||
      !ltc6810_read_register(driver, address, LTC6810_CMD_RDCVB, group_b)) {
    return false;
  }

  for (index = 0U; index < 3U; ++index) {
    out[index] = ltc6810_unpack16(&group_a[index * 2U]);
    out[index + 3U] = ltc6810_unpack16(&group_b[index * 2U]);
  }
  return true;
}

bool ltc6810_read_aux(ltc6810_t *driver, uint8_t address, uint16_t gpio[4],
                      uint16_t *ref2) {
  uint8_t group_a[LTC6810_REGISTER_BYTES];
  uint8_t group_b[LTC6810_REGISTER_BYTES];

  if (gpio == NULL || ref2 == NULL ||
      !ltc6810_read_register(driver, address, LTC6810_CMD_RDAUXA, group_a) ||
      !ltc6810_read_register(driver, address, LTC6810_CMD_RDAUXB, group_b)) {
    return false;
  }

  gpio[0] = ltc6810_unpack16(&group_a[2]);
  gpio[1] = ltc6810_unpack16(&group_a[4]);
  gpio[2] = ltc6810_unpack16(&group_b[0]);
  gpio[3] = ltc6810_unpack16(&group_b[2]);
  *ref2 = ltc6810_unpack16(&group_b[4]);
  return true;
}

bool ltc6810_read_status_b(
    ltc6810_t *driver, uint8_t address,
    uint8_t out[LTC6810_REGISTER_BYTES]) {
  return ltc6810_read_register(driver, address, LTC6810_CMD_RDSTATB, out);
}

uint32_t ltc6810_pec_errors(const ltc6810_t *driver) {
  return driver != NULL ? driver->pec_errors : 0U;
}

uint32_t ltc6810_transaction_errors(const ltc6810_t *driver) {
  return driver != NULL ? driver->transaction_errors : 0U;
}

ltc6810_config_status_t ltc6810_config_status(const ltc6810_t *driver,
                                               uint8_t address) {
  if (!ltc6810_valid_address(driver, address)) {
    return LTC6810_CONFIG_NOT_RUN;
  }
  return driver->config_status[address];
}

const uint8_t *ltc6810_config_readback(const ltc6810_t *driver,
                                       uint8_t address) {
  if (!ltc6810_valid_address(driver, address)) {
    return NULL;
  }
  return driver->config_readback[address];
}

const uint8_t *ltc6810_config_expected(const ltc6810_t *driver) {
  return driver != NULL ? driver->config_expected : NULL;
}
