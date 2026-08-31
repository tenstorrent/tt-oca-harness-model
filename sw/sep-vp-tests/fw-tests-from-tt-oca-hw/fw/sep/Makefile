SHELL := /usr/bin/env
.SHELLFLAGS := bash -c

# SEP firmware build entrypoint (for TTEM cgen)
#
# This Makefile is meant to be called from a wrapper script that already:
# - sourced repo bin/setup_env.sh
# - ran bender checkout
# - exported RV_ROOT and SNAPSHOT_DIR via tools/sep_el2_config.sh
#
# Targets:
#   make -C fw/sep fw_tests             # build a default list
#   make -C fw/sep fw_test TEST=hello_world    # build one
#   make -C fw/sep fw_tests TESTS="hello_world dma_test"  # build subset
#

REPO_ROOT ?= $(shell git rev-parse --show-toplevel)
OCH_ROOT  ?= $(REPO_ROOT)

FW_SEP_TESTS_DIR := $(OCH_ROOT)/fw/sep/tests

# Default test list (keep aligned with dv/sep/tb/tb_uvm/yaml/testlist_sep.yaml +FW_TEST=...).
DEFAULT_TESTS := \
	hello_world \
	aes_test \
	aes_sanity \
	sep_aes_basic_smoke_test \
	sep_aes_all_modes_test \
	sep_aes_status_flow_test \
	sep_aes_192_256_keylen_test \
	dma_test \
	efuse_sanity_csr_test \
	hmac_test \
	hmac_reg_sanity_test \
	hmac_cfg_digest_size_test \
	hmac_fifo_status_test \
	hmac_interrupt_test \
	hmac_error_handling_test \
	hmac_hmac_mode_sha256_test \
	hmac_wipe_secret_test \
	hmac_key_length_test \
	hmac_stop_continue_test \
	hmac_endian_swap_test \
	kmac_test \
	kmac_reg_sanity_test \
	kmac_sha3_256_test \
	kmac_cfg_mode_test \
	kmac_fifo_status_test \
	kmac_interrupt_test \
	kmac_error_handling_test \
	kmac_entropy_config_test \
	kmac_key_length_test \
	kmac_prefix_test \
	kmac_cfg_regwen_test \
	kmac_shake128_test \
	kmac_state_read_test \
	kmac_sw_reset_test \
	kmac_key_sideload_test \
	kmac_cshake_arbitration_test \
	memory_sanity \
	rom_sanity_test \
	otbn_loops_test \
	otbn_smoke_test \
	otbn_p256_verify_test \
	otbn_sep_integration_test \
	otbn_sw_error_test \
	otbn_rsa_3072_verify_test \
	otbn_fw_control_test \
	otbn_plic_test \
	spi_sanity \
	spi_sanity_cadence \
	spi_sanity_ot \
	spi_ot_reg_test \
	spi_ot_sanity_test \
	spi_ot_clock_config_test \
	spi_ot_tx_fifo_test \
	spi_ot_rx_fifo_test \
	spi_ot_cmd_queue_test \
	spi_ot_interrupt_test \
	spi_ot_error_handling_test \
	spi_ot_standard_spi_txrx_test \
	spi_ot_dma_trigger_test \
	spi_ot_dma_tx_test \
	spi_ot_dma_rx_test \
	spi_ot_flash_jedec_id_test \
	spi_ot_flash_read_test \
	spi_ot_flash_write_read_test \
	spi_ot_flash_sector_erase_test \
	spi_ot_flash_dual_read_test \
	spi_ot_flash_quad_read_test \
	spi_ot_quad_spi_test \
	spi_ot_dual_spi_test \
	spi_ot_accessinval_test \
	spi_ot_watermark_test \
	spi_ot_cs_timing_test \
	spi_ot_multi_segment_test \
	spi_phy_reg_test \
	spi_write_read_test \
	spi_crc_test \
	spi_xspi_dma_test \
	xspi_flash_jedec_id_test \
	xspi_flash_read_test \
	xspi_flash_write_read_test \
	xspi_flash_multi_sector_test \
	firmware_spi_dma_test \
	sram_perf_test \
	nmi_sanity_test \
	wdt_sanity_test \
	dma_hash_test \
	dma_path_test \
	sep_aes_reset_clear_test \
	sep_aes_back_to_back_test \
	local_alias_sanity \
	global_alias_remap_sanity \
	ap_stee_output_remap_test \
	wdt_threshold_jump_test \
	wdt_cfg_lock_test \
	wdt_pause_sleep_test \
	wdt_pet_reset_test \
	wdt_intr_test \
	wdt_lc_escalate_test \
	wdt_bark_bite_order_test \
	wdt_cdc_sync_test \
	wdt_intr_clear_test \
	wdt_poll_consistency_test \
	wdt_stress_all_test \
	wdt_wkup_timer_test \
	wdt_bite_before_bark_test \
	wdt_count_overflow_test \
	hamc_p1_extendmode_test \
	hmac_p2_edian_covert_test \
	hmac_p2_errorrpt_test \
	hmac_p2_fifo_stress_test \
	hmac_p2_sensreg_access_test \
	hmac_p2_stress_test \
	kmac_p2_emergency_wipe_test \
	kmac_p2_entropy_refresh_test \
	kmac_p2_shadow_protection_test \
	kmac_p2_sw_error_test \
	sep_efuse_fw_otp_rw_test \
	sep_efuse_fw_shadow_rw_test \
	sep_efuse_fw_token_match_test \
	sep_efuse_jtag_el2_cpu_mux_test \
	xspi_flash_sram_loopback_test

.PHONY: fw_tests fw_test

fw_tests:
	@set -euo pipefail; \
	TESTS_LIST="$${TESTS:-$(DEFAULT_TESTS)}"; \
	for t in $$TESTS_LIST; do \
	  $(MAKE) -C "$(FW_SEP_TESTS_DIR)/$$t" clean all; \
	done

fw_test:
	@set -euo pipefail; \
	if [[ -z "$${TEST:-}" ]]; then \
	  echo "[fw/sep] ERROR: TEST is required (e.g. make -C fw/sep fw_test TEST=hello_world)" >&2; \
	  exit 2; \
	fi; \
	$(MAKE) -C "$(FW_SEP_TESTS_DIR)/$${TEST}" clean all
