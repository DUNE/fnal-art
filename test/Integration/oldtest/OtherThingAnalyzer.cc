#include <iostream>
#include "FWCore/Integration/test/OtherThingAnalyzer.h"
#include "test/TestObjects/OtherThing.h"
#include "test/TestObjects/OtherThingCollection.h"
#include "art/Framework/Core/Event.h"
#include "art/Framework/Core/SubRun.h"
#include "art/Framework/Core/Run.h"
#include "art/Persistency/Common/Handle.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "cetlib/exception.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

namespace arttest {
  OtherThingAnalyzer::OtherThingAnalyzer(fhicl::ParameterSet const& pset) :
    thingWasDropped_(pset.get<bool>("thingWasDropped", false)) {
  }

  void OtherThingAnalyzer::analyze(art::Event const& e, art::EventSetup const&) {
    doit(e, std::string("testUserTag"));
  }

  void OtherThingAnalyzer::doit(art::Event const& dv, std::string const& label) {
    art::Handle<OtherThingCollection> otherThings;
    assert(dv.getByLabel("OtherThing", label, otherThings));
    mf::LogInfo("OtherThingAnalyzer") << " --------------- next event ------------ \n";
    int i = 0;
    for (OtherThingCollection::const_iterator it = otherThings->begin(), itEnd = otherThings->end();
        it != itEnd; ++it, ++i) {
      OtherThing const& otherThing = *it;

      if (otherThing.oneNullOneNot.size() != 2) {
	throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
	  << " oneNullOneNot has wrong length: " << otherThing.oneNullOneNot.size()
	  << " should be 2\n";
      }
      if (otherThing.oneNullOneNot[0].isNonnull()) {
	throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
	  << " expected null Ref is not null\n";
      }
      if (otherThing.oneNullOneNot[1].isNull()) {
	throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
	  << " expected non-null Ref is null\n";
      }

      bool shouldBeTrue = otherThing.refVec[0] != otherThing.refVec[1];
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "Inequality has incorrect value\n";
      }
      shouldBeTrue = otherThing.refVec[0] == otherThing.refVec[0];
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "Equality has incorrect value\n";
      }
      shouldBeTrue = otherThing.refProd == otherThing.refProd;
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "RefProd Equality has incorrect value\n";
      }
      shouldBeTrue = otherThing.refVec[0].isNonnull();
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "Non-null check has incorrect value\n";
      }
      shouldBeTrue = otherThing.refProd.isNonnull();
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "RefProd non-null check has incorrect value\n";
      }
      shouldBeTrue = !(!otherThing.refVec[0]);
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "'!' has incorrect value\n";
      }
      shouldBeTrue = !(!otherThing.refProd);
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "RefProd '!' has incorrect value\n";
      }
      shouldBeTrue = !otherThing.refVec.empty();
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "empty() has incorrect value\n";
      }
      shouldBeTrue = (otherThing.refVec == otherThing.refVec);
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "RefVector equality has incorrect value\n";
      }
      shouldBeTrue = !(otherThing.refVec != otherThing.refVec);
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "RefVector inequality has incorrect value\n";
      }
      assert(otherThing.refProd.isAvailable() != thingWasDropped_);
      assert(otherThing.ref.isAvailable() != thingWasDropped_);
      assert(otherThing.refVec.isAvailable() != thingWasDropped_);

      if (otherThing.ptrOneNullOneNot.size() != 2) {
	throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
	  << " ptrOneNullOneNot has wrong length: " << otherThing.ptrOneNullOneNot.size()
	  << " should be 2\n";
      }
      if (otherThing.ptrOneNullOneNot[0].isNonnull()) {
	throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
	  << " expected null Ptr is not null\n";
      }
      if (otherThing.ptrOneNullOneNot[1].isNull()) {
	throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
	  << " expected non-null Ptr is null\n";
      }

      shouldBeTrue = otherThing.ptrVec[0] != otherThing.ptrVec[1];
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "Inequality has incorrect value\n";
      }
      shouldBeTrue = otherThing.ptrVec[0] == otherThing.ptrVec[0];
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "Equality has incorrect value\n";
      }
      shouldBeTrue = otherThing.ptrVec[0].isNonnull();
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "Non-null check has incorrect value\n";
      }
      shouldBeTrue = !(!otherThing.ptrVec[0]);
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "'!' has incorrect value\n";
      }
      shouldBeTrue = !otherThing.ptrVec.empty();
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "empty() has incorrect value\n";
      }
      shouldBeTrue = (otherThing.ptrVec == otherThing.ptrVec);
      if (!shouldBeTrue) {
        throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "PtrVector equality has incorrect value\n";
      }
      assert(otherThing.ptr.isAvailable() != thingWasDropped_);
      assert(otherThing.ptrVec.isAvailable() != thingWasDropped_);

      if (thingWasDropped_) return;
      {
        ThingCollection const& tcoll = *otherThing.refProd;
        ThingCollection::size_type size1 = tcoll.size();
        ThingCollection::size_type size2 = otherThing.refProd->size();
        if (size1 == 0 || size1 != size2) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << " RefProd size mismatch " << std::endl;
        }

        Thing const& tc = *otherThing.ref;
        int const& x = otherThing.ref->a;
        if (tc.a == i && x == i) {
          mf::LogInfo("OtherThingAnalyzer") << " ITEM " << i << " LABEL " << label << " dereferenced from art::Ref successfully.\n";
        } else {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "ITEM " << i << " has incorrect art::Ref value " << tc.a << '\n';
        }

        art::View<Thing> const& viewThing = *otherThing.refToBaseProd;
        art::View<Thing>::size_type const viewSize1 = viewThing.size();
        art::View<Thing>::size_type const viewSize2 = otherThing.refToBaseProd->size();
        if (viewSize1 == 0 || viewSize2 != viewSize1) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << " RefToBaseProd size mismatch " << std::endl;
        }
        if (viewSize1 != size1) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << " RefToBaseProd size mismatch to RefProd size" << std::endl;
        }
        Thing const& tcBase = *otherThing.refToBase;
        int const& xBase = otherThing.refToBase->a;
        if (tcBase.a == i && xBase == i) {
          mf::LogInfo("OtherThingAnalyzer") << " ITEM " << i << " LABEL " << label << " RefToBase dereferenced successfully.\n";
        } else {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "ITEM " << i << " RefToBase has incorrect value " << tc.a << '\n';
        }

        Thing const& tcv = *otherThing.refVec[0];
        int const& xv = otherThing.refVec[0]->a;
        if (xv != tcv.a || xv != i) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "VECTOR ITEM 0 " << i << " has incorrect value " << tcv.a << '\n';
        }
        Thing const& tcv1 = *otherThing.refVec[1];
        int const& xv1 = otherThing.refVec[1]->a;
        if (xv1 != tcv1.a || xv1 != 19-i) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "VECTOR ITEM 1 " << i << " has incorrect value " << tcv1.a << '\n';
        }
        for (art::RefVector<ThingCollection>::iterator it = otherThing.refVec.begin(), itEnd = otherThing.refVec.end();
            it != itEnd; ++it) {
          art::Ref<ThingCollection> tcol = *it;
          Thing const& ti = **it;
          int const& xi = (*it)->a;
  	if (xi != ti.a) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "iterator item " << ti.a << " " << xi << '\n';
          } else if (it == otherThing.refVec.begin() && xi != i) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "iterator item 0" << xi << '\n';
          } else if (it != otherThing.refVec.begin() && xi != 19-i) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "iterator item 1" << xi << '\n';
          }
        }
        art::RefVector<ThingCollection>::iterator it0 = otherThing.refVec.begin();
        int zero = (*it0)->a;
        art::RefVector<ThingCollection>::iterator it1 = it0 + 1;
        int one = (*it1)->a;
        it1 = 1 + it0;
        int x1 = (*it1)->a;
        if (x1 != one) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator+ iterator error: " << x1 << " " << one << '\n';
        it0 = it1 - 1;
        int x0 = (*it0)->a;
        if (x0 != zero) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator- iterator error: " << x0 << " " << zero << '\n';
        x0 = (*(it0++))->a;
        if (x0 != zero) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator++ iterator error: " << x0 << " " << zero << '\n';
        x1 = (*it0)->a;
        if (x1 != one) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator++ iterator error 2: " << x1 << " " << one << '\n';
        x1 = (*(it0--))->a;
        if (x1 != one) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator-- iterator error: " << x1 << " " << one << '\n';
        x0 = (*it0)->a;
        if (x0 != zero) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator-- iterator error 2: " << x0 << " " << zero << '\n';
        x1 = it0[1]->a;
        if (x1 != one) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator[] iterator error: " << x1 << " " << one << '\n';
        x1 = it1[0]->a;
        if (x1 != one) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator[] iterator error 2: " << x1 << " " << one << '\n';
      }
      {
        Thing const& tc = *otherThing.ptr;
        int const& x = otherThing.ptr->a;
        if (tc.a == i && x == i) {
          mf::LogInfo("OtherThingAnalyzer") << " ITEM " << i << " LABEL " << label << " dereferenced from art::Ptr successfully.\n";
        } else {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "ITEM " << i << " has incorrect art::Ptr value " << tc.a << '\n';
        }

        Thing const& tcv = *otherThing.ptrVec[0];
        int const& xv = otherThing.ptrVec[0]->a;
        if (xv != tcv.a || xv != i) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "VECTOR ITEM 0 " << i << " has incorrect value " << tcv.a << '\n';
        }
        Thing const& tcv1 = *otherThing.ptrVec[1];
        int const& xv1 = otherThing.ptrVec[1]->a;
        if (xv1 != tcv1.a || xv1 != 19-i) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "VECTOR ITEM 1 " << i << " has incorrect value " << tcv1.a << '\n';
        }
        for (art::PtrVector<Thing>::const_iterator it = otherThing.ptrVec.begin(), itEnd = otherThing.ptrVec.end();
            it != itEnd; ++it) {
          art::Ptr<Thing> tcol = *it;
          Thing const& ti = **it;
          int const& xi = (*it)->a;
  	if (xi != ti.a) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "iterator item " << ti.a << " " << xi << '\n';
          } else if (it == otherThing.ptrVec.begin() && xi != i) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "iterator item 0" << xi << '\n';
          } else if (it != otherThing.ptrVec.begin() && xi != 19-i) {
          throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze") << "iterator item 1" << xi << '\n';
          }
        }
        art::PtrVector<Thing>::const_iterator it0 = otherThing.ptrVec.begin();
        int zero = (*it0)->a;
        art::PtrVector<Thing>::const_iterator it1 = it0 + 1;
        int one = (*it1)->a;
        it1 = it0 + 1;
        int x1 = (*it1)->a;
        if (x1 != one) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator+ iterator error: " << x1 << " " << one << '\n';
        it0 = it1 - 1;
        int x0 = (*it0)->a;
        if (x0 != zero) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator- iterator error: " << x0 << " " << zero << '\n';
        x0 = (*(it0++))->a;
        if (x0 != zero) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator++ iterator error: " << x0 << " " << zero << '\n';
        x1 = (*it0)->a;
        if (x1 != one) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator++ iterator error 2: " << x1 << " " << one << '\n';
        x1 = (*(it0--))->a;
        if (x1 != one) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator-- iterator error: " << x1 << " " << one << '\n';
        x0 = (*it0)->a;
        if (x0 != zero) throw cet::exception("Inconsistent Data", "OtherThingAnalyzer::analyze")
          << "operator-- iterator error 2: " << x0 << " " << zero << '\n';
      }
    }
  }
}
using arttest::OtherThingAnalyzer;
DEFINE_ART_MODULE(OtherThingAnalyzer);