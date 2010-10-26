#include "art/Framework/Core/Path.h"

#include "art/Framework/Core/Actions.h"
#include "art/Utilities/Algorithms.h"

#include "MessageFacility/MessageLogger.h"
#include "boost/bind.hpp"

#include <algorithm>

using fhicl::ParameterSet;


namespace art {
  Path::Path(int bitpos, std::string const& path_name,
             WorkersInPath const& workers,
             TrigResPtr trptr,
             ParameterSet const&,
             ActionTable& actions,
             boost::shared_ptr<ActivityRegistry> areg,
             bool isEndPath):
    stopwatch_(new RunStopwatch::StopwatchPointer::element_type),
    timesRun_(),
    timesPassed_(),
    timesFailed_(),
    timesExcept_(),
    state_(art::hlt::Ready),
    bitpos_(bitpos),
    name_(path_name),
    trptr_(trptr),
    actReg_(areg),
    act_table_(&actions),
    workers_(workers),
    isEndPath_(isEndPath)
  {
  }

  bool
  Path::handleWorkerFailure(artZ::Exception const& e,
                            int nwrwue, bool isEvent) {
    bool should_continue = true;

    // there is no support as of yet for specific paths having
    // different exception behavior

    // If not processing an event, always rethrow.
    actions::ActionCodes action = (isEvent ? act_table_->find(e.rootCause()) : actions::Rethrow);
    assert (action != actions::FailModule);
    switch(action) {
      case actions::FailPath: {
          should_continue = false;
          mf::LogWarning(e.category())
            << "Failing path " << name_ << ", due to exception, message:\n"
            << e.what() << "\n";
          break;
      }
      default: {
          if (isEvent) ++timesExcept_;
          state_ = art::hlt::Exception;
          recordStatus(nwrwue, isEvent);
          throw art::Exception(errors::ScheduleExecutionFailure,
              "ProcessingStopped", e)
              << "Exception going through path " << name_ << "\n";
      }
    }

    return should_continue;
  }

  void
  Path::recordUnknownException(int nwrwue, bool isEvent) {
    mf::LogError("PassingThrough")
      << "Exception passing through path " << name_ << "\n";
    if (isEvent) ++timesExcept_;
    state_ = art::hlt::Exception;
    recordStatus(nwrwue, isEvent);
  }

  void
  Path::recordStatus(int nwrwue, bool isEvent) {
    if(isEvent) {
      (*trptr_)[bitpos_]=HLTPathStatus(state_, nwrwue);
    }
  }

  void
  Path::updateCounters(bool success, bool isEvent) {
    if (success) {
      if (isEvent) ++timesPassed_;
      state_ = art::hlt::Pass;
    } else {
      if(isEvent) ++timesFailed_;
      state_ = art::hlt::Fail;
    }
  }

  void
  Path::clearCounters() {
    timesRun_ = timesPassed_ = timesFailed_ = timesExcept_ = 0;
    for_all(workers_, boost::bind(&WorkerInPath::clearCounters, _1));
  }

}
