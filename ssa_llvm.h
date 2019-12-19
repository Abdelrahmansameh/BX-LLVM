#pragma once

#include "llvm.h"
#include "ssa.h"

namespace bx {

using AsmProgram = std::vector<std::shared_ptr<amd64::Asm>>;

AsmProgram asm_generate(source::Program::GlobalVarTable const &,
                        ertl::Program const &);

} // namespace bx