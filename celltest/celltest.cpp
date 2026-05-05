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

    // copying Option<T> should be a compilation error, both on construction and assignment.
    auto op1 = Option<double> { Some(3.5) };
    auto op2 = Option<double> { Some(5.5) };

    // op1 = op2; // this fails compilation, no copy possible
    // auto op3 = op2; // same as above, no copies for Option
    //auto op2 = op1;

    // TODO: when .unwrap() is ready, test here that the opmove has value == 5.5 of the moved-from Option.
    Option<double> opmove = std::move(op2);
    CL_ASSERT(opmove.is_some(), "Moved-into Option is None?..");
    CL_ASSERT(op2.is_none(), "Moved-from Option is Some?..");

    std::println("[OK] Control reached end of main() successfully.");

    return 0;
}