
art_add_service(MyService_service
  MyService.h
  MyService_service.cc
  )

art_add_service(MyServiceUsingIface_service MyServiceUsingIface_service.cc)
target_link_libraries(MyServiceUsingIface_service
  art_Framework_Services_Optional_TrivialFileDelivery_service
  )

art_add_module(MyServiceUser_module MyServiceUser_module.cc)

cet_test(MyService_t HANDBUILT
  TEST_EXEC art
  TEST_ARGS -c MyService_t.fcl
  DATAFILES fcl/MyService_t.fcl
  )

cet_test(MyServiceUsingIface_t HANDBUILT
  TEST_EXEC art
  TEST_ARGS --rethrow-all -c MyServiceUsingIface_t.fcl
  DATAFILES fcl/MyServiceUsingIface_t.fcl
  )

art_add_service(PSTest_service PSTest_service.cc)
art_add_module(PSTestAnalyzer_module PSTestAnalyzer_module.cc)
target_link_libraries(PSTestAnalyzer_module PSTest_service)


art_add_service(PSTestInterfaceImpl_service
  PSTestInterfaceImpl.h
  PSTestInterfaceImpl_service.cc
  )

cet_test(PSTest_t HANDBUILT
  TEST_EXEC art
  TEST_ARGS --rethrow-all -c PSTest_t.fcl
  DATAFILES fcl/PSTest_t.fcl
  )
