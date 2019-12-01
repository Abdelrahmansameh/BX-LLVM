#include "ertl.h"
#include "rtl.h"

namespace bx {
ertl::Program make_explicit(source::Program::GlobalVarTable const &global_vars,
                            rtl::Program const &prog);
}