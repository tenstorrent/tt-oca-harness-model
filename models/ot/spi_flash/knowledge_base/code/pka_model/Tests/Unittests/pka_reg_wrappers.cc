/***************************************************************************
 * PKA register access - C++ implementation, dispatches to test class
 ***************************************************************************/

#include "pka_reg_access.h"
#include "pka_reg_handler.h"
#include <cstdio>

static PkaRegAccess *g_pka_handler = nullptr;

void pka_register_handler(void *handler)
{
    g_pka_handler = static_cast<PkaRegAccess *>(handler);
    fprintf(stderr, "pka_register_handler: g_pka_handler=%p\n", (void*)g_pka_handler);
}

uint32_t pka_reg_read(uint32_t addr)
{
    if (!g_pka_handler)
        return 0;
    return g_pka_handler->pka_read(addr);
}

void pka_reg_write(uint32_t addr, uint32_t data)
{
    fprintf(stderr, "pka_reg_write called: addr=0x%x data=0x%x g_pka_handler=%p\n", addr, data, (void*)g_pka_handler);
    if (g_pka_handler)
        g_pka_handler->pka_write(addr, data);
}

void pka_reg_read_bigint(uint32_t addr, uint8_t *buffer, uint32_t length)
{
    if (g_pka_handler)
        g_pka_handler->pka_read_bigint(addr, buffer, length);
}

void pka_reg_write_bigint(uint32_t addr, const uint8_t *buffer, uint32_t length)
{
    fprintf(stderr, "pka_reg_write_bigint called: addr=0x%x length=%u g_pka_handler=%p\n", addr, (unsigned)length, (void*)g_pka_handler);
    if (g_pka_handler)
        g_pka_handler->pka_write_bigint(addr, buffer, length);
}
