/***************************************************************************
 * PKA register access interface - implemented by test class
 ***************************************************************************/

#ifndef PKA_REG_HANDLER_H
#define PKA_REG_HANDLER_H

#include <stdint.h>

class PkaRegAccess {
public:
    virtual ~PkaRegAccess() {}
    virtual uint32_t pka_read(uint32_t addr) = 0;
    virtual void pka_write(uint32_t addr, uint32_t data) = 0;
    virtual void pka_read_bigint(uint32_t addr, uint8_t *buffer, uint32_t length) = 0;
    virtual void pka_write_bigint(uint32_t addr, const uint8_t *buffer, uint32_t length) = 0;
};

#endif /* PKA_REG_HANDLER_H */
