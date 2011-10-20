/**
   Test Modules for ScheduleBuilder
*/

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "cpp0x/memory"
#include "fhiclcpp/ParameterSet.h"
#include "test/TestObjects/ToyProducts.h"
#include <string>

using namespace art;

static const char CVSId[] = "";

class TestSchedulerModule1 : public EDProducer
{
 public:
  explicit TestSchedulerModule1(ParameterSet const& p):pset_(p){
    produces<arttest::StringProduct>();
  }

  void produce(Event& e, EventSetup const&);

private:
  ParameterSet pset_;
};


void TestSchedulerModule1::produce(Event& e, EventSetup const&)
{
  std::string myname = pset_.get<std::string>("module_name");
  std::auto_ptr<arttest::StringProduct> product(new arttest::StringProduct(myname));
  e.put(product);
}

DEFINE_ART_MODULE(TestSchedulerModule1)


// Configure (x)emacs for this file ...
// Local Variables:
// mode:c++
// compile-command: "make -C .. -k"
// End:
