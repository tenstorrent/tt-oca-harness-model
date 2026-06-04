/**
 * GPIO PROT Extension for TLM-2.0
 *
 * This extension carries AXI/APB PROT signals through TLM generic payloads,
 * enabling ACCESS_FILTER enforcement without requiring protocol-specific wrappers.
 *
 * Usage:
 *   gpio_prot_extension* ext = new gpio_prot_extension();
 *   ext->set_awprot(0x1);  // Privileged, secure write
 *   ext->set_arprot(0x1);  // Privileged, secure read
 *   payload.set_extension(ext);
 */

#pragma once
#include <systemc>
#include <tlm>

/**
 * TLM-2.0 Extension for AXI/APB PROT signals
 *
 * PROT bit encoding (AXI4/APB4 standard):
 * [0] - Privileged (1) vs Unprivileged (0)
 * [1] - Secure (0) vs Non-secure (1)
 * [2] - Instruction (1) vs Data (0)
 *
 * For Tenstorrent SEP GPIO:
 * - SEP access:     PROT = 0x1 (privileged, secure, data)
 * - Non-SEP access: PROT = 0x0 (unprivileged, secure, data)
 */
class gpio_prot_extension : public tlm::tlm_extension<gpio_prot_extension>
{
  public:
    gpio_prot_extension() : m_awprot(0x0), m_arprot(0x0) {}

    /**
     * Set write PROT value
     * @param awprot AWPROT[2:0] value (default 0x0 = unprivileged, secure, data)
     */
    void set_awprot(uint8_t awprot) { m_awprot = awprot & 0x7; }

    /**
     * Set read PROT value
     * @param arprot ARPROT[2:0] value (default 0x0 = unprivileged, secure, data)
     */
    void set_arprot(uint8_t arprot) { m_arprot = arprot & 0x7; }

    /**
     * Get write PROT value
     * @return AWPROT[2:0]
     */
    uint8_t get_awprot() const { return m_awprot; }

    /**
     * Get read PROT value
     * @return ARPROT[2:0]
     */
    uint8_t get_arprot() const { return m_arprot; }

    // TLM-2.0 required interface methods
    virtual tlm_extension_base* clone() const override
    {
        gpio_prot_extension* ext = new gpio_prot_extension();
        ext->m_awprot = this->m_awprot;
        ext->m_arprot = this->m_arprot;
        return ext;
    }

    virtual void copy_from(const tlm_extension_base& ext) override
    {
        const gpio_prot_extension& prot_ext = static_cast<const gpio_prot_extension&>(ext);
        m_awprot = prot_ext.m_awprot;
        m_arprot = prot_ext.m_arprot;
    }

  private:
    uint8_t m_awprot;  // Write PROT[2:0]
    uint8_t m_arprot;  // Read PROT[2:0]
};
