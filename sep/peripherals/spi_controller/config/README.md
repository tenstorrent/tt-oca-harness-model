# SPI Controller Configuration Files

This directory contains JSON configuration files for the SPI Controller IP using SystemC CCI (Configuration, Control and Inspection).

## Available Configurations

### `spi_controller_default.json`
Default configuration matching datasheet specifications:
- NumCS: 1 (single chip select)
- TxDepth: 72 words (288 bytes)
- RxDepth: 64 words (256 bytes)
- ByteOrder: Little-Endian
- CmdDepth: 4

### `spi_controller_multi_device.json`
Configuration for multi-device testing:
- NumCS: 2 (two chip selects)
- TxDepth: 72 words
- RxDepth: 64 words
- ByteOrder: Little-Endian
- CmdDepth: 4

### `spi_controller_large_fifo.json`
Configuration with larger FIFOs for high-throughput scenarios:
- NumCS: 1
- TxDepth: 128 words (512 bytes)
- RxDepth: 128 words (512 bytes)
- ByteOrder: Little-Endian
- CmdDepth: 8

## Usage

### Run with Default Configuration (Programmatic)
```bash
./bin/out
```
Uses programmatic configuration in testbench.cpp (NumCS=2, TxDepth=72, RxDepth=64)

### Run with JSON Configuration
```bash
./bin/out config/spi_controller_default.json
./bin/out config/spi_controller_multi_device.json
./bin/out config/spi_controller_large_fifo.json
```

## Creating Custom Configurations

Create a new JSON file following this template:

```json
{
  "testbench": {
    "spi_controller_dut": {
      "NumCS": 4,
      "TxDepth": 256,
      "RxDepth": 256,
      "ByteOrder": true,
      "CmdDepth": 16
    }
  }
}
```

## Parameter Descriptions

| Parameter | Type | Description |
|-----------|------|-------------|
| NumCS | uint32_t | Number of chip select lines (1-8) |
| TxDepth | uint32_t | TX FIFO depth in 32-bit words |
| RxDepth | uint32_t | RX FIFO depth in 32-bit words |
| ByteOrder | bool | true=Little-Endian, false=Big-Endian |
| CmdDepth | uint32_t | Command FIFO depth |

## Hierarchical Path Format

CCI uses hierarchical naming based on the SystemC module hierarchy:
```
testbench.spi_controller_dut.<parameter_name>
```

Where:
- `testbench` = top-level testbench module name
- `spi_controller_dut` = SPI Controller DUT instance name
- `<parameter_name>` = configuration parameter (NumCS, TxDepth, etc.)

## Integration Example

For SoC integration with multiple SPI Controller instances:

```json
{
  "top_soc": {
    "peripheral_subsystem": {
      "spi_controller_flash": {
        "NumCS": 1,
        "TxDepth": 128,
        "RxDepth": 128
      },
      "spi_controller_sensors": {
        "NumCS": 4,
        "TxDepth": 32,
        "RxDepth": 32
      }
    }
  }
}
```

## Notes

- JSON configuration files override programmatic configuration
- Configuration must be set before SystemC elaboration phase
- Invalid configurations may cause runtime errors
- All parameters have default values if not specified
