#ifndef art_Framework_Modules_MixFilter_h
#define art_Framework_Modules_MixFilter_h

#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/IO/ProductMix/MixHelper.h"
#include "art/Framework/Services/Optional/RandomNumberGenerator.h"
#include "cpp0x/type_traits"

namespace art {
  template <class T>
  class MixFilter;

  namespace detail {
    // Template metaprogramming.
    typedef char (& no_tag )[1]; // type indicating FALSE
    typedef char (& yes_tag)[2]; // type indicating TRUE

    ////////////////////////////////////////////////////////////////////
    // Does the detail object have a method void startEvent()?
    template <typename T, void (T::*)()> struct startEvent_function;

    template <typename T> struct void_do_not_call_startEvent {
    public:
      void operator()(T &t) {}
    };

    template <typename T> struct call_startEvent {
    public:
      void operator()(T &t) { t.startEvent(); }
    };

    template <typename T>
    no_tag
    has_startEvent_helper(...);

    template <typename T>
    yes_tag
    has_startEvent_helper(startEvent_function<T, &T::startEvent> * dummy);

    template <typename T>
    struct has_startEvent {
      static bool const value =
        sizeof(has_startEvent_helper<T>(0)) == sizeof(yes_tag);
    };
    ////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////
    // Does the detail object have a method void finalizeEvent(Event&)?
    template <typename T, void (T::*)(Event &)> struct finalizeEvent_function;

    template <typename T> struct void_do_not_call_finalizeEvent {
    public:
      void operator()(T &, Event &) {}
    };

    template <typename T> struct call_finalizeEvent {
    public:
      void operator()(T &t, Event &e) { t.finalizeEvent(e); }
    };

    template <typename T>
    no_tag
    has_finalizeEvent_helper(...);

    template <typename T>
    yes_tag
    has_finalizeEvent_helper(finalizeEvent_function<T, &T::finalizeEvent> * dummy);

    template <typename T>
    struct has_finalizeEvent {
      static bool const value =
        sizeof(has_finalizeEvent_helper<T>(0)) == sizeof(yes_tag);
    };
    ////////////////////////////////////////////////////////////////////

  } // detail namespace

} // art namespace

template <class T>
class art::MixFilter : public art::EDFilter {
public:
  typedef T MixDetail;
  explicit MixFilter(fhicl::ParameterSet const &p);

  virtual void beginJob();
  virtual bool filter(art::Event &e);

private:
  MixHelper helper_;
  MixDetail detail_;
};

template <class T>
art::MixFilter<T>::MixFilter(fhicl::ParameterSet const &p)
  :
  EDFilter(),
  helper_((createEngine(get_seed_value(p)),p), *this), // See note below
  detail_(p, helper_)
{
  // Note that the random number engine is created in the initializer
  // list as part of a comma-separated argument bundle to the
  // constructor of MixHelper. This enables the engine to be obtained
  // by the helper and or detail objects in their constructors via a
  // service handle to the random number generator service. Do NOT
  // remove the seemingly-superfluous parentheses.
}

template <class T>
void
art::MixFilter<T>::beginJob() {
  helper_.postRegistrationInit();
}

template <class T>
bool
art::MixFilter<T>::filter(art::Event &e) {
  // 1. Call detail object's startEvent() if it exists.
  typename std::conditional<detail::has_startEvent<T>::value,
                            detail::call_startEvent<T>,
                            detail::void_do_not_call_startEvent<T> >::type
           maybe_call_startEvent;
  maybe_call_startEvent(detail_);

  // 2. Ask detail object how many events to read.
  size_t nSecondaries = detail_.nSecondaries();

  // 3. Make the MixHelper read info into all the products, invoke the
  // mix functions and put the products into the event.
  helper_.mixAndPut(nSecondaries, e);

  // 4. Call detail object's finalizeEvent() if it exists.
  typename std::conditional<detail::has_finalizeEvent<T>::value,
                            detail::call_finalizeEvent<T>,
                            detail::void_do_not_call_finalizeEvent<T> >::type
           maybe_call_finalizeEvent;
  maybe_call_finalizeEvent(detail_, e);
  return false;
}

// }
#endif /* art_Framework_Modules_MixFilter_h */

// Local Variables:
// mode: c++
// End: