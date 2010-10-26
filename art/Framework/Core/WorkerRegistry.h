#ifndef Framework_WorkerRegistry_h
#define Framework_WorkerRegistry_h

/**
   \file
   Declaration of class ModuleRegistry

   \author Stefano ARGIRO
   \version
   \date 18 May 2005
*/


#include "art/Framework/Core/WorkerParams.h"
#include "art/Framework/Services/Registry/ActivityRegistry.h"
#include "art/Persistency/Provenance/PassID.h"
#include "art/Persistency/Provenance/ReleaseVersion.h"

#include "boost/shared_ptr.hpp"
#include "boost/utility.hpp"
#include "fhiclcpp/ParameterSet.h"

#include <map>
#include <string>


namespace art {

  class Worker;

  /**
     \class ModuleRegistry ModuleRegistry.h "edm/ModuleRegistry.h"

     \brief The Registry of all workers that where requested
     Holds all instances of workers. In this implementation, Workers
     are owned.

     \author Stefano ARGIRO
     \date 18 May 200
  */

  class WorkerRegistry : private boost::noncopyable {

  public:

    explicit WorkerRegistry(boost::shared_ptr<ActivityRegistry> areg);
    ~WorkerRegistry();

    /// Retrieve the particular instance of the worker
    /** If the worker with that set of parameters does not exist,
        create it
        @note Workers are owned by this class, do not delete them*/
    Worker*  getWorker(WorkerParams const&);
    void clear();

  private:
    /// Get a unique name for the worker
    /** Form a string to be used as a key in the map of workers */
    std::string mangleWorkerParameters(fhicl::ParameterSet const& parameterSet,
				       std::string const& processName,
				       ReleaseVersion const& releaseVersion,
				       PassID const& passID);

    /// the container of workers
    typedef std::map<std::string, boost::shared_ptr<Worker> > WorkerMap;

    /// internal map of registered workers (owned).
    WorkerMap m_workerMap;
    boost::shared_ptr<ActivityRegistry> actReg_;

  }; // WorkerRegistry


} // namespace art

#endif  // Framework_WorkerRegistry_h
