# OTBN SystemC TLM Model specification
This document specifies how the OTBN (Open Titan Big Number) accellerator is modelled in SystemC.

## Introduction
OTBN is a specialized coprocessor designed for cryptography. The interfaces, theory of operation, etc of OTBN are further described in documentation under `doc/` folder.

The SystemC model of OTBN differs from the RTL implementation in the following significant ways:
* The SystemC model has abstract TLM ports, e.g., simple target socket, instead of the TL-UL interface
* The SystemC model does NOT have a processor core or ISS (Instruction Set Simulator). Instead, it provides an API to allow an external algorithm (written in C++) access the DMEM contents and registers.
* The model does implement IMEM and DMEM - to allow the external core download the OTBN firmware and input data respectively. However, the model ignores the IMEM contents (does not execute it)
* The model implements the 32-bit CSR (Control/Status Register) and 256-bit WDR (Wide Data Registers). The OTBN core registers are not modelled.
* Instead, when the external core triggers OTBN with an "EXECUTE" command (to the `CMD` register), the OTBN core invokes the algorithm implemented in C++. The algorithm can read and write contents of DMEM and OTBN registers.
* Once the algorithm is completed, OTBN issues an interrupt to the main core.
* The model supports `RND` and `URND` registers, but does not implement the RND prefetch and single-entry cache. Writes to RND pre-fetch is ignored.

Note: The SystemC model implementation (header and source files) must have inline comments in doxygen format.

## OTBN Model interfaces

* The model has a simple target socket for CSR access (register access)
* The model has a simple target socket that is connected to an external key manager. The key manager can program the keys in the WDR registers - `KEY_S0_L`/`KEY_S0_H` and  `KEY_S1_L`/`KEY_S1_H` wide registers. As seen by the KeyManager, the LSB of `KEY_S0_L` register is at offset 0x0, followed by `KEY_S0_H`, `KEY_S1_L` and `KEY_S1_H` - in that order. 
* The model includes a reset port and a clock port
* The model implements the interface to EDN (Entropy Distribution Network) as a pure C++ interface (not SystemC). This is described further below.
* The model implements the interface to the external algorithm as a pure C++ interface (not SystemC). This is described further below.

## EDN interface and PRNG implementation

The EDN algorithm is implemented external to the model and is responsible for generating the required number of bits of entropy - which then determines the value of RND registers. The EDN algorithm derives from the following abstract base class:

```
class edn_if
{
   public:
      // Default constructor
      edn_if();

      // Fetch 'N' bytes of random bytes into `data` - caller allocates and freezes `data`
      void fill_rand(unsigned char *data, size_t N);
      
};
```

The reads to `URND` register are implemented via call to the C `rand()` function. The initial seed is set using `srand()` - and the initial seed is a configuration parameter of the model.

## Algorithm interface

The OTBN algorithm is derived from the following abstract base class:

```
class otbn_algorithm
{
   public:
      // Constructor takes dmem size
      otbn_algorithm(size_t dmem_size);

      // The following callback functions can be used by the algorithm to read (and write)
      // OTBN registers during algorithm execution

      // Register a callback function for CSR register reads
      // The callback function returns a success/error status and takes as arguments
      // the address and a data pointer (to return register contents)
      // Always reads an entire register (32 bits)
      void register_csr_read_cb(std::function<status_t<uint32_t /*Address*/, uint32_t * /*data*/>> fn);

      // Register a callback function for CSR register writes
      // The callback function returns a success/error status and takes as arguments
      // the address and write data
      // Always writes the entire register (32 bits)
      void register_csr_write_cb(std::function<status_t<uint32_t /*Address*/, uint32_t /*data*/>> fn);

      // Register a callback function for WDR register reads
      // The callback function returns a success/error status and takes as arguments
      // the address and a data pointer (to return register contents)
      // Always reads an entire register (256 bits) - data pointer points to an
      // array of 4 elements (each 64 bits)
      void register_wdr_read_cb(std::function<status_t<uint32_t /*Address*/, uint64_t * /*data*/>> fn);

      // Register a callback function for WDR register writes
      // The callback function returns a success/error status and takes as arguments
      // the address and a write data pointer
      // Always reads an entire register (256 bits) - data pointer points to an
      // array of 4 elements (each 64 bits)
      void register_wdr_write_cb(std::function<status_t<uint32_t /*Address*/, uint64_t * /*data*/>> fn);

      // Executes the main algorithm, may read and write to dmem pointer.
      // DMEM is already loaded with input data at this time, the algorithm can read
      // and write the result.
      // The pointer is allocated and freed on the caller-side.
      // May attempt to access CSR and WDR registers during execution
      // Returns SUCCESS or ERROR status.
      virtual status_t execute(char *dmem) = 0;

      // Returns a mock of estimated instructions executed by the algorithm
      // in the last run
      virtual uint64_t get_instruction_count() = 0;

      // Resets the algorithm
      virtual void reset() = 0;

      // Used to set ostream objects for printing messages
      virtual void message_objects(std::ostream &debug, std::ostream &info) = 0;
};
```
## Algorithms supported

The model should include two reference algorithms:

   * A simple summation of 'N' bytes in unsigned int notation - where `DMEM[0]` has the number 'N' of input bytes, followed by `DMEM[1]` .. `DMEM[N]` with the byte value. The number of bytes in the final result should be stored in `DMEM[N+1]`, followed by the result (sum) in `DMEM[N+2]` onwards.
   * The RSA-2048 algorithm should be implemented along with the OTBN model - using the interface defined above. Use OpenSSL as the crypto library.
