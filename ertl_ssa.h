#pragma once

#include "ssa.h"
#include "ertl.h"

namespace bx {

ssa::Program ssa_generate(source::Program::GlobalVarTable const &,
                        ertl::Program const &);

} // namespace bx