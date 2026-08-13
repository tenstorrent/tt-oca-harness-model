#include "local_alias_remap_basetest.h"

local_alias_remap_basetest::Register_Property_t reg_map[3] = {
{local_alias_remap_basetest::REGION_START_OFFSET, local_alias_remap_basetest::REGION_START_READ, local_alias_remap_basetest::REGION_START_WRITE, local_alias_remap_basetest::REGION_START_RESET, "REGION_START"}, 
{local_alias_remap_basetest::REGION_END_OFFSET, local_alias_remap_basetest::REGION_END_READ, local_alias_remap_basetest::REGION_END_WRITE, local_alias_remap_basetest::REGION_END_RESET, "REGION_END"}, 
{local_alias_remap_basetest::REGION_ATTRS_OFFSET, local_alias_remap_basetest::REGION_ATTRS_READ, local_alias_remap_basetest::REGION_ATTRS_WRITE, local_alias_remap_basetest::REGION_ATTRS_RESET, "REGION_ATTRS"}};