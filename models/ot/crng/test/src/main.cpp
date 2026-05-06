#include <systemc.h>
#include "testbench.h"

// gcov coverage data flushing (GCC 11+)
// Required when using std::quick_exit() to ensure .gcda files are written
#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

int sc_main(int argc, char* argv[])
{
    // Initialize CCI broker and optionally load INI config file.
    load_config_file(argc > 1 ? argv[1] : nullptr);

    testbench tb("testbench");

    CSML_INFO(1, tb.logger) << "Starting CRNG Testbench";
    sc_start();
    CSML_INFO(1, tb.logger) << "Simulation completed";

#ifdef ACCELLERA_CCI_STD
#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    std::quick_exit(0);
#endif
    return 0;
}
