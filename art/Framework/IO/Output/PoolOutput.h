#ifndef IOPool_Output_PoolOutput_h
#define IOPool_Output_PoolOutput_h

// ======================================================================
//
// PoolOutput
//
// ======================================================================

#include "art/Framework/Core/Frameworkfwd.h"
#include "art/Framework/Core/OutputModule.h"
#include "boost/scoped_ptr.hpp"
#include "fhiclcpp/ParameterSet.h"
#include <string>

// ----------------------------------------------------------------------

class TTree;

namespace art {

  class RootOutputFile;

  class PoolOutput : public OutputModule {
    enum DropMetaData { DropNone, DropPrior, DropAll };
  public:
    friend class RootOutputFile;
    explicit PoolOutput(fhicl::ParameterSet const& ps);
    virtual ~PoolOutput();
    std::string const& fileName() const {return fileName_;}
    std::string const& logicalFileName() const {return logicalFileName_;}
    int const& compressionLevel() const {return compressionLevel_;}
    int const& basketSize() const {return basketSize_;}
    int const& splitLevel() const {return splitLevel_;}
    int const& treeMaxVirtualSize() const {return treeMaxVirtualSize_;}
    bool const& fastCloning() const {return fastCloning_;}
    DropMetaData const& dropMetaData() const {return dropMetaData_;}
    bool const& dropMetaDataForDroppedData() const {return dropMetaDataForDroppedData_;}

    struct OutputItem {
      class Sorter {
      public:
        explicit Sorter(TTree * tree);
        bool operator() (OutputItem const& lh, OutputItem const& rh) const;
      private:
        std::map<std::string, int> treeMap_;
      };

      OutputItem() : branchDescription_(0), product_(0) {}

      explicit OutputItem(BranchDescription const* bd) :
        branchDescription_(bd), product_(0) {}

      ~OutputItem() {}

      BranchID branchID() const { return branchDescription_->branchID(); }
      std::string const& branchName() const { return branchDescription_->branchName(); }

      BranchDescription const* branchDescription_;
      mutable void const* product_;

      bool operator <(OutputItem const& rh) const {
        return *branchDescription_ < *rh.branchDescription_;
      }
    };

    typedef std::vector<OutputItem> OutputItemList;

    typedef boost::array<OutputItemList, NumBranchTypes> OutputItemListArray;

    OutputItemListArray const& selectedOutputItemList() const {return selectedOutputItemList_;}

  private:
    virtual void openFile(FileBlock const& fb);
    virtual void respondToOpenInputFile(FileBlock const& fb);
    virtual void respondToCloseInputFile(FileBlock const& fb);
    virtual void write(EventPrincipal const& e);
    virtual void writeSubRun(SubRunPrincipal const& lb);
    virtual void writeRun(RunPrincipal const& r);

    virtual bool isFileOpen() const;
    virtual bool shouldWeCloseFile() const;
    virtual void doOpenFile();


    virtual void startEndFile();
    virtual void writeFileFormatVersion();
    virtual void writeFileIdentifier();
    virtual void writeFileIndex();
    virtual void writeEventHistory();
    virtual void writeProcessConfigurationRegistry();
    virtual void writeProcessHistoryRegistry();
    virtual void writeParameterSetRegistry();
    virtual void writeProductDescriptionRegistry();
    virtual void writeParentageRegistry();
    virtual void writeBranchIDListRegistry();
    virtual void writeProductDependencies();
    virtual void finishEndFile();

    void fillSelectedItemList(BranchType branchtype, TTree *theTree);

    OutputItemListArray selectedOutputItemList_;
    std::string const fileName_;
    std::string const logicalFileName_;
    std::string const catalog_;
    unsigned int const maxFileSize_;
    int const compressionLevel_;
    int const basketSize_;
    int const splitLevel_;
    int const treeMaxVirtualSize_;
    bool fastCloning_;
    DropMetaData dropMetaData_;
    bool dropMetaDataForDroppedData_;
    std::string const moduleLabel_;
    int outputFileCount_;
    int inputFileCount_;
    boost::scoped_ptr<RootOutputFile> rootOutputFile_;
  };

}  // art

// ======================================================================

#endif