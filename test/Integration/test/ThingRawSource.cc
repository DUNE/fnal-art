#include "FWCore/Integration/test/ThingRawSource.h"
#include "test/TestObjects/ThingCollection.h"
#include "art/Framework/Core/Event.h"
#include "art/Framework/Core/SubRun.h"
#include "art/Framework/Core/Run.h"
#include "art/Framework/Core/InputSourceMacros.h"

namespace edmtest {
  ThingRawSource::ThingRawSource(edm::ParameterSet const& pset, edm::InputSourceDescription const& desc) :
    RawInputSource(pset, desc), alg_(), eventID_(1, 1) {
    produces<ThingCollection, edm::InEvent>();
    produces<ThingCollection, edm::InSubRun>("beginLumi");
    produces<ThingCollection, edm::InSubRun>("endLumi");
    produces<ThingCollection, edm::InRun>("beginRun");
    produces<ThingCollection, edm::InRun>("endRun");
  }

  // Virtual destructor needed.
  ThingRawSource::~ThingRawSource() { }

  // Functions that gets called by framework every event
  std::auto_ptr<edm::Event> ThingRawSource::readOneEvent() {
    edm::Timestamp tstamp;

    // Fake running out of data
    if (eventID_.event() > 2) return std::auto_ptr<edm::Event>();

    // Step B: Create empty output
    std::auto_ptr<ThingCollection> result(new ThingCollection);  //Empty

    // Step C: Invoke the algorithm, passing in inputs (NONE) and getting back outputs.
    alg_.run(*result);

    // Make an event.
    // makeEvent is provided by the base class.
    // You must call makeEvent,
    // providing the eventId (containing run# and event#)
    // and timestamp.
    std::auto_ptr<edm::Event> e = makeEvent(eventID_.run(), 1U, eventID_.event(), tstamp);
    eventID_ = eventID_.next();

    // put your product(s) into the event.  One put call per product.
    e->put(result);

    // return the auto_ptr.
    return e;
  }

  // Functions that gets called by framework every luminosity block
  void ThingRawSource::beginSubRun(edm::SubRun& lb) {
    // Step A: Get Inputs

    // Step B: Create empty output
    std::auto_ptr<ThingCollection> result(new ThingCollection);  //Empty

    // Step C: Invoke the algorithm, passing in inputs (NONE) and getting back outputs.
    alg_.run(*result);

    // Step D: Put outputs into lumi block
    lb.put(result, "beginLumi");
  }

  void ThingRawSource::endSubRun(edm::SubRun& lb) {
    // Step A: Get Inputs

    // Step B: Create empty output
    std::auto_ptr<ThingCollection> result(new ThingCollection);  //Empty

    // Step C: Invoke the algorithm, passing in inputs (NONE) and getting back outputs.
    alg_.run(*result);

    // Step D: Put outputs into lumi block
    lb.put(result, "endLumi");
  }

  // Functions that gets called by framework every run
  void ThingRawSource::beginRun(edm::Run& r) {
    // Step A: Get Inputs

    // Step B: Create empty output
    std::auto_ptr<ThingCollection> result(new ThingCollection);  //Empty

    // Step C: Invoke the algorithm, passing in inputs (NONE) and getting back outputs.
    alg_.run(*result);

    // Step D: Put outputs into event
    r.put(result, "beginRun");
  }

  void ThingRawSource::endRun(edm::Run& r) {
    // Step A: Get Inputs

    // Step B: Create empty output
    std::auto_ptr<ThingCollection> result(new ThingCollection);  //Empty

    // Step C: Invoke the algorithm, passing in inputs (NONE) and getting back outputs.
    alg_.run(*result);

    // Step D: Put outputs into event
    r.put(result, "endRun");
  }

}
using edmtest::ThingRawSource;
DEFINE_FWK_INPUT_SOURCE(ThingRawSource);
