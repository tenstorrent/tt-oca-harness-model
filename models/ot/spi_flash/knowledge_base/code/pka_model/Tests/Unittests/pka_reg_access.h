/***************************************************************************
 * PKA register access - C-callable interface for driver integration
 ***************************************************************************/

#ifndef PKA_REG_ACCESS_H
#define PKA_REG_ACCESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t pka_reg_read(uint32_t addr);
void pka_reg_write(uint32_t addr, uint32_t data);
void pka_reg_read_bigint(uint32_t addr, uint8_t *buffer, uint32_t length);
void pka_reg_write_bigint(uint32_t addr, const uint8_t *buffer, uint32_t length);

void pka_register_handler(void *handler);

#ifdef __cplusplus
}
#endif

#endif /* PKA_REG_ACCESS_H */
