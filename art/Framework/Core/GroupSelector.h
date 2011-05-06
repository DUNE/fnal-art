#ifndef art_Framework_Core_GroupSelector_h
#define art_Framework_Core_GroupSelector_h

// ======================================================================
//
// Class GroupSelector. Class for user to select specific groups in event.
//
// ======================================================================

#include "cpp0x/regex"
#include "fhiclcpp/ParameterSet.h"
#include <iosfwd>
#include <string>
#include <vector>

// ----------------------------------------------------------------------

namespace art {
  class BranchDescription;
  class GroupSelectorRules;

  class GroupSelector
  {
  public:
    GroupSelector();

    // N.B.: we assume there are not null pointers in the vector allBranches.
    void initialize(GroupSelectorRules const& rules,
                    std::vector<BranchDescription const*> const& branchDescriptions);

    bool selected(BranchDescription const& desc) const;

    // Printout intended for debugging purposes.
    void print(std::ostream& os) const;

    bool initialized() const {return initialized_;}

  private:

    // We keep a sorted collection of branch names, indicating the
    // groups which are to be selected.

    // TODO: See if we can keep pointer to (const) BranchDescriptions,
    // so that we can do pointer comparison rather than string
    // comparison. This will work if the BranchDescription we are
    // given in the 'selected' member function is one of the instances
    // that are managed by the ProductRegistry used to initialize the
    // entity that contains this GroupSelector.
    std::vector<std::string> groupsToSelect_;
    bool initialized_;
  };  // GroupSelector

  std::ostream&
  operator<< (std::ostream& os, const GroupSelector& gs);

}  // art

// ======================================================================

#endif /* art_Framework_Core_GroupSelector_h */

// Local Variables:
// mode: c++
// End:
