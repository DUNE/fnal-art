#ifndef art_Framework_Core_EDProducer_h
#define art_Framework_Core_EDProducer_h

// ======================================================================
//
// EDProducer - The base class of "modules" whose main purpose is to
//              insert new EDProducts into an Event.
//
// ======================================================================

#include "art/Framework/Core/EngineCreator.h"
#include "art/Framework/Principal/fwd.h"
#include "art/Framework/Core/Frameworkfwd.h"
#include "art/Persistency/Provenance/MasterProductRegistry.h"
#include "art/Framework/Core/ProducerBase.h"
#include "art/Framework/Core/WorkerT.h"
#include "art/Persistency/Provenance/ModuleDescription.h"
#include "cpp0x/memory"
#include "fhiclcpp/ParameterSet.h"
#include <ostream>
#include <string>

// ----------------------------------------------------------------------

namespace art
{

  class EDProducer
    : public ProducerBase
    , public EngineCreator
  {
  public:
    template <typename T> friend class WorkerT;
    typedef EDProducer ModuleType;
    typedef WorkerT<EDProducer> WorkerType;

    EDProducer ();
    virtual ~EDProducer();

    template <typename PROD, BranchType B, typename TRANS>
    ProductID
    getProductID(TRANS const &translator,
                 std::string const& instanceName=std::string()) const;

    template <typename PROD, typename TRANS>
    ProductID
    getProductID(TRANS const &translator,
                 std::string const& instanceName=std::string()) const;

  protected:
    // The returned pointer will be null unless the this is currently
    // executing its event loop function ('produce').
    CurrentProcessingContext const* currentContext() const;

  private:
    bool doEvent(EventPrincipal& ep,
                   CurrentProcessingContext const* cpcp);
    void doBeginJob();
    void doEndJob();
    bool doBeginRun(RunPrincipal & rp,
                   CurrentProcessingContext const* cpc);
    bool doEndRun(RunPrincipal & rp,
                   CurrentProcessingContext const* cpc);
    bool doBeginSubRun(SubRunPrincipal & srp,
                   CurrentProcessingContext const* cpc);
    bool doEndSubRun(SubRunPrincipal & srp,
                   CurrentProcessingContext const* cpc);
    void doRespondToOpenInputFile(FileBlock const& fb);
    void doRespondToCloseInputFile(FileBlock const& fb);
    void doRespondToOpenOutputFiles(FileBlock const& fb);
    void doRespondToCloseOutputFiles(FileBlock const& fb);

    std::string workerType() const {return "WorkerT<EDProducer>";}

    virtual void produce(Event &) = 0;
    virtual void beginJob(){}
    virtual void endJob(){}
    virtual void reconfigure(fhicl::ParameterSet const&);

    virtual void beginRun(Run &){}
    virtual void endRun(Run &){}
    virtual void beginSubRun(SubRun &){}
    virtual void endSubRun(SubRun &){}
    virtual void respondToOpenInputFile(FileBlock const&) {}
    virtual void respondToCloseInputFile(FileBlock const&) {}
    virtual void respondToOpenOutputFiles(FileBlock const&) {}
    virtual void respondToCloseOutputFiles(FileBlock const&) {}

    void setModuleDescription(ModuleDescription const& md) {
      moduleDescription_ = md;
    }
    ModuleDescription moduleDescription_;
    CurrentProcessingContext const* current_context_;
  };  // EDProducer

  template <typename PROD, BranchType B, typename TRANS>
  inline
  ProductID
  EDProducer::getProductID(TRANS const &translator,
                           std::string const& instanceName) const {
    return ProducerBase::getProductID<PROD, B>(translator,
                                               moduleDescription_,
                                               instanceName);
  }

  template <typename PROD, typename TRANS>
  inline
  ProductID
  EDProducer::getProductID(TRANS const &translator,
                           std::string const& instanceName) const {
    return ProducerBase::getProductID<PROD, InEvent>(translator,
                                                     moduleDescription_,
                                                     instanceName);
  }

}  // art

// ======================================================================

#endif /* art_Framework_Core_EDProducer_h */

// Local Variables:
// mode: c++
// End:
