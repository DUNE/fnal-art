#ifndef art_Persistency_Common_OrphanHandle_h
#define art_Persistency_Common_OrphanHandle_h

/*----------------------------------------------------------------------

OrphanHandle: Non-owning "smart pointer" for reference to EDProducts.


This is a very preliminary version, and lacks safety features and
elegance.

If the pointed-to EDProduct is destroyed, use of the OrphanHandle
becomes undefined. There is no way to query the OrphanHandle to
discover if this has happened.

OrphanHandles can have:
  -- Product pointer null and id == 0;
  -- Product pointer valid and id != 0;

To check validity, one can use the isValid() function.

----------------------------------------------------------------------*/

#include "art/Persistency/Provenance/ProductID.h"
#include <cassert>

namespace art {
  class EDProduct;

  template <typename T>
  class OrphanHandle {
  public:
    typedef T element_type;

    // Default constructed handles are invalid.
    OrphanHandle();

    OrphanHandle(T const* prod, ProductID const& id);

    // use compiler-generated copy c'tor and copy assignment

    ~OrphanHandle();

    void swap(OrphanHandle<T>& other);

    bool isValid() const;

    T const* product() const;
    T const* operator->() const; // alias for product()
    T const& operator*() const;

    ProductID id() const;

    void clear();

  private:
    T const* prod_;
    ProductID id_;
  };

  template <class T>
  OrphanHandle<T>::OrphanHandle() :
    prod_(0),
    id_()
  { }

  template <class T>
  OrphanHandle<T>::OrphanHandle(T const* prod, ProductID const& theId) :
    prod_(prod),
    id_(theId) {
      assert(prod_);
  }

  template <class T>
  OrphanHandle<T>::~OrphanHandle() {
    // Really nothing to do -- we do not own the things to which we
    // point.  For help in debugging, we clear the data.
    clear();
  }

  template <class T>
  void
  OrphanHandle<T>::clear()
  {
    prod_ = 0;
    id_ = ProductID();
  }

  template <class T>
  void
  OrphanHandle<T>::swap(OrphanHandle<T>& other) {
    using std::swap;
    std::swap(prod_, other.prod_);
    swap(id_, other.id_);
  }

  template <class T>
  bool
  OrphanHandle<T>::isValid() const {
    return prod_ != 0 && id_ != ProductID();
  }

  template <class T>
  inline
  T const*
  OrphanHandle<T>::product() const {
    // Should we throw if the pointer is null?
    return prod_;
  }

  template <class T>
  inline
  T const*
  OrphanHandle<T>::operator->() const {
    return product();
  }

  template <class T>
  inline
  T const&
  OrphanHandle<T>::operator*() const {
    return *product();
  }

  template <class T>
  inline
  ProductID
  OrphanHandle<T>::id() const {
    return id_;
  }

  // Free swap function
  template <class T>
  inline
  void
  swap(OrphanHandle<T>& a, OrphanHandle<T>& b)
  {
    a.swap(b);
  }
}

#endif /* art_Persistency_Common_OrphanHandle_h */

// Local Variables:
// mode: c++
// End: