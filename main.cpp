#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

#include "antlr4-runtime.h"

#include "ast.h"
#include "ast_rtl.h"
#include "rtl.h"
#include "type_check.h"
#include "ssa.h"
#include "rtl_ssa.h"
#include "ssa_llvm.h"

using namespace bx;

int main(int argc, char *argv[]) {
  const std::string rt_flags = "-L build -lbxrt -Wl,-rpath," +
                               std::filesystem::current_path().string() +
                               "/build/";

  if (argc >= 2) {
    std::string bx_file{argv[1]};

    if (bx_file.size() < 3 || bx_file.substr(bx_file.size() - 3, 3) != ".bx") {
      std::cerr << "Bad file name: " << bx_file << std::endl;
      std::exit(1);
    }

    auto file_root = bx_file.substr(0, bx_file.size() - 3);

    auto prog = source::read_program(bx_file);
    {
      check::type_check(prog);
      std::cout << bx_file << " parsed and type checked.\n";
      auto p_file = file_root + ".parsed";
      std::ofstream p_out;
      p_out.open(p_file);
      p_out << prog;
      p_out.close();
      std::cout << p_file << " written.\n";
    }

    auto rtl_prog = rtl::transform(prog);
    {
      auto rtl_file = file_root + ".rtl";
      std::ofstream rtl_out;
      rtl_out.open(rtl_file);
      for (auto const &gv : prog.global_vars)
        rtl_out << "GLOBAL " << gv.first << " = " << *(gv.second->init) << " : "
                << gv.second->ty << "\n\n";
      for (auto const &rtl_cbl : rtl_prog)
        rtl_out << rtl_cbl << '\n';
      rtl_out.close();
      std::cout << rtl_file << " written.\n";
    }
    auto ssa_prog = blocks_generate(prog.global_vars, rtl_prog);
    {
      auto ssa_file = file_root + ".ssa";
      std::ofstream ssa_out;
      ssa_out.open(ssa_file);
      for (auto const &gv : prog.global_vars)
        ssa_out << "GLOBAL " << gv.first << " = " << *(gv.second->init) << " : "
                << gv.second->ty << "\n\n";
      for (auto const &ssa_cbl : ssa_prog)
        ssa_out << ssa_cbl << '\n';
      ssa_out.close();
      std::cout << ssa_file << " written.\n";
    }
    auto llvm_prog = llvm_generate(prog.global_vars, ssa_prog);
    auto llvm_file = file_root + ".ll";
    {
      std::ofstream llvm_out;
      llvm_out.open(llvm_file);
      for (auto const &l : llvm_prog)
        llvm_out << *l;
      llvm_out.close();
      std::cout << llvm_file << " written.\n";
    }
    auto exe_file = file_root + ".exe";
    std::string cmd = "/usr/local/llvm-6.0.1/bin/clang -o " + exe_file + " " + llvm_file + " bxrt.c";
    std::cout << "Running: " << cmd << std::endl;
    if (std::system(cmd.c_str()) != 0) {
      std::cerr << "Could not run llvm-as successfully!\n";
      std::exit(2);
    }  
    std::cout << exe_file << " created.\n";
  }
}
