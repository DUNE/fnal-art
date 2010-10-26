// ======================================================================
//
// Maintain multiple independent random number engines,
// including saving and restoring state.
//
// ======================================================================
//
// Notes
// -----
//
// 0) These notes predate this version.
//    TODO: review/apply/update these notes as/when time permits.
//
// 1) The CMS code on which is this modelled is available at
//    http://cmslxr.fnal.gov/lxr/source/IOMC/RandomEngine/src/RandomNumberGeneratorService.cc
//
// 2) CLHEP specifies that state will be returned as vector<unsigned long>.
//    The size of a long is machine dependent. If unsigned long is an
//    8 byte variable, only the least significant 4 bytes are filled
//    and the most significant 4 bytes are zero.  We need to store the
//    state with a machine independent size, which we choose to be
//    uint32_t. This conversion really belongs in the RandomEngineState
//    class but we are at the moment constrained by the framework's
//    HepRandomGenerator interface.
//
// 3) We are currently linking to clhep v1.9.3.2. This is out of date
//    and does not contain the correct code for saving state on 64 bit
//    machines. So I call getState() instead of get() to restore the
//    state.  Change back when we switch to the correct version of CLHEP.
//    (The issue is masking out the high order word returned from crc32ul
//    on a 64 bit machine.)
//
// ======================================================================


// Header corresponding to this implementation file:
#include "art/Framework/Core/RandomNumberGeneratorService.h"
  using art::RandomNumberGeneratorService;
  using art::RNGsnapshot;

// Framework support:
#include "art/Framework/Core/Event.h"
#include "art/Framework/Services/Basic/CurrentModule.h"
#include "art/Framework/Services/Registry/ActivityRegistry.h"
#include "art/Framework/Services/Registry/Service.h"
#include "art/Framework/Services/Registry/ServiceMaker.h"
#include "fhiclcpp/ParameterSet.h"
#include "art/Persistency/Common/Handle.h"
#include "art/Persistency/Provenance/ModuleDescription.h"
#include "art/Utilities/Exception.h"

// MessageFacility support:
#include "MessageFacility/MessageLogger.h"

// CLHEP support:
#include "CLHEP/Random/DRand48Engine.h"
#include "CLHEP/Random/DualRand.h"
#include "CLHEP/Random/Hurd160Engine.h"
#include "CLHEP/Random/Hurd288Engine.h"
#include "CLHEP/Random/JamesRandom.h"
#include "CLHEP/Random/MTwistEngine.h"
#include "CLHEP/Random/NonRandomEngine.h"
#include "CLHEP/Random/Random.h"
#include "CLHEP/Random/RandomEngine.h"
#include "CLHEP/Random/RanecuEngine.h"
#include "CLHEP/Random/Ranlux64Engine.h"
#include "CLHEP/Random/RanluxEngine.h"
#include "CLHEP/Random/RanshiEngine.h"
#include "CLHEP/Random/TripleRand.h"


// C++ support:
#include <cassert>
#include <fstream>
  using std::ifstream;
  using std::ofstream;
#include <string>
  using std::string;
#include <vector>
// DEBUG */ #include <iostream>
  // DEBUG */ using std::cerr;


// ======================================================================
// implementation details
// ======================================================================


namespace {

  typedef  RandomNumberGeneratorService  RNGservice;
  typedef  RNGservice::eptr_t            eptr_t;
  typedef  RNGservice::label_t           label_t;
  typedef  RNGservice::seed_t            seed_t;
  typedef  RNGservice::base_engine_t     base_engine_t;

  bool              const  DEFAULT_DEBUG_VALUE( false );
  int               const  DEFAULT_NPRINT_VALUE( 10 );
  fhicl::ParameterSet const  DEFAULT_PSET;
  string            const  EMPTY_STRING( "" );
  string            const  DEFAULT_ENGINE_KIND( "HepJamesRandom" );
  seed_t            const  MAXIMUM_CLHEP_SEED( 900000000 );
  seed_t            const  USE_DEFAULT_SEED( -1 );
  RNGsnapshot       const  EMPTY_SNAPSHOT;

  struct G4Engine { };

  struct no_ownership  // for shared_ptrs not taking ownership
  {
    void
      operator () ( void const * ) const
    { /* nothing to do */ }
  };  // no_ownership

  void
    throw_if_invalid_seed( seed_t seed )
  {
    if( seed == USE_DEFAULT_SEED )
      return;
    if( seed > MAXIMUM_CLHEP_SEED )
      throw artZ::Exception("RANGE")
        << "RNGservice::throw_if_invalid_seed():\n"
           "Seed " << seed << " exceeds permitted maximum "
           << MAXIMUM_CLHEP_SEED << ".\n";
    if( seed < 0 )  // too small for CLHEP
      throw artZ::Exception("RANGE")
        << "RNGservice::throw_if_invalid_seed():\n"
           "Seed " << seed << " is not permitted to be negative.\n";
  }  // throw_if_invalid_seed()

  inline  label_t
    qualify_engine_label( label_t const & engine_label )
  {
    return art::Service<art::CurrentModule>() -> label()
           + ":" + engine_label;
  }  // qualify_engine_label()

  template< class DesiredEngineType >
  inline eptr_t
    manufacture_an_engine( seed_t  seed )
  {
    return eptr_t( seed == USE_DEFAULT_SEED ? new DesiredEngineType
                                            : new DesiredEngineType(seed)
                 );
  }  // manufacture_an_engine<>()

  template<>
  inline eptr_t
    manufacture_an_engine<CLHEP::NonRandomEngine>( seed_t  /* unused */ )
  {
    // no engine c'tor takes a seed:
    return eptr_t( new CLHEP::NonRandomEngine );
  }  // manufacture_an_engine<NonRandomEngine>()

  template<>
  inline eptr_t
    manufacture_an_engine<G4Engine>( seed_t  seed )
  {
    if( seed != USE_DEFAULT_SEED )
      CLHEP::HepRandom::setTheSeed( seed );

    return eptr_t( CLHEP::HepRandom::getTheEngine()
                 , no_ownership()
                 );
  }  // manufacture_an_engine<G4Engine>()

  eptr_t
    engine_factory( string const &  kind_of_engine_to_make
                  , seed_t          seed
                  )
  {
#define MANUFACTURE_EXPLICIT(KIND,TYPE) \
  if( kind_of_engine_to_make == string(KIND) ) \
    return manufacture_an_engine<TYPE>( seed );
    MANUFACTURE_EXPLICIT("G4Engine",G4Engine)
#define MANUFACTURE_IMPLICIT(ENGINE) MANUFACTURE_EXPLICIT(#ENGINE,CLHEP::ENGINE)
    MANUFACTURE_IMPLICIT(DRand48Engine)
    MANUFACTURE_IMPLICIT(DualRand)
    MANUFACTURE_IMPLICIT(Hurd160Engine)
    MANUFACTURE_IMPLICIT(Hurd288Engine)
    MANUFACTURE_IMPLICIT(HepJamesRandom)
    MANUFACTURE_IMPLICIT(MTwistEngine)
    MANUFACTURE_IMPLICIT(NonRandomEngine)
    MANUFACTURE_IMPLICIT(RanecuEngine)
    MANUFACTURE_IMPLICIT(Ranlux64Engine)
    MANUFACTURE_IMPLICIT(RanluxEngine)
    MANUFACTURE_IMPLICIT(RanshiEngine)
    MANUFACTURE_IMPLICIT(TripleRand)
#undef MANUFACTURE_IMPLICIT
#undef MANUFACTURE_EXPLICIT

    throw artZ::Exception("RANDOM")
      << "engine_factory():\n"
         "Attempt to create engine of unknown kind \""
          << kind_of_engine_to_make << "\".\n";
  }  // engine_factory()

  void
    expand_if_abbrev_kind( std::string & requested_engine_kind )
  {
    if(  requested_engine_kind.empty()
      || requested_engine_kind == "DefaultEngine"
      || requested_engine_kind == "JamesRandom"
      ) {
      requested_engine_kind = DEFAULT_ENGINE_KIND;
    }
  }  // expand_if_abbrev_kind()

}  // namespace


// ======================================================================
// RNGservice public functionality
// ======================================================================


RNGservice::RandomNumberGeneratorService( fhicl::ParameterSet const & pset
                                        , art::ActivityRegistry   & reg
                                        )
: engine_creation_is_okay_( true )
, dict_( )
, tracker_( )
, kind_( )
, snapshot_( )
, restoreStateLabel_( pset.get<std::string>( "restoreStateLabel"
                                                        , EMPTY_STRING
                    )                                   )
, saveToFilename_( pset.get<std::string>( "saveTo"
                                                     , EMPTY_STRING
                 )                                   )
, restoreFromFilename_( pset.get<std::string>( "restoreFrom"
                                                          , EMPTY_STRING
                      )                                   )
, nPrint_( pset.get<int>( "nPrint"
                                          , DEFAULT_NPRINT_VALUE
         )                                )
, debug_( pset.get<bool>( "debug"
                                          , DEFAULT_DEBUG_VALUE
        )                                 )
{
  // Register for callbacks:
  reg.watchPostBeginJob    ( this, & RNGservice::postBeginJob     );
  reg.watchPostEndJob      ( this, & RNGservice::postEndJob       );
#ifdef NOTYET
  reg.watchPreProcessEvent ( this, & RNGservice::preProcessEvent  );
#endif  // NOTYET

  assert( invariant_holds_() && "RNGservice::RNGservice()" );
}  // RNGservice()


base_engine_t &
  RNGservice::getEngine( ) const
{
  return getEngine( label_t() );
}


base_engine_t &
  RNGservice::getEngine( label_t const & engine_label ) const
{
  label_t const &  label = qualify_engine_label( engine_label );
  dict_t::const_iterator  d = dict_.find(label);

  // DEBUG */ cerr << "RNGService::getEngine(): requested engine \"" << label << "\"\n";

  if( d == dict_.end() ) {
    throw artZ::Exception("RANDOM")
      << "RNGservice::getEngine():\n"
         "The requested engine \"" << label << "\" has not been established.\n";
  }
  assert( d->second != 0 && "RNGservice::getEngine()" );

  return *(d->second);
}  // getEngine()


// ======================================================================
// RNGservice private functionality
// ======================================================================


bool
  RNGservice::invariant_holds_( )
{
  return  dict_.size() == tracker_.size()
      &&  dict_.size() == kind_.size();
}  // invariant_holds_()


base_engine_t &
  RNGservice::createEngine( seed_t  seed )
{
  return createEngine( seed
                     , DEFAULT_ENGINE_KIND
                     );
}


base_engine_t &
  RNGservice::createEngine( seed_t           seed
                          , string const &   requested_engine_kind
                          )
{
  return createEngine( seed
                     , requested_engine_kind
                     , label_t()
                     );
}


base_engine_t &
  RNGservice::createEngine( seed_t           seed
                          , string           requested_engine_kind
                          , label_t const &  engine_label
                          )
{
  label_t const &  label  = qualify_engine_label( engine_label );

  if( ! engine_creation_is_okay_ ) {
    throw artZ::Exception("RANDOM")
      << "RNGservice::createEngine():\n"
         "Attempt to create engine \"" << label << "\" is too late.\n";
  }

  if( tracker_.find(label) != tracker_.end() ) {
    throw artZ::Exception("RANDOM")
      << "RNGservice::createEngine():\n"
         "Engine \"" << label << "\" has already been created.\n";
  }

  throw_if_invalid_seed( seed );
  expand_if_abbrev_kind( requested_engine_kind );
  eptr_t  eptr( engine_factory(requested_engine_kind, seed) );
  assert( eptr != 0 && "RNGservice::createEngine()" );
  dict_   [label] = eptr;
  tracker_[label] = VIA_SEED;
  kind_   [label] = requested_engine_kind;

  mf::LogInfo  log("RANDOM");
  log << "Instantiated " << requested_engine_kind
      << " engine \"" << label
      << "\" with ";
  if( seed == USE_DEFAULT_SEED )  log << "default seed";
  else                            log << "seed " << seed;
  log << ".\n";

  assert( invariant_holds_() && "RNGservice::createEngine()" );
  return *eptr;

}  // createEngine<>()


void
  RNGservice::print_()
{
  static int  ncalls( 0 );

  if( !debug_  ||  ++ncalls > nPrint_ )
    return;

  mf::LogInfo  log("RANDOM");

  if( snapshot_.size() == 0 ) {
    log << "No snapshot has yet been made.\n";
    return;
  }

  log << "Snapshot information:";
  for(  size_t i = 0; i != snapshot_.size(); ++i ) {
    log << "\nEngine: " << snapshot_[i].label()
           << "  Kind: " << snapshot_[i].ekind()
           << "  State size: " << snapshot_[i].state().size();
  }
  log << "\n";
}  // print_()


void
  RNGservice::takeSnapshot_( )
{
  mf::LogInfo  log("RANDOM");
  log << "RNGservice::takeSnapshot_() of the following engine labels:\n";

  snapshot_.clear();
  for( dict_t::const_iterator it = dict_.begin()
                            , e  = dict_.end()
     ; it != e; ++it
     ) {
    label_t const &  label = it->first;
    eptr_t  const &  eptr  = it->second;
    assert( eptr != 0 && "RNGservice::takeSnapshot_()" );

    snapshot_.push_back( EMPTY_SNAPSHOT );
    snapshot_.back().saveFrom( kind_[label], label, eptr->put() );
    log << " | " << label;
  }
  log << " |\n";
}  // takeSnapshot_()


void
  RNGservice::restoreSnapshot_( art::Event const & event )
{
  if( restoreStateLabel_.empty() )
    return;

  // access the saved-states product:
  typedef  std::vector<RNGsnapshot>  saved_t;
  art::Handle< saved_t >  saved;
  event.getByLabel( restoreStateLabel_, saved );

  // restore engines from saved-states product:
  for( saved_t::const_iterator it = saved->begin()
                             , e  = saved->end()
     ; it != e; ++it
     ) {
    label_t const &  label = it->label();
    mf::LogInfo  log("RANDOM");
    log << "RNGservice::restoreSnapshot_(): label \"" << label << "\"";

    tracker_t::iterator  t = tracker_.find( label );
    if( t == tracker_.end() ) {
      log << " could not be restored;\n"
             "no established engine bears this label.\n";
      continue;
#ifdef PREPARE_TO_RESTORE_ANYWAY
      eptr_t  eptr( engine_factory(it->ekind(), USE_DEFAULT_SEED) );
      assert( eptr != 0 && "RNGservice::restoreSnapshot_()" );
      dict_   [label] = eptr;
      tracker_[label] = VIA_PRODUCT;
      kind_   [label] = it->ekind();
      t = tracker_.find( label );
#endif  // PREPARE_TO_RESTORE_ANYWAY
    }

    if( t->second == VIA_FILE ) {
      throw artZ::Exception("RANDOM")
        << "RNGservice::restoreSnapshot_():\n"
           "The state of engine \"" << label
             << "\" has been previously read from a file;\n"
                "it is therefore not restorable from a snapshot product.\n";
    }

    engine_state_t  est;
    it->restoreTo( est );
    assert( dict_[label] != 0 && "RNGservice::restoreSnapshot_()" );
    bool  status = dict_[label]->getState(est);
    tracker_[label] = VIA_PRODUCT;
    if( status ) {
      log << " successfully restored.\n";
    }
    else {
      throw artZ::Exception("RANDOM")
        << "RNGservice::restoreSnapshot_():\n"
           "Failed during restore of state of engine for \"" << label << "\"\n";
    }
  }  // for

  assert( invariant_holds_() && "RNGsnapshot::restoreSnapshot_()" );
}  // restoreSnapshot_()


void
  RNGservice::saveToFile_( )
{
  if( saveToFilename_.empty() )
    return;

  // access the file:
  ofstream  outfile( saveToFilename_.c_str() );
  if( !outfile )
    mf::LogWarning("RANDOM")
      << "Can't create/access file \"" << saveToFilename_ << "\"\n";

  // save each engine:
  for( dict_t::const_iterator it = dict_.begin()
                            , e  = dict_.end()
     ; it != e; ++it
     ) {
    label_t const &  label = it->first;
    eptr_t  const &  eptr  = it->second;
    assert( eptr != 0 && "RNGservice::saveToFile_()" );

    eptr->put(outfile);
    if( !outfile )
      mf::LogWarning("RANDOM")
        << "This module's engine has not been saved;\n"
           "file \"" << saveToFilename_ << "\" is likely now corrupted.\n";
  }
}  // saveToFile_()


void
  RNGservice::restoreFromFile_( )
{
  if( restoreFromFilename_.empty() )
    return;

  // access the file:
  ifstream  infile( restoreFromFilename_.c_str() );
  if( !infile )
      throw artZ::Exception("RANDOM")
        << "RNGservice::restoreFromFile_():\n"
           "Can't open file \"" << restoreFromFilename_
            << "\" to initialize engines\n";

  // restore engines:
  label_t  label;
  while( infile >> label ) {
    dict_t::iterator  d = dict_.find( label );
    if( d == dict_.end() ) {
      eptr_t &  eptr  = d->second;
      assert( eptr != 0 && "RNGservice::restoreFromFile_()" );
      if( ! eptr->get(infile) ) {
        throw artZ::Exception("RANDOM")
          << "RNGservice::restoreFromFile_():\n"
             "Failed during restore of state of engine for: " << label
              << "from file \"" << restoreFromFilename_ << "\"\n";
      }
      tracker_[label] = VIA_FILE;
    }
    else {
      throw artZ::Exception("RANDOM")
        << "RNGservice::restoreFromFile_():\n"
           "Engine \"" << label << "\" has already been established.\n";
    }
  }

  assert( invariant_holds_() && "RNGservice::restoreFromFile_()" );
}  // restoreFromFile_()


void
  RNGservice::postBeginJob( )
{
  restoreFromFile_();
  engine_creation_is_okay_ = false;
}  // postBeginJob()


#ifdef NOTYET
void
  RNGservice::preProcessEvent( art::EventID   const &  // unused
                             , art::Timestamp const &  // unused
                             )
{
  takeSnapshot_();
  //restoreSnapshot_(e);  // TODO: needs Event argument!
}
#endif  // NOTYET


void
  RNGservice::postEndJob( )
{
  saveToFile_();
}  // postEndJob()


// ======================================================================


DEFINE_FWK_SERVICE(RandomNumberGeneratorService);


// ======================================================================
