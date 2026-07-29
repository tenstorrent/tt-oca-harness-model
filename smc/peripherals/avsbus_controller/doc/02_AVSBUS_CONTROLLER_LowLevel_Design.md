# AVSBus Controller — Low-Level Design

## 1. Module API

```
namespace smc {
class avsbus_controller : public sc_module {
  tlm_utils::simple_target_socket<avsbus_controller> reg_socket; // 32-bit
  sc_in<bool>  rst_n_i;
  sc_out<bool> irq_o;
  sc_out<bool> avs_gpio_enable_o;

  void set_slave_model(avs_slave_model_fn);
  void inject_interrupt(unsigned bit);
  uint32_t dbg_reg(uint64_t off) const;
  static uint8_t crc3(uint32_t msg);
};
}
```

## 2. Internal architecture

| Block | Mechanism |
|-------|-----------|
| Register decode | Hybrid: `regmodel::RegisterMap32` for RW storage; switch for FIFO / status / W1C |
| Command FIFO | `std::deque<uint32_t>`, depth = CCI `command_fifo_depth` (default 8) |
| Readback FIFO | `std::deque<uint32_t>`, depth = CCI `readback_fifo_depth` (default 8) |
| Protocol engine | `SC_THREAD(xfer_thread)` woken by `xfer_event_` after `xfer_delay_ns` |
| Resync | `FORCE_SLAVE_RESYNC` → `slave_in_resync_` for `resync_delay_ns` |
| IRQ | Sticky `irq_status_`; `recompute_method` sole driver of `irq_o` |
| GPIO enable | Mirror of `AVS_CONFIG[0]` on `avs_gpio_enable_o` |

## 3. TLM contract

- 32-bit aligned accesses only; wrong size → `TLM_BURST_ERROR_RESPONSE`
- Outside window / unaligned / decode gap → `TLM_ADDRESS_ERROR_RESPONSE`
- `b_transport` never `wait()`; adds `access_delay_ns` to annotated delay

## 4. CCI parameters

| Name | Default | Mutability |
|------|---------|------------|
| `command_fifo_depth` | 8 | immutable |
| `readback_fifo_depth` | 8 | immutable |
| `access_delay_ns` | 2.0 | mutable |
| `xfer_delay_ns` | 50.0 | mutable |
| `resync_delay_ns` | 20.0 | mutable |

## 5. Default slave responder

When no `set_slave_model` is installed, reads return canned values (e.g. voltage
1000 mV = `0x03E8`) with `SLAVE_ACK=OK` and `STATUS_RESPONSE` = VDone|AVS_Control.
Writes return `CMD_DATA=0xFFFF`.
