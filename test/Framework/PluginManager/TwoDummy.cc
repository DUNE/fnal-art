// -*- C++ -*-
//
// Package:     PluginManager
// Class  :     TwoDummy
//
// Implementation:
//     <Notes on implementation>
//
// Original Author:  Chris Jones
//         Created:  Fri Apr  6 15:32:53 EDT 2007
//
//

// system include files

// user include files
#include "test/Framework/PluginManager/DummyFactory.h"

namespace testedmplugin {
  struct DummyTwo : public DummyBase {
    int value() const {
      return 2;
    }
  };
}

DEFINE_EDM_PLUGIN(testartplugin::DummyFactory,testartplugin::DummyTwo,"DummyTwo");
