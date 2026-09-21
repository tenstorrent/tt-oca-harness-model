// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "spi_controller_basetest.h"

spi_controller_basetest::Register_Property_t reg_map[14] = {
{spi_controller_basetest::INTR_STATE_OFFSET, spi_controller_basetest::INTR_STATE_READ, spi_controller_basetest::INTR_STATE_WRITE, spi_controller_basetest::INTR_STATE_RESET, "INTR_STATE"},
{spi_controller_basetest::INTR_ENABLE_OFFSET, spi_controller_basetest::INTR_ENABLE_READ, spi_controller_basetest::INTR_ENABLE_WRITE, spi_controller_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"},
{spi_controller_basetest::INTR_TEST_OFFSET, spi_controller_basetest::INTR_TEST_READ, spi_controller_basetest::INTR_TEST_WRITE, spi_controller_basetest::INTR_TEST_RESET, "INTR_TEST"},
{spi_controller_basetest::ALERT_TEST_OFFSET, spi_controller_basetest::ALERT_TEST_READ, spi_controller_basetest::ALERT_TEST_WRITE, spi_controller_basetest::ALERT_TEST_RESET, "ALERT_TEST"},
{spi_controller_basetest::CONTROL_OFFSET, spi_controller_basetest::CONTROL_READ, spi_controller_basetest::CONTROL_WRITE, spi_controller_basetest::CONTROL_RESET, "CONTROL"},
{spi_controller_basetest::STATUS_OFFSET, spi_controller_basetest::STATUS_READ, spi_controller_basetest::STATUS_WRITE, spi_controller_basetest::STATUS_RESET, "STATUS"},
{spi_controller_basetest::CONFIGOPTS_OFFSET, spi_controller_basetest::CONFIGOPTS_READ, spi_controller_basetest::CONFIGOPTS_WRITE, spi_controller_basetest::CONFIGOPTS_RESET, "CONFIGOPTS"},
{spi_controller_basetest::CSID_OFFSET, spi_controller_basetest::CSID_READ, spi_controller_basetest::CSID_WRITE, spi_controller_basetest::CSID_RESET, "CSID"},
{spi_controller_basetest::COMMAND_OFFSET, spi_controller_basetest::COMMAND_READ, spi_controller_basetest::COMMAND_WRITE, spi_controller_basetest::COMMAND_RESET, "COMMAND"},
{spi_controller_basetest::RXDATA_OFFSET, spi_controller_basetest::RXDATA_READ, spi_controller_basetest::RXDATA_WRITE, spi_controller_basetest::RXDATA_RESET, "RXDATA"},
{spi_controller_basetest::TXDATA_OFFSET, spi_controller_basetest::TXDATA_READ, spi_controller_basetest::TXDATA_WRITE, spi_controller_basetest::TXDATA_RESET, "TXDATA"},
{spi_controller_basetest::ERROR_ENABLE_OFFSET, spi_controller_basetest::ERROR_ENABLE_READ, spi_controller_basetest::ERROR_ENABLE_WRITE, spi_controller_basetest::ERROR_ENABLE_RESET, "ERROR_ENABLE"},
{spi_controller_basetest::ERROR_STATUS_OFFSET, spi_controller_basetest::ERROR_STATUS_READ, spi_controller_basetest::ERROR_STATUS_WRITE, spi_controller_basetest::ERROR_STATUS_RESET, "ERROR_STATUS"},
{spi_controller_basetest::EVENT_ENABLE_OFFSET, spi_controller_basetest::EVENT_ENABLE_READ, spi_controller_basetest::EVENT_ENABLE_WRITE, spi_controller_basetest::EVENT_ENABLE_RESET, "EVENT_ENABLE"}};