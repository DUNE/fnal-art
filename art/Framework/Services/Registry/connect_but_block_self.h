#ifndef art_Framework_Services_Registry_connect_but_block_self_h
#define art_Framework_Services_Registry_connect_but_block_self_h

// ======================================================================
//
// connect_but_block_self - Connects a functional object to a signal, but
// guarantees that the functional object will never see a signal caused
// by its own action.
//
// ======================================================================

#include "cpp0x/memory"
#include "sigc++/signal.h"
#include "sigc++/connection.h"
#include "cpp0x/functional"

namespace art {
  template< class Func, class T1=void*, class T2=void*, class T3=void* >
  class BlockingWrapper
  {
  public:
    BlockingWrapper(Func iFunc): func_(iFunc), numBlocks_(0) {}

    void
      operator () ()
    {
      std::shared_ptr<void> guard( static_cast<void*>(0)
                                   , std::bind(&BlockingWrapper::unblock,this)
                                   );
      if( startBlocking() ) { func_(); }
    }

    void
      operator () (T1 iT)
    {
      std::shared_ptr<void> guard( static_cast<void*>(0)
                                   , std::bind(&BlockingWrapper::unblock,this)
                                   );
      if( startBlocking() ) { func_(iT); }
    }

    void
      operator () (T1 iT1, T2 iT2)
    {
      std::shared_ptr<void> guard( static_cast<void*>(0)
                                   , std::bind(&BlockingWrapper::unblock,this)
                                   );
      if( startBlocking() ) { func_(iT1,iT2); }
    }

    void
      operator () (T1 iT1, T2 iT2, T3 iT3)
    {
      std::shared_ptr<void> guard( static_cast<void*>(0)
                                   , std::bind(&BlockingWrapper::unblock,this)
                                   );
      if( startBlocking() ) { func_(iT1,iT2,iT3); }
    }

  private:
    bool
      startBlocking() { return 1 == ++numBlocks_; }
    void
      unblock() { --numBlocks_;}

    Func func_;
    int numBlocks_;

  };  // BlockingWrapper<>

  template< class T >
  typename T::value_type
    deref(T& iT)
  { return *iT; }

  template< class T >
  BlockingWrapper<T>
    make_blockingwrapper(T iT, const sigc::slot<void>*)
  { return BlockingWrapper<T>(iT); }

  template< class T, class TArg >
  BlockingWrapper<T,TArg>
    make_blockingwrapper(T iT, const sigc::slot<void,TArg>*)
  { return BlockingWrapper<T,TArg>(iT); }

  template<class Func, class Signal>
  void
    connect_but_block_self(Signal& oSignal, const Func& iFunc)
  {
    std::shared_ptr<std::shared_ptr<sigc::connection> > holder(new std::shared_ptr<sigc::connection>());
    *holder = std::shared_ptr<sigc::connection>(
    new sigc::connection(oSignal.connect( make_blockingwrapper(iFunc, static_cast<const typename Signal::slot_type*>(0)))));
  }

}  // art

// ======================================================================

#endif /* art_Framework_Services_Registry_connect_but_block_self_h */

// Local Variables:
// mode: c++
// End:
