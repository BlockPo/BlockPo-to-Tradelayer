#ifndef TL_CONTEXT_H
#define TL_CONTEXT_H

#include <cassert>
#include <functional>
#include <memory>
#include <vector>

class FeesCache;

namespace tl
{
//! TradeLayer <Context> struct is meant to contain references to its internal state
//!
//! This is used by init, rpc, and test code to pass object references around
//! without needing to declare the same variables and parameters repeatedly, or
//! to use globals. More variables could be added to this struct (particularly
//! references to validation objects) to eliminate use of globals
//! and make code more modular and testable. The struct isn't intended to have
//! any member functions. It should just be a collection of references that can
//! be used without pulling in unwanted dependencies or functionality.
struct Context {
    // std::unique_ptr<FeesCache> fees;
    // std::function<void()> foo = [] {};

    //! Declare default constructor and destructor that are not inline, so code
    //! instantiating the Context struct doesn't need to #include class
    //! definitions for all the unique_ptr members.
    Context();
    ~Context();
};
} // namespace tl

#endif // TL_CONTEXT_H