#ifndef art_Framework_Core_InputSourceFactory_h
#define art_Framework_Core_InputSourceFactory_h

// ======================================================================
//
// InputSourceFactory
//
// ======================================================================

#include "art/Framework/Core/InputSource.h"
#include "art/Framework/Core/LibraryManager.h"
#include "fhiclcpp/ParameterSet.h"
#include <memory>
#include <string>

// ----------------------------------------------------------------------

namespace art {
  class InputSourceFactory;
}

class art::InputSourceFactory
{
  // non-copyable:
  InputSourceFactory( InputSourceFactory const & );
  void  operator = ( InputSourceFactory const & );

 public:
  static std::auto_ptr<InputSource>
     make(fhicl::ParameterSet const&,
          InputSourceDescription &);

private:
  LibraryManager lm_;

  InputSourceFactory();
  ~InputSourceFactory();

  static InputSourceFactory &
     the_factory_();

};  // InputSourceFactory


// ======================================================================

#endif /* art_Framework_Core_InputSourceFactory_h */

// Local Variables:
// mode: c++
// End:
