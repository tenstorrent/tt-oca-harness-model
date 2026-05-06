#include "spi_controller_basetest.h"

spi_controller_basetest::Register_Property_t reg_map[13] = {
{spi_controller_basetest::INTR_STATUS_OFFSET, spi_controller_basetest::INTR_STATUS_READ, spi_controller_basetest::INTR_STATUS_WRITE, spi_controller_basetest::INTR_STATUS_RESET, "INTR_STATUS"},
{spi_controller_basetest::INTR_ENABLE_OFFSET, spi_controller_basetest::INTR_ENABLE_READ, spi_controller_basetest::INTR_ENABLE_WRITE, spi_controller_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"},
{spi_controller_basetest::INTR_TEST_OFFSET, spi_controller_basetest::INTR_TEST_READ, spi_controller_basetest::INTR_TEST_WRITE, spi_controller_basetest::INTR_TEST_RESET, "INTR_TEST"},
{spi_controller_basetest::CTRL_OFFSET, spi_controller_basetest::CTRL_READ, spi_controller_basetest::CTRL_WRITE, spi_controller_basetest::CTRL_RESET, "CTRL"},
{spi_controller_basetest::STATUS_OFFSET, spi_controller_basetest::STATUS_READ, spi_controller_basetest::STATUS_WRITE, spi_controller_basetest::STATUS_RESET, "STATUS"},
{spi_controller_basetest::CFG_OFFSET, spi_controller_basetest::CFG_READ, spi_controller_basetest::CFG_WRITE, spi_controller_basetest::CFG_RESET, "CFG"},
{spi_controller_basetest::CSID_OFFSET, spi_controller_basetest::CSID_READ, spi_controller_basetest::CSID_WRITE, spi_controller_basetest::CSID_RESET, "CSID"},
{spi_controller_basetest::CMD_OFFSET, spi_controller_basetest::CMD_READ, spi_controller_basetest::CMD_WRITE, spi_controller_basetest::CMD_RESET, "CMD"},
{spi_controller_basetest::RXDATA_OFFSET, spi_controller_basetest::RXDATA_READ, spi_controller_basetest::RXDATA_WRITE, spi_controller_basetest::RXDATA_RESET, "RXDATA"},
{spi_controller_basetest::TXDATA_OFFSET, spi_controller_basetest::TXDATA_READ, spi_controller_basetest::TXDATA_WRITE, spi_controller_basetest::TXDATA_RESET, "TXDATA"},
{spi_controller_basetest::ERROR_ENABLE_OFFSET, spi_controller_basetest::ERROR_ENABLE_READ, spi_controller_basetest::ERROR_ENABLE_WRITE, spi_controller_basetest::ERROR_ENABLE_RESET, "ERROR_ENABLE"},
{spi_controller_basetest::ERROR_STATUS_OFFSET, spi_controller_basetest::ERROR_STATUS_READ, spi_controller_basetest::ERROR_STATUS_WRITE, spi_controller_basetest::ERROR_STATUS_RESET, "ERROR_STATUS"},
{spi_controller_basetest::EVENT_ENABLE_OFFSET, spi_controller_basetest::EVENT_ENABLE_READ, spi_controller_basetest::EVENT_ENABLE_WRITE, spi_controller_basetest::EVENT_ENABLE_RESET, "EVENT_ENABLE"}};