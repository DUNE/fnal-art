#include <iostream>
#include <string>
#include <utility>

#include "test/CppUnit_testdriver.icpp"
#include <cppunit/extensions/HelperMacros.h>

#include "art/Persistency/Common/Ref.h"
#include "art/Persistency/Common/EDProductGetter.h"
#include "art/Utilities/EDMException.h"

#include "SimpleEDProductGetter.h"

class TestRef: public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestRef);
  CPPUNIT_TEST(default_ctor_without_active_getter);
  //CPPUNIT_TEST(default_ctor_without_active_getter_string_key);
  CPPUNIT_TEST(default_ctor_with_active_getter);
  CPPUNIT_TEST(nondefault_ctor);
  //CPPUNIT_TEST(nondefault_ctor_2);
  CPPUNIT_TEST(using_wrong_productid);
  CPPUNIT_TEST_SUITE_END();

 public:
  typedef std::vector<int>           product1_t;
  typedef std::map<std::string, int> product2_t;

  typedef art::Ref<product1_t> ref1_t;
  //typedef art::Ref<product2_t, int> ref2_t;


  TestRef() { }
  ~TestRef() {}
  void setUp() {}
  void tearDown() {}

  void default_ctor_without_active_getter();
  //  void default_ctor_without_active_getter_string_key();
  void default_ctor_with_active_getter();
  void nondefault_ctor();
  //  void nondefault_ctor_2();
  void using_wrong_productid();

 private:
};

CPPUNIT_TEST_SUITE_REGISTRATION(TestRef);

void TestRef::default_ctor_without_active_getter()
{
  ref1_t  default_ref;
  CPPUNIT_ASSERT(default_ref.isNull());
  CPPUNIT_ASSERT(default_ref.isNonnull()==false);
  CPPUNIT_ASSERT(!default_ref);
  CPPUNIT_ASSERT(default_ref.productGetter()==0);
  CPPUNIT_ASSERT(default_ref.id().isValid()==false);
  CPPUNIT_ASSERT(default_ref.isAvailable()==false);
}

// void TestRef::default_ctor_without_active_getter_string_key()
// {
//   ref2_t  default_ref;
//   CPPUNIT_ASSERT(default_ref.isNull());
//   CPPUNIT_ASSERT(default_ref.isNonnull()==false);
//   CPPUNIT_ASSERT(!default_ref);
//   CPPUNIT_ASSERT(default_ref.productGetter()==0);
//   CPPUNIT_ASSERT(default_ref.id().isValid()==false);
//   CPPUNIT_ASSERT(default_ref.id().isAvailable()==false);
// }

void TestRef::default_ctor_with_active_getter()
{
  SimpleEDProductGetter getter;
  art::EDProductGetter::Operate op(&getter);
  ref1_t  default_ref;
  CPPUNIT_ASSERT(default_ref.isNull());
  CPPUNIT_ASSERT(default_ref.isNonnull()==false);
  CPPUNIT_ASSERT(!default_ref);
  CPPUNIT_ASSERT(default_ref.productGetter()==&getter);
  CPPUNIT_ASSERT(default_ref.id().isValid()==false);
  CPPUNIT_ASSERT(default_ref.isAvailable()==false);
  CPPUNIT_ASSERT_THROW(default_ref.operator->(), art::Exception);
  CPPUNIT_ASSERT_THROW(*default_ref, art::Exception);
}

void TestRef::nondefault_ctor()
{
  SimpleEDProductGetter getter;

  art::EDProductGetter::Operate op(&getter);
  art::ProductID id(1, 201U);
  CPPUNIT_ASSERT(id.isValid());

  std::auto_ptr<product1_t> prod(new product1_t);
  prod->push_back(1);
  prod->push_back(2);
  getter.addProduct(id, prod);


  ref1_t  ref0(id, 0, &getter);
  CPPUNIT_ASSERT(ref0.isNull()==false);
  CPPUNIT_ASSERT(ref0.isNonnull());
  CPPUNIT_ASSERT(!!ref0);
  CPPUNIT_ASSERT(ref0.productGetter()==&getter);
  CPPUNIT_ASSERT(ref0.id().isValid());
  CPPUNIT_ASSERT(ref0.isAvailable()==true);
  CPPUNIT_ASSERT(*ref0 == 1);

  ref1_t  ref1(id, 1, &getter);
  CPPUNIT_ASSERT(ref1.isNonnull());
  CPPUNIT_ASSERT(ref1.isAvailable()==true);
  CPPUNIT_ASSERT(*ref1 == 2);

  // Note that nothing stops one from making an art::Ref into a
  // collection using an index that is invalid. So there is no testing
  // of such use to be done.
}

// void TestRef::nondefault_ctor_2()
// {
//   SimpleEDProductGetter getter;

//   art::EDProductGetter::Operate op(&getter);
//   art::ProductID id(1, 201U);
//   CPPUNIT_ASSERT(id.isValid());

//   std::auto_ptr<product2_t> prod(new product2_t);
//   prod->insert(std::make_pair(std::string("a"), 1));
//   prod->insert(std::make_pair(std::string("b"), 2));
//   prod->insert(std::make_pair(std::string("c"), 3));
//   getter.addProduct(id, prod);

//   ref2_t  refa(id, std::string("a"), &getter);
//   CPPUNIT_ASSERT(refa.isNull()==false);
//   CPPUNIT_ASSERT(refa.isNonnull());
//   CPPUNIT_ASSERT(!!refa);
//   CPPUNIT_ASSERT(refa.productGetter()==&getter);
//   CPPUNIT_ASSERT(refa.id().isValid());
//   CPPUNIT_ASSERT(*refa == 1);

//   ref2_t  refb(id, "b", &getter);
//   CPPUNIT_ASSERT(refb.isNonnull());
//   CPPUNIT_ASSERT(*refb == 2);
// }


void TestRef::using_wrong_productid()
{
  SimpleEDProductGetter getter;

  art::EDProductGetter::Operate op(&getter);
  art::ProductID id(1, 1U);
  CPPUNIT_ASSERT(id.isValid());

  std::auto_ptr<product1_t> prod(new product1_t);
  prod->push_back(1);
  prod->push_back(2);
  getter.addProduct(id, prod);

  art::ProductID wrong_id(1, 100U);
  CPPUNIT_ASSERT(wrong_id.isValid()); // its valid, but not used.

  ref1_t  ref(wrong_id, 0, &getter);
  CPPUNIT_ASSERT_THROW(*ref, art::Exception);
  CPPUNIT_ASSERT_THROW(ref.operator->(), art::Exception);
}
