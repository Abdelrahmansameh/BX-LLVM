#pragma once

/**
 * This module defines the final concrete target of the compilation, which is
 * llvm assembly.
 */

#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace bx {
namespace llvm {

using Label = std::string;

// LLVM Assembly

struct Llvm {

  /** String of "destination", name of the operation */
  std::string dest;

  /** String of type of arguments */
  std::string type;

  /** labels that are mentioned as arguments */
  std::vector<Label> args;

  /*
   * The representation template. This string is allowed to contain
   * the following kinds of occurrences which are replaced automatically
   * by the elements of the use/def/jump_dests vectors above.
   *
   *   `d                -- destination
   *   `t                -- types
   *   `a0, `a1, ...     -- arguments
   */

  const std::string repr_template;

  using ptr = std::shared_ptr<Llvm>;

private:
  /**
   * The constructor is hidden so that only the static methods of this class
   * can be used to build instances
   */
  explicit Llvm(std::string const &dest, std::string const &type,
                std::vector<Label> const &args, std::string const &repr)
      : dest{dest}, type{type}, args{args}, repr_template{repr} {}

public:

static ptr directive(std::string const &directive) {
    return std::shared_ptr<Llvm>(
        new Llvm{{}, {}, {}, directive});
}

static ptr set_label(int64_t imm) {
    return std::shared_ptr<Llvm>(
        new Llvm{{}, {}, {}, std::to_string(imm) + ":"});
}

#define ARITH_BINOP1(mnemonic)                                                 \
  static ptr mnemonic##q(std::string const &dest, std::string const &type,     \
                         std::string const &arg1, std::string const &arg2) {   \
    std::string repr = "\t %`d = " #mnemonic " nsw `t `a0, `a1";               \
    return std::shared_ptr<Llvm>(                                              \
        new Llvm{{dest}, {type}, {{arg1, arg2}}, repr});                       \
  }                                                                            \
  static ptr mnemonic##q(std::string const &dest, std::string const &type,     \
                         int64_t imm, std::string const &arg2) {               \
    std::string repr =                                                         \
        "\t %`d = " #mnemonic " nsw `t " + std::to_string(imm) + ", `a1";      \
    return std::shared_ptr<Llvm>(new Llvm{{dest}, {type}, {arg2}, repr});      \
  }                                                                            \
  static ptr mnemonic##q(std::string const &dest, std::string const &type,     \
                         std::string const &arg1, int64_t imm) {               \
    std::string repr =                                                         \
        "\t %`d = " #mnemonic " nsw `t `a0, " + std::to_string(imm);           \
    return std::shared_ptr<Llvm>(new Llvm{{dest}, {type}, {arg1}, repr});      \
  }
  ARITH_BINOP1(add)
  ARITH_BINOP1(sub)
  ARITH_BINOP1(mul)
#undef ARITH_BINOP1

#define ARITH_BINOP2(mnemonic)                                                 \
  static ptr mnemonic##q(std::string const &dest, std::string const &type,     \
                         std::string const &arg1, std::string const &arg2) {   \
    std::string repr = "\t %`d = " #mnemonic " `t `a0, `a1";                   \
    return std::shared_ptr<Llvm>(                                              \
        new Llvm{{dest}, {type}, {{arg1, arg2}}, repr});                       \
  }                                                                            \
  static ptr mnemonic##q(std::string const &dest, std::string const &type,     \
                         int64_t imm, std::string const &arg2) {               \
    std::string repr =                                                         \
        "\t %`d = " #mnemonic " `t " + std::to_string(imm) + ", `a1";          \
    return std::shared_ptr<Llvm>(new Llvm{{dest}, {type}, {arg2}, repr});      \
  }                                                                            \
  static ptr mnemonic##q(std::string const &dest, std::string const &type,     \
                         std::string const &arg1, int64_t imm) {               \
    std::string repr =                                                         \
        "\t %`d = " #mnemonic " `t `a0, " + std::to_string(imm);               \
    return std::shared_ptr<Llvm>(new Llvm{{dest}, {type}, {arg1}, repr});      \
  }
  ARITH_BINOP2(udiv)
  ARITH_BINOP2(shl)  // left shift
  ARITH_BINOP2(ashr) // arithmetic right shift
  ARITH_BINOP2(and)
  ARITH_BINOP2(or)
  ARITH_BINOP2 (xor)
#undef ARITH_BINOP2

#define COMP(mnemonic)                                                         \
  static ptr mnemonic##q(std::string const &dest, std::string const &type,     \
                         std::string const &arg1, std::string const &arg2) {   \
    std::string repr = "\t %`d = icomp " #mnemonic " `t `a0, `a1";             \
    return std::shared_ptr<Llvm>(                                              \
        new Llvm{{dest}, {type}, {{arg1, arg2}}, repr});                       \
  }                                                                            \
  static ptr mnemonic##q(std::string const &dest, std::string const &type,     \
                         int64_t imm, std::string const &arg2) {               \
    std::string repr =                                                         \
        "\t %`d = icomp " #mnemonic " `t " + std::to_string(imm) + ", `a1";    \
    return std::shared_ptr<Llvm>(new Llvm{{dest}, {type}, {arg2}, repr});      \
  }                                                                            \
  static ptr mnemonic##q(std::string const &dest, std::string const &type,     \
                         std::string const &arg1, int64_t imm) {               \
    std::string repr =                                                         \
        "\t %`d = icomp " #mnemonic " `t `a0, " + std::to_string(imm);         \
    return std::shared_ptr<Llvm>(new Llvm{{dest}, {type}, {arg1}, repr});      \
  }
  COMP(eq)  // equal
  COMP(ne)  // not equal
  COMP(sgt) // signed greater than
  COMP(sge) // signed greater and equal
  COMP(slt) // signed less than
  COMP(sle) // signed less and equal
#undef COMP

  static ptr global_with_value(std::string const &name, std::string const &type,
                               int64_t imm) {
    std::string repr =
        "\t %`d = global `t " + std::to_string(imm) + ", align 8 ";
    return std::unique_ptr<Llvm>(new Llvm{{name}, {type}, {}, repr});
  }

  static ptr global_no_value(std::string const &name, std::string const &type) {
    std::string repr = "\t %`d = global `t, align 8 ";
    return std::unique_ptr<Llvm>(new Llvm{{name}, {type}, {}, repr});
  }

  static ptr ret_void() {
    std::string repr = "\t ret void";
    return std::unique_ptr<Llvm>(new Llvm{{}, {}, {}, repr});
  }

  static ptr ret_type(std::string const &type, std::string const &arg) {
    std::string repr = "\t ret `t " + arg;
    return std::unique_ptr<Llvm>(new Llvm{{}, {type}, {}, repr});
  }

  static ptr allocation(std::string const &name, std::string const &glb_var) {
    std::string repr = "\t %`d = alloca %" + glb_var + " align 8";
    return std::unique_ptr<Llvm>(new Llvm{{name}, {}, {}, repr});
  }

  static ptr br_cond(std::string const &name, std::string const &fst, std::string const &snd) {
    std::string repr = "\t br i1 %`d, label %" + fst + ", label %" + snd;
    return std::unique_ptr<Llvm>(new Llvm{{name}, {}, {}, repr});
  }

  static ptr br_uncond(std::string const &fst) {
    std::string repr = "\t br label %" + fst;
    return std::unique_ptr<Llvm>(new Llvm{{}, {}, {}, repr});
  }

  static ptr call(std::string const &name, std::string const &type, std::vector<std::vector<Label>> const &args) {
    std::string repr = "\t call `t  @`d(";
    if (!args.empty()){
      int s = args.size();
      for (int i=0; i<s; i++) {
        if ( i == s-1 ){
          repr += args[i][0] + " %" + args[i][1];
        }
        repr += args[i][0] + " %" + args[i][1] + ", ";
      }
    }
    repr = repr + ")";
    return std::unique_ptr<Llvm>(
        new Llvm{{name}, {type}, {}, repr});
  }

  static ptr define(std::string const &name, std::string const &type, std::vector<std::vector<Label>> const &args, std::string const &body) {
    std::string repr = "define `t  @`d(";
    if (!args.empty()){
      int s = args.size();
      for (int i=0; i<s; i++) {
        if ( i == s-1 ){
          repr += args[i][0] + " %" + args[i][1];
        }
        repr += args[i][0] + " %" + args[i][1] + ", ";
      }
    }
    repr += ") { \n" + body + "\n }";
    return std::unique_ptr<Llvm>(
        new Llvm{{name}, {type}, {}, repr});
  }

  static ptr phi(std::string const &name, std::string const &type, std::vector<std::vector<Label>> const &args) {
    std::string repr = "\t %`d = phi `t ";
     if (!args.empty()){
      int s = args.size();
      for (int i=0; i<s; i++) {
        if ( i == s-1 ){
          repr += "[ %" + args[i][0] + ", %" + args[i][1] + "] ";
        }
        repr += "[ %" + args[i][0] + ", %" + args[i][1] + "], ";
      }
    }
    return std::unique_ptr<Llvm>(
        new Llvm{{name}, {type}, {}, repr});
  }


  /////////////////////////////////////////////////////////////////////////////////////////////
  /*
  static ptr alloca(std::string const &name) {
      std::string repr = "\t %`d = icomp   `t `a0, "+ std::to_string(imm);
      return std::unique_ptr<Llvm>(
          new Llvm{{}, {dest}, {}, repr});
    }

  static ptr getelementptr(std::string const &name) {
      std::string repr = "\t %`d = icomp  `t `a0, "+ std::to_string(imm);
      return std::unique_ptr<Llvm>(
          new Llvm{{}, {dest}, {}, repr});
    }

  static ptr load(std::string const &name) {
      std::string repr = "\t %`d = icomp  `t `a0, "+ std::to_string(imm);
      return std::unique_ptr<Llvm>(
          new Llvm{}, {dest}, {}, repr});
    }

  static ptr store(std::string const &name) {
      std::string repr = "\t %`d = icomp  `t `a0, "+ std::to_string(imm);
      return std::unique_ptr<Llvm>(
          new Llvm{{}, {dest}, {}, repr});
    }
  */
};

std::ostream &operator<<(std::ostream &out, Llvm const &line);

} // namespace llvm
} // namespace bx
