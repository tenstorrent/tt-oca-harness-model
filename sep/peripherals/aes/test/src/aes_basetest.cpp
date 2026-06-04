#include "aes_basetest.h"

aes_basetest::Register_Property_t reg_map[11] = {
{aes_basetest::ALERT_TEST_OFFSET, aes_basetest::ALERT_TEST_READ, aes_basetest::ALERT_TEST_WRITE, aes_basetest::ALERT_TEST_RESET, "ALERT_TEST"}, 
{aes_basetest::KEY_SHARE0_OFFSET, aes_basetest::KEY_SHARE0_READ, aes_basetest::KEY_SHARE0_WRITE, aes_basetest::KEY_SHARE0_RESET, "KEY_SHARE0"}, 
{aes_basetest::KEY_SHARE1_OFFSET, aes_basetest::KEY_SHARE1_READ, aes_basetest::KEY_SHARE1_WRITE, aes_basetest::KEY_SHARE1_RESET, "KEY_SHARE1"}, 
{aes_basetest::IV_OFFSET, aes_basetest::IV_READ, aes_basetest::IV_WRITE, aes_basetest::IV_RESET, "IV"}, 
{aes_basetest::DATA_IN_OFFSET, aes_basetest::DATA_IN_READ, aes_basetest::DATA_IN_WRITE, aes_basetest::DATA_IN_RESET, "DATA_IN"}, 
{aes_basetest::DATA_OUT_OFFSET, aes_basetest::DATA_OUT_READ, aes_basetest::DATA_OUT_WRITE, aes_basetest::DATA_OUT_RESET, "DATA_OUT"}, 
{aes_basetest::CTRL_SHADOWED_OFFSET, aes_basetest::CTRL_SHADOWED_READ, aes_basetest::CTRL_SHADOWED_WRITE, aes_basetest::CTRL_SHADOWED_RESET, "CTRL_SHADOWED"}, 
{aes_basetest::CTRL_AUX_SHADOWED_OFFSET, aes_basetest::CTRL_AUX_SHADOWED_READ, aes_basetest::CTRL_AUX_SHADOWED_WRITE, aes_basetest::CTRL_AUX_SHADOWED_RESET, "CTRL_AUX_SHADOWED"}, 
{aes_basetest::CTRL_AUX_REGWEN_OFFSET, aes_basetest::CTRL_AUX_REGWEN_READ, aes_basetest::CTRL_AUX_REGWEN_WRITE, aes_basetest::CTRL_AUX_REGWEN_RESET, "CTRL_AUX_REGWEN"}, 
{aes_basetest::TRIGGER_OFFSET, aes_basetest::TRIGGER_READ, aes_basetest::TRIGGER_WRITE, aes_basetest::TRIGGER_RESET, "TRIGGER"}, 
{aes_basetest::STATUS_OFFSET, aes_basetest::STATUS_READ, aes_basetest::STATUS_WRITE, aes_basetest::STATUS_RESET, "STATUS"}};