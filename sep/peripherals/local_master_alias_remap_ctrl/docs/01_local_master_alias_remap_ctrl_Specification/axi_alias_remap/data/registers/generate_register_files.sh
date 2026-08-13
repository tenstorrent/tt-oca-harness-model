#!/bin/bash
# Record which PeakRDL-regblock produced the RTL; the generator is installed from
# an unpinned git fork (@main), so its emitted syntax drifts between regenerations.
PEAKRDL_REGBLOCK_VERSION=$(python3 -c "from importlib.metadata import version; print(version('peakrdl-regblock'))" 2>/dev/null || echo "unknown")

stamp_peakrdl_version() {
    sed -i "2a //  PeakRDL-regblock version: ${PEAKRDL_REGBLOCK_VERSION}" "$1"
}

# Set the OCH_ROOT environment variable to the repo's root before sourcing this script

export ALIAS_REMAP_ROOT=$OCH_ROOT/hw/ip/axi_alias_remap

mkdir -p $ALIAS_REMAP_ROOT/data/registers/rtl
peakrdl regblock $OCH_ROOT/tools/reg_flow/regblock_udps.rdl $ALIAS_REMAP_ROOT/data/registers/rdl/alias_remap.rdl    -o $ALIAS_REMAP_ROOT/data/registers/rtl --cpuif axi4-lite-flat --default-reset arst_n --module-name alias_remap_reg    --package-name alias_remap_reg_pkg
stamp_peakrdl_version "$ALIAS_REMAP_ROOT/data/registers/rtl/alias_remap_reg.sv"

mkdir -p $ALIAS_REMAP_ROOT/data/registers/adoc
python3 $OCH_ROOT/tools/reg_flow/create_reg_asciidoc.py $ALIAS_REMAP_ROOT/data/registers/rdl/alias_remap.rdl    $ALIAS_REMAP_ROOT/data/registers/adoc/alias_remap_reg.adoc

mkdir -p $ALIAS_REMAP_ROOT/data/registers/html
python3 $OCH_ROOT/tools/reg_flow/create_reg_html.py -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl -t alias_remap -i $ALIAS_REMAP_ROOT/data/registers/rdl $ALIAS_REMAP_ROOT/data/registers/rdl/alias_remap.rdl $ALIAS_REMAP_ROOT/data/registers/html/alias_remap_reg.html

mkdir -p $ALIAS_REMAP_ROOT/data/registers/rst
python3 $OCH_ROOT/tools/reg_flow/create_reg_rst.py -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl -t alias_remap $ALIAS_REMAP_ROOT/data/registers/rdl/alias_remap.rdl $ALIAS_REMAP_ROOT/data/registers/rst/alias_remap_reg.rst

mkdir -p $ALIAS_REMAP_ROOT/data/registers/svh
python3 $OCH_ROOT/tools/reg_flow/create_reg_sv_header.py -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl $ALIAS_REMAP_ROOT/data/registers/rdl/alias_remap.rdl    $ALIAS_REMAP_ROOT/data/registers/svh/alias_remap_reg.svh

mkdir -p $ALIAS_REMAP_ROOT/data/registers/c
python3 $OCH_ROOT/tools/reg_flow/create_reg_c_header.py -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl $ALIAS_REMAP_ROOT/data/registers/rdl/alias_remap.rdl    $ALIAS_REMAP_ROOT/data/registers/c/alias_remap_reg.h

mkdir -p $ALIAS_REMAP_ROOT/data/registers/py_headers
python3 $OCH_ROOT/tools/reg_flow/create_py_header_from_c_header.py $ALIAS_REMAP_ROOT/data/registers/c/alias_remap_reg.h    $ALIAS_REMAP_ROOT/data/registers/py_headers/alias_remap_reg.py
