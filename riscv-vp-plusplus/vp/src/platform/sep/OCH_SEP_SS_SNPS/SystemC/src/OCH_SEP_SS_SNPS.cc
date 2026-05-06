/***************************************************************************
 * Copyright 1996-2025 Synopsys, Inc.
 *
 * This Synopsys software and all associated documentation are proprietary
 * to Synopsys, Inc. and may only be used pursuant to the terms and
 * conditions of a written license agreement with Synopsys, Inc.
 * All other use, reproduction, modification, or distribution of the
 * Synopsys software or the associated documentation is strictly prohibited.
 ***************************************************************************/
 

/***************************************************************************
 * Generated snippet, used for detecting user edits.
 * CHECKSUM:43b64870b9e2fe6dd2d7351a79f498b31a58e8e3
 ***************************************************************************/
 
#include "OCH_SEP_SS_SNPS.h"
#include "custom_property_server.h"


OCH_SEP_SS_SNPS::OCH_SEP_SS_SNPS(sc_core::sc_module_name name) : OCH_SEP_SS_SNPSBase(name)  {

            scml_property_server_if* old_ps = scml_property_registry::inst().getCustomPropertyServer();

            auto* const cs = custom_property_server::getInstance();
            cs->setCurrentInstanceName(std::string(this->name()) + ".");
            cs->addProperty("och_sep_ss1.targets", HART0_TARGET);
            cs->addProperty("och_sep_ss1.configFile", HART0_CONFIG_FILE);
            cs->addProperty("och_sep_ss1.traceFile", HART0_TRACEFILE);
            cs->addProperty("och_sep_ss1.instFreqFile", HART0_INSTFREQFILE);
            cs->addProperty("och_sep_ss1.regInits", HART0_REGINITS);
            cs->addProperty("och_sep_ss1.gdbTcpPort", HART0_GDBTCPPORT);
            cs->addProperty("och_sep_ss1.trace", HART0_TRACE);
            cs->addProperty("och_sep_ss1.verbose", HART0_VERBOSE);
            cs->addProperty("och_sep_ss1.traceLdSt", HART0_TRACELDST);
            cs->addProperty("och_sep_ss1.csv", HART0_CSV);
            cs->addProperty("och_sep_ss1.triggers", HART0_TRIGGERS);
            cs->addProperty("och_sep_ss1.counters", HART0_COUNTERS);
            cs->addProperty("och_sep_ss1.gdb", HART0_GDB);
            cs->addProperty("och_sep_ss1.otbn.algorithm_type",OTBN_ALGORITHM_TYPE);

            cs->addProperty("och_sep_ss1.uart.verbosity", UART_VERBOSITY);
            cs->addProperty("och_sep_ss1.uart.uart_inst.verbosity", UART_INST_VERBOSITY);
            cs->addProperty("och_sep_ss1.uartTcpPort", UART_TCP_PORT);

            cs->addProperty("och_sep_ss1.gpio.verbosity", GPIO_VERBOSITY);

            cs->addProperty("och_sep_ss1.dma.verbosity", DMA_VERBOSITY);

            cs->addProperty("och_sep_ss1.csrng.verbosity", CRNG_VERBOSITY);
            cs->addProperty("och_sep_ss1.csrng.nhwapp", CRNG_NHWAPP);

            cs->addProperty("och_sep_ss1.hmac.verbosity", HMAC_VERBOSITY);
            cs->addProperty("och_sep_ss1.kmac.verbosity", KMAC_VERBOSITY);
            cs->addProperty("och_sep_ss1.aes.verbosity", AES_VERBOSITY);
            cs->addProperty("och_sep_ss1.mailbox.verbosity", MAILBOX_VERBOSITY);
            cs->addProperty("och_sep_ss1.aon_timer.verbosity", AON_TIMER_VERBOSITY);
            cs->addProperty("och_sep_ss1.sep_efuse.verbosity", SEP_EFUSE_VERBOSITY);
            cs->addProperty("och_sep_ss1.lc_ctrl.verbosity", LC_CTRL_VERBOSITY);

			cs->addProperty("och_sep_ss1.lc_ctrl.lc_state", LC_CTRL_LC_STATE);
			cs->addProperty("och_sep_ss1.lc_ctrl.sip_dis_lo", LC_CTRL_SIP_DIS_LO);
			cs->addProperty("och_sep_ss1.lc_ctrl.sip_dis_hi", LC_CTRL_SIP_DIS_HI);
			cs->addProperty("och_sep_ss1.lc_ctrl.sys_dis_lo", LC_CTRL_SYS_DIS_LO);
			cs->addProperty("och_sep_ss1.lc_ctrl.sys_dis_hi", LC_CTRL_SYS_DIS_HI);
			cs->addProperty("och_sep_ss1.lc_ctrl.security_disable", LC_CTRL_SECURITY_DISABLE);
			cs->addProperty("och_sep_ss1.lc_ctrl.secure_tm", LC_CTRL_SECURE_TM);

			cs->addProperty("och_sep_ss1.sep_efuse.lc_state", SEP_EFUSE_LC_STATE);
			cs->addProperty("och_sep_ss1.sep_efuse.sboot_dis", SEP_EFUSE_SBOOT_DIS);
			cs->addProperty("och_sep_ss1.sep_efuse.transient_rma_en", SEP_EFUSE_TRANSIENT_RMA_EN);
			cs->addProperty("och_sep_ss1.sep_efuse.sip_dis_lo", SEP_EFUSE_SIP_DIS_LO);
			cs->addProperty("och_sep_ss1.sep_efuse.sip_dis_hi", SEP_EFUSE_SIP_DIS_HI);
			cs->addProperty("och_sep_ss1.sep_efuse.sys_dis_lo", SEP_EFUSE_SYS_DIS_LO);
			cs->addProperty("och_sep_ss1.sep_efuse.sys_dis_hi", SEP_EFUSE_SYS_DIS_HI);
			cs->addProperty("och_sep_ss1.sep_efuse.chiplet_pubk_revoke", SEP_EFUSE_CHIPLET_PUBK_REVOKE);
			cs->addProperty("och_sep_ss1.sep_efuse.status_rpt", SEP_EFUSE_STATUS_RPT);
			cs->addProperty("och_sep_ss1.sep_efuse.sep_rom_ctrl", SEP_EFUSE_SEP_ROM_CTRL);
			cs->addProperty("och_sep_ss1.sep_efuse.sep_spi_ctrl_field_en", SEP_EFUSE_SEP_SPI_CTRL_FIELD_EN);
			cs->addProperty("och_sep_ss1.sep_efuse.spi_discovery_ctrl", SEP_EFUSE_SPI_DISCOVERY_CTRL);
			cs->addProperty("och_sep_ss1.sep_efuse.spi_phy_dq_timing", SEP_EFUSE_SPI_PHY_DQ_TIMING);
			cs->addProperty("och_sep_ss1.sep_efuse.spi_phy_dqs_timing", SEP_EFUSE_SPI_PHY_DQS_TIMING);
			cs->addProperty("och_sep_ss1.sep_efuse.spi_phy_gate_lpbk", SEP_EFUSE_SPI_PHY_GATE_LPBK);
			cs->addProperty("och_sep_ss1.sep_efuse.spi_phy_dll_slave", SEP_EFUSE_SPI_PHY_DLL_SLAVE);
			cs->addProperty("och_sep_ss1.sep_efuse.spi_phy_dll_master", SEP_EFUSE_SPI_PHY_DLL_MASTER);
			cs->addProperty("och_sep_ss1.sep_efuse.spi_phy_misc", SEP_EFUSE_SPI_PHY_MISC);
			cs->addProperty("och_sep_ss1.sep_efuse.spi_rb_valid_time", SEP_EFUSE_SPI_RB_VALID_TIME);
			cs->addProperty("och_sep_ss1.sep_efuse.rma_sip_token_match", SEP_EFUSE_RMA_SIP_TOKEN_MATCH);
			cs->addProperty("och_sep_ss1.sep_efuse.rma_chiplet_token_match", SEP_EFUSE_RMA_CHIPLET_TOKEN_MATCH);
			cs->addProperty("och_sep_ss1.sep_efuse.sec_disable_token_match", SEP_EFUSE_SEC_DISABLE_TOKEN_MATCH);
			cs->addProperty("och_sep_ss1.sep_efuse.rma_sip_token", SEP_EFUSE_RMA_SIP_TOKEN);
			cs->addProperty("och_sep_ss1.sep_efuse.rma_chiplet_token", SEP_EFUSE_RMA_CHIPLET_TOKEN);
			cs->addProperty("och_sep_ss1.sep_efuse.class_key", SEP_EFUSE_CLASS_KEY);
			cs->addProperty("och_sep_ss1.sep_efuse.bl1_version", SEP_EFUSE_BL1_VERSION);
			cs->addProperty("och_sep_ss1.sep_efuse.bl2_version", SEP_EFUSE_BL2_VERSION);
			cs->addProperty("och_sep_ss1.sep_efuse.chiplet_uid", SEP_EFUSE_CHIPLET_UID);
			cs->addProperty("och_sep_ss1.sep_efuse.sip_uid", SEP_EFUSE_SIP_UID);
			cs->addProperty("och_sep_ss1.sep_efuse.sys_uid", SEP_EFUSE_SYS_UID);
			cs->addProperty("och_sep_ss1.sep_efuse.sip_pubk", SEP_EFUSE_SIP_PUBK);
			cs->addProperty("och_sep_ss1.sep_efuse.sys_pubk", SEP_EFUSE_SYS_PUBK);
			cs->addProperty("och_sep_ss1.sep_efuse.public_key_0", SEP_EFUSE_PUBLIC_KEY_0);
			cs->addProperty("och_sep_ss1.sep_efuse.public_key_1", SEP_EFUSE_PUBLIC_KEY_1);

            cs->addProperty("och_sep_ss1.fuse_map.verbosity", FUSE_MAP_VERBOSITY);

            cs->addProperty("och_sep_ss1.spi_controller.verbosity", SPI_VERBOSITY);

            cs->addProperty("och_sep_ss1.otbn.verbosity", OTBN_VERBOSITY);

            cs->addProperty("och_sep_ss1.rv31imc.verbosity", VEEREL2_VERBOSITY);
			cs->addProperty("och_sep_ss1.rv31imc.instrBatchSize", VEEREL2_INSTRBATCHSIZE);
			cs->addProperty("och_sep_ss1.rv31imc.resetMemoryMappedRegister", VEEREL2_RESETMEMORYMAPPEDREGISTER);
			cs->addProperty("och_sep_ss1.rv31imc.enableNmi", VEEREL2_ENABLENMI);
			cs->addProperty("och_sep_ss1.globalQuantumNs", GLOBAL_QUANTUM_NS);

            scml_property_registry::inst().setCustomPropertyServer(cs);

	        och_sep_ss1 = new och_sep_ss("och_sep_ss1");

            // Restore the property server to the old value
	        scml_property_registry::inst().setCustomPropertyServer(old_ps);
}



