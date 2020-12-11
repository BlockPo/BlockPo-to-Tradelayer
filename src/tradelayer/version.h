#ifndef TRADELAYER_VERSION_H
#define TRADELAYER_VERSION_H

/* #if defined(HAVE_CONFIG_H) */
/* #include <config/bitcoin-config.h> */
/* #else */

//
// Trade Layer version information are also to be defined in configure.ac.
//
// During the configuration, this information are used for other places.
//

// Increase with every consensus affecting change
#define TL_VERSION_MAJOR       0

// Increase with every non-consensus affecting feature
#define TL_VERSION_MINOR       2

// Increase with every patch, which is not a feature or consensus affecting
#define TL_VERSION_PATCH       0

// Non-public build number/revision (usually zero)
#define TL_VERSION_BUILD       0

//#endif // HAVE_CONFIG_H

#if !defined(WINDRES_PREPROC)

//
// *-res.rc includes this file, but it cannot cope with real c++ code.
// WINDRES_PREPROC is defined to indicate that its pre-processor is running.
// Anything other than a define should be guarded below:
//

#include <string>

//! Trade Layer client version
static const int TL_VERSION =
                    +100000000000 * TL_VERSION_MAJOR
                    +    10000000 * TL_VERSION_MINOR
                    +        1000 * TL_VERSION_PATCH
                    +           1 * TL_VERSION_BUILD;

//! Returns formatted Trade Layer version, e.g. "1.2.0"
const std::string TradeLayerVersion();

//! Returns formatted Bitcoin Core version, e.g. "0.10", "0.9.3"
const std::string BitcoinCoreVersion();

//! Returns build date
const std::string BuildDate();

//! Returns commit identifier, if available
const std::string BuildCommit();


#endif // WINDRES_PREPROC

#endif // TRADELAYER_VERSION_H
