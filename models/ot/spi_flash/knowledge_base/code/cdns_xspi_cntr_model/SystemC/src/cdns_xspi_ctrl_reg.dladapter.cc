#include "scml2/pair_signal.h"
#include "SystemC/include/cdns_extension.h"
#include "SystemC/include/cdns_xspi_ctrl_reg.h"
#include "SystemC/include/cdns_xspi_ctrl_regCovermodel.h"
#include "SystemC/include/sfdp.h"
#include "tlm.h"
#include "cassert"
#include "cwr_dynamic_loader.h"
#include "cwr_sc_dynamic_stubs.h"
#include "cwr_sc_hierarch_module.h"
#include "cwr_sc_object_creator.h"
#include "scmlinc/scml_abstraction_level_switch.h"
#include "scmlinc/scml_property_registry.h"

namespace cdns_xspi_ctrl_reg_mylibrary__cdns_xspi_ctrl_reg_FastBuild {

using namespace conf;
using namespace std;


class cdns_xspi_ctrl_regPctWrapper : public mylibrary::cdns_xspi_ctrl_reg, public conf::component_helper_wrapper_base
{
private:
  ::mylibrary::cdns_xspi_ctrl_regCovermodel* _cdns_xspi_ctrl_regCovermodel;

public:
  cdns_xspi_ctrl_regPctWrapper(sc_module_name _name_)
    : mylibrary::cdns_xspi_ctrl_reg(_name_)
    , _cdns_xspi_ctrl_regCovermodel(this->createHelper<::mylibrary::cdns_xspi_ctrl_regCovermodel, cdns_xspi_ctrl_regPctWrapper >("coverage", this))
  {}
  virtual ~cdns_xspi_ctrl_regPctWrapper() {
    delete _cdns_xspi_ctrl_regCovermodel;
  }
};


class mylibrary__cdns_xspi_ctrl_reg0Creator : public ScObjectCreatorBase
{
public:
  static unsigned int creationVerboseMode() {
    const char * const env_var_val = ::getenv("SNPS_SLS_DYNAMIC_CREATION_VERBOSE");
    return env_var_val ? (::atoi(env_var_val)) : 3;
  }
  sc_object* create ( const string& name ) {
    string hierach_name = getHierarchicalName(name);
    if (::getenv("SNPS_VP_PRINT_PROPERTIES_FOR") != nullptr && hierach_name == ::getenv("SNPS_VP_PRINT_PROPERTIES_FOR")) scml_property_registry::inst().printPropertiesFor(hierach_name);
    if (scml_property_registry::inst().hasProperty(scml_property_registry::MODULE, scml_property_registry::BOOL, hierach_name, "runtime_disabled") && 
        scml_property_registry::inst().getBoolProperty(scml_property_registry::MODULE, hierach_name, "runtime_disabled")) {
      sc_module_name n(name.c_str());
      if (creationVerboseMode() >= 6) { std::cout << "cdns_xspi_ctrl_reg/mylibrary::cdns_xspi_ctrl_reg: STUB for " << hierach_name << " created (1.0)." << std::endl; }
      conf::stub *result = new conf::stub(n);
      registerStubPorts(result, name);
      return result;
    } else {
      if (creationVerboseMode() >= 3) { std::cout << "cdns_xspi_ctrl_reg/mylibrary::cdns_xspi_ctrl_reg: " << hierach_name << " created (1.0)." << std::endl; }
      cdns_xspi_ctrl_regPctWrapper* result = new cdns_xspi_ctrl_regPctWrapper(name.c_str());
      registerPorts(result, name);
      return result;
    }
  }
  void registerPorts(cdns_xspi_ctrl_regPctWrapper* result, const string& name) {
    string hierach_name = getHierarchicalName(name);
    cwr_sc_object_registry::inst().addTargetSocket(&result->t_reg_socket, string(static_cast<sc_object*>(result)->name()) + ".t_reg_socket" );
    cwr_sc_object_registry::inst().addPort(&result->reset_in, string(static_cast<sc_object*>(result)->name()) + ".reset_in" );
    cwr_sc_object_registry::inst().addTargetSocket(&result->t_axi_slave_socket, string(static_cast<sc_object*>(result)->name()) + ".t_axi_slave_socket" );
    cwr_sc_object_registry::inst().addInitiatorSocket(&result->i_dma_socket, string(static_cast<sc_object*>(result)->name()) + ".i_dma_socket" );
    cwr_sc_object_registry::inst().addPort(&result->int_out, string(static_cast<sc_object*>(result)->name()) + ".int_out" );
    cwr_sc_object_registry::inst().addTargetSocket(&result->PoR_input_signals, string(static_cast<sc_object*>(result)->name()) + ".PoR_input_signals" );
    {
      unsigned pctDynamicPortArraySize = 0;
      {
      int NUM_TARGETS = (int)scml_property_registry::inst().getIntProperty(scml_property_registry::MODULE, result->name(), "NUM_TARGETS");
        pctDynamicPortArraySize = NUM_TARGETS;
      }
      // coverity[dead_error_condition]
      for (unsigned port_array_index = 0; port_array_index != pctDynamicPortArraySize; ++port_array_index) {
        std::ostringstream port_array_index_tmp;
        port_array_index_tmp << "[" << port_array_index << "]";
        cwr_sc_object_registry::inst().addInitiatorSocket(&result->xspi_bus_socket[port_array_index], string(static_cast<sc_object*>(result)->name()) + ".xspi_bus_socket" + port_array_index_tmp.str());
      }
    }
  }
  void registerStubPorts(conf::stub* result, const string& name) {
    string hierach_name = getHierarchicalName(name);
    cwr_sc_object_registry::inst().addTargetSocket(new conf::tlm_target_socket_stub<32, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>(std::string("t_reg_socket" ).c_str()), string(static_cast<sc_object*>(result)->name()) + ".t_reg_socket" );
    conf::stub_port_registrator<>::register_stub_port(&mylibrary::cdns_xspi_ctrl_reg::reset_in, "reset_in" , string(static_cast<sc_object*>(result)->name()) + ".reset_in" );
    cwr_sc_object_registry::inst().addTargetSocket(new conf::tlm_target_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>(std::string("t_axi_slave_socket" ).c_str()), string(static_cast<sc_object*>(result)->name()) + ".t_axi_slave_socket" );
    cwr_sc_object_registry::inst().addInitiatorSocket(new conf::tlm_initiator_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>(std::string("i_dma_socket" ).c_str()), string(static_cast<sc_object*>(result)->name()) + ".i_dma_socket" );
    conf::stub_port_registrator<>::register_stub_port(&mylibrary::cdns_xspi_ctrl_reg::int_out, "int_out" , string(static_cast<sc_object*>(result)->name()) + ".int_out" );
    cwr_sc_object_registry::inst().addTargetSocket(new conf::tlm_target_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>(std::string("PoR_input_signals" ).c_str()), string(static_cast<sc_object*>(result)->name()) + ".PoR_input_signals" );
    {
      unsigned pctDynamicPortArraySize = 0;
      {
      int NUM_TARGETS = (int)scml_property_registry::inst().getIntProperty(scml_property_registry::MODULE, result->name(), "NUM_TARGETS");
        pctDynamicPortArraySize = NUM_TARGETS;
      }
      // coverity[dead_error_condition]
      for (unsigned port_array_index = 0; port_array_index != pctDynamicPortArraySize; ++port_array_index) {
        std::ostringstream port_array_index_tmp;
        port_array_index_tmp << "[" << port_array_index << "]";
        cwr_sc_object_registry::inst().addInitiatorSocket(new conf::tlm_initiator_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>(std::string("xspi_bus_socket" + port_array_index_tmp.str()).c_str()), string(static_cast<sc_object*>(result)->name()) + ".xspi_bus_socket" + port_array_index_tmp.str());
      }
    }
  }
};



struct DllAdapter {
  DllAdapter() {
    ScInitiatorSocketFactory::inst().addCreator ("conf::tlm_initiator_socket_stub<32, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScInitiatorSocketCreator<conf::tlm_initiator_socket_stub<32, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    ScInitiatorSocketFactory::inst().addCreator ("conf::tlm_initiator_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScInitiatorSocketCreator<conf::tlm_initiator_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    ScInitiatorSocketFactory::inst().addCreator ("tlm::tlm_base_initiator_socket<32, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScInitiatorSocketCreator<tlm::tlm_base_initiator_socket<32, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    ScInitiatorSocketFactory::inst().addCreator ("tlm::tlm_base_initiator_socket<64, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScInitiatorSocketCreator<tlm::tlm_base_initiator_socket<64, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    ScObjectFactory::inst().addCreator ("mylibrary::cdns_xspi_ctrl_reg0", new mylibrary__cdns_xspi_ctrl_reg0Creator());
    ScObjectFactory::inst().addCreator ("sc_signal<bool>", new ScPrimChannelCreator<sc_signal<bool> >());
    ScPortFactory::inst().addCreator ("sc_in<bool>", new ScPortCreator<sc_in<bool> >());
    ScPortFactory::inst().addCreator ("sc_inout<bool>", new ScPortCreator<sc_inout<bool> >());
    ScPortFactory::inst().addCreator ("sc_out<bool>", new ScPortCreator<sc_out<bool> >());
    ScTargetSocketFactory::inst().addCreator ("conf::tlm_target_socket_stub<32, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScTargetSocketCreator<conf::tlm_target_socket_stub<32, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    ScTargetSocketFactory::inst().addCreator ("conf::tlm_target_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScTargetSocketCreator<conf::tlm_target_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    ScTargetSocketFactory::inst().addCreator ("tlm::tlm_base_target_socket<32, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScTargetSocketCreator<tlm::tlm_base_target_socket<32, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    ScTargetSocketFactory::inst().addCreator ("tlm::tlm_base_target_socket<64, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScTargetSocketCreator<tlm::tlm_base_target_socket<64, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    if (::getenv("SNPS_SLS_DYNAMIC_LOADER_VERBOSE")) { std::cout << "cdns_xspi_ctrl_reg/mylibrary::cdns_xspi_ctrl_reg (1.0) loaded. Build timestamp: " << __DATE__ << " " << __TIME__ << std::endl; }
  }
  ~DllAdapter() {
  }
  static DllAdapter sInstance;
};

DllAdapter DllAdapter::sInstance;

}
