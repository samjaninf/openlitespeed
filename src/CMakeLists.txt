cmake_minimum_required(VERSION 3.0)

link_directories("/usr/lib64")


add_subdirectory(adns)
add_subdirectory(edio)
add_subdirectory(extensions)
add_subdirectory(http)
add_subdirectory(liblsquic)
add_subdirectory(log4cxx)
add_subdirectory(lsiapi)
add_subdirectory(lsr)
add_subdirectory(main)
add_subdirectory(modules)
add_subdirectory(quic)
add_subdirectory(shm)
add_subdirectory(socket)
add_subdirectory(spdy)
add_subdirectory(sslpp)
add_subdirectory(ssi)
add_subdirectory(thread)
add_subdirectory(util)
add_subdirectory(h2)

########### next target ###############

SET(openlitespeed_SRCS
   modules/prelinkedmods.cpp
   httpdtest.cpp
   main.cpp
)


SET(unittest_STAT_SRCS
   ../test/edio/bufferedostest.cpp
   ../test/edio/multiplexertest.cpp
   ../test/extensions/fcgistartertest.cpp
   ../test/http/expirestest.cpp
   ../test/http/rewritetest.cpp
   ../test/http/httprequestlinetest.cpp
   ../test/http/httprangetest.cpp
   ../test/http/denieddirtest.cpp
   ../test/http/statusurlmaptest.cpp
   ../test/http/contexttreetest.cpp
   ../test/http/httpmimetest.cpp
   ../test/http/httpcgitooltest.cpp
   ../test/http/chunkostest.cpp
   ../test/http/chunkistest.cpp
   ../test/http/httplistenerstest.cpp
   ../test/http/httpvhostlisttest.cpp
   ../test/http/httpreqtest.cpp
   ../test/http/httpreqheaderstest.cpp
   ../test/http/httpbuftest.cpp
   ../test/http/httpheadertest.cpp
   ../test/http/datetimetest.cpp
   ../test/http/reqparsertest.cpp
   ../test/socket/hostinfotest.cpp
   ../test/socket/tcpsockettest.cpp
   ../test/socket/coresockettest.cpp
   ../test/util/pcregextest.cpp
   ../test/util/ghashtest.cpp
   ../test/util/linkedobjtest.cpp
   ../test/util/dlinkqueuetest.cpp
   ../test/util/gzipbuftest.cpp
   ../test/util/brotlibuftest.cpp
   ../test/util/vmembuftest.cpp
   ../test/util/gpathtest.cpp
   ../test/util/poolalloctest.cpp
   ../test/util/xmlnodetest.cpp
   ../test/util/accesscontroltest.cpp
   ../test/util/loopbuftest.cpp
   ../test/util/logfiletest.cpp
   ../test/util/stringmaptest.cpp
   ../test/util/httpfetchtest.cpp
   ../test/util/partitioninfotest.cpp
   ../test/util/filtermatchtest.cpp
   ../test/util/gmaptest.cpp
   ../test/util/ahotest.cpp
   ../test/util/stringtooltest.cpp
   ../test/util/gpointerlisttest.cpp
   ../test/util/objarraytest.cpp
   ../test/util/objpooltest.cpp
   ../test/util/radixtreetest.cpp
   ../test/spdy/spdyzlibfiltertest.cpp
   ../test/spdy/spdyconnectiontest.cpp
   ../test/spdy/dummiostream.cpp
   ../test/spdy/pushtest.cpp
   ../test/lsiapi/moduledata.cpp
   ../test/lsiapi/moduletimertest.cpp
   ../test/lsiapi/lsiapihookstest.cpp
   ../test/lsiapi/envhandler.cpp
   ../test/lsiapi/moduleconf.cpp
   ../test/lsr/ls_ahotest.cpp
   ../test/lsr/ls_confparsertest.cpp
   ../test/lsr/ls_base64test.cpp
   ../test/lsr/ls_buftest.cpp
   ../test/lsr/ls_dlinkqtest.cpp
   ../test/lsr/ls_hashtest.cpp
   ../test/lsr/ls_linktest.cpp
   ../test/lsr/ls_loopbuftest.cpp
   ../test/lsr/ls_maptest.cpp
   ../test/lsr/ls_md5test.cpp
   ../test/lsr/ls_objarraytest.cpp
   ../test/lsr/ls_objpooltest.cpp
   ../test/lsr/ls_pcregtest.cpp
   ../test/lsr/ls_pooltest.cpp
   ../test/lsr/ls_ptrlisttest.cpp
   ../test/lsr/ls_sha1test.cpp
   ../test/lsr/ls_strtest.cpp
   ../test/lsr/ls_strlisttest.cpp
   ../test/lsr/ls_strtooltest.cpp
   ../test/lsr/ls_xpooltest.cpp
   ../test/thread/pthreadworkqueuetest.cpp
   ../test/thread/threadtest.cpp
   ../test/thread/workcrewtest.cpp
   ../test/shm/shmbaselrutest.cpp
   ../test/shm/shmxtest.cpp
)

add_executable(openlitespeed ${openlitespeed_SRCS}
    ${unittest_STAT_SRCS}
)

add_executable(ls_shmstat
    shm/ls_shmstat.cpp
)

add_executable(ls_shmhashstat
    shm/ls_shmhashstat.cpp
)


# add_executable(ctbench
#     modules/prelinkedmods.cpp
#     ../test/http/contexttreebench.cpp
#     httpdtest.cpp
# )

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
  set(LIBATOMIC atomic)
endif()

if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "FreeBSD")
  set(STDCXX libc++.a)
else()
  set(STDCXX libstdc++.a)
endif()
   
# NOTE: When creating a new directory, the order it is placed in this list
# matters.  Everything listed depends on things listed AFTER it, though
# it won't depend on everything after it.  Inter-dependency may be fixed
# by listing one directory twice, though this isn't recommended.
# So for example, if edio depends on your new directory, your directory
# should be listed AFTER edio.  PLEASE TRY TO KEEP THIS NEAT!
SET( litespeedlib
    ${ASAN_LIB} cache modgzip lsiapi main http spdy ssi http
    registry cgi fcgi jk extensions lsapi proxy
    socket sslpp lsshm thread log4cxx adns 
    quic h2 lsquic -Wl,--whole-archive util lsr -Wl,--no-whole-archive ${MMDB_LIB}
    edio libssl.a libcrypto.a ${BSSL_ADD_LIB} ${libUnitTest}
    libz.a libpcre.a libexpat.a libxml2.a
    ${IP2LOC_ADD_LIB} ${BROTLI_ADD_LIB} udns ${LINUX_AIO_LIB} 
    -nodefaultlibs pthread rt ${LIBSAN} ${LIBATOMIC} 
    ${CMAKE_DL_LIBS} ${STDCXX} crypt bcrypt m gcc_eh c c_nonshared gcc
)

target_link_libraries(openlitespeed ${litespeedlib} )

target_link_libraries(ls_shmstat lsshm log4cxx edio util lsr pthread rt )

target_link_libraries(ls_shmhashstat lsshm log4cxx edio util lsr pthread rt )

# target_link_libraries(ctbench ${litespeedlib} )

# target_link_libraries(shmtest ${litespeedlib} )

# target_link_libraries(shmlru_test ${litespeedlib} )



install(TARGETS openlitespeed DESTINATION bin)

