#include "art/Framework/Core/EDProducer.h"
#include "test/TestObjects/ToyProducts.h"
#include "art/Framework/Core/ModuleMacros.h"

namespace arttest {
   class FailingProducer;
}

//--------------------------------------------------------------------
//
// throws an exception.
// Announces an IntProduct but does not produce one;
// every call to FailingProducer::produce throws an art::Exception
//
class arttest::FailingProducer : public art::EDProducer {
public:
   explicit FailingProducer(fhicl::ParameterSet const&) {
      produces<arttest::IntProduct>();
   }
   virtual ~FailingProducer() { }
   virtual void produce(art::Event& e);
};

void
arttest::FailingProducer::produce(art::Event&) {
   // We throw an edm exception with a configurable action.
   throw art::Exception(art::errors::ProductNotFound) << "Intentional 'ProductNotFound' exception for testing purposes\n";
}

DEFINE_ART_MODULE(arttest::FailingProducer)
