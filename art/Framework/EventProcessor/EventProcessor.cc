#include "art/Framework/EventProcessor/EventProcessor.h"

#include "art/Framework/Core/Breakpoints.h"
#include "art/Framework/Core/DecrepitRelicInputSourceImplementation.h"
#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/Core/InputSource.h"
#include "art/Framework/Core/InputSourceDescription.h"
#include "art/Framework/Core/InputSourceFactory.h"
#include "art/Framework/EventProcessor/EPStates.h"
#include "art/Framework/EventProcessor/detail/writeSummary.h"
#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Principal/OccurrenceTraits.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "art/Framework/Services/Optional/RandomNumberGenerator.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Services/Registry/ServiceRegistry.h"
#include "art/Framework/Services/System/CurrentModule.h"
#include "art/Framework/Services/System/FileCatalogMetadata.h"
#include "art/Framework/Services/System/FloatingPointControl.h"
#include "art/Framework/Services/System/PathSelection.h"
#include "art/Framework/Services/System/ScheduleContext.h"
#include "art/Framework/Services/System/TriggerNamesService.h"
#include "art/Persistency/Provenance/BranchIDListHelper.h"
#include "art/Persistency/Provenance/BranchType.h"
#include "art/Persistency/Provenance/ProcessConfiguration.h"
#include "art/Utilities/DebugMacros.h"
#include "art/Utilities/Exception.h"
#include "art/Utilities/GetPassID.h"
#include "art/Utilities/ScheduleID.h"
#include "art/Utilities/UnixSignalHandlers.h"
#include "art/Version/GetReleaseVersion.h"
#include "boost/thread/xtime.hpp"
#include "cetlib/exception_collector.h"
#include "cetlib/container_algorithms.h"
#include "cpp0x/utility"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using fhicl::ParameterSet;

namespace {
  // Most signals.
  class SignalSentry {
public:
    typedef art::GlobalSignal<art::detail::SignalResponseType::FIFO, void> PreSig_t;
    typedef art::GlobalSignal<art::detail::SignalResponseType::LIFO, void> PostSig_t;
    SignalSentry(SignalSentry const &) = delete;
    SignalSentry & operator=(SignalSentry const &) = delete;
    SignalSentry(PreSig_t & pre, PostSig_t & post)
      :
      post_(post) {
      pre.invoke();
    }
    ~SignalSentry() {
      post_.invoke();
    }
private:
    PostSig_t & post_;
  };

  ////////////////////////////////////
  void setupAsDefaultEmptySource(ParameterSet & p)
  {
    p.put("module_type", "EmptyEvent");
    p.put("module_label", "source");
    p.put("maxEvents", 1);
  }

  std::unique_ptr<art::InputSource>
  makeInput(ParameterSet const & params,
            std::string const & processName,
            art::MasterProductRegistry & preg,
            art::ActivityRegistry & areg)
  {
    ParameterSet defaultEmptySource;
    setupAsDefaultEmptySource(defaultEmptySource);
    // find single source
    bool sourceSpecified = false;
    ParameterSet main_input = defaultEmptySource;
    try {
      if (!params.get_if_present("source", main_input)) {
        mf::LogInfo("EventProcessorSourceConfig")
          << "Could not find a source configuration: using default.";
      }
      // Fill in "ModuleDescription", in case the input source produces
      // any EDproducts,which would be registered in the
      // MasterProductRegistry.  Also fill in the process history item
      // for this process.
      art::ModuleDescription md(main_input.id(),
                                main_input.get<std::string>("module_type"),
                                main_input.get<std::string>("module_label"),
                                art::ProcessConfiguration(processName,
                                                          params.id(),
                                                          art::getReleaseVersion(),
                                                          art::getPassID()));
      sourceSpecified = true;
      art::InputSourceDescription isd(md, preg, areg);
      return std::unique_ptr<art::InputSource>
        (art::InputSourceFactory::make(main_input, isd).release());
    }
    catch (art::Exception const & x) {
      if (sourceSpecified == false &&
          art::errors::Configuration == x.categoryCode()) {
        throw art::Exception(art::errors::Configuration, "FailedInputSource")
          << "Configuration of main input source has failed\n"
          << x;
      }
      else {
        throw;
      }
    }
    catch (cet::exception const & x) {
      throw art::Exception(art::errors::Configuration, "FailedInputSource")
        << "Configuration of main input source has failed\n"
        << x;
    }
    return std::unique_ptr<art::InputSource>();
  }

}

art::EventProcessor::EventProcessor(ParameterSet const & pset)
  :
  helper_(pset),
  act_table_(helper_.schedulerPS()),
  actReg_(),
  mfStatusUpdater_(actReg_),
  preg_(),
  serviceToken_(),
  tbbManager_(tbb::task_scheduler_init::deferred),
  pathManager_(pset, preg_, act_table_, actReg_),
  serviceDirector_(initServices_(pset, actReg_, serviceToken_)),
  destructorOperate_(),
  input_(),
  schedule_(),
  endPathExecutor_(),
  fb_(),
  machine_(),
  principalCache_(),
  sm_evp_(),
  shouldWeStop_(false),
  stateMachineWasInErrorState_(false),
  fileMode_(helper_.schedulerPS().get<std::string>("fileMode", "")),
  handleEmptyRuns_(helper_.schedulerPS().get<bool>("handleEmptyRuns", true)),
  handleEmptySubRuns_(helper_.schedulerPS().get<bool>("handleEmptySubRuns", true)),
  exceptionMessageFiles_(),
  exceptionMessageRuns_(),
  exceptionMessageSubRuns_(),
  alreadyHandlingException_(false)
{
  std::string const processName = pset.get<std::string>("process_name");

  // Services
  ServiceRegistry::Operate operate(serviceToken_); // Make usable.
  serviceToken_.forceCreation();
  // System service FileCatalogMetadata needs to know about the process name.
  ServiceHandle<art::FileCatalogMetadata>()->addMetadataString("process_name", processName);

  input_ = makeInput(pset, processName, preg_, actReg_);
  initSchedules_(pset);
  endPathExecutor_.reset(new EndPathExecutor(pathManager_,
                                             act_table_,
                                             actReg_,
                                             preg_));
  FDEBUG(2) << pset.to_string() << std::endl;
  BranchIDListHelper::updateRegistries(preg_);
}

art::EventProcessor::~EventProcessor()
{
  // Populating the destructOperate_ data member will ensure that
  // services stay usable until it goes out of scope, meaning that
  // modules may (say) use services in their destructors.
  destructorOperate_.reset(new ServiceRegistry::Operate(serviceToken_));
  // The state machine should have already been cleaned up
  // and destroyed at this point by a call to EndJob or
  // earlier when it completed processing events, but if it
  // has not been we'll take care of it here at the last moment.
  // This could cause problems if we are already handling an
  // exception and another one is thrown here ..  For a critical
  // executable the solution to this problem is for the code using
  // the EventProcessor to explicitly call EndJob or use runToCompletion,
  // then the next line of code is never executed.
  terminateMachine_();
}

void
art::EventProcessor::beginJob()
{
  breakpoints::beginJob();
  // make the services available
  ServiceRegistry::Operate operate(serviceToken_);
  // NOTE:  This implementation assumes 'Job' means one call
  // the EventProcessor::run
  // If it really means once per 'application' then this code will
  // have to be changed.
  // Also have to deal with case where have 'run' then new Module
  // added and do 'run'
  // again.  In that case the newly added Module needs its 'beginJob'
  // to be called.
  try {
    input_->doBeginJob();
  }
  catch (cet::exception & e) {
    mf::LogError("BeginJob") << "A cet::exception happened while processing"
                             " the beginJob of the 'source'\n";
    e << "A cet::exception happened while processing"
      " the beginJob of the 'source'\n";
    throw;
  }
  catch (std::exception & e) {
    mf::LogError("BeginJob") << "A std::exception happened while processing"
                             " the beginJob of the 'source'\n";
    throw;
  }
  catch (...) {
    mf::LogError("BeginJob") << "An unknown exception happened while"
                             " processing the beginJob of the 'source'\n";
    throw;
  }
  schedule_->beginJob();
  endPathExecutor_->beginJob();
  actReg_.sPostBeginJob.invoke();

  invokePostBeginJobWorkers_();
}

void
art::EventProcessor::endJob()
{
  // Collects exceptions, so we don't throw before all operations are performed.
  cet::exception_collector c;
  // Make the services available
  ServiceRegistry::Operate operate(serviceToken_);
  c.call([this](){ this->terminateMachine_(); });
  c.call([this](){ schedule_.get()->endJob(); });
  c.call([this](){ endPathExecutor_.get()->endJob(); });
  bool summarize = ServiceHandle<TriggerNamesService>()->wantSummary();
  c.call([this,summarize](){ detail::writeSummary(pathManager_, summarize); });
  c.call([this](){ input_.get()->doEndJob(); });
  c.call([this](){ actReg_.sPostEndJob.invoke(); });
}

art::ServiceDirector
art::EventProcessor::
initServices_(ParameterSet const & top_pset,
              ActivityRegistry & areg,
              ServiceToken & token)
{
  auto services =
    top_pset.get<ParameterSet>("services", ParameterSet());

  // Save and non-standard service config, "floating_point_control" to
  // prevent ServiceDirector trying to make one itself.
  ParameterSet const fpc_pset =
    services.get<ParameterSet>("floating_point_control", ParameterSet());
  services.erase("floating_point_control");

  // Remove non-standard non-service config, "message."
  services.erase("message");

  // Move all services from user into main services block.
  auto const user =
    services.get<ParameterSet>("user", ParameterSet());
  if (!user.is_empty()) {
    mf::LogWarning("CONFIG")
      << "Use of services.user parameter set is deprecated.\n"
      << "Define all services in services parameter set.";
  }
  for (auto const & key : user.get_keys()) {
    if (user.is_key_to_table(key)) {
      if (!services.has_key(key)) {
        services.put(key, user.get<ParameterSet>(key));
      } else {
        throw Exception(errors::Configuration)
          << "Detected a name clash: key "
          << key
          << " is defined in services and services.user.";
      }
    } else {
      throw Exception(errors::Configuration)
        << "Detected a non-table parameter "
        << key
        << " in services.user.";
    }
  }
  services.erase("user");

  // Deal with possible configuration for system service requiring
  // special construction:
  auto pathSelection = services.get<ParameterSet>("PathSelection", {});
  services.erase("PathSelection");

  // Create the service director and all user-configured services.
  ServiceDirector director(std::move(services), areg, token);

  // Services requiring special construction.
  director.addSystemService(std::make_unique<CurrentModule>(areg));
  director.addSystemService(std::make_unique<TriggerNamesService>
                            (top_pset, pathManager_.triggerPathNames()));
  director.addSystemService(std::make_unique<FloatingPointControl>(fpc_pset, areg));
  director.addSystemService(std::make_unique<ScheduleContext>());
  if (!pathSelection.is_empty()) {
    director.addSystemService(std::make_unique<PathSelection>(*this));
  }
  return std::move(director);
}

void
art::EventProcessor::
initSchedules_(ParameterSet const & pset)
{

  // Initialize TBB with desired number of threads.
  int num_threads =
    helper_.servicesPS().get<int>("num_threads",
                                  tbb::task_scheduler_init::default_num_threads());
  tbbManager_.initialize(num_threads);

  schedule_ =
    std::unique_ptr<Schedule>
    (new Schedule(ScheduleID::first(),
                  pathManager_,
                  pset,
                  ServiceRegistry::instance().get<TriggerNamesService>(),
                  preg_,
                  act_table_,
                  actReg_));
}

void
art::EventProcessor::
invokePostBeginJobWorkers_()
{
  // Need to convert multiple lists of workers into a long list that the
  // postBeginJobWorkers callbacks can understand.
  std::vector<Worker *> allWorkers;
  allWorkers.reserve(pathManager_.triggerPathsInfo(ScheduleID::first()).workers().size() +
                     pathManager_.endPathInfo().workers().size());
  auto workerStripper = [&allWorkers](WorkerMap::value_type const & val) {
    allWorkers.emplace_back(val.second.get());
  };
  cet::for_all(pathManager_.triggerPathsInfo(ScheduleID::first()).workers(),
               workerStripper);
  cet::for_all(pathManager_.endPathInfo().workers(),
                workerStripper);
  actReg_.sPostBeginJobWorkers.invoke(input_.get(), allWorkers);
}

art::ServiceToken
art::EventProcessor::getToken_()
{
  return serviceToken_;
}

art::EventProcessor::StatusCode
art::EventProcessor::runToCompletion()
{
  StatusCode returnCode = runCommon_();
  if (machine_.get() != 0) {
    throw art::Exception(errors::LogicError)
        << "State machine not destroyed on exit from EventProcessor::runToCompletion\n"
        << "Please report this error to the Framework group\n";
  }
  return returnCode;
}

art::EventProcessor::StatusCode
art::EventProcessor::runCommon_()
{
  StatusCode returnCode = epSuccess;
  stateMachineWasInErrorState_ = false;
  // Make the services available
  ServiceRegistry::Operate operate(serviceToken_);
  if (machine_.get() == 0) {
    statemachine::FileMode fileMode;
    if (fileMode_.empty()) { fileMode = statemachine::FULLMERGE; }
    else if (fileMode_ == std::string("MERGE")) { fileMode = statemachine::MERGE; }
    else if (fileMode_ == std::string("NOMERGE")) { fileMode = statemachine::NOMERGE; }
    else if (fileMode_ == std::string("FULLMERGE")) { fileMode = statemachine::FULLMERGE; }
    else if (fileMode_ == std::string("FULLLUMIMERGE")) { fileMode = statemachine::FULLLUMIMERGE; }
    else {
      throw art::Exception(errors::Configuration, "Illegal fileMode parameter value: ")
          << fileMode_ << ".\n"
          << "Legal values are 'MERGE', 'NOMERGE', 'FULLMERGE', and 'FULLLUMIMERGE'.\n";
    }
    machine_.reset(new statemachine::Machine(this,
                   fileMode,
                   handleEmptyRuns_,
                   handleEmptySubRuns_));
    machine_->initiate();
  }
  try {
    input::ItemType itemType;
    int iEvents = 0;
    while (true) {
      itemType = input_->nextItemType();
      FDEBUG(1) << "itemType = " << itemType << "\n";
      // Look for a shutdown signal
      {
        boost::mutex::scoped_lock sl(usr2_lock);
        if (art::shutdown_flag > 0) {
          //changeState(mShutdownSignal);
          returnCode = epSignal;
          machine_->process_event(statemachine::Stop());
          break;
        }
      }
      if (itemType == input::IsStop) {
        machine_->process_event(statemachine::Stop());
      }
      else if (itemType == input::IsFile) {
        machine_->process_event(statemachine::File());
      }
      else if (itemType == input::IsRun) {
        machine_->process_event(statemachine::Run(input_->run()));
      }
      else if (itemType == input::IsSubRun) {
        machine_->process_event(statemachine::SubRun(input_->subRun()));
      }
      else if (itemType == input::IsEvent) {
        machine_->process_event(statemachine::Event());
        ++iEvents;
      }
      // This should be impossible
      else {
        throw art::Exception(errors::LogicError)
            << "Unknown next item type passed to EventProcessor\n"
            << "Please report this error to the art developers\n";
      }
      if (machine_->terminated()) {
        break;
      }
    }  // End of loop over state machine events
  } // Try block
  // Some comments on exception handling related to the boost state machine:
  //
  // Some states used in the machine are special because they
  // perform actions while the machine is being terminated, actions
  // such as close files, call endRun, call endSubRun etc ..  Each of these
  // states has two functions that perform these actions.  The functions
  // are almost identical.  The major difference is that one version
  // catches all exceptions and the other lets exceptions pass through.
  // The destructor catches them and the other function named "exit" lets
  // them pass through.  On a normal termination, boost will always call
  // "exit" and then the state destructor.  In our state classes, the
  // the destructors do nothing if the exit function already took
  // care of things.  Here's the interesting part.  When boost is
  // handling an exception the "exit" function is not called (a boost
  // feature).
  //
  // If an exception occurs while the boost machine is in control
  // (which usually means inside a process_event call), then
  // the boost state machine destroys its states and "terminates" itself.
  // This already done before we hit the catch blocks below. In this case
  // the call to terminateMachine below only destroys an already
  // terminated state machine.  Because exit is not called, the state destructors
  // handle cleaning up subRuns, runs, and files.  The destructors swallow
  // all exceptions and only pass through the exceptions messages which
  // are tacked onto the original exception below.
  //
  // If an exception occurs when the boost state machine is not
  // in control (outside the process_event functions), then boost
  // cannot destroy its own states.  The terminateMachine function
  // below takes care of that.  The flag "alreadyHandlingException"
  // is set true so that the state exit functions do nothing (and
  // cannot throw more exceptions while handling the first).  Then the
  // state destructors take care of this because exit did nothing.
  //
  // In both cases above, the EventProcessor::endOfLoop function is
  // not called because it can throw exceptions.
  //
  // One tricky aspect of the state machine is that things which can
  // throw should not be invoked by the state machine while another
  // exception is being handled.
  // Another tricky aspect is that it appears to be important to
  // terminate the state machine before invoking its destructor.
  // We've seen crashes which are not understood when that is not
  // done.  Maintainers of this code should be careful about this.
  catch (cet::exception & e) {
    terminateAbnormally_();
    e << "cet::exception caught in EventProcessor and rethrown\n";
    e << exceptionMessageSubRuns_;
    e << exceptionMessageRuns_;
    e << exceptionMessageFiles_;
    throw e;
  }
  catch (std::bad_alloc & e) {
    terminateAbnormally_();
    throw cet::exception("std::bad_alloc")
        << "The EventProcessor caught a std::bad_alloc exception and converted it to a cet::exception\n"
        << "The job has probably exhausted the virtual memory available to the process.\n"
        << exceptionMessageSubRuns_
        << exceptionMessageRuns_
        << exceptionMessageFiles_;
  }
  catch (std::exception & e) {
    terminateAbnormally_();
    throw cet::exception("StdException")
        << "The EventProcessor caught a std::exception and converted it to a cet::exception\n"
        << "Previous information:\n" << e.what() << "\n"
        << exceptionMessageSubRuns_
        << exceptionMessageRuns_
        << exceptionMessageFiles_;
  }
  catch (std::string const & e) {
    terminateAbnormally_();
    throw cet::exception("Unknown")
        << "The EventProcessor caught a string-based exception type and converted it to a cet::exception\n"
        << e
        << "\n"
        << exceptionMessageSubRuns_
        << exceptionMessageRuns_
        << exceptionMessageFiles_;
  }
  catch (char const * e) {
    terminateAbnormally_();
    throw cet::exception("Unknown")
        << "The EventProcessor caught a string-based exception type and converted it to a cet::exception\n"
        << e
        << "\n"
        << exceptionMessageSubRuns_
        << exceptionMessageRuns_
        << exceptionMessageFiles_;
  }
  catch (...) {
    terminateAbnormally_();
    throw cet::exception("Unknown")
        << "The EventProcessor caught an unknown exception type and converted it to a cet::exception\n"
        << exceptionMessageSubRuns_
        << exceptionMessageRuns_
        << exceptionMessageFiles_;
  }
  if (machine_->terminated()) {
    FDEBUG(1) << "The state machine reports it has been terminated\n";
    machine_.reset();
  }
  if (stateMachineWasInErrorState_) {
    throw cet::exception("BadState")
        << "The boost state machine in the EventProcessor exited after\n"
        << "entering the Error state.\n";
  }
  return returnCode;
}

void
art::EventProcessor::readFile()
{
  actReg_.sPreOpenFile.invoke();
  FDEBUG(1) << " \treadFile\n";
  fb_ = input_->readFile(preg_);
  if (!fb_) {
    throw Exception(errors::LogicError)
        << "Source readFile() did not return a valid FileBlock: FileBlock "
        << "should be valid or readFile() should throw.\n";
  }
  actReg_.sPostOpenFile.invoke(fb_->fileName());
}

void
art::EventProcessor::closeInputFile()
{
  SignalSentry fileCloseSentry(actReg_.sPreCloseFile,
                               actReg_.sPostCloseFile);
  input_->closeFile();
  FDEBUG(1) << "\tcloseInputFile\n";
}

void
art::EventProcessor::openOutputFiles()
{
  endPathExecutor_->openOutputFiles(*fb_);
  FDEBUG(1) << "\topenOutputFiles\n";
}

void
art::EventProcessor::closeOutputFiles()
{
  endPathExecutor_->closeOutputFiles();
  FDEBUG(1) << "\tcloseOutputFiles\n";
}

void
art::EventProcessor::respondToOpenInputFile()
{
  schedule_->respondToOpenInputFile(*fb_);
  endPathExecutor_->respondToOpenInputFile(*fb_);
  FDEBUG(1) << "\trespondToOpenInputFile\n";
}

void
art::EventProcessor::respondToCloseInputFile()
{
  schedule_->respondToCloseInputFile(*fb_);
  endPathExecutor_->respondToCloseInputFile(*fb_);
  FDEBUG(1) << "\trespondToCloseInputFile\n";
}

void
art::EventProcessor::respondToOpenOutputFiles()
{
  schedule_->respondToOpenOutputFiles(*fb_);
  endPathExecutor_->respondToOpenOutputFiles(*fb_);
  FDEBUG(1) << "\trespondToOpenOutputFiles\n";
}

void
art::EventProcessor::respondToCloseOutputFiles()
{
  schedule_->respondToCloseOutputFiles(*fb_);
  endPathExecutor_->respondToCloseOutputFiles(*fb_);
  FDEBUG(1) << "\trespondToCloseOutputFiles\n";
}

void
art::EventProcessor::startingNewLoop()
{
  shouldWeStop_ = false;
  FDEBUG(1) << "\tstartingNewLoop\n";
}

bool
art::EventProcessor::endOfLoop()
{
  FDEBUG(1) << "\tendOfLoop\n";
  return true;
}

void
art::EventProcessor::rewindInput()
{
  input_->rewind();
  FDEBUG(1) << "\trewind\n";
}

void
art::EventProcessor::prepareForNextLoop()
{
  FDEBUG(1) << "\tprepareForNextLoop\n";
}

void
art::EventProcessor::writeSubRunCache()
{
  while (!principalCache_.noMoreSubRuns()) {
    auto const & lowestSubRun = principalCache_.lowestSubRun();
    if (!lowestSubRun.id().isFlush()) {
      endPathExecutor_->writeSubRun(lowestSubRun);
    }
    principalCache_.deleteLowestSubRun();
  }
  FDEBUG(1) << "\twriteSubRunCache\n";
}

void
art::EventProcessor::writeRunCache()
{
  while (!principalCache_.noMoreRuns()) {
    auto const & lowestRun = principalCache_.lowestRun();
    if (!lowestRun.id().isFlush()) {
      endPathExecutor_->writeRun(lowestRun);
    }
    principalCache_.deleteLowestRun();
  }
  FDEBUG(1) << "\twriteRunCache\n";
}

bool
art::EventProcessor::shouldWeCloseOutput() const
{
  FDEBUG(1) << "\tshouldWeCloseOutput\n";
  return endPathExecutor_->shouldWeCloseOutput();
}

void
art::EventProcessor::doErrorStuff()
{
  FDEBUG(1) << "\tdoErrorStuff\n";
  mf::LogError("StateMachine")
      << "The EventProcessor state machine encountered an unexpected event\n"
      "and went to the error state\n"
      "Will attempt to terminate processing normally\n"
      "This likely indicates a bug in an input module, corrupted input, or both\n";
  stateMachineWasInErrorState_ = true;
}

void
art::EventProcessor::beginRun(RunID run)
{
  if (!run.isFlush()) {
    RunPrincipal & runPrincipal = principalCache_.runPrincipal(run);
    processOneOccurrence_<OccurrenceTraits<RunPrincipal, BranchActionBegin> >
      (runPrincipal);
    FDEBUG(1) << "\tbeginRun " << run.run() << "\n";
  }
}

void
art::EventProcessor::endRun(RunID run)
{
  if (!run.isFlush()) {
    RunPrincipal & runPrincipal = principalCache_.runPrincipal(run);
    processOneOccurrence_<OccurrenceTraits<RunPrincipal, BranchActionEnd> >
      (runPrincipal);
    FDEBUG(1) << "\tendRun " << run.run() << "\n";
  }
}

void
art::EventProcessor::beginSubRun(SubRunID const & sr)
{
  if (!sr.isFlush()) {
    // NOTE: Using 0 as the event number for the begin of a subRun block
    // is a bad idea subRun blocks know their start and end times why
    // not also start and end events?
    SubRunPrincipal & subRunPrincipal = principalCache_.subRunPrincipal(sr);
    processOneOccurrence_<OccurrenceTraits<SubRunPrincipal, BranchActionBegin> >
      (subRunPrincipal);
    FDEBUG(1) << "\tbeginSubRun " << sr << "\n";
  }
}

void
art::EventProcessor::endSubRun(SubRunID const & sr)
{
  if (!sr.isFlush()) {
    // NOTE: Using the max event number for the end of a subRun block is
    // a bad idea subRun blocks know their start and end times why not
    // also start and end events?
    SubRunPrincipal & subRunPrincipal = principalCache_.subRunPrincipal(sr);
    processOneOccurrence_<OccurrenceTraits<SubRunPrincipal, BranchActionEnd> >
      (subRunPrincipal);
    FDEBUG(1) << "\tendSubRun " << sr << "\n";
  }
}

art::RunID
art::EventProcessor::readAndCacheRun()
{
  SignalSentry runSourceSentry(actReg_.sPreSourceRun,
                               actReg_.sPostSourceRun);
  principalCache_.insert(input_->readRun());
  art::DecrepitRelicInputSourceImplementation* DRSI =
    dynamic_cast<art::DecrepitRelicInputSourceImplementation*>(input_.get());
  if (DRSI) {
    auto rps = DRSI->readRunFromSecondaryFiles();
    for (auto rp : rps) {
      principalCache_.insert(rp);
    }
  }
  FDEBUG(1) << "\treadAndCacheRun " << "\n";
  return principalCache_.runPrincipal().id();
}

art::SubRunID
art::EventProcessor::readAndCacheSubRun()
{
  SignalSentry subRunSourceSentry(actReg_.sPreSourceSubRun,
                                  actReg_.sPostSourceSubRun);
  principalCache_.insert(input_->readSubRun(principalCache_.runPrincipalPtr()));
  art::DecrepitRelicInputSourceImplementation* DRSI =
    dynamic_cast<art::DecrepitRelicInputSourceImplementation*>(input_.get());
  if (DRSI) {
    auto srps = DRSI->readSubRunFromSecondaryFiles(
                  principalCache_.runPrincipalPtr());
    for (auto srp : srps) {
      principalCache_.insert(srp);
    }
  }
  FDEBUG(1) << "\treadAndCacheSubRun " << "\n";
  return principalCache_.subRunPrincipal().id();
}

void
art::EventProcessor::writeRun(RunID run)
{
  if (!run.isFlush()) {
    endPathExecutor_->writeRun(principalCache_.runPrincipal(run));
    FDEBUG(1) << "\twriteRun " << run.run() << "\n";
  }
}

void
art::EventProcessor::deleteRunFromCache(RunID run)
{
  principalCache_.deleteRun(run);
  FDEBUG(1) << "\tdeleteRunFromCache " << run.run() << "\n";
}

void
art::EventProcessor::writeSubRun(SubRunID const & sr)
{
  if (!sr.isFlush()) {
    endPathExecutor_->writeSubRun(principalCache_.subRunPrincipal(sr));
    FDEBUG(1) << "\twriteSubRun " << sr.run() << "/" << sr.subRun() << "\n";
  }
}

void
art::EventProcessor::deleteSubRunFromCache(SubRunID const & sr)
{
  principalCache_.deleteSubRun(sr);
  FDEBUG(1) << "\tdeleteSubRunFromCache " << sr.run() << "/" << sr.subRun() << "\n";
}

void
art::EventProcessor::readEvent()
{
  SignalSentry sourceSentry(actReg_.sPreSource, actReg_.sPostSource);
  sm_evp_ = input_->readEvent(principalCache_.subRunPrincipalPtr());
  FDEBUG(1) << "\treadEvent\n";
}

void
art::EventProcessor::processEvent()
{
  if (!sm_evp_->id().isFlush()) {
    processOneOccurrence_<OccurrenceTraits<EventPrincipal, BranchActionBegin> >
      (*sm_evp_);
    FDEBUG(1) << "\tprocessEvent\n";
  }
}

bool
art::EventProcessor::shouldWeStop() const
{
  FDEBUG(1) << "\tshouldWeStop\n";
  if (shouldWeStop_) { return true; }
  return endPathExecutor_->terminate();
}

void
art::EventProcessor::setExceptionMessageFiles(std::string & message)
{
  exceptionMessageFiles_ = message;
}

void
art::EventProcessor::setExceptionMessageRuns(std::string & message)
{
  exceptionMessageRuns_ = message;
}

void
art::EventProcessor::setExceptionMessageSubRuns(std::string & message)
{
  exceptionMessageSubRuns_ = message;
}

bool
art::EventProcessor::alreadyHandlingException() const
{
  return alreadyHandlingException_;
}

bool
art::EventProcessor::
setTriggerPathEnabled(std::string const & name, bool enable)
{
  return schedule_->setTriggerPathEnabled(name, enable);
}

bool
art::EventProcessor::
setEndPathModuleEnabled(std::string const & label, bool enable)
{
  return endPathExecutor_->setEndPathModuleEnabled(label, enable);
}

void
art::EventProcessor::terminateMachine_()
{
  if (machine_.get() != 0) {
    if (!machine_->terminated()) {
      machine_->process_event(statemachine::Stop());
    }
    else {
      FDEBUG(1) << "EventProcess::terminateMachine_: The state machine was already terminated \n";
    }
    if (machine_->terminated()) {
      FDEBUG(1) << "The state machine reports it has been terminated (3)\n";
    }
    machine_.reset();
  }
}

void
art::EventProcessor::terminateAbnormally_() try
{
  alreadyHandlingException_ = true;
  if (ServiceRegistry::instance().isAvailable<RandomNumberGenerator>()) {
    ServiceHandle<RandomNumberGenerator>()->saveToFile_();
  }
  terminateMachine_();
  alreadyHandlingException_ = false;
}
catch (...)
{
}
