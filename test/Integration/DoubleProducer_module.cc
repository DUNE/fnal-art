//--------------------------------------------------------------------
//
// Produces an DoubleProduct instance.
//
//--------------------------------------------------------------------

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/Event.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "fhiclcpp/ParameterSet.h"
#include "art/Persistency/Common/Handle.h"
#include "test/TestObjects/ToyProducts.h"
#include <memory>
#include <iostream>

namespace arttest {
  class DoubleProducer;
}

class arttest::DoubleProducer
  : public art::EDProducer
{
public:
  explicit DoubleProducer( fhicl::ParameterSet const& p )
  : value_( p.get<double>("dvalue") )
  {
    produces<DoubleProduct>();
  }

  explicit DoubleProducer( double d )
  : value_(d)
  {
    produces<DoubleProduct>();
  }

  virtual ~DoubleProducer() { }

  virtual void produce( art::Event& e );

private:
  double value_;
};  // DoubleProducer

void
arttest::DoubleProducer::produce( art::Event& e )
{
  std::cerr << "Holy cow, DoubleProducer::produce is running!\n";
  std::auto_ptr<DoubleProduct> p(new DoubleProduct(value_));
  e.put(p);
}

DEFINE_ART_MODULE(arttest::DoubleProducer);