#pragma once
#include <systemc.h>

/**
 * @file otbn_interfaces.h
 * @brief Abstract interface classes for OTBN port interfaces
 *
 * This file defines the abstract interface classes used for OTBN's external
 * communication ports including Key Manager, OTP, and Life Cycle Controller
 * interfaces. Each interface uses the sc_port/sc_export pattern for bidirectional
 * communication.
 *
 * Note: EDN (Entropy Distribution Network) interfaces were removed. The VP
 * does not model EDN — entropy is sourced from rand()/OpenSSL directly inside
 * the model.
 */

// ============================================================================
// OTP Controller Interface
// ============================================================================

/**
 * @brief OTP Key Request Interface
 * Used to request scrambling keys and configuration from OTP.
 * Model uses sc_port, Test uses sc_export.
 */
class otp_key_req_if : public virtual sc_interface {
public:

    //Request scrambling key from OTP
    virtual void request_scramble_key() = 0;
};

/**
 * @brief OTP Key Response Interface
 * Provides 128-bit scrambling key, nonce, and seed.
 * Model uses sc_export, Test uses sc_port.
 */
class otp_key_rsp_if : public virtual sc_interface {
public:

    //Check if scrambling key is available
    virtual bool key_available() = 0;

    // Get 128-bit scrambling key with nonce and seed

    virtual void get_scramble_key(uint32_t key[4], uint32_t& nonce, uint32_t& seed) = 0;
};

// ============================================================================
// Life Cycle Controller Interface
// ============================================================================

/**
 * @brief Life Cycle Controller Request Interface
 * Used for both escalation and RMA (Return Merchandise Authorization) signals.
 * Model uses sc_port, Test uses sc_export.
 */
class lc_ctrl_req_if : public virtual sc_interface {
public:
    //Check if LC signal is asserted
    virtual bool is_asserted() = 0;
};

/**
 * @brief Life Cycle Controller Response Interface
 * Provides acknowledgment for LC signals.
 * Model uses sc_export, Test uses sc_port.
 */
class lc_ctrl_rsp_if : public virtual sc_interface {
public:

    //Acknowledge LC signal
    virtual void acknowledge() = 0;
};
