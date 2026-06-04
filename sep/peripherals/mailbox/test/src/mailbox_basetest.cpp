/**
 * @file mailbox_basetest.cpp
 * @brief Register property map for test automation
 *
 * Defines the register property table aggregating offset, access masks, reset values,
 * and names for all 10 mailbox registers. Used for table-driven testing.
 */

#include "mailbox_basetest.h"

/**
 * @brief Register property map for all 10 mailbox registers
 *
 * Table-driven register properties enabling automated testing of:
 * - Register reset values
 * - Access type enforcement (RO/WO/RW)
 * - Reserved bit behavior
 * - Register offset correctness
 */
mailbox_basetest::Register_Property_t reg_map[10] = {
{mailbox_basetest::WRITE_DATA_OFFSET, mailbox_basetest::WRITE_DATA_READ, mailbox_basetest::WRITE_DATA_WRITE, mailbox_basetest::WRITE_DATA_RESET, "WRITE_DATA"}, 
{mailbox_basetest::READ_DATA_OFFSET, mailbox_basetest::READ_DATA_READ, mailbox_basetest::READ_DATA_WRITE, mailbox_basetest::READ_DATA_RESET, "READ_DATA"}, 
{mailbox_basetest::STATUS_OFFSET, mailbox_basetest::STATUS_READ, mailbox_basetest::STATUS_WRITE, mailbox_basetest::STATUS_RESET, "STATUS"}, 
{mailbox_basetest::ERROR_FLAGS_OFFSET, mailbox_basetest::ERROR_FLAGS_READ, mailbox_basetest::ERROR_FLAGS_WRITE, mailbox_basetest::ERROR_FLAGS_RESET, "ERROR_FLAGS"}, 
{mailbox_basetest::WIRQT_OFFSET, mailbox_basetest::WIRQT_READ, mailbox_basetest::WIRQT_WRITE, mailbox_basetest::WIRQT_RESET, "WIRQT"}, 
{mailbox_basetest::RIRQT_OFFSET, mailbox_basetest::RIRQT_READ, mailbox_basetest::RIRQT_WRITE, mailbox_basetest::RIRQT_RESET, "RIRQT"}, 
{mailbox_basetest::IRQS_OFFSET, mailbox_basetest::IRQS_READ, mailbox_basetest::IRQS_WRITE, mailbox_basetest::IRQS_RESET, "IRQS"}, 
{mailbox_basetest::IRQEN_OFFSET, mailbox_basetest::IRQEN_READ, mailbox_basetest::IRQEN_WRITE, mailbox_basetest::IRQEN_RESET, "IRQEN"}, 
{mailbox_basetest::IRQP_OFFSET, mailbox_basetest::IRQP_READ, mailbox_basetest::IRQP_WRITE, mailbox_basetest::IRQP_RESET, "IRQP"}, 
{mailbox_basetest::CTRL_OFFSET, mailbox_basetest::CTRL_READ, mailbox_basetest::CTRL_WRITE, mailbox_basetest::CTRL_RESET, "CTRL"}};