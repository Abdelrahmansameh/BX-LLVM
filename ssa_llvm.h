#pragma once

#include "llvm.h"
#include "ssa.h"

namespace bx {

using LlvmProgram = std::vector<std::shared_ptr<llvm::Llvm>>;

LlvmProgram llvm_generate(source::Program::GlobalVarTable const &,
                        ssa::Program const &);

} // namespace bx