#include "art/Framework/IO/Root/RootBranchInfoList.h"
#include "art/Utilities/Exception.h"

#include "boost/regex.hpp"

#include "TObjArray.h"
#include "TIterator.h"

art::RootBranchInfoList::RootBranchInfoList()
  :
  data_()
{}

art::RootBranchInfoList::RootBranchInfoList(TTree *tree)
  :
  data_()
{
  reset(tree);
}

void art::RootBranchInfoList::reset(TTree *tree) {
  if (!tree) {
    throw Exception(errors::NullPointerError)
      << "RootInfoBranchList given null TTree pointer.\n";
  }
  TObjArray *branches = tree->GetListOfBranches();
  TIter it(branches, kIterBackward);
  // Load the list backward, then searches can take place in the forward
  // direction.
  while (TBranch *b = dynamic_cast<TBranch *>(it.Next())) {
    data_.push_back(RootBranchInfo(b));
  }
}

bool
art::RootBranchInfoList::
findBranchInfo(TypeID const &type,
               InputTag const &tag,
               RootBranchInfo &rbInfo) const {
  std::ostringstream pat_s;
  pat_s << '^'
        << type.friendlyClassName()
        << '_'
        << tag.label()
        << '_'
        << tag.instance()
        << '_';
  if (tag.process().empty()) {
    pat_s << ".*";
  } else {
    pat_s << tag.process();
  }
  pat_s << '$';
  boost::regex r(pat_s.str());
  // data_t is ordered so that the first match is the best.
  for (Data_t::const_iterator
         i = data_.begin(),
         e = data_.end();
       i !=e;
       ++i) {
    if (boost::regex_match(i->branchName(), r)) {
      rbInfo = *i;
      return true;
    }
  }
  return false;
}