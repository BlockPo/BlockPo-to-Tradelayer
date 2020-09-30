#include <tradelayer/version.h>

#include <clientversion.h>
#include <tinyformat.h>

#include <string>

// #ifdef HAVE_BUILD_INFO FIXME: build.h doesnt exist
// #    include <build.h>
// #endif

#ifndef COMMIT_ID
#   ifdef GIT_ARCHIVE
#       define COMMIT_ID "$Format:%h$"
#   elif defined(BUILD_SUFFIX)
#       define COMMIT_ID STRINGIZE(BUILD_SUFFIX)
#   else
#       define COMMIT_ID ""
#   endif
#endif

#ifndef BUILD_DATE
#    ifdef GIT_COMMIT_DATE
#        define BUILD_DATE GIT_COMMIT_DATE
#    else
#        define BUILD_DATE __DATE__ ", " __TIME__
#    endif
#endif

#ifdef TL_VERSION_STATUS
#    define TL_VERSION_SUFFIX STRINGIZE(TL_VERSION_STATUS)
#else
#    define TL_VERSION_SUFFIX ""
#endif

//! Returns formatted Trade Layer version, e.g. "1.2.0" or "1.3.4.1"
const std::string TradeLayerVersion()
{
    if (TL_VERSION_BUILD) {
        return strprintf("%d.%d.%d.%d",
                TL_VERSION_MAJOR,
                TL_VERSION_MINOR,
                TL_VERSION_PATCH,
                TL_VERSION_BUILD);
    } else {
        return strprintf("%d.%d.%d",
                TL_VERSION_MAJOR,
                TL_VERSION_MINOR,
                TL_VERSION_PATCH);
    }
}

//! Returns formatted Bitcoin Core version, e.g. "0.10", "0.9.3"
const std::string BitcoinCoreVersion()
{
    if (CLIENT_VERSION_BUILD) {
        return strprintf("%d.%d.%d.%d",
                CLIENT_VERSION_MAJOR,
                CLIENT_VERSION_MINOR,
                CLIENT_VERSION_REVISION,
                CLIENT_VERSION_BUILD);
    } else {
        return strprintf("%d.%d.%d",
                CLIENT_VERSION_MAJOR,
                CLIENT_VERSION_MINOR,
                CLIENT_VERSION_REVISION);
    }
}

//! Returns build date
const std::string BuildDate()
{
    return std::string(BUILD_DATE);
}

//! Returns commit identifier, if available
const std::string BuildCommit()
{
    return std::string(COMMIT_ID);
}
