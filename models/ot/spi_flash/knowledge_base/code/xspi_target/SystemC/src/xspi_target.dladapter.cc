#include "scml2/pair_signal.h"
#include "SystemC/include/extension.h"
#include "SystemC/include/sfdp.h"
#include "SystemC/include/xspi_target.h"
#include "SystemC/include/xspi_targetCovermodel.h"
#include "SystemC/include/xspi_target_lib.h"
#include "tlm.h"
#include "cassert"
#include "cwr_dynamic_loader.h"
#include "cwr_sc_dynamic_stubs.h"
#include "cwr_sc_hierarch_module.h"
#include "cwr_sc_object_creator.h"
#include "scmlinc/scml_abstraction_level_switch.h"
#include "scmlinc/scml_property_registry.h"

namespace xspi_target_xspi_target_FastBuild {

using namespace conf;
using namespace std;


class xspi_targetPctWrapper : public xspi_target, public conf::component_helper_wrapper_base
{
private:
  ::xspi_targetCovermodel* _xspi_targetCovermodel;

public:
  xspi_targetPctWrapper(sc_module_name _name_)
    : xspi_target(_name_)
    , _xspi_targetCovermodel(this->createHelper<::xspi_targetCovermodel, xspi_targetPctWrapper >("coverage", this))
  {}
  virtual ~xspi_targetPctWrapper() {
    delete _xspi_targetCovermodel;
  }
};


class xspi_target0Creator : public ScObjectCreatorBase
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
      if (creationVerboseMode() >= 6) { std::cout << "xspi_target/xspi_target: STUB for " << hierach_name << " created." << std::endl; }
      conf::stub *result = new conf::stub(n);
      registerStubPorts(result, name);
      return result;
    } else {
      if (creationVerboseMode() >= 6) { std::cout << "xspi_target/xspi_target: " << hierach_name << " created." << std::endl; }
      xspi_targetPctWrapper* result = new xspi_targetPctWrapper(name.c_str());
      registerPorts(result, name);
      return result;
    }
  }
  void registerPorts(xspi_targetPctWrapper* result, const string& name) {
    string hierach_name = getHierarchicalName(name);
    cwr_sc_object_registry::inst().addTargetSocket(&result->xspi_bus, string(static_cast<sc_object*>(result)->name()) + ".xspi_bus" );
    cwr_sc_object_registry::inst().addPort(&result->reset_in, string(static_cast<sc_object*>(result)->name()) + ".reset_in" );
  }
  void registerStubPorts(conf::stub* result, const string& name) {
    string hierach_name = getHierarchicalName(name);
    cwr_sc_object_registry::inst().addTargetSocket(new conf::tlm_target_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>(std::string("xspi_bus" ).c_str()), string(static_cast<sc_object*>(result)->name()) + ".xspi_bus" );
    conf::stub_port_registrator<>::register_stub_port(&xspi_target::reset_in, "reset_in" , string(static_cast<sc_object*>(result)->name()) + ".reset_in" );
  }
};



struct DllAdapter {
  DllAdapter() {
    ScInitiatorSocketFactory::inst().addCreator ("conf::tlm_initiator_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScInitiatorSocketCreator<conf::tlm_initiator_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    ScInitiatorSocketFactory::inst().addCreator ("tlm::tlm_base_initiator_socket<64, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScInitiatorSocketCreator<tlm::tlm_base_initiator_socket<64, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    ScObjectFactory::inst().addCreator ("sc_signal<bool>", new ScPrimChannelCreator<sc_signal<bool> >());
    ScObjectFactory::inst().addCreator ("xspi_target0", new xspi_target0Creator());
    ScPortFactory::inst().addCreator ("sc_in<bool>", new ScPortCreator<sc_in<bool> >());
    ScPortFactory::inst().addCreator ("sc_inout<bool>", new ScPortCreator<sc_inout<bool> >());
    ScPortFactory::inst().addCreator ("sc_out<bool>", new ScPortCreator<sc_out<bool> >());
    ScTargetSocketFactory::inst().addCreator ("conf::tlm_target_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScTargetSocketCreator<conf::tlm_target_socket_stub<64, tlm::tlm_base_protocol_types, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    ScTargetSocketFactory::inst().addCreator ("tlm::tlm_base_target_socket<64, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND>", new ScTargetSocketCreator<tlm::tlm_base_target_socket<64, tlm::tlm_fw_transport_if<tlm::tlm_base_protocol_types>, tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types>, 1, sc_core::SC_ONE_OR_MORE_BOUND> >());
    if (::getenv("SNPS_SLS_DYNAMIC_LOADER_VERBOSE")) { std::cout << "xspi_target/xspi_target loaded. Build timestamp: " << __DATE__ << " " << __TIME__ << std::endl; }
  }
  ~DllAdapter() {
  }
  static DllAdapter sInstance;
};

DllAdapter DllAdapter::sInstance;

}
