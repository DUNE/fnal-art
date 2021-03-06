#include "art/Framework/IO/Root/RootInputFile.h"
// vim: set sw=2:

#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/Core/GroupSelector.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "art/Framework/IO/Root/DuplicateChecker.h"
#include "art/Framework/IO/Root/FastCloningInfoProvider.h"
#include "art/Framework/IO/Root/GetFileFormatEra.h"
#include "art/Framework/IO/Root/setFileIndexPointer.h"
#include "art/Framework/IO/Root/rootNames.h"
#include "art/Persistency/Common/EDProduct.h"
#include "art/Persistency/Provenance/BranchChildren.h"
#include "art/Persistency/Provenance/BranchDescription.h"
#include "art/Persistency/Provenance/BranchType.h"
#include "art/Persistency/Provenance/ParameterSetBlob.h"
#include "art/Persistency/Provenance/ParameterSetMap.h"
#include "art/Persistency/Provenance/ParentageRegistry.h"
#include "art/Persistency/Provenance/ProcessHistoryRegistry.h"
#include "art/Persistency/Provenance/ProductList.h"
#include "art/Persistency/Provenance/RunID.h"
#include "art/Persistency/RootDB/SQLite3Wrapper.h"
#include "art/Utilities/Exception.h"
#include "art/Utilities/FriendlyName.h"
#include "cetlib/container_algorithms.h"
#include "cpp0x/algorithm"
#include "cpp0x/utility"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/ParameterSetID.h"
#include "fhiclcpp/ParameterSetRegistry.h"
#include "fhiclcpp/make_ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "Rtypes.h"
#include "TClass.h"
#include "TFile.h"
#include "TTree.h"

extern "C" {
#include "sqlite3.h"
}

using namespace cet;
using namespace std;

namespace art {

RootInputFile::
RootInputFile(string const& fileName,
              string const& catalogName,
              ProcessConfiguration const& processConfiguration,
              string const& logicalFileName,
              shared_ptr<TFile> filePtr,
              EventID const& origEventID,
              unsigned int eventsToSkip,
              vector<SubRunID> const& whichSubRunsToSkip,
              FastCloningInfoProvider const& fcip,
              unsigned int treeCacheSize,
              int64_t treeMaxVirtualSize,
              int64_t saveMemoryObjectThreshold,
              bool delayedReadSubRunProducts,
              bool delayedReadRunProducts,
              InputSource::ProcessingMode processingMode,
              int forcedRunOffset,
              bool noEventSort,
              GroupSelectorRules const& groupSelectorRules,
              bool dropMergeable,
              shared_ptr<DuplicateChecker> duplicateChecker,
              bool dropDescendants,
              exempt_ptr<RootInputFile> primaryFile,
              int secondaryFileNameIdx,
              vector<string> const& secondaryFileNames,
              RootInputFileSequence* rifSequence)
  : file_(fileName)
  , logicalFile_(logicalFileName)
  , catalog_(catalogName)
  , delayedReadSubRunProducts_(delayedReadSubRunProducts)
  , delayedReadRunProducts_(delayedReadRunProducts)
  , processConfiguration_(processConfiguration)
  , filePtr_(filePtr)
  , fileFormatVersion_()
  , fileIndexSharedPtr_(new FileIndex)
  , fileIndex_(*fileIndexSharedPtr_)
  , fiBegin_(fileIndex_.begin())
  , fiEnd_(fiBegin_)
  , fiIter_(fiBegin_)
  , origEventID_(origEventID)
  , eventsToSkip_(eventsToSkip)
  , whichSubRunsToSkip_(whichSubRunsToSkip)
  , noEventSort_(noEventSort)
  , fastClonable_(false)
  , eventAux_()
  , subRunAux_()
  , runAux_()
  , eventTree_(filePtr_, InEvent, saveMemoryObjectThreshold, this)
  , subRunTree_(filePtr_, InSubRun, saveMemoryObjectThreshold, this)
  , runTree_(filePtr_, InRun, saveMemoryObjectThreshold, this)
  , treePointers_()
  , productListHolder_(new ProductRegistry)
  , branchIDLists_()
  , processingMode_(processingMode)
  , forcedRunOffset_(forcedRunOffset)
  , eventHistoryTree_(0)
  , history_(new History)
  , branchChildren_(new BranchChildren)
  , duplicateChecker_(duplicateChecker)
  , primaryFile_(primaryFile ? primaryFile : this)
  , secondaryFileNameIdx_(secondaryFileNameIdx)
  , secondaryFileNames_(secondaryFileNames)
  , secondaryFiles_()
  , rifSequence_(rifSequence)
  , primaryEP_()
  , primaryRP_()
  , primarySRP_()
  , secondaryRPs_()
  , secondarySRPs_()
{
  secondaryFiles_.resize(secondaryFileNames_.size());
  eventTree_.setCacheSize(treeCacheSize);
  eventTree_.setTreeMaxVirtualSize(treeMaxVirtualSize);
  subRunTree_.setTreeMaxVirtualSize(treeMaxVirtualSize);
  runTree_.setTreeMaxVirtualSize(treeMaxVirtualSize);
  treePointers_[InEvent] = &eventTree_;
  treePointers_[InSubRun] = &subRunTree_;
  treePointers_[InRun] = &runTree_;
  // Read the metadata tree.
  auto metaDataTree = dynamic_cast<TTree*>(
                          filePtr_->Get(rootNames::metaDataTreeName().c_str()));
  if (!metaDataTree) {
    throw art::Exception(errors::FileReadError)
        << couldNotFindTree(rootNames::metaDataTreeName());
  }
  auto fftPtr = &fileFormatVersion_;
  using namespace art::rootNames;
  metaDataTree->SetBranchAddress(metaBranchRootName<FileFormatVersion>(),
                                 &fftPtr);
  auto findexPtr = &fileIndex_;
  setFileIndexPointer(filePtr_.get(), metaDataTree, findexPtr);
  auto plhPtr = productListHolder_.get();
  assert(plhPtr != nullptr &&
         "INTERNAL ERROR: productListHolder_ not initialized prior to use!.");
  metaDataTree->SetBranchAddress(metaBranchRootName<ProductRegistry>(),
                                 &plhPtr);
  if (plhPtr != productListHolder_.get()) {
    // Should never happen, but just in case ROOT's behavior changes.
    throw Exception(errors::LogicError)
      << "ROOT has changed behavior and caused a memory leak while setting "
      << "a branch address.";
  }
  ParameterSetMap psetMap;
  auto psetMapPtr = &psetMap;
  if (metaDataTree->GetBranch(metaBranchRootName<ParameterSetMap>())) {
    // May be in MetaData tree or DB.
    metaDataTree->SetBranchAddress(metaBranchRootName<ParameterSetMap>(),
                                   &psetMapPtr);
  }
  ProcessHistoryMap pHistMap;
  auto pHistMapPtr = &pHistMap;
  metaDataTree->SetBranchAddress(metaBranchRootName<ProcessHistoryMap>(),
                                 &pHistMapPtr);
  unique_ptr<BranchIDLists> branchIDListsAPtr(new BranchIDLists);
  auto branchIDListsPtr = branchIDListsAPtr.get();
  metaDataTree->SetBranchAddress(metaBranchRootName<BranchIDLists>(),
                                 &branchIDListsPtr);
  auto branchChildrenBuffer = branchChildren_.get();
  metaDataTree->SetBranchAddress(metaBranchRootName<BranchChildren>(),
                                 &branchChildrenBuffer);
  // Here we read the metadata tree
  input::getEntry(metaDataTree, 0);
  branchIDLists_.reset(branchIDListsAPtr.release());
  // Check the, "Era" of the input file (new since art v0.5.0). If it
  // does not match what we expect we cannot read the file. Required
  // since we reset the file versioning since forking off from
  // CMS. Files written by art prior to v0.5.0 will *also* not be
  // readable because they do not have this datum and because the run,
  // subrun and event-number handling has changed significantly.
  string const expected_era = art::getFileFormatEra();
  if (fileFormatVersion_.era_ != expected_era) {
    throw art::Exception(art::errors::FileReadError)
        << "Can only read files written during the \""
        << expected_era
        << "\" era: "
        << "Era of "
        << "\""
        << file_
        << "\" was "
        << (fileFormatVersion_.era_.empty() ?
            "not set" :
            ("set to \"" + fileFormatVersion_.era_ + "\" "))
        << ".\n";
  }
  // Merge into the hashed registries.
  // Parameter Set
  for (auto I = psetMap.cbegin(), E = psetMap.cend(); I != E; ++I) {
    fhicl::ParameterSet pset;
    fhicl::make_ParameterSet(I->second.pset_, pset);
    // Note ParameterSet::id() has the side effect of
    // making sure the parameter set *has* an ID.
    pset.id();
    fhicl::ParameterSetRegistry::put(pset);
  }
  // Also need to check MetaData DB if we have one.
  if (fileFormatVersion_.value_ >= 5) {
    // Open the DB.
    SQLite3Wrapper sqliteDB(filePtr_.get(), "RootFileDB");
    fhicl::ParameterSetRegistry::importFrom(sqliteDB);
  }
  ProcessHistoryRegistry::put(pHistMap);
  validateFile();
  // Read the parentage tree.  Old format files are
  // handled internally in readParentageTree().
  readParentageTree();
  initializeDuplicateChecker();
  if (noEventSort_) {
    fileIndex_.sortBy_Run_SubRun_EventEntry();
  }
  fiIter_ = fileIndex_.begin();
  fiBegin_ = fileIndex_.begin();
  fiEnd_ = fileIndex_.end();
  readEventHistoryTree();
  // Update transient presence information to match input tree contents.
  auto& prodList = productListHolder_->productList_;
  for (auto I = prodList.begin(), E = prodList.end(); I != E; ++I) {
    auto& bd = I->second;
    bd.setPresent(treePointers_[bd.branchType()]->hasBranch(bd.branchName()));
  }
  dropOnInput(groupSelectorRules, dropDescendants, /*unused*/dropMergeable,
              prodList);
  // Create branches to read the products from the input file.
  for (auto I = prodList.cbegin(), E = prodList.cend(); I != E; ++I) {
    auto const& bd = I->second;
    treePointers_[bd.branchType()]->addBranch(I->first, bd, bd.branchName());
  }
  // Determine if this file is fast clonable.
  fastClonable_ = setIfFastClonable(fcip);
  reportOpened();
}

void
RootInputFile::
readParentageTree()
{
  //
  //  Auxiliary routine for the constructor.
  //
  auto parentageTree = dynamic_cast<TTree*>(filePtr_->Get(
                         rootNames::parentageTreeName().c_str()));
  if (!parentageTree) {
    throw art::Exception(errors::FileReadError)
        << couldNotFindTree(rootNames::parentageTreeName());
  }
  ParentageID idBuffer;
  auto pidBuffer = &idBuffer;
  parentageTree->SetBranchAddress(rootNames::parentageIDBranchName().c_str(),
                                  &pidBuffer);
  Parentage parentageBuffer;
  auto pParentageBuffer = &parentageBuffer;
  parentageTree->SetBranchAddress(rootNames::parentageBranchName().c_str(),
                                  &pParentageBuffer);
  for (Long64_t i = 0, numEntries = parentageTree->GetEntries(); i < numEntries;
       ++i) {
    input::getEntry(parentageTree, i);
    if (idBuffer != parentageBuffer.id()) {
      throw art::Exception(errors::DataCorruption)
          << "Corruption of Parentage tree detected.\n";
    }
    ParentageRegistry::put(parentageBuffer);
  }
  parentageTree->SetBranchAddress(rootNames::parentageIDBranchName().c_str(),
                                  0);
  parentageTree->SetBranchAddress(rootNames::parentageBranchName().c_str(), 0);
}

EventID
RootInputFile::
eventIDForFileIndexPosition() const
{
  if (fiIter_ == fiEnd_) {
    return EventID();
  }
  return fiIter_->eventID_;
}


bool
RootInputFile::
setIfFastClonable(FastCloningInfoProvider const& fcip) const
{
  if (!fcip.fastCloningPermitted()) {
    return false;
  }
  if (!fileFormatVersion_.fastCopyPossible()) {
    return false;
  }
  if (secondaryFileNames_.size() != 0) {
    return false;
  }
  if (!fileIndex_.allEventsInEntryOrder()) {
    return false;
  }
  if (eventsToSkip_ != 0) {
    return false;
  }
  if ((fcip.remainingEvents() >= 0) &&
      (eventTree_.entries() > fcip.remainingEvents())) {
    return false;
  }
  if ((fcip.remainingSubRuns() >= 0) &&
      (subRunTree_.entries() > fcip.remainingSubRuns())) {
    return false;
  }
  if (processingMode_ != InputSource::RunsSubRunsAndEvents) {
    return false;
  }
  if (forcedRunOffset_ != 0) {
    return false;
  }
  // Find entry for first event in file.
  auto it = fiBegin_;
  while ((it != fiEnd_) && (it->getEntryType() != FileIndex::kEvent)) {
    ++it;
  }
  if (it == fiEnd_) {
    return false;
  }
  if (it->eventID_ < origEventID_) {
    return false;
  }
  for (auto I = whichSubRunsToSkip_.cbegin(), E = whichSubRunsToSkip_.cend();
       I != E; ++I) {
    if (fileIndex_.findSubRunPosition(*I, true) != fiEnd_) {
      // We must skip a subRun in this file.  We will simply assume that
      // it may contain an event, in which case we cannot fast copy.
      return false;
    }
  }
  return true;
}


int
RootInputFile::
setForcedRunOffset(RunNumber_t const& forcedRunNumber)
{
  if (fiBegin_ == fiEnd_) {
    return 0;
  }
  forcedRunOffset_ = 0;
  if (!RunID(forcedRunNumber).isValid()) {
    return 0;
  }
  forcedRunOffset_ = forcedRunNumber - fiBegin_->eventID_.run();
  if (forcedRunOffset_ != 0) {
    fastClonable_ = false;
  }
  return forcedRunOffset_;
}

shared_ptr<FileBlock>
RootInputFile::
createFileBlock() const
{
  return shared_ptr<FileBlock>(
           new FileBlock(
             fileFormatVersion_
             , eventTree_.tree()
             , eventTree_.metaTree()
             , subRunTree_.tree()
             , subRunTree_.metaTree()
             , runTree_.tree()
             , runTree_.metaTree()
             , fastClonable()
             , file_
             , branchChildren_
           )
         );
}

FileIndex::EntryType
RootInputFile::
getEntryType() const
{
  if (fiIter_ == fiEnd_) {
    return FileIndex::kEnd;
  }
  return fiIter_->getEntryType();
}

// Temporary KLUDGE until we can properly merge runs and subRuns across files
// This KLUDGE skips duplicate run or subRun entries.
FileIndex::EntryType
RootInputFile::
getEntryTypeSkippingDups()
{
  while (1) {
    if (fiIter_ == fiEnd_) {
      return FileIndex::kEnd;
    }
    if (fiIter_->eventID_.isValid() || (fiIter_ == fiBegin_) ||
        ((fiIter_ - 1)->eventID_.subRun() != fiIter_->eventID_.subRun())) {
      return fiIter_->getEntryType();
    }
    nextEntry();
  }
}

FileIndex::EntryType
RootInputFile::
getNextEntryTypeWanted()
{
  auto entryType = getEntryTypeSkippingDups();
  if (entryType == FileIndex::kEnd) {
    return FileIndex::kEnd;
  }
  RunID currentRun(fiIter_->eventID_.runID());
  if (!currentRun.isValid()) {
    return FileIndex::kEnd;
  }
  if (entryType == FileIndex::kRun) {
    // Skip any runs before the first run specified
    if (currentRun < origEventID_.runID()) {
      fiIter_ = fileIndex_.findRunPosition(origEventID_.runID(), false);
      return getNextEntryTypeWanted();
    }
    return FileIndex::kRun;
  }
  if (processingMode_ == InputSource::Runs) {
    fiIter_ = fileIndex_.findRunPosition(currentRun.isValid() ?
                     currentRun.next() : currentRun, false);
    return getNextEntryTypeWanted();
  }
  SubRunID const& currentSubRun = fiIter_->eventID_.subRunID();
  if (entryType == FileIndex::kSubRun) {
    // Skip any subRuns before the first subRun specified
    if ((currentRun == origEventID_.runID()) &&
        (currentSubRun < origEventID_.subRunID())) {
      fiIter_ = fileIndex_.findSubRunOrRunPosition(origEventID_.subRunID());
      return getNextEntryTypeWanted();
    }
    // Skip the subRun if it is in whichSubRunsToSkip_.
    if (binary_search_all(whichSubRunsToSkip_, currentSubRun)) {
      fiIter_ = fileIndex_.findSubRunOrRunPosition(currentSubRun.next());
      return getNextEntryTypeWanted();
    }
    return FileIndex::kSubRun;
  }
  if (processingMode_ == InputSource::RunsAndSubRuns) {
    fiIter_ = fileIndex_.findSubRunOrRunPosition(currentSubRun.next());
    return getNextEntryTypeWanted();
  }
  assert(entryType == FileIndex::kEvent);
  // Skip any events before the first event specified
  if (fiIter_->eventID_ < origEventID_) {
    fiIter_ = fileIndex_.findPosition(origEventID_);
    return getNextEntryTypeWanted();
  }
  if (duplicateChecker_.get() &&
      duplicateChecker_->isDuplicateAndCheckActive(fiIter_->eventID_, file_)) {
    nextEntry();
    return getNextEntryTypeWanted();
  }
  if (eventsToSkip_ == 0) {
    return FileIndex::kEvent;
  }
  // We have specified a count of events to skip,
  // keep skipping events in this subRun block
  // until we reach the end of the subRun block or
  // the full count of the number of events to skip.
  while ((eventsToSkip_ != 0) && (fiIter_ != fiEnd_) &&
         (getEntryTypeSkippingDups() == FileIndex::kEvent)) {
    nextEntry();
    --eventsToSkip_;
    while ((eventsToSkip_ != 0) && (fiIter_ != fiEnd_) &&
           (fiIter_->getEntryType() == FileIndex::kEvent) &&
           duplicateChecker_.get() &&
           duplicateChecker_->isDuplicateAndCheckActive(
             fiIter_->eventID_, file_)) {
      nextEntry();
    }
  }
  return getNextEntryTypeWanted();
}

void
RootInputFile::
validateFile()
{
  if (!fileFormatVersion_.isValid()) {
    fileFormatVersion_.value_ = 0;
  }
  if (!eventTree_.isValid()) {
    throw art::Exception(errors::DataCorruption)
        << "'Events' tree is corrupted or not present\n"
        << "in the input file.\n";
  }
  if (fileIndex_.empty()) {
    throw art::Exception(art::errors::FileReadError)
        << "FileIndex information is missing for the input file.\n";
  }
}

void
RootInputFile::
reportOpened()
{
}

void
RootInputFile::
close(bool reallyClose)
{
  if (!reallyClose) {
    return;
  }
  filePtr_->Close();
  for (auto const & sf : secondaryFiles_) {
    if (!sf) {
      continue;
    }
    sf->filePtr_->Close();
  }
}

void
RootInputFile::
fillEventAuxiliary()
{
  auto pEvAux = &eventAux_;
  eventTree_.fillAux<EventAuxiliary>(pEvAux);
}

void
RootInputFile::
fillHistory()
{
  // We could consider doing delayed reading, but because we have to
  // store this History object in a different tree than the event
  // data tree, this is too hard to do in this first version.
  auto pHistory = history_.get();
  auto eventHistoryBranch = eventHistoryTree_->GetBranch(
                                  rootNames::eventHistoryBranchName().c_str());
  if (!eventHistoryBranch) {
    throw art::Exception(errors::DataCorruption)
        << "Failed to find history branch in event history tree.\n";
  }
  eventHistoryBranch->SetAddress(&pHistory);
  input::getEntry(eventHistoryTree_, eventTree_.entryNumber());
}

void
RootInputFile::
fillSubRunAuxiliary()
{
  SubRunAuxiliary* pSubRunAux = &subRunAux_;
  subRunTree_.fillAux<SubRunAuxiliary>(pSubRunAux);
}

void
RootInputFile::
fillRunAuxiliary()
{
  auto pRunAux = &runAux_;
  runTree_.fillAux<RunAuxiliary>(pRunAux);
}

int
RootInputFile::
skipEvents(int offset)
{
  while ((offset > 0) && (fiIter_ != fiEnd_)) {
    if (fiIter_->getEntryType() == FileIndex::kEvent) {
      --offset;
    }
    nextEntry();
  }
  while ((offset < 0) && (fiIter_ != fiBegin_)) {
    previousEntry();
    if (fiIter_->getEntryType() == FileIndex::kEvent) {
      ++offset;
    }
  }
  while ((fiIter_ != fiEnd_) &&
         (fiIter_->getEntryType() != FileIndex::kEvent)) {
    nextEntry();
  }
  return offset;
}

// readEvent() is responsible for creating, and setting up, the
// EventPrincipal.
//
//   1. create an EventPrincipal with a unique EventID
//   2. For each entry in the provenance, put in one Group,
//      holding the Provenance for the corresponding EDProduct.
//   3. set up the caches in the EventPrincipal to know about this
//      Group.
//
// We do *not* create the EDProduct instance (the equivalent of
// reading the branch containing this EDProduct). That will be done
// by the Delayed Reader, when it is asked to do so.
//
unique_ptr<EventPrincipal>
RootInputFile::
readEvent()
{
  assert(fiIter_ != fiEnd_);
  assert(fiIter_->getEntryType() == FileIndex::kEvent);
  assert(fiIter_->eventID_.runID().isValid());
  setAtEventEntry(fiIter_->entry_);
  auto ep = readCurrentEvent();
  assert(ep);
  assert(eventAux_.run() == fiIter_->eventID_.run() + forcedRunOffset_);
  assert(eventAux_.subRunID() == fiIter_->eventID_.subRunID());
  nextEntry();
  return ep;
}

// Reads event at the current entry in the tree.
// Note: This function neither uses nor sets fiIter_.
unique_ptr<EventPrincipal>
RootInputFile::
readCurrentEvent()
{
  unique_ptr<EventPrincipal> ep;
  if (!eventTree_.current()) {
    // Error, the event tree does not have a valid current entry number.
    return ep;
  }
  fillEventAuxiliary();
  assert(eventAux_.id() == fiIter_->eventID_);
  fillHistory();
  overrideRunNumber(const_cast<EventID&>(eventAux_.id()),
                    eventAux_.isRealData());
  ep.reset(
    new EventPrincipal(
      eventAux_, processConfiguration_, history_,
      eventTree_.makeBranchMapper(),
      eventTree_.makeDelayedReader(InEvent, eventAux_.id()), 0, nullptr));
  eventTree_.fillGroups(*ep);
  ////////////////////////////////////
  // This code would be activated if one decided to open all secondary
  // files at the beginning of the job, instead of on demand as
  // currently.
  //vector<unique_ptr<Principal>> sp;
  //for (auto sf : secondaryFiles_) {
  //  if (!sf) {
  //    break;
  //  }
  //  auto sf_sp = sf->readCurrentEventFromSecondaryFile();
  //  for (auto I = sf_sp.begin(), E = sf_sp.end(); I != E; ++I) {
  //    sp.emplace_back(move(*I));
  //  }
  //}
  //ep->setSecondaryPrincipals(sp);
  ////////////////////////////////////
  primaryEP_ = make_exempt_ptr(ep.get());
  return ep;
}

////////////////////////////////////
// This code would be activated (at the expense of the implementation
// below) if one decided to open all secondary files at the beginning of
// the job, instead of on demand as currently.
//vector<unique_ptr<Principal>>
//RootInputFile::
//readCurrentEventFromSecondaryFile()
//{
//  vector<unique_ptr<Principal>> sp;
//  if (!eventTree_.current()) {
//    // Error, the event tree does not have a valid current entry number.
//    return sp;
//  }
//  fillEventAuxiliary();
//  fillHistory();
//  overrideRunNumber(const_cast<EventID&>(eventAux_.id()),
//                    eventAux_.isRealData());
//  unique_ptr<EventPrincipal> ep(
//    new EventPrincipal(
//      eventAux_, processConfiguration_, history_, eventTree_.makeBranchMapper(),
//      eventTree_.makeDelayedReader(InEvent, eventAux_.id()), 0,
//      primaryFile_->primaryEP_.get()));
//  eventTree_.fillGroups(*ep);
//  sp.emplace_back(unique_ptr<Principal>(ep.release()));
//  for (auto sf : secondaryFiles_) {
//    if (!sf) {
//      break;
//    }
//    auto sf_sp = sf->readCurrentEventFromSecondaryFile();
//    for (auto I = sf_sp.begin(), E = sf_sp.end(); I != E; ++I) {
//      sp.emplace_back(move(*I));
//    }
//  }
//  return sp;
//}
////////////////////////////////////

bool
RootInputFile::
readEventForSecondaryFile(EventID eID)
{
  // Used just after opening a new secondary file in response to a failed
  // product lookup.  Synchronize the file index to the event needed and
  // create a secondary EventPrincipal for it.
  if (!setEntryAtEvent(eID, /*exact=*/true)) {
    // Error, could not find specified event in file.
    return false;
  }
  fillEventAuxiliary();
  fillHistory();
  overrideRunNumber(const_cast<EventID&>(eventAux_.id()),
                    eventAux_.isRealData());
  unique_ptr<EventPrincipal> sep(
    new EventPrincipal(
      eventAux_, processConfiguration_, history_,
      eventTree_.makeBranchMapper(),
      eventTree_.makeDelayedReader(InEvent, eventAux_.id()),
      secondaryFileNameIdx_ + 1, primaryFile_->primaryEP_.get()));
  eventTree_.fillGroups(*sep);
  primaryFile_->primaryEP_->addSecondaryPrincipal(
    unique_ptr<Principal>(move(sep)));
  return true;
}

shared_ptr<RunPrincipal>
RootInputFile::
readRun()
{
  assert(fiIter_ != fiEnd_);
  assert(fiIter_->getEntryType() == FileIndex::kRun);
  assert(fiIter_->eventID_.runID().isValid());
  setAtRunEntry(fiIter_->entry_);
  auto rp = readCurrentRun();
  nextEntry();
  return move(rp);
}

vector<shared_ptr<RunPrincipal>>
RootInputFile::
readRunFromSecondaryFiles()
{
  vector<shared_ptr<RunPrincipal>> rps;
  for (auto const& val : secondaryRPs_) {
    rps.emplace_back(dynamic_pointer_cast<RunPrincipal>(val));
  }
  return rps;
}

unique_ptr<RunPrincipal>
RootInputFile::
readCurrentRun()
{
  unique_ptr<RunPrincipal> rp;
  if (!runTree_.current()) {
    // Error, the run tree does not have a valid current entry number.
    return rp;
  }
  secondaryRPs_.clear();
  fillRunAuxiliary();
  assert(runAux_.id() == fiIter_->eventID_.runID());
  overrideRunNumber(runAux_.id_);
  if (runAux_.beginTime() == Timestamp::invalidTimestamp()) {
    // RunAuxiliary did not contain a valid timestamp.
    // Take it from the next event.
    if (eventTree_.next()) {
      fillEventAuxiliary();
      // back up, so event will not be skipped.
      eventTree_.previous();
    }
    runAux_.beginTime_ = eventAux_.time();
    runAux_.endTime_ = Timestamp::invalidTimestamp();
  }
  rp.reset(
    new RunPrincipal(
      runAux_, processConfiguration_, runTree_.makeBranchMapper(),
      runTree_.makeDelayedReader(InRun, fiIter_->eventID_), 0, nullptr));
  runTree_.fillGroups(*rp);
  if (!delayedReadRunProducts_) {
    rp->readImmediate();
  }
  //for (auto sf : secondaryFiles_) {
  //  if (!sf) {
  //    break;
  //  }
  //  sf->readCurrentRunFromSecondaryFile();
  //}
  //rp->setSecondaryPrincipals(secondaryRPs_);
  primaryRP_ = make_exempt_ptr(rp.get());
  return rp;
}

//void
//RootInputFile::
//readCurrentRunFromSecondaryFile()
//{
//  if (!runTree_.current()) {
//    // Error, the event tree does not have a valid current entry number.
//    return;
//  }
//  assert(fiIter_ != fiEnd_);
//  assert(fiIter_->getEntryType() == FileIndex::kRun);
//  assert(fiIter_->eventID_.runID().isValid());
//  fillRunAuxiliary();
//  assert(runAux_.id() == fiIter_->eventID_.runID());
//  overrideRunNumber(runAux_.id_);
//  if (runAux_.beginTime() == Timestamp::invalidTimestamp()) {
//    // RunAuxiliary did not contain a valid timestamp.
//    // Take it from the next event.
//    if (eventTree_.next()) {
//      fillEventAuxiliary();
//      // back up, so event will not be skipped.
//      eventTree_.previous();
//    }
//    runAux_.beginTime_ = eventAux_.time();
//    runAux_.endTime_ = Timestamp::invalidTimestamp();
//  }
//  unique_ptr<RunPrincipal> rp(
//    new RunPrincipal(
//      runAux_, processConfiguration_, runTree_.makeBranchMapper(),
//      runTree_.makeDelayedReader(InRun, fiIter_->eventID_),
//      primaryFile_->primaryRP_.get()));
//  runTree_.fillGroups(*rp);
//  if (!delayedReadRunProducts_) {
//    rp->readImmediate();
//  }
//  primaryFile_->secondaryRPs_.emplace_back(rp.release());
//  for (auto sf : secondaryFiles_) {
//    if (!sf) {
//      break;
//    }
//    sf->readCurrentRunFromSecondaryFile();
//  }
//}

bool
RootInputFile::
readRunForSecondaryFile(RunID rID)
{
  // Used just after opening a new secondary file in response to a failed
  // product lookup.  Synchronize the file index to the run needed and
  // create a secondary RunPrincipal for it.
  if (!setEntryAtRun(rID)) {
    // Error, could not find specified run in file.
    return false;
  }
  assert(fiIter_ != fiEnd_);
  assert(fiIter_->getEntryType() == FileIndex::kRun);
  assert(fiIter_->eventID_.runID().isValid());
  fillRunAuxiliary();
  assert(runAux_.id() == fiIter_->eventID_.runID());
  overrideRunNumber(runAux_.id_);
  if (runAux_.beginTime() == Timestamp::invalidTimestamp()) {
    // RunAuxiliary did not contain a valid timestamp.
    // Take it from the next event.
    if (eventTree_.next()) {
      fillEventAuxiliary();
      // back up, so event will not be skipped.
      eventTree_.previous();
    }
    runAux_.beginTime_ = eventAux_.time();
    runAux_.endTime_ = Timestamp::invalidTimestamp();
  }
  shared_ptr<RunPrincipal> rp(
    new RunPrincipal(
      runAux_, processConfiguration_, runTree_.makeBranchMapper(),
      runTree_.makeDelayedReader(InRun, fiIter_->eventID_),
      secondaryFileNameIdx_ + 1, primaryFile_->primaryRP_.get()));
  runTree_.fillGroups(*rp);
  if (!delayedReadRunProducts_) {
    rp->readImmediate();
  }
  primaryFile_->primaryRP_->addSecondaryPrincipal(
    static_pointer_cast<Principal>(rp));
  // FIXME: These secondary run principals will never be cached
  // FIXME: by the event processor!  This means that we cannot
  // FIXME: output run data products from them.
  primaryFile_->secondaryRPs_.emplace_back(move(rp));
  return true;
}

shared_ptr<SubRunPrincipal>
RootInputFile::
readSubRun(shared_ptr<RunPrincipal> rp)
{
  assert(fiIter_ != fiEnd_);
  assert(fiIter_->getEntryType() == FileIndex::kSubRun);
  setAtSubRunEntry(fiIter_->entry_);
  auto srp = readCurrentSubRun(rp);
  nextEntry();
  return move(srp);
}

vector<shared_ptr<SubRunPrincipal>>
RootInputFile::
readSubRunFromSecondaryFiles(shared_ptr<RunPrincipal>)
{
  vector<shared_ptr<SubRunPrincipal>> srps;
  for (auto const& val : secondarySRPs_) {
    srps.emplace_back(dynamic_pointer_cast<SubRunPrincipal>(val));
  }
  return srps;
}

unique_ptr<SubRunPrincipal>
RootInputFile::
readCurrentSubRun(shared_ptr<RunPrincipal> rp [[gnu::unused]])
{
  unique_ptr<SubRunPrincipal> srp;
  if (!subRunTree_.current()) {
    // Error, the subRun tree does not have a valid current entry number.
    return srp;
  }
  secondarySRPs_.clear();
  fillSubRunAuxiliary();
  assert(subRunAux_.id() == fiIter_->eventID_.subRunID());
  overrideRunNumber(subRunAux_.id_);
  assert(subRunAux_.runID() == rp->id());
  if (subRunAux_.beginTime() == Timestamp::invalidTimestamp()) {
    // SubRunAuxiliary did not contain a timestamp.
    // Take it from the next event.
    if (eventTree_.next()) {
      fillEventAuxiliary();
      // back up, so event will not be skipped.
      eventTree_.previous();
    }
    subRunAux_.beginTime_ = eventAux_.time();
    subRunAux_.endTime_ = Timestamp::invalidTimestamp();
  }
  srp.reset(
    new SubRunPrincipal(
      subRunAux_, processConfiguration_, subRunTree_.makeBranchMapper(),
      subRunTree_.makeDelayedReader(InSubRun, fiIter_->eventID_), 0, nullptr));
  subRunTree_.fillGroups(*srp);
  if (!delayedReadSubRunProducts_) {
    srp->readImmediate();
  }
  //for (auto sf : secondaryFiles_) {
  //  if (!sf) {
  //    break;
  //  }
  //  sf->readCurrentSubRunFromSecondaryFile(rp);
  //}
  //srp->setSecondaryPrincipals(secondarySRPs_);
  primarySRP_ = make_exempt_ptr(srp.get());
  return srp;
}

//void
//RootInputFile::
//readCurrentSubRunFromSecondaryFile(shared_ptr<RunPrincipal> rp)
//{
//  if (!subRunTree_.current()) {
//    // Error, the subRun tree does not have a valid current entry number.
//    return;
//  }
//  assert(fiIter_ != fiEnd_);
//  assert(fiIter_->getEntryType() == FileIndex::kSubRun);
//  fillSubRunAuxiliary();
//  assert(subRunAux_.id() == fiIter_->eventID_.subRunID());
//  overrideRunNumber(subRunAux_.id_);
//  assert(subRunAux_.runID() == rp->id());
//  if (subRunAux_.beginTime() == Timestamp::invalidTimestamp()) {
//    // SubRunAuxiliary did not contain a timestamp.
//    // Take it from the next event.
//    if (eventTree_.next()) {
//      fillEventAuxiliary();
//      // back up, so event will not be skipped.
//      eventTree_.previous();
//    }
//    subRunAux_.beginTime_ = eventAux_.time();
//    subRunAux_.endTime_ = Timestamp::invalidTimestamp();
//  }
//  unique_ptr<SubRunPrincipal> srp(
//    new SubRunPrincipal(
//      subRunAux_, processConfiguration_, subRunTree_.makeBranchMapper(),
//      subRunTree_.makeDelayedReader(InSubRun, fiIter_->eventID_),
//      primaryFile_->primarySRP_.get()));
//  subRunTree_.fillGroups(*srp);
//  if (!delayedReadSubRunProducts_) {
//    srp->readImmediate();
//  }
//  primaryFile_->secondarySRPs_.emplace_back(srp.release());
//  for (auto sf : secondaryFiles_) {
//    if (!sf) {
//      break;
//    }
//    sf->readCurrentSubRunFromSecondaryFile(rp);
//  }
//}

bool
RootInputFile::
readSubRunForSecondaryFile(SubRunID srID)
{
  // Used just after opening a new secondary file in response to a failed
  // product lookup.  Synchronize the file index to the subRun needed and
  // create a secondary SubRunPrincipal for it.
  if (!setEntryAtSubRun(srID)) {
    // Error, could not find specified subRun in file.
    return false;
  }
  assert(fiIter_ != fiEnd_);
  assert(fiIter_->getEntryType() == FileIndex::kSubRun);
  fillSubRunAuxiliary();
  assert(subRunAux_.id() == fiIter_->eventID_.subRunID());
  overrideRunNumber(subRunAux_.id_);
  if (subRunAux_.beginTime() == Timestamp::invalidTimestamp()) {
    // SubRunAuxiliary did not contain a timestamp.
    // Take it from the next event.
    if (eventTree_.next()) {
      fillEventAuxiliary();
      // back up, so event will not be skipped.
      eventTree_.previous();
    }
    subRunAux_.beginTime_ = eventAux_.time();
    subRunAux_.endTime_ = Timestamp::invalidTimestamp();
  }
  shared_ptr<SubRunPrincipal> srp(
    new SubRunPrincipal(
      subRunAux_, processConfiguration_, subRunTree_.makeBranchMapper(),
      subRunTree_.makeDelayedReader(InSubRun, fiIter_->eventID_),
      secondaryFileNameIdx_ + 1, primaryFile_->primarySRP_.get()));
  subRunTree_.fillGroups(*srp);
  if (!delayedReadSubRunProducts_) {
    srp->readImmediate();
  }
  primaryFile_->primarySRP_->addSecondaryPrincipal(
    static_pointer_cast<Principal>(srp));
  // FIXME: These secondary subRun principals will never be cached
  // FIXME: by the event processor!  This means that we cannot
  // FIXME: output subrun data products from them.
  primaryFile_->secondarySRPs_.emplace_back(move(srp));
  return true;
}

void
RootInputFile::
setAtEventEntry(FileIndex::EntryNumber_t entry)
{
  eventTree_.setEntryNumber(entry);
}

void
RootInputFile::
setAtRunEntry(FileIndex::EntryNumber_t entry)
{
  runTree_.setEntryNumber(entry);
}

void
RootInputFile::
setAtSubRunEntry(FileIndex::EntryNumber_t entry)
{
  subRunTree_.setEntryNumber(entry);
}

bool
RootInputFile::
setEntryAtEvent(EventID const& eID, bool exact)
{
  fiIter_ = fileIndex_.findEventPosition(eID, exact);
  if (fiIter_ == fiEnd_) {
    return false;
  }
  eventTree_.setEntryNumber(fiIter_->entry_);
  return true;
}

bool
RootInputFile::
setEntryAtSubRun(SubRunID const& subRun)
{
  fiIter_ = fileIndex_.findSubRunPosition(subRun, true);
  if (fiIter_ == fiEnd_) {
    return false;
  }
  subRunTree_.setEntryNumber(fiIter_->entry_);
  return true;
}

bool
RootInputFile::
setEntryAtRun(RunID const& run)
{
  fiIter_ = fileIndex_.findRunPosition(run, true);
  if (fiIter_ == fiEnd_) {
    return false;
  }
  runTree_.setEntryNumber(fiIter_->entry_);
  return true;
}

void
RootInputFile::
overrideRunNumber(RunID& id)
{
  if (forcedRunOffset_ != 0) {
    id = RunID(id.run() + forcedRunOffset_);
  }
  if (id < RunID::firstRun()) {
    id = RunID::firstRun();
  }
}

void
RootInputFile::
overrideRunNumber(SubRunID& id)
{
  if (forcedRunOffset_ != 0) {
    id = SubRunID(id.run() + forcedRunOffset_, id.subRun());
  }
}

void
RootInputFile::
overrideRunNumber(EventID& id, bool isRealData)
{
  if (forcedRunOffset_ == 0) {
    return;
  }
  if (isRealData) {
    throw art::Exception(errors::Configuration,
                         "RootInputFile::overrideRunNumber()")
        << "The 'setRunNumber' parameter of RootInput cannot "
        << "be used with real data.\n";
  }
  id = EventID(id.run() + forcedRunOffset_, id.subRun(), id.event());
}

void
RootInputFile::
readEventHistoryTree()
{
  // Read in the event history tree, if we have one...
  eventHistoryTree_ = dynamic_cast<TTree*>(filePtr_->Get(
                        rootNames::eventHistoryTreeName().c_str()));
  if (!eventHistoryTree_) {
    throw art::Exception(errors::DataCorruption)
        << "Failed to find the event history tree.\n";
  }
}

void
RootInputFile::
initializeDuplicateChecker()
{
  if (duplicateChecker_.get() == nullptr) {
    return;
  }
  if (eventTree_.next()) {
    fillEventAuxiliary();
    duplicateChecker_->init(eventAux_.isRealData(), fileIndex_);
  }
  eventTree_.setEntryNumber(-1);
}

void
RootInputFile::
dropOnInput(GroupSelectorRules const& rules, bool dropDescendants,
            bool /*dropMergeable*/, ProductList& prodList)
{
  // This is the selector for drop on input.
  GroupSelector groupSelector;
  groupSelector.initialize(rules, prodList);
  // Do drop on input. On the first pass, just fill
  // in a set of branches to be dropped.
  set<BranchID> branchesToDrop;
  for (auto I = prodList.cbegin(), E = prodList.cend(); I != E; ++I) {
    auto const& bd = I->second;
    if (!groupSelector.selected(bd)) {
      if (dropDescendants) {
        branchChildren_->appendToDescendants(bd.branchID(), branchesToDrop);
      }
      else {
        branchesToDrop.insert(bd.branchID());
      }
    }
  }
  // On this pass, actually drop the branches.
  auto branchesToDropEnd = branchesToDrop.cend();
  for (auto I = prodList.begin(), E = prodList.end(); I != E;) {
    auto const& bd = I->second;
    bool drop = branchesToDrop.find(bd.branchID()) != branchesToDropEnd;
    if (!drop) {
      ++I;
      continue;
    }
    if (groupSelector.selected(bd)) {
      mf::LogWarning("RootInputFile")
          << "Branch '"
          << bd.branchName()
          << "' is being dropped from the input\n"
          << "of file '"
          << file_
          << "' because it is dependent on a branch\n"
          << "that was explicitly dropped.\n";
    }
    treePointers_[bd.branchType()]->dropBranch(bd.branchName());
    auto icopy = I++;
    prodList.erase(icopy);
  }
}

void
RootInputFile::
openSecondaryFile(int const idx)
{
  secondaryFiles_[idx] =
    rifSequence_->openSecondaryFile(idx, secondaryFileNames_[idx], this);
}

} // namespace art

