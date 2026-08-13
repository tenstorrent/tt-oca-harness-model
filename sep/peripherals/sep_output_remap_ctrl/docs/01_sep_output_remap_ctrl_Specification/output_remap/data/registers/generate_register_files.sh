#!/bin/bash
# Record which PeakRDL-regblock produced the RTL; the generator is installed from
# an unpinned git fork (@main), so its emitted syntax drifts between regenerations.
PEAKRDL_REGBLOCK_VERSION=$(python3 -c "from importlib.metadata import version; print(version('peakrdl-regblock'))" 2>/dev/null || echo "unknown")

stamp_peakrdl_version() {
    sed -i "2a //  PeakRDL-regblock version: ${PEAKRDL_REGBLOCK_VERSION}" "$1"
}

# Set the OCH_ROOT environment variable prior to running this script
# Usage: OCH_ROOT=/path/to/tt-och ./generate_register_files.sh
# This script generates AXI4-Lite register files for the Output Remap components

if [ -z "$OCH_ROOT" ]; then
    echo "Error: OCH_ROOT environment variable is not set"
    echo "Please set it to the tt-och root directory"
    exit 1
fi

# Fixed to AXI4-Lite only
IO_PORT_TYPE="axi4-lite-flat"

# Get REGISTER_ROOT from OCH_ROOT - pointing to output remap registers
REGISTER_ROOT=$OCH_ROOT/hw/ip/output_remap/data/registers

echo "Generating Output Remap AXI4-Lite Registers..."
echo "OCH_ROOT: $OCH_ROOT"
echo "Port Type: AXI4-Lite Flat"

RDL_DIR="$REGISTER_ROOT/rdl"

# Check if RDL directory exists
if [ ! -d "$RDL_DIR" ]; then
    echo "Error: RDL directory $RDL_DIR does not exist"
    exit 1
fi

echo "Using RDL directory: $RDL_DIR"

# Create output directories
mkdir -p $REGISTER_ROOT/rtl
mkdir -p $REGISTER_ROOT/c
mkdir -p $REGISTER_ROOT/py_headers
mkdir -p $REGISTER_ROOT/svh
mkdir -p $REGISTER_ROOT/adoc
mkdir -p $REGISTER_ROOT/html
mkdir -p $REGISTER_ROOT/rst

# Initialize arrays to track generated files
declare -a generated_rtl_files
declare -a generated_adoc_files
declare -a generated_c_files
declare -a generated_py_files
declare -a generated_svh_files
declare -a generated_html_files
declare -a generated_rst_files

echo ""
echo "=========================================="
echo "=== Generating output_remap registers ==="
echo "=========================================="

# Process all RDL files in the directory
for rdl_file in "$RDL_DIR"/*.rdl; do
    # Check if any .rdl files exist
    if [ ! -f "$rdl_file" ]; then
        echo "Warning: No .rdl files found in $RDL_DIR"
        continue
    fi

    # Extract base name without path and extension
    base_name=$(basename "$rdl_file" .rdl)

    # Skip remapped_region.rdl as it doesn't need collateral generation
    if [ "$base_name" = "remapped_region" ]; then
        echo "Skipping $base_name.rdl (no collateral generation needed)"
        continue
    fi

    echo ""
    echo "=== Processing $base_name.rdl ==="

    # Generate RTL (only for CSR registers, not the full wrapper)
    echo "Generating AXI4-Lite RTL for $base_name registers..."
    peakrdl regblock $OCH_ROOT/tools/reg_flow/regblock_udps.rdl "$rdl_file" \
        -o $REGISTER_ROOT/rtl \
        --cpuif $IO_PORT_TYPE \
        --default-reset arst_n \
        --module-name "${base_name}_reg" \
        --package-name "${base_name}_reg_pkg"
    stamp_peakrdl_version "$REGISTER_ROOT/rtl/${base_name}_reg.sv"
    generated_rtl_files+=("$REGISTER_ROOT/rtl/${base_name}_reg.sv")

    # Generate ADOC
    echo "Generating $base_name ADOC..."
    python3 $OCH_ROOT/tools/reg_flow/create_reg_asciidoc.py "$rdl_file" "$REGISTER_ROOT/adoc/${base_name}.adoc"
    generated_adoc_files+=("$REGISTER_ROOT/adoc/${base_name}.adoc")

    # Generate C headers
    echo "Generating $base_name C headers..."
    python3 $OCH_ROOT/tools/reg_flow/create_reg_c_header.py \
        -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl \
        "$rdl_file" \
        "$REGISTER_ROOT/c/${base_name}_regs.h"
    generated_c_files+=("$REGISTER_ROOT/c/${base_name}_regs.h")

    # Generate Python headers from C headers
    echo "Generating $base_name Python headers..."
    python3 $OCH_ROOT/tools/reg_flow/create_py_header_from_c_header.py \
        "$REGISTER_ROOT/c/${base_name}_regs.h" \
        "$REGISTER_ROOT/py_headers/${base_name}_reg.py"
    generated_py_files+=("$REGISTER_ROOT/py_headers/${base_name}_reg.py")

    # Generating SVH file
    echo "Generating $base_name SVH file..."
    python3 $OCH_ROOT/tools/reg_flow/create_reg_sv_header.py -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl "$rdl_file" "$REGISTER_ROOT/svh/${base_name}_reg.svh"
    generated_svh_files+=("$REGISTER_ROOT/svh/${base_name}_reg.svh")

    # Generate HTML for web documentation
    echo "Generating $base_name HTML..."
    python3 $OCH_ROOT/tools/reg_flow/create_reg_html.py \
        -t $base_name \
        -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl \
        "$rdl_file" \
        "$REGISTER_ROOT/html/${base_name}.html"
    generated_html_files+=("$REGISTER_ROOT/html/${base_name}.html")

    # Generate RST for Sphinx compatibility
    echo "Generating $base_name RST..."
    python3 $OCH_ROOT/tools/reg_flow/create_reg_rst.py \
        -t $base_name \
        -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl \
        "$rdl_file" \
        "$REGISTER_ROOT/rst/${base_name}.rst"
    generated_rst_files+=("$REGISTER_ROOT/rst/${base_name}.rst")

    echo "$base_name register generation complete!"
done

echo ""
echo "=== All Register Generation Complete! ==="
echo ""
echo "Generated files:"

# Display all generated RTL files
if [ ${#generated_rtl_files[@]} -gt 0 ]; then
    echo "RTL Files (CSRs only):"
    for file in "${generated_rtl_files[@]}"; do
        echo "  - $file"
    done
    echo ""
fi

# Display all generated ADOC files
if [ ${#generated_adoc_files[@]} -gt 0 ]; then
    echo "ADOC Documentation:"
    for file in "${generated_adoc_files[@]}"; do
        echo "  - $file"
    done
    echo ""
fi

# Display all generated C header files
if [ ${#generated_c_files[@]} -gt 0 ]; then
    echo "C Headers:"
    for file in "${generated_c_files[@]}"; do
        echo "  - $file"
    done
    echo ""
fi

# Display all generated Python header files
if [ ${#generated_py_files[@]} -gt 0 ]; then
    echo "Python Headers:"
    for file in "${generated_py_files[@]}"; do
        echo "  - $file"
    done
    echo ""
fi

# Display all generated SVH files
if [ ${#generated_svh_files[@]} -gt 0 ]; then
    echo "SystemVerilog Headers:"
    for file in "${generated_svh_files[@]}"; do
        echo "  - $file"
    done
    echo ""
fi

# Display all generated HTML files
if [ ${#generated_html_files[@]} -gt 0 ]; then
    echo "HTML Documentation:"
    for file in "${generated_html_files[@]}"; do
        echo "  - $file"
    done
    echo ""
fi

# Display all generated RST files
if [ ${#generated_rst_files[@]} -gt 0 ]; then
    echo "RST Documentation:"
    for file in "${generated_rst_files[@]}"; do
        echo "  - $file"
    done
    echo ""
fi
