// NightModeProvider.cpp — exists solely so MOC can generate vtable/meta-object
// for the Q_OBJECT interface class. See gotchas in CLAUDE.md.
#include "NightModeProvider.hpp"

namespace oap {
namespace aa {

// Intentionally empty — all methods are either pure virtual or inline in the header.
// The MOC-generated code for Q_OBJECT needs exactly one .cpp to anchor the vtable.

} // namespace aa
} // namespace oap
