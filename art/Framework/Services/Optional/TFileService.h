#ifndef SERVICES_TFILESERVICE_H
#define SERVICES_TFILESERVICE_H

// ======================================================================
//
// TFileService
//
// ======================================================================

#include "art/Framework/Core/TFileDirectory.h"

namespace art {
  class ActivityRegistry;   // declaration only
  class ModuleDescription;  // declaration only
  class TFileService;       // defined below
}
namespace fhicl {
  class ParameterSet;       // declaration only
}

// ----------------------------------------------------------------------

class art::TFileService
  : public TFileDirectory
{
  // non-copyable:
  TFileService( TFileService const & );
  void  operator = ( TFileService const & );

public:
  // c'tor:
  TFileService( fhicl::ParameterSet const & cfg
              , art::ActivityRegistry     & r
              );

  // d'tor:
  ~TFileService();

  // accessor:
  TFile &
    file( ) const { return * file_; }

private:
  TFile * file_;           // pointer to opened TFile
  bool closeFileFast_;

  // set current directory according to module name and prepare to create directory
  void setDirectoryName( art::ModuleDescription const & desc );
};  // TFileService

// ======================================================================

#endif