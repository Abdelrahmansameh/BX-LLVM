#include "llvm.h"

namespace bx {
namespace llvm {

/*
int Pseudo::__last_pseudo_id = 0;

std::ostream &operator<<(std::ostream &out, Pseudo const &p) {
  if (!p.binding.has_value())
    return out << "<pseudo#" << p.id << '>';
  auto b = p.binding.value();
  if (auto reg = std::get_if<0>(&b))
    return out << *reg;
  return out << -8 * std::get<1>(b) << "(%rbp)";
}
*/

std::ostream &operator<<(std::ostream &out, Llvm const &line) {
  for (std::size_t i = 0; i < line.repr_template.size(); i++)
    if (line.repr_template[i] == '`')
      switch (line.repr_template[++i]) {
      case '`':
        out << '`';
        break;
      case 'd':
        out << line.dest;
        break;
      case 't':
        out << line.type;
        break;
      case 'a':
        out << line.args[line.repr_template[++i] - '0'];
        break;
      default:
        throw std::runtime_error{"bad repr_template"};
      }
    else
      out << line.repr_template[i];

  return out << '\n';
}

} // namespace llvm
} // namespace bx
