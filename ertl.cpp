#include "ertl.h"
#include "rtl.h"

namespace bx {
namespace ertl {

// Mach::RBP is treated specially
const Mach callee_saves[5] = {Mach::RBX, Mach::R12, Mach::R13, Mach::R14,
                              Mach::R15};
const Mach input_regs[6] = {Mach::RDI, Mach::RSI, Mach::RDX,
                            Mach::RCX, Mach::R8,  Mach::R9};

const std::map<Unop::Code, char const *> Unop::code_map{
    {Unop::Code::NOT, "not"}, {Unop::Code::NEG, "neg"}};

const std::map<Binop::Code, char const *> Binop::code_map{
    {Binop::Code::ADD, "add"}, {Binop::Code::SUB, "sub"},
    {Binop::Code::MUL, "mul"}, {Binop::Code::DIV, "div"},
    {Binop::Code::REM, "rem"}, {Binop::Code::SAL, "sal"},
    {Binop::Code::SAR, "sar"}, {Binop::Code::AND, "and"},
    {Binop::Code::OR, "or"},   {Binop::Code::XOR, "xor"},
};

const std::map<Ubranch::Code, char const *> Ubranch::code_map{
    {Ubranch::Code::JZ, "jz"},
    {Ubranch::Code::JNZ, "jnz"},
};

const std::map<Bbranch::Code, char const *> Bbranch::code_map{
    {Bbranch::Code::JE, "je"},   {Bbranch::Code::JNE, "jne"},
    {Bbranch::Code::JL, "jl"},   {Bbranch::Code::JNL, "jnl"},
    {Bbranch::Code::JLE, "jle"}, {Bbranch::Code::JNLE, "jnle"},
    {Bbranch::Code::JG, "jg"},   {Bbranch::Code::JNG, "jng"},
    {Bbranch::Code::JGE, "jge"}, {Bbranch::Code::JNGE, "jnge"},
};

char const *to_string(Mach m) {
  switch (m) {
    // clang-format off
#define CASE(C, s) case Mach::C: return "%" # s;
  CASE(RAX, rax) CASE(RBX, rbx) CASE(RCX, rcx) CASE(RDX, rdx)
  CASE(RBP, rbp) CASE(RDI, rdi) CASE(RSI, rsi) CASE(RSP, rsp)
  CASE(R8,  r8)  CASE(R9,  r9)  CASE(R10, r10) CASE(R11, r11)
  CASE(R12, r12) CASE(R13, r13) CASE(R14, r14) CASE(R15, r15)
#undef CASE
    // clang-format on
  }
  return nullptr;
}

std::ostream &operator<<(std::ostream &out, Mach m) {
  return out << to_string(m);
}

std::ostream &operator<<(std::ostream &out, Callable const &cbl) {
  out << "CALLABLE \"" << cbl.name << "\":";
  out << "\nenter: " << cbl.enter << "\nleave: " << cbl.leave;
  out << "\n----\n";
  for (auto const &in_lab : cbl.schedule)
    out << in_lab << ": " << *(cbl.body.at(in_lab)) << '\n';
  return out << "END CALLABLE\n\n";
}

} // namespace ertl
} // namespace bx