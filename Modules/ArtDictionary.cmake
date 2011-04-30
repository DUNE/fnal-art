INCLUDE(BuildDictionary)
INCLUDE(CetParseArgs)
INCLUDE(CheckClassVersion)

MACRO(art_dictionary)
  CET_PARSE_ARGS(ART_DICT
    "LIBRARIES"
    "UPDATE_IN_PLACE"
    ${ARGN}
    )
  build_dictionary(${ART_DICT_DEFAULT_ARGS})
  IF(ART_DICT_LIBRARIES)
    SET(ARGS "LIBRARIES" ${ART_DICT_LIBRARIES})
  ENDIF()
  SET(ARGS ${ARGS} "UPDATE_IN_PLACE" ${ART_DICT_UPDATE_IN_PLACE})
  check_class_version(${ART_DICT_LIBRARIES} UPDATE_IN_PLACE ${ARGS})
ENDMACRO()