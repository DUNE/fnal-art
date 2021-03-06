#include "art/Framework/IO/Root/RootTree.h"
// vim: set sw=2:

#include "art/Framework/IO/Root/RootDelayedReader.h"
#include "art/Framework/Principal/Principal.h"
#include "art/Framework/Principal/Provenance.h"
#include "art/Persistency/Provenance/BranchDescription.h"
#include "art/Utilities/WrappedClassName.h"
#include "Rtypes.h"
#include "TFile.h"
#include "TTreeCache.h"
#include "TTreeIndex.h"
#include "TVirtualIndex.h"
#include <iostream>
#include <utility>

namespace art {

namespace {

TBranch* getAuxiliaryBranch(TTree* tree, BranchType const& branchType)
{
  TBranch* branch = tree->GetBranch(BranchTypeToAuxiliaryBranchName(
                                      branchType).c_str());
  return branch;
}

TBranch* getProductProvenanceBranch(TTree* tree, BranchType const& branchType)
{
  TBranch* branch = tree->GetBranch(productProvenanceBranchName(
                                      branchType).c_str());
  return branch;
}

} // unnamed namespace

RootTree::
RootTree(std::shared_ptr<TFile> filePtr, BranchType const& branchType,
         int64_t saveMemoryObjectThreshold,
         cet::exempt_ptr<RootInputFile> primaryFile)
  : filePtr_(filePtr)
  , tree_(0)
  , metaTree_(0)
  , branchType_(branchType)
  , saveMemoryObjectThreshold_(saveMemoryObjectThreshold)
  , auxBranch_(0)
  , productProvenanceBranch_(0)
  , entries_(0)
  , entryNumber_(-1)
  , branchNames_()
  , branches_(new BranchMap)
  , primaryFile_(primaryFile)
{
  if (filePtr_) {
    tree_ = static_cast<TTree*>(filePtr->Get(
      BranchTypeToProductTreeName(branchType).c_str()));
    metaTree_ = static_cast<TTree*>(filePtr->Get(
      BranchTypeToMetaDataTreeName(branchType).c_str()));
  }
  if (tree_) {
    auxBranch_ = getAuxiliaryBranch(tree_, branchType_);
    entries_ = tree_->GetEntries();
  }
  if (metaTree_) {
    productProvenanceBranch_ =
      getProductProvenanceBranch(metaTree_, branchType_);
  }
  if (!isValid()) {
    throw Exception(errors::FileReadError)
        << "RootTree for branch type "
        << BranchTypeToString(branchType)
        << "Could not be initialized correctly from input file.\n";
  }
}

bool
RootTree::
isValid() const
{
  if ((metaTree_ == 0) || (metaTree_->GetNbranches() == 0)) {
    return tree_ && auxBranch_ && (tree_->GetNbranches() == 1);
  }
  return tree_ && auxBranch_ && metaTree_ && productProvenanceBranch_;
}

bool
RootTree::
hasBranch(std::string const& branchName) const
{
  return tree_->GetBranch(branchName.c_str()) != 0;
}

void
RootTree::
addBranch(BranchKey const& key, BranchDescription const& prod,
          std::string const& branchName)
{
  assert(isValid());
  TBranch* branch = tree_->GetBranch(branchName.c_str());
  assert(prod.present() == (branch != 0));
  input::BranchInfo info(prod);
  info.productBranch_ = 0;
  if (prod.present()) {
    info.productBranch_ = branch;
    branchNames_.emplace_back(prod.branchName());
  }
  branches_->emplace(key, info);
}

void
RootTree::
dropBranch(std::string const& branchName)
{
  TBranch* branch = tree_->GetBranch(branchName.c_str());
  if (!branch) {
    return;
  }
  TObjArray* leaves = tree_->GetListOfLeaves();
  int entries = leaves->GetEntries();
  for (int i = 0; i < entries; ++i) {
    TLeaf* leaf = (TLeaf*)(*leaves)[i];
    if (leaf == 0) {
      continue;
    }
    TBranch* br = leaf->GetBranch();
    if (br == 0) {
      continue;
    }
    if (br->GetMother() == branch) {
      leaves->Remove(leaf);
    }
  }
  leaves->Compress();
  tree_->GetListOfBranches()->Remove(branch);
  tree_->GetListOfBranches()->Compress();
  delete branch;
}

std::unique_ptr<DelayedReader>
RootTree::
makeDelayedReader(BranchType branchType, EventID eID) const
{
  return
    std::make_unique<RootDelayedReader>(entryNumber_,
                                        branches_,
                                        filePtr_,
                                        saveMemoryObjectThreshold_,
                                        primaryFile_,
                                        branchType,
                                        eID);
}

void
RootTree::
setCacheSize(unsigned int cacheSize) const
{
  tree_->SetCacheSize(static_cast<Long64_t>(cacheSize));
}

void
RootTree::
setTreeMaxVirtualSize(int treeMaxVirtualSize)
{
  if (treeMaxVirtualSize >= 0) {
    tree_->SetMaxVirtualSize(static_cast<Long64_t>(treeMaxVirtualSize));
  }
}

void
RootTree::
setEntryNumber(EntryNumber theEntryNumber)
{
  // Note: An entry number of -1 is ok, this can be used
  //       to put the tree an an invalid entry.
  if (TTreeCache* tc = dynamic_cast<TTreeCache*>(
                         filePtr_->GetCacheRead(tree_))) {
    assert(tree_ == tc->GetTree());
    if ((theEntryNumber >= 0) && tc->IsLearning()) {
      tc->SetLearnEntries(1);
      tc->SetEntryRange(0, tree_->GetEntries());
      for (auto i = branches_->cbegin(), e = branches_->cend(); i != e; ++i) {
        if (i->second.productBranch_) {
          tc->AddBranch(i->second.productBranch_, kTRUE);
        }
      }
      tc->StopLearningPhase();
    }
  }
  entryNumber_ = theEntryNumber;
  auto err = tree_->LoadTree(theEntryNumber);
  if (err == -2) {
    // FIXME: Throw an error here!
    // FIXME: -2 means entry number too big.
  }
}

std::unique_ptr<BranchMapper>
RootTree::
makeBranchMapper() const
{
  std::unique_ptr<BranchMapper> bm(
    new BranchMapperWithReader(productProvenanceBranch_, entryNumber_));
  return bm;
}

namespace input {

Int_t
getEntry(TBranch* branch, EntryNumber entryNumber) try
{
  return branch->GetEntry(entryNumber);
}
catch (cet::exception& e)
{
  throw art::Exception(art::errors::FileReadError) << e.explain_self() << "\n";
}

Int_t
getEntry(TTree* tree, EntryNumber entryNumber) try
{
  return tree->GetEntry(entryNumber);
}
catch (cet::exception& e)
{
  throw art::Exception(art::errors::FileReadError) << e.explain_self() << "\n";
}

} // namespace input
} // namespace art

