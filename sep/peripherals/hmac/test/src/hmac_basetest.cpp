#include "hmac_basetest.h"

hmac_basetest::Register_Property_t reg_map[14] = {
{hmac_basetest::INTR_STATE_OFFSET, hmac_basetest::INTR_STATE_READ, hmac_basetest::INTR_STATE_WRITE, hmac_basetest::INTR_STATE_RESET, "INTR_STATE"}, 
{hmac_basetest::INTR_ENABLE_OFFSET, hmac_basetest::INTR_ENABLE_READ, hmac_basetest::INTR_ENABLE_WRITE, hmac_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"}, 
{hmac_basetest::INTR_TEST_OFFSET, hmac_basetest::INTR_TEST_READ, hmac_basetest::INTR_TEST_WRITE, hmac_basetest::INTR_TEST_RESET, "INTR_TEST"}, 
{hmac_basetest::ALERT_TEST_OFFSET, hmac_basetest::ALERT_TEST_READ, hmac_basetest::ALERT_TEST_WRITE, hmac_basetest::ALERT_TEST_RESET, "ALERT_TEST"}, 
{hmac_basetest::CFG_OFFSET, hmac_basetest::CFG_READ, hmac_basetest::CFG_WRITE, hmac_basetest::CFG_RESET, "CFG"}, 
{hmac_basetest::CMD_OFFSET, hmac_basetest::CMD_READ, hmac_basetest::CMD_WRITE, hmac_basetest::CMD_RESET, "CMD"}, 
{hmac_basetest::STATUS_OFFSET, hmac_basetest::STATUS_READ, hmac_basetest::STATUS_WRITE, hmac_basetest::STATUS_RESET, "STATUS"}, 
{hmac_basetest::ERR_CODE_OFFSET, hmac_basetest::ERR_CODE_READ, hmac_basetest::ERR_CODE_WRITE, hmac_basetest::ERR_CODE_RESET, "ERR_CODE"}, 
{hmac_basetest::WIPE_SECRET_OFFSET, hmac_basetest::WIPE_SECRET_READ, hmac_basetest::WIPE_SECRET_WRITE, hmac_basetest::WIPE_SECRET_RESET, "WIPE_SECRET"}, 
{hmac_basetest::KEY_OFFSET, hmac_basetest::KEY_READ, hmac_basetest::KEY_WRITE, hmac_basetest::KEY_RESET, "KEY"}, 
{hmac_basetest::DIGEST_OFFSET, hmac_basetest::DIGEST_READ, hmac_basetest::DIGEST_WRITE, hmac_basetest::DIGEST_RESET, "DIGEST"}, 
{hmac_basetest::MSG_LENGTH_LOWER_OFFSET, hmac_basetest::MSG_LENGTH_LOWER_READ, hmac_basetest::MSG_LENGTH_LOWER_WRITE, hmac_basetest::MSG_LENGTH_LOWER_RESET, "MSG_LENGTH_LOWER"}, 
{hmac_basetest::MSG_LENGTH_UPPER_OFFSET, hmac_basetest::MSG_LENGTH_UPPER_READ, hmac_basetest::MSG_LENGTH_UPPER_WRITE, hmac_basetest::MSG_LENGTH_UPPER_RESET, "MSG_LENGTH_UPPER"}, 
{hmac_basetest::MSG_FIFO_OFFSET, hmac_basetest::MSG_FIFO_READ, hmac_basetest::MSG_FIFO_WRITE, hmac_basetest::MSG_FIFO_RESET, "MSG_FIFO"}};