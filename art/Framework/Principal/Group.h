#ifndef art_Framework_Principal_Group_h
#define art_Framework_Principal_Group_h

/*----------------------------------------------------------------------

Group: A collection of information related to a single EDProduct. This
is the storage unit of such information.

----------------------------------------------------------------------*/

#include "Reflex/Type.h"
#include "art/Framework/Principal/GroupFactory.h"
#include "art/Framework/Principal/fwd.h"
#include "art/Persistency/Common/DelayedReader.h"
#include "art/Persistency/Common/EDProduct.h"
#include "art/Persistency/Common/EDProductGetter.h"
#include "art/Persistency/Provenance/BranchDescription.h"
#include "art/Persistency/Provenance/BranchMapper.h"
#include "art/Persistency/Provenance/ProductID.h"
#include "art/Persistency/Provenance/ProductProvenance.h"
#include "art/Utilities/fwd.h"
#include "cetlib/exempt_ptr.h"
#include "cetlib/value_ptr.h"
#include "cpp0x/memory"

namespace art {
  class Group;

  std::ostream &operator<<(std::ostream &os, Group const &g);
}

// Noncopyable through inheritance from EDProductGetter.
class art::Group : public EDProductGetter {

public:
  Group();
protected: // Use GroupFactory to make.
  Group(BranchDescription const &bd,
        ProductID const &pid,
        art::TypeID const &wrapper_type,
        cet::exempt_ptr<Worker> productProducer = cet::exempt_ptr<Worker>(),
        cet::exempt_ptr<EventPrincipal> onDemandPrincipal =
          cet::exempt_ptr<EventPrincipal>());
  Group(std::auto_ptr<EDProduct> edp,
        BranchDescription const &bd,
        ProductID const &pid,
        art::TypeID const &wrapper_type);
  friend std::auto_ptr<Group> gfactory::make_group(BranchDescription const&, ProductID const &);
  friend std::auto_ptr<Group> gfactory::make_group(BranchDescription const&, ProductID const &, cet::exempt_ptr<Worker>, cet::exempt_ptr<EventPrincipal>);
  friend std::auto_ptr<Group> gfactory::make_group(std::auto_ptr<EDProduct>, BranchDescription const&, ProductID const &);
public:
  virtual ~Group();

  void swap(Group &other);

  // product is not available (dropped or never created)
  bool productUnavailable() const;

  // Scheduled for on-demand production
  bool onDemand() const { return productProducer_ && onDemandPrincipal_; }

  virtual EDProduct const *getIt() const { resolveProductIfAvailable(true, producedWrapperType()); return uniqueProduct(); }
  virtual EDProduct const *anyProduct() const { return product_.get(); }
  virtual EDProduct const *uniqueProduct() const { return product_.get(); }
  virtual EDProduct const *uniqueProduct(TypeID const &) const { return product_.get(); }

  cet::exempt_ptr<ProductProvenance const> productProvenancePtr() const;

  BranchDescription const &productDescription() const {return *branchDescription_;}

  std::string const &moduleLabel() const {return branchDescription_->moduleLabel();}

  std::string const &productInstanceName() const {return branchDescription_->productInstanceName();}

  std::string const &processName() const {return branchDescription_->processName();}

  ProductStatus status() const;

  void setResolvers(BranchMapper  const &bm,
                    DelayedReader const &dr) {
    ppResolver_.reset(&bm);
    productResolver_.reset(&dr);
  }

  virtual bool
  resolveProduct(bool fillOnDemand, TypeID const &) const;
  virtual bool
  resolveProductIfAvailable(bool fillOnDemand, TypeID const &) const;

  // Write the group to the stream.
  void write(std::ostream &os) const;

  // Replace the existing group with a new one
  void replace(Group &g);

  ProductID const &productID() const {return pid_;};

  TypeID const & producedWrapperType() const { return wrapper_type_; }

protected:
  std::auto_ptr<EDProduct> obtainDesiredProduct(bool fillOnDemand, TypeID const &) const;
  void setProduct(std::auto_ptr<EDProduct> prod) const;

private:
  bool dropped() const;

  art::TypeID                               wrapper_type_;
  cet::exempt_ptr<BranchMapper const>       ppResolver_;
  cet::exempt_ptr<DelayedReader const>      productResolver_;
  mutable cet::value_ptr<EDProduct>         product_;
  cet::exempt_ptr<BranchDescription const>  branchDescription_;
  mutable ProductID                         pid_;
  cet::exempt_ptr<Worker>                   productProducer_;
  // FIXME: This will be a generic principal when meta data is fixed.
  cet::exempt_ptr<EventPrincipal>           onDemandPrincipal_;
};  // Group

inline
std::ostream &
art::operator << (std::ostream &os, art::Group const &g)
{
  g.write(os);
  return os;
}

#endif /* art_Framework_Principal_Group_h */

// Local Variables:
// mode: c++
// End: