cmake_minimum_required(VERSION 3.19)
project(krita_wasm LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(KRITA_SOURCE_DIR "/opt/krita" CACHE PATH "Path to official KDE/krita source tree")
set(WASM_DEPS_PREFIX "/opt/wasm-deps/install" CACHE PATH "Path to cross-compiled WASM C/C++ dependencies")

if(NOT EXISTS "${KRITA_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "KRITA_SOURCE_DIR (${KRITA_SOURCE_DIR}) does not contain KDE/krita source.")
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Xml
    Sql
    OpenGL
    OpenGLWidgets
    Core5Compat
    Svg
)

add_compile_definitions(
    KRITA_WASM_BUILD=1
    HAVE_LCMS2=1
    HAVE_LCMS24=1
    QUAZIP_STATIC=1
    XSIMD_IMPL=Scalar
)

add_compile_options(
    -msimd128
    $<$<COMPILE_LANGUAGE:CXX>:-fno-operator-names>
)

# Common include directories: compatibility layer + upstream Krita core libs + WASM deps
set(KRITA_COMPAT_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/generated"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/kf"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/widgetutils"
    "${WASM_DEPS_PREFIX}/include"
    "${WASM_DEPS_PREFIX}/include/quazip"
    "/usr/include/eigen3"
)

# ==============================================================================
# 1. KDE Frameworks Compatibility Layer (KConfig, KSharedConfig, KI18n, KPlugin)
# ==============================================================================
add_library(krita_kf_compat STATIC
    compat/kf/kf_compat.cpp
    compat/kf/kpluginfactory.h
    compat/widgetutils/kactioncollection.h
)
target_include_directories(krita_kf_compat PUBLIC ${KRITA_COMPAT_INCLUDES})
target_link_libraries(krita_kf_compat PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)

# ==============================================================================
# 2. Upstream Krita: kritaversion (libs/version)
# ==============================================================================
add_library(kritaversion STATIC
    "${KRITA_SOURCE_DIR}/libs/version/KritaVersionWrapper.cpp"
)
target_include_directories(kritaversion PUBLIC
    ${KRITA_COMPAT_INCLUDES}
    "${KRITA_SOURCE_DIR}/libs/version"
)
target_link_libraries(kritaversion PUBLIC Qt6::Core)

# ==============================================================================
# 3. Upstream Krita: kritaglobal (libs/global)
# ==============================================================================
set(KRITAGLOBAL_SRCS
    "${KRITA_SOURCE_DIR}/libs/global/kis_assert.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_debug.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_algebra_2d.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_memory_leak_tracker.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_shared.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_dom_utils.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_painting_tweaks.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisHandlePainterHelper.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisHandleStyle.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_signal_compressor.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_signal_compressor_with_param.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_thread_safe_signal_compressor.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_acyclic_signal_connector.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_latency_tracker.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisQPainterStateSaver.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisRollingMeanAccumulatorWrapper.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisRollingSumAccumulatorWrapper.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_config_notifier.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisDeleteLaterWrapper.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisUsageLogger.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisFileUtils.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisSignalMapper.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisRegion.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KoID.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KoUnit.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisBezierUtils.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisBezierPatch.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisBezierMesh.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisRectsGrid.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisSynchronizedConnection.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisFilteredRollingMean.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisBackup.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisSampleRectIterator.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisCursorOverrideLock.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/kis_random_source.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisMessageBoxWrapper.cpp"
    "${KRITA_SOURCE_DIR}/libs/global/KisTransformComponents.cpp"
)
add_library(kritaglobal STATIC ${KRITAGLOBAL_SRCS})
target_include_directories(kritaglobal PUBLIC
    ${KRITA_COMPAT_INCLUDES}
    "${KRITA_SOURCE_DIR}/libs/global"
)
target_link_libraries(kritaglobal PUBLIC
    kritaversion
    krita_kf_compat
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Xml
    Qt6::Core5Compat
    Qt6::OpenGL
)

# ==============================================================================
# 4. Upstream Krita: kritamultiarch (libs/multiarch)
# ==============================================================================
add_library(kritamultiarch STATIC
    "${KRITA_SOURCE_DIR}/libs/multiarch/KisSupportedArchitectures.cpp"
    "${KRITA_SOURCE_DIR}/libs/multiarch/KoMultiArchBuildSupport.cpp"
)
target_include_directories(kritamultiarch PUBLIC
    ${KRITA_COMPAT_INCLUDES}
    "${KRITA_SOURCE_DIR}/libs/multiarch"
)
target_link_libraries(kritamultiarch PUBLIC kritaglobal krita_kf_compat Qt6::Core)

# ==============================================================================
# 5. Upstream Krita: kritaplugin (libs/koplugin)
# ==============================================================================
add_library(kritaplugin STATIC
    "${KRITA_SOURCE_DIR}/libs/koplugin/KoPluginLoader.cpp"
    "${KRITA_SOURCE_DIR}/libs/koplugin/KoJsonTrader.cpp"
    "${KRITA_SOURCE_DIR}/libs/koplugin/KisMimeDatabase.cpp"
)
target_include_directories(kritaplugin PUBLIC
    ${KRITA_COMPAT_INCLUDES}
    "${KRITA_SOURCE_DIR}/libs/koplugin"
)
target_link_libraries(kritaplugin PUBLIC kritaglobal krita_kf_compat Qt6::Core)

# ==============================================================================
# 6. Upstream Krita: kritastore (libs/store - .kra ZIP store via QuaZip)
# ==============================================================================
add_library(kritastore STATIC
    "${KRITA_SOURCE_DIR}/libs/store/KoDirectoryStore.cpp"
    "${KRITA_SOURCE_DIR}/libs/store/KoStoreDevice.cpp"
    "${KRITA_SOURCE_DIR}/libs/store/KoStore.cpp"
    "${KRITA_SOURCE_DIR}/libs/store/KoXmlNS.cpp"
    "${KRITA_SOURCE_DIR}/libs/store/KoXmlWriter.cpp"
    "${KRITA_SOURCE_DIR}/libs/store/KoQuaZipStore.cpp"
    "${KRITA_SOURCE_DIR}/libs/store/StoreDebug.cpp"
)
target_include_directories(kritastore PUBLIC
    ${KRITA_COMPAT_INCLUDES}
    "${KRITA_SOURCE_DIR}/libs/store"
)
target_link_libraries(kritastore PUBLIC
    kritaversion
    kritaglobal
    krita_kf_compat
    Qt6::Xml
    Qt6::Gui
    Qt6::Core5Compat
    "${WASM_DEPS_PREFIX}/lib/libquazip1-qt6.a"
    "${WASM_DEPS_PREFIX}/lib/libz.a"
)

# ==============================================================================
# 7. Upstream Krita: kritacommand (libs/command - KUndo2QStack undo engine)
# ==============================================================================
add_library(kritacommand STATIC
    "${KRITA_SOURCE_DIR}/libs/command/kundo2stack.cpp"
    "${KRITA_SOURCE_DIR}/libs/command/kundo2group.cpp"
    "${KRITA_SOURCE_DIR}/libs/command/kundo2view.cpp"
    "${KRITA_SOURCE_DIR}/libs/command/kundo2model.cpp"
    "${KRITA_SOURCE_DIR}/libs/command/kundo2magicstring.cpp"
    "${KRITA_SOURCE_DIR}/libs/command/kundo2commandextradata.cpp"
    "${KRITA_SOURCE_DIR}/libs/command/kis_undo_store.cpp"
    "${KRITA_SOURCE_DIR}/libs/command/kis_undo_stores.cpp"
    "${KRITA_SOURCE_DIR}/libs/command/kis_command_utils.cpp"
    "${KRITA_SOURCE_DIR}/libs/command/KisCumulativeUndoData.cpp"
)
target_include_directories(kritacommand PUBLIC
    ${KRITA_COMPAT_INCLUDES}
    "${KRITA_SOURCE_DIR}/libs/command"
)
target_link_libraries(kritacommand PUBLIC
    kritaglobal
    krita_kf_compat
    Qt6::Core
    Qt6::Widgets
)

# ==============================================================================
# 8. Upstream Krita: kritaresources (libs/resources - SQLite resource locator)
# ==============================================================================
set(KRITARESOURCES_SRCS
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceCacheDb.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceLoader.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceLoaderRegistry.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceLocator.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceStorage.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceModel.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisTagFilterResourceProxyModel.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceModelProvider.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceTypeModel.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisStorageModel.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisStorageFilterProxyModel.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceIterator.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceSearchBoxFilter.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceQueryMapper.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceTypes.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisStoragePlugin.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisBundleStorage.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisFolderStorage.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisMemoryStorage.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisTag.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisTagModel.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisTagResourceModel.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoResource.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoResourceBundle.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoResourceBundleManifest.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoMD5Generator.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoResourcePaths.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoResourceSignature.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoEmbeddedResource.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoResourceLoadResult.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/ResourceDebug.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisRequiredResourcesOperators.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourcesInterface.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisLocalStrokeResources.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisGlobalResourcesInterface.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoResourceCacheInterface.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoResourceCacheStorage.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoResourceCachePrefixedStorageWrapper.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoCanvasResourcesInterface.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KoLocalStrokeCanvasResources.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisEmbeddedResourceStorageProxy.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceThumbnailCache.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisResourceMetaDataModel.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisSqlQueryLoader.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisDatabaseTransactionLock.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/KisTemporaryResourceStorageLock.cpp"
    "${KRITA_SOURCE_DIR}/libs/resources/sql.qrc"
)
add_library(kritaresources STATIC ${KRITARESOURCES_SRCS})
target_include_directories(kritaresources PUBLIC
    ${KRITA_COMPAT_INCLUDES}
    "${KRITA_SOURCE_DIR}/libs/resources"
)
target_link_libraries(kritaresources PUBLIC
    kritaversion
    kritaglobal
    kritaplugin
    kritastore
    krita_kf_compat
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
)

# ==============================================================================
# 9. Upstream Krita: kritapigment (libs/pigment + LCMS2 color engine)
# ==============================================================================
set(KRITAPIGMENT_SRCS
    "${KRITA_SOURCE_DIR}/libs/pigment/DebugPigment.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoBasicHistogramProducers.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoAlphaMaskApplicatorBase.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoAlphaMaskApplicatorFactory.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoAlphaMaskApplicatorFactoryImpl.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoOptimizedPixelDataScalerU8ToU16Base.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoOptimizedPixelDataScalerU8ToU16Factory.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoOptimizedPixelDataScalerU8ToU16FactoryImpl.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColor.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorDisplayRendererInterface.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorConversionAlphaTransformation.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorConversionCache.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorConversions.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorConversionSystem.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorConversionTransformation.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorProofingConversionTransformation.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorConversionTransformationFactory.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorModelStandardIds.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorProfile.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorSpace.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorSpaceEngine.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorSpaceFactory.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorSpaceMaths.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorimetryUtils.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoCmykColorSpaceMaths.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoLabColorSpaceMaths.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorSpaceRegistry.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorProfileStorage.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorTransformation.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorTransformationFactory.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoColorTransformationFactoryRegistry.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoCompositeColorTransformation.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoCompositeOp.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoCompositeOpRegistry.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoCopyColorConversionTransformation.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoFallBackColorTransformation.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoHistogramProducer.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoGamutMappingUtils.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/KoMultipleColorConversionTransformation.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/colorspaces/KoAlphaColorSpace.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/colorspaces/KoLabColorSpace.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/colorspaces/KoRgbU16ColorSpace.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/colorspaces/KoRgbU8ColorSpace.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/colorspaces/KoSimpleColorSpaceEngine.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/compositeops/KoOptimizedCompositeOpFactory.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/compositeops/KoOptimizedCompositeOpFactoryPerArch_Scalar.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/compositeops/KoAlphaDarkenParamsWrapper.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/compositeops/KoColorSpaceBlendingPolicy.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/colorprofiles/KoDummyColorProfile.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/resources/KoAbstractGradient.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/resources/KoColorSet.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/resources/KisSwatch.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/resources/KisSwatchGroup.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/resources/KoPattern.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/resources/KoStopGradient.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/resources/KoSegmentGradient.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/resources/KisGradientConversion.cpp"
    "${KRITA_SOURCE_DIR}/libs/pigment/resources/KisUniqueColorSet.cpp"
)
add_library(kritapigment STATIC ${KRITAPIGMENT_SRCS})
target_include_directories(kritapigment PUBLIC
    ${KRITA_COMPAT_INCLUDES}
    "${KRITA_SOURCE_DIR}"
    "${KRITA_SOURCE_DIR}/libs/pigment"
    "${KRITA_SOURCE_DIR}/libs/pigment/resources"
    "${KRITA_SOURCE_DIR}/libs/pigment/compositeops"
)
target_link_libraries(kritapigment PUBLIC
    kritaplugin
    kritastore
    kritaglobal
    kritaresources
    kritacommand
    kritamultiarch
    krita_kf_compat
    Qt6::Core
    Qt6::Gui
    Qt6::Xml
    "${WASM_DEPS_PREFIX}/lib/liblcms2.a"
)

# ==============================================================================
# 10. Krita WebAssembly Executable Target (Qt6 + WebGL2 + Krita C++ Engine)
# ==============================================================================
qt_add_executable(krita_wasm
    src/main.cpp
    src/KritaWasmApp.h
    src/KritaWasmApp.cpp
)
target_include_directories(krita_wasm PRIVATE
    ${KRITA_COMPAT_INCLUDES}
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(krita_wasm PRIVATE
    kritapigment
    kritacommand
    kritaresources
    kritastore
    kritaplugin
    kritamultiarch
    kritaglobal
    kritaversion
    krita_kf_compat
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Xml
    Qt6::Sql
    Qt6::Svg
    Qt6::Core5Compat
    "${WASM_DEPS_PREFIX}/lib/liblcms2.a"
    "${WASM_DEPS_PREFIX}/lib/libquazip1-qt6.a"
    "${WASM_DEPS_PREFIX}/lib/libz.a"
)

set_target_properties(krita_wasm PROPERTIES
    QT_WASM_INITIAL_MEMORY "134217728"
    QT_WASM_EXTRA_EXPORTED_METHODS "IDBFS,ccall,cwrap,UTF8ToString,stringToUTF8"
)

target_link_options(krita_wasm PRIVATE
    -lidbfs.js
    "SHELL:-s EXPORTED_FUNCTIONS=_main,_krita_wasm_open_file,_krita_wasm_export_kra,_krita_wasm_export_png,_krita_wasm_new_document,_krita_wasm_get_engine_info,_krita_wasm_undo,_krita_wasm_redo"
)
