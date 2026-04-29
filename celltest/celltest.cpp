#include "testing.hpp"
#include "utils.hpp"

using namespace cellulon;

int main() {
    Option<int> oi { Some(3) };

    CL_ASSERT(oi.is_some(), "Option<int> with value Some(3) is NOT some.");
    CL_ASSERT(!oi.is_none(), "Option<int> with value Some(3) is None?");

    oi = None;
    CL_ASSERT(oi.is_none(), "Reset Option<int>, but it's STILL not None.");
    CL_ASSERT(!oi.is_some(), "Reset Option<int>, but !is_some() is not true.");

    std::println("[OK] Control reached end of main() successfully.");

    return 0;
}