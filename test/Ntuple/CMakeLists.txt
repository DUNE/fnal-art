cet_test(ntuple_t
  LIBRARIES art_Ntuple art_Utilities ${SQLITE3}
  )

cet_test(sqlite_stringstream_t
  LIBRARIES art_Ntuple art_Utilities
  )

cet_test( messagefacility_destination_sqlite_t HANDBUILT
  TEST_EXEC ELdestinationTester
  TEST_ARGS -c sqlite_mfplugin_t.fcl
  DATAFILES
  sqlite_mfplugin_t.fcl
  )
