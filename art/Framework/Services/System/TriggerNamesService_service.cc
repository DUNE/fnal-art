// ======================================================================
//
// TriggerNamesService
//
// ======================================================================

#include "art/Framework/Services/System/TriggerNamesService.h"

#include "art/Persistency/Common/TriggerResults.h"
#include "art/Utilities/Exception.h"
#include "cetlib/container_algorithms.h"
#include "cetlib/exception.h"
#include "fhiclcpp/ParameterSetRegistry.h"

using art::TriggerNamesService;
using namespace cet;
using namespace fhicl;
using namespace std;

// ----------------------------------------------------------------------

static  ParameterSet   empty_pset;
static  vector<string> empty_svec;

// ----------------------------------------------------------------------

TriggerNamesService::TriggerNamesService(ParameterSet const & procPS,
                                         std::vector<std::string> const & trigger_path_names)
  :
  trignames_(trigger_path_names),
  trigpos_(),
  end_names_(),
  end_pos_(),
  trigger_pset_(),
  modulenames_(),
  process_name_(procPS.get<string>("process_name")),
  wantSummary_(procPS.get<bool>("services.scheduler.wantSummary", false))
{
  // Make and hold onto a parameter set for posterity.
  trigger_pset_.put("trigger_paths",  trignames_);
  ParameterSetRegistry::put(trigger_pset_);

  ParameterSet physics = procPS.get<ParameterSet>("physics", empty_pset);
  end_names_ = physics.get<vector<string> >("end_paths", empty_svec);
  loadPosMap(trigpos_, trignames_);
  loadPosMap(end_pos_, end_names_);
  unsigned int const n(trignames_.size());
  for (unsigned int i = 0; i != n; ++i) {
    modulenames_.push_back(physics.get<vector<string> >(trignames_[i]));
  }
}  // c'tor

// ----------------------------------------------------------------------

bool
TriggerNamesService::getTrigPaths(TriggerResults const & triggerResults,
                                  Strings & trigPaths) const
{
  ParameterSet pset;
  if (! ParameterSetRegistry::get(triggerResults.parameterSetID(), pset))
  { return false; }
  trigPaths = pset.get<vector<string> >("trigger_paths", Strings());
  if (trigPaths.size() != triggerResults.size()) {
    throw art::Exception(art::errors::Unknown)
        << "TriggerNamesService::getTrigPaths, Trigger names vector and\n"
        "TriggerResults are different sizes.  This should be impossible,\n"
        "please send information to reproduce this problem to\n"
        "the ART developers.\n";
  }
  return true;
}  // getTrigPaths()

// ======================================================================
PROVIDE_FILE_PATH()
// ======================================================================
