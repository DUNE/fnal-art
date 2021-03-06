#ifndef art_Framework_IO_Root_RootOutput_h
#define art_Framework_IO_Root_RootOutput_h
// vim: set sw=2:

// FIXME! There is an incestuous relationship between RootOutputFile and
// RootOutput that only works because the methods of RootOutput and
// OutputItem used by RootOutputFile are all inline. A correct and
// robust implementation would have a OutputItem defined in a separate
// file and the information (basket size, etc) in a different class in
// the main art/Framework/Root library accessed by both RootOutputFile
// and RootOutput. This has been entered as issue #2885.

#include "art/Framework/Core/Frameworkfwd.h"
#include "art/Framework/Core/OutputModule.h"
#include "art/Framework/IO/FileStatsCollector.h"
#include "boost/scoped_ptr.hpp"
#include "fhiclcpp/ParameterSet.h"
#include <string>

class TTree;

namespace art {

class RootOutputFile;

class RootOutput : public OutputModule {

  friend class RootOutputFile;

public: // MEMBER FUNCTIONS

  virtual
  ~RootOutput();

  explicit
  RootOutput(fhicl::ParameterSet const&);

  virtual
  void
  selectProducts(FileBlock const&) override;

private: // TYPES

  enum DropMetaData {
    DropNone
    , DropPrior
    , DropAll
  };

private: // MEMBER FUNCTIONS

  std::string const&
  lastClosedFileName() const override;

  int const&
  compressionLevel() const
  {
    return compressionLevel_;
  }

  int const&
  basketSize() const
  {
    return basketSize_;
  }

  int const&
  splitLevel() const
  {
    return splitLevel_;
  }

  int64_t const&
  treeMaxVirtualSize() const
  {
    return treeMaxVirtualSize_;
  }

  int64_t const&
  saveMemoryObjectThreshold() const
  {
    return saveMemoryObjectThreshold_;
  }

  bool const&
  fastCloning() const
  {
    return fastCloning_;
  }

  DropMetaData const&
  dropMetaData() const
  {
    return dropMetaData_;
  }

  bool const&
  dropMetaDataForDroppedData() const
  {
    return dropMetaDataForDroppedData_;
  }

  void
  openFile(FileBlock const&) override;

  void
  respondToOpenInputFile(FileBlock const&) override;

  void
  respondToCloseInputFile(FileBlock const&) override;

  void
  write(EventPrincipal const&) override;

  void
  writeSubRun(SubRunPrincipal const&) override;

  void
  writeRun(RunPrincipal const&) override;

  bool
  isFileOpen() const override;

  bool
  shouldWeCloseFile() const override;

  void
  doOpenFile();

  void
  startEndFile() override;

  void
  writeFileFormatVersion() override;

  void
  writeFileIndex() override;

  void
  writeEventHistory() override;

  void
  writeProcessConfigurationRegistry() override;

  void
  writeProcessHistoryRegistry() override;

  void
  writeParameterSetRegistry() override;

  void
  writeProductDescriptionRegistry() override;

  void
  writeParentageRegistry() override;

  void
  writeBranchIDListRegistry() override;

  void
  doWriteFileCatalogMetadata(FileCatalogMetadata::collection_type const& md,
                             FileCatalogMetadata::collection_type const& ssmd)
                             override;

  void
  writeProductDependencies() override;

  void
  finishEndFile() override;

private:

  std::string const catalog_;
  unsigned int const maxFileSize_;
  int const compressionLevel_;
  int const basketSize_;
  int const splitLevel_;
  int64_t const treeMaxVirtualSize_;
  int64_t const saveMemoryObjectThreshold_;
  bool fastCloning_;
  bool dropAllEvents_;
  bool dropAllSubRuns_;
  DropMetaData dropMetaData_;
  bool dropMetaDataForDroppedData_;
  std::string const moduleLabel_;
  int inputFileCount_;
  boost::scoped_ptr<RootOutputFile> rootOutputFile_;
  FileStatsCollector fstats_;
  std::string const filePattern_;
  std::string tmpDir_;
  std::string lastClosedFileName_;

};

} // namespace art

// Local Variables:
// mode: c++
// End:
#endif // art_Framework_IO_Root_RootOutput_h
