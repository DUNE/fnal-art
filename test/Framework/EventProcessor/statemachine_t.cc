/*----------------------------------------------------------------------

Test of the statemachine classes.



----------------------------------------------------------------------*/

#include "art/Framework/EventProcessor/EPStates.h"
#include "art/Framework/Core/IEventProcessor.h"
#include "test/Framework/EventProcessor/MockEventProcessor.h"

#include "boost/program_options.hpp"

#include <string>
#include <iostream>
#include <fstream>


int main(int argc, char* argv[]) {
  using namespace statemachine;
  std::cout << "Running test in statemachine_t.cc\n";

  // Handle the command line arguments
  std::string inputFile;
  std::string outputFile;
  boost::program_options::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "produce help message")
    ("inputFile,i", boost::program_options::value<std::string>(&inputFile)->default_value(""))
    ("outputFile,o", boost::program_options::value<std::string>(&outputFile)->default_value("statemachine_test_output.txt"))
    ("skipmode,m", "NOMERGE, FULLLUMIMERGE and FULLMERGE only")
    ("skipmodes,s", "NOMERGE and FULLMERGE only");
  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
  boost::program_options::notify(vm);
  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  // Get some fake data from an input file.
  // The fake data has the format of a series pairs of items.
  // The first is a letter to indicate the data type
  // r for run, l for subRun, e for event, f for file, s for stop
  // The second item is the run number or subRun number
  // for the run and subRun cases.  For the other cases the number
  // is not not used.  This series of fake data items is terminated
  // by a period (blank space and newlines are ignored).
  // Use the trivial default in the next line if no input file
  // has been specified
  std::string mockData = "s 1";
  if (inputFile != "") {
    std::ifstream input;
    input.open(inputFile.c_str());
    if (input.fail()) {
      std::cerr << "Error, Unable to open mock input file named "
                << inputFile << "\n";
      return 1;
    }
    std::getline(input, mockData, '.');
  }

  std::ofstream output(outputFile.c_str());

  std::vector<FileMode> fileModes;
  fileModes.reserve(4);
  fileModes.push_back(NOMERGE);
  if (!vm.count("skipmode") && !vm.count("skipmodes")) {
    fileModes.push_back(MERGE);
  }
  if (!vm.count("skipmodes")) {
    fileModes.push_back(FULLLUMIMERGE);
  }
  fileModes.push_back(FULLMERGE);

  for (size_t k = 0; k < fileModes.size(); ++k) {
    FileMode fileMode = fileModes[k];
    for (int i = 0; i < 2; ++i) {
      bool handleEmptyRuns = i;
      for (int j = 0; j < 2; ++j) {
        bool handleEmptySubRuns = j;
        output << "\nMachine parameters:  ";
        if (fileMode == NOMERGE) output << "mode = NOMERGE";
        else if (fileMode == MERGE) output << "mode = MERGE";
        else if (fileMode == FULLLUMIMERGE) output << "mode = FULLLUMIMERGE";
        else output << "mode = FULLMERGE";
        output << "  handleEmptyRuns = " << handleEmptyRuns;
        output << "  handleEmptySubRuns = " << handleEmptySubRuns << "\n";

        art::MockEventProcessor mockEventProcessor(mockData,
                                                   output,
                                                   fileMode,
                                                   handleEmptyRuns,
                                                   handleEmptySubRuns);
        mockEventProcessor.runToCompletion();
      }
    }
  }


  return 0;
}
