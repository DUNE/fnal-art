////////////////////////////////////////////////////////////////////////
// Class:       ProductIDGetter
// Module Type: producer
// File:        ProductIDGetter_module.cc
//
// Generated at Wed Jun 15 17:19:52 2011 by Chris Green using artmod
// from art v0_07_09.
////////////////////////////////////////////////////////////////////////

#define BOOST_TEST_DYN_LINK
#include <boost/test/included/unit_test.hpp>

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Persistency/Provenance/ProductID.h"

namespace arttest {
  class ProductIDGetter;
}

class arttest::ProductIDGetter : public art::EDProducer {
public:
  explicit ProductIDGetter(fhicl::ParameterSet const &p);
  virtual ~ProductIDGetter();

  virtual void produce(art::Event &e);

private:

};


arttest::ProductIDGetter::ProductIDGetter(fhicl::ParameterSet const &p)
{
  produces<int>();
  produces<int>("i1");
  produces<std::vector<int> >();
  produces<art::Ptr<int> >();
}

arttest::ProductIDGetter::~ProductIDGetter() {
}

void arttest::ProductIDGetter::produce(art::Event &e) {
  art::ProductID p1(getProductID<int>(e));
  BOOST_REQUIRE(p1.isValid());
  art::ProductID p2(getProductID<int>(e, "i1"));
  BOOST_REQUIRE(p2.isValid());
  BOOST_REQUIRE_NE(p1, p2);
  std::auto_ptr<std::vector<int> > vip(new std::vector<int>);
  vip->push_back(0);
  vip->push_back(2);
  vip->push_back(4);
  vip->push_back(6);

  art::ProductID pv(getProductID<std::vector<int> >(e));
  std::auto_ptr<art::Ptr<int> >ptr(new art::Ptr<int>(pv, 2, e.productGetter()));

  BOOST_REQUIRE(ptr->id().isValid());

  art::OrphanHandle<std::vector<int> > h(e.put(vip));

  art::Ptr<int> ptr_check(h, 2);

  BOOST_REQUIRE_EQUAL(ptr->id(), ptr_check.id());

  e.put(ptr);
}

DEFINE_ART_MODULE(arttest::ProductIDGetter);
