#!/bin/bash
# Set the OCH_ROOT environment variable to the repo's root before sourcing this script

export ENTROPY_SOURCE_ROOT=$OCH_ROOT/hw/ip/entropy_source

mkdir -p $ENTROPY_SOURCE_ROOT/data/registers/rtl
peakrdl regblock $OCH_ROOT/tools/reg_flow/regblock_udps.rdl $ENTROPY_SOURCE_ROOT/data/registers/rdl/entropy_source.rdl -o $ENTROPY_SOURCE_ROOT/data/registers/rtl --cpuif axi4-lite --default-reset arst_n --module-name entropy_source_reg --package-name entropy_source_reg_pkg

mkdir -p $ENTROPY_SOURCE_ROOT/data/registers/adoc
python3 $OCH_ROOT/tools/reg_flow/create_reg_asciidoc.py $ENTROPY_SOURCE_ROOT/data/registers/rdl/entropy_source.rdl $ENTROPY_SOURCE_ROOT/data/registers/adoc/entropy_source_reg.adoc

mkdir -p $ENTROPY_SOURCE_ROOT/data/registers/html
python3 $OCH_ROOT/tools/reg_flow/create_reg_html.py -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl -t entropy_source -i $ENTROPY_SOURCE_ROOT/data/registers/rdl $ENTROPY_SOURCE_ROOT/data/registers/rdl/entropy_source.rdl $ENTROPY_SOURCE_ROOT/data/registers/html/entropy_source_reg.html

mkdir -p $ENTROPY_SOURCE_ROOT/data/registers/rst
python3 $OCH_ROOT/tools/reg_flow/create_reg_rst.py -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl -t entropy_source $ENTROPY_SOURCE_ROOT/data/registers/rdl/entropy_source.rdl $ENTROPY_SOURCE_ROOT/data/registers/rst/entropy_source_reg.rst

mkdir -p $ENTROPY_SOURCE_ROOT/data/registers/svh
python3 $OCH_ROOT/tools/reg_flow/create_reg_sv_header.py -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl $ENTROPY_SOURCE_ROOT/data/registers/rdl/entropy_source.rdl $ENTROPY_SOURCE_ROOT/data/registers/svh/entropy_source_reg.svh

mkdir -p $ENTROPY_SOURCE_ROOT/data/registers/c
python3 $OCH_ROOT/tools/reg_flow/create_reg_c_header.py -u $OCH_ROOT/tools/reg_flow/regblock_udps.rdl $ENTROPY_SOURCE_ROOT/data/registers/rdl/entropy_source.rdl $ENTROPY_SOURCE_ROOT/data/registers/c/entropy_source_reg.h

mkdir -p $ENTROPY_SOURCE_ROOT/data/registers/py_headers
python3 $OCH_ROOT/tools/reg_flow/create_py_header_from_c_header.py $ENTROPY_SOURCE_ROOT/data/registers/c/entropy_source_reg.h $ENTROPY_SOURCE_ROOT/data/registers/py_headers/entropy_source_reg.py

# Copy generated RTL files to main rtl directory
echo "Copying generated RTL files to ../../rtl/"
cp rtl/entropy_source_reg.sv ../../rtl/
cp rtl/entropy_source_reg_pkg.sv ../../rtl/
echo "RTL files copied successfully"
