#ifndef LTC6810_H
#define LTC6810_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LTC6810_MAX_DEVICES 16U
#define LTC6810_REGISTER_BYTES 6U

typedef enum {
  LTC6810_CONFIG_NOT_RUN = 0,
  LTC6810_CONFIG_OK = 1,
  LTC6810_CONFIG_READ_FAILED = 2,
  LTC6810_CONFIG_MISMATCH = 3
} ltc6810_config_status_t;

/*
 * Driver state. Initialize this structure with ltc6810_init() before use.
 * The LTC6810 address field is four bits, so a bus can contain at most
 * LTC6810_MAX_DEVICES addressed devices.
 */
typedef struct {
  uint32_t spi_frequency_hz;
  uint32_t pec_errors;
  uint32_t transaction_errors;
  uint8_t device_count;
  uint8_t read_retries;
  ltc6810_config_status_t config_status[LTC6810_MAX_DEVICES];
  uint8_t config_readback[LTC6810_MAX_DEVICES][LTC6810_REGISTER_BYTES];
  uint8_t config_expected[LTC6810_REGISTER_BYTES];
} ltc6810_t;

/*
 * Platform hooks
 * --------------
 * Ltc6810.c supplies weak, no-op definitions for all of these functions on
 * GCC, Clang, and IAR. A platform integration should provide strong
 * definitions with the same signatures. Define LTC6810_NO_DEFAULT_PLATFORM
 * while compiling Ltc6810.c on a toolchain without weak-symbol support.
 * Transactions must use SPI mode 3 and transmit MSB first.
 *
 * chip_select(true) deasserts CS; chip_select(false) asserts CS.
 */
void ltc6810_platform_init(uint32_t spi_frequency_hz);
void ltc6810_platform_begin_transaction(uint32_t spi_frequency_hz);
uint8_t ltc6810_platform_spi_transfer(uint8_t value);
void ltc6810_platform_end_transaction(void);
void ltc6810_platform_chip_select(bool high);
void ltc6810_platform_delay_us(uint32_t microseconds);

void ltc6810_init(ltc6810_t *driver, uint8_t device_count,
                  uint8_t read_retries, uint32_t spi_frequency_hz);
void ltc6810_begin(const ltc6810_t *driver);

/*
 * Writes the same six-byte CFGR image to every configured device and verifies
 * it by readback. CFGR0 GPIO and DTEN readback bits are intentionally ignored,
 * because they report pin state rather than only the written configuration.
 */
bool ltc6810_configure_all(
    ltc6810_t *driver,
    const uint8_t config[LTC6810_REGISTER_BYTES]);

void ltc6810_start_cell_conversion(const ltc6810_t *driver);
void ltc6810_start_aux_conversion(const ltc6810_t *driver);
void ltc6810_start_open_wire_conversion(const ltc6810_t *driver,
                                        bool pull_up);
void ltc6810_start_mux_diagnostic(const ltc6810_t *driver);

bool ltc6810_read_cells(ltc6810_t *driver, uint8_t address,
                        uint16_t out[6]);
bool ltc6810_read_aux(ltc6810_t *driver, uint8_t address, uint16_t gpio[4],
                      uint16_t *ref2);
bool ltc6810_read_status_b(
    ltc6810_t *driver, uint8_t address,
    uint8_t out[LTC6810_REGISTER_BYTES]);

uint32_t ltc6810_pec_errors(const ltc6810_t *driver);
uint32_t ltc6810_transaction_errors(const ltc6810_t *driver);
ltc6810_config_status_t ltc6810_config_status(const ltc6810_t *driver,
                                               uint8_t address);
const uint8_t *ltc6810_config_readback(const ltc6810_t *driver,
                                       uint8_t address);
const uint8_t *ltc6810_config_expected(const ltc6810_t *driver);

/* Exposed for protocol tests and integrations that need to build PECs. */
uint16_t ltc6810_calculate_pec15(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
