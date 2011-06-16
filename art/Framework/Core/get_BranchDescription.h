#ifndef art_Framework_Core_get_BranchDescription_h
#define art_Framework_Core_get_BranchDescription_h
////////////////////////////////////////////////////////////////////////
// Helper class to find a product's BranchDescription in the principal's
// ProductRegistry.
//
// 2011/03/15 CG,
////////////////////////////////////////////////////////////////////////

#include "art/Persistency/Provenance/BranchType.h"
#include "art/Persistency/Provenance/ProductRegistry.h"
#include "art/Utilities/TypeID.h"

#include <string>

namespace art {
  // Forward declarations.
  class ConstBranchDescription;
  class Principal;

  // 1. Get:
  // a. type info from template arg;
  // b. process name from TriggerNameService;
  // c. product registry from ConstProductRegistry.
  template <typename T>
  ConstBranchDescription const &
  get_BranchDescription(BranchType branch_type,
                        std::string const &module_label,
                        std::string const &instance_name);

  // 2. Get:
  // a. type info from template arg;
  // b. process name from principal;
  // c. product registry from principal;
  // d. branch type from principal.
  template <typename T>
  ConstBranchDescription const &
  get_BranchDescription(Principal const &principal,
                        std::string const &module_label,
                        std::string const &instance_name);

  // 3. Get:
  // a. type info from TypeID;
  // b. process name from TriggerNameService;
  // c. product registry from ConstProductRegistry.
  ConstBranchDescription const &
  get_BranchDescription(TypeID type_id,
                        BranchType branch_type,
                        std::string const &module_label,
                        std::string const &instance_name);

  // 4. Get:
  // a. type info from TypeID;
  // b. process name from principal;
  // c. product registry from principal. 
  // d. branch type from principal.
  ConstBranchDescription const &
  get_BranchDescription(TypeID type_id,
                        Principal const &principal,
                        std::string const &module_label,
                        std::string const &instance_name);

  // 5. Get:
  // a. type info from TypeID;
  // b. process name from string;
  // c. product list from reference.
  ConstBranchDescription const &
  get_BranchDescription(TypeID tid,
                        std::string const &process_name,
                        ProductRegistry::ConstProductList const &product_list,
                        BranchType branch_type,
                        std::string const &module_label,
                        std::string const &instance_name);
}

// 1
template <typename T>
inline
art::ConstBranchDescription const &
art::get_BranchDescription(BranchType branch_type,
                           std::string const &module_label,
                           std::string const &instance_name) {
  return get_BranchDescription(TypeID(typeid(T)),
                               branch_type,
                               module_label,
                               instance_name); // 3.
}

// 2.
template <typename T>
inline
art::ConstBranchDescription const &
art::get_BranchDescription(Principal const &principal,
                           std::string const &module_label,
                           std::string const &instance_name) {
  return get_BranchDescription(TypeID(typeid(T)),
                               principal,
                               module_label,
                               instance_name); // 4.
}

#endif /* art_Framework_Core_get_BranchDescription_h */

// Local Variables:
// mode: c++
// End:
