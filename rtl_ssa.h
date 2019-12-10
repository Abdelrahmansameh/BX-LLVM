#pragma once

#include "ssa.h"
#include "rtl.h"

namespace bx {

ssa::Program blocks_generate(source::Program::GlobalVarTable const &,
                        rtl::Program  &);

} // namespace bx