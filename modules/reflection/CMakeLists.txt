# start of hacks to simulate installed QoreConfig.cmake
# which cannot be available yet - the QORE_LIBRARY is missing
# as find_library does not work yet
#include(${CMAKE_BINARY_DIR}/cmake/QoreConfig.cmake)
if (NOT QORE_MODULES_DIR)
    set(QORE_MODULES_DIR ${MODULE_DIR})
endif (NOT QORE_MODULES_DIR)

if (NOT QORE_LIBRARY)
    set(QORE_LIBRARY libqore)
endif (NOT QORE_LIBRARY)

set(QORE_API_VERSION ${MODULE_API_MAJOR}.${MODULE_API_MINOR})

include(${CMAKE_SOURCE_DIR}/cmake/QoreMacros.cmake)
### end of hacks

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR/build/include})
include_directories(${CMAKE_SOURCE_DIR/include})

# source files
set(REFLECTION_QPP_SRC
    src/QC_AbstractVariant.qpp
    src/QC_FunctionVariant.qpp
    src/QC_AbstractMethodVariant.qpp
    src/QC_PseudoMethodVariant.qpp
    src/QC_NormalMethodVariant.qpp
    src/QC_StaticMethodVariant.qpp
    src/QC_ConstructorMethodVariant.qpp
    src/QC_DestructorMethodVariant.qpp
    src/QC_CopyMethodVariant.qpp
    src/QC_AbstractReflectionFunction.qpp
    src/QC_Function.qpp
    src/QC_AbstractMethod.qpp
    src/QC_PseudoMethod.qpp
    src/QC_NormalMethod.qpp
    src/QC_StaticMethod.qpp
    src/QC_ConstructorMethod.qpp
    src/QC_DestructorMethod.qpp
    src/QC_CopyMethod.qpp
    src/QC_AbstractMember.qpp
    src/QC_AbstractClassMember.qpp
    src/QC_NormalMember.qpp
    src/QC_StaticMember.qpp
    src/QC_AbstractClass.qpp
    src/QC_Class.qpp
    src/QC_PseudoClass.qpp
    src/QC_AbstractConstant.qpp
    src/QC_ClassConstant.qpp
    src/QC_Type.qpp
    src/QC_Namespace.qpp
    src/QC_Constant.qpp
    src/QC_GlobalVar.qpp
    src/QC_TypedHash.qpp
    src/QC_TypedHashMember.qpp
    src/QC_Enum.qpp
    src/QC_EnumMember.qpp
)

set(REFLECTION_CPP_SRC
    src/reflection-module.cpp
    src/AbstractReflectionObject.cpp
)

# we need copy header files to avoid doxygen warning about duplicate files *.cpp
# when src/ dir is in included_directory
set(REFLECTION_H_SRC
    src/qore_reflection.h
    src/AbstractReflectionObject.h
    src/QC_AbstractClass.h
    src/QC_AbstractClassMember.h
    src/QC_AbstractConstant.h
    src/QC_AbstractMember.h
    src/QC_AbstractMethod.h
    src/QC_AbstractMethodVariant.h
    src/QC_AbstractReflectionFunction.h
    src/QC_AbstractVariant.h
    src/QC_ClassConstant.h
    src/QC_Class.h
    src/QC_Constant.h
    src/QC_ConstructorMethod.h
    src/QC_ConstructorMethodVariant.h
    src/QC_CopyMethod.h
    src/QC_CopyMethodVariant.h
    src/QC_DestructorMethod.h
    src/QC_DestructorMethodVariant.h
    src/QC_Function.h
    src/QC_FunctionVariant.h
    src/QC_GlobalVar.h
    src/QC_Namespace.h
    src/QC_NormalMember.h
    src/QC_NormalMethod.h
    src/QC_NormalMethodVariant.h
    src/QC_PseudoClass.h
    src/QC_PseudoMethod.h
    src/QC_PseudoMethodVariant.h
    src/QC_StaticMember.h
    src/QC_StaticMethod.h
    src/QC_StaticMethodVariant.h
    src/QC_TypedHash.h
    src/QC_TypedHashMember.h
    src/QC_Type.h
    src/QC_Enum.h
    src/QC_EnumMember.h
)

set(QORE_DOX_REFLECTION_TMPL_SRC
    docs/mainpage.dox.tmpl
)

foreach (it ${REFLECTION_H_SRC})
    get_filename_component(_outfile ${it} NAME)
    get_filename_component(_infile ${it} ABSOLUTE)
    configure_file(${_infile} ${_outfile} COPYONLY)
    set(REFLECTION_H_SOURCES, ${REFLECTION_H_SOURCES} ${_outfile})
endforeach(it)

qore_wrap_qpp_value(REFLECTION_QPP_SOURCES DOXLIST _dox_src ${REFLECTION_QPP_SRC})
add_custom_target(REFLECTION_QPP_GENERATED_FILES DEPENDS qpp ${REFLECTION_QPP_SOURCES} ${REFLECTION_H_SOURCES})

# prepare reflection module target
set(reflection_module_name "reflection")

if (SINGLE_COMPILATION_UNIT)
    add_library(${reflection_module_name} MODULE src/reflection_scu.cpp)
else (SINGLE_COMPILATION_UNIT)
    add_library(${reflection_module_name} MODULE ${REFLECTION_CPP_SRC} ${REFLECTION_QPP_SOURCES})
endif (SINGLE_COMPILATION_UNIT)

add_dependencies(${reflection_module_name} libqore REFLECTION_QPP_GENERATED_FILES)
set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/reflection/mainpage.dox ${_dox_src})
qore_binary_module_qore(${reflection_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_REFLECTION_SRC ${QORE_DOX_REFLECTION_TMPL_SRC})
    add_custom_target(QORE_REFLECTION_MOD_DOX_FILES DEPENDS ${QORE_DOX_REFLECTION_SRC})
    add_dependencies(docs-reflection QORE_REFLECTION_MOD_DOX_FILES)
endif()
