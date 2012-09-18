#include "art/Framework/Services/System/FileCatalogMetadata.h"

#include "art/Framework/Services/Registry/ServiceMacros.h"
#include "art/Utilities/Exception.h"
#include "fhiclcpp/ParameterSet.h"

namespace {
  void throw_if_empty(std::string const & par,
                      char const * par_name) {
    if (par.empty()) {
      throw art::Exception(art::errors::Configuration, "Missing Required Metadata")
        <<  par_name;
    }
  }
}

art::FileCatalogMetadata::FileCatalogMetadata(fhicl::ParameterSet const &ps,
                                              ActivityRegistry &)
  :
  md_()
{
  std::string applicationFamily;
  std::string applicationVersion;
  std::string fileType;
  if (ps.get_if_present("applicationFamily", applicationFamily) ||
      ps.get_if_present("applicationVersion", applicationVersion) ||
      ps.get_if_present("fileType", fileType)) {
    throw_if_empty(applicationFamily, "applicationFamily");
    addMetadata("applicationFamily", applicationFamily);
    throw_if_empty(applicationVersion, "applicationVersion");
    addMetadata("applicationVersion", applicationVersion);
    throw_if_empty(fileType,"fileType");
    addMetadata("fileType", fileType);
  }
}

void
art::FileCatalogMetadata::
addMetadata(std::string const & key, std::string const & value)
{
  md_.emplace_back(key, value);
}

// Standard constructor / maker is just fine.
DEFINE_ART_SERVICE(art::FileCatalogMetadata)
