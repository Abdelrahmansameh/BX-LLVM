#pragma once

#include "amd64.h"
#include "ertl.h"

namespace bx {

using AsmProgram = std::vector<std::unique_ptr<amd64::Asm>>;

AsmProgram asm_generate(source::Program::GlobalVarTable const &,
                        ertl::Program const &);

} // namespace bx