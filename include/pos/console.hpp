#pragma once

#include <iosfwd>

namespace pos::application {
class PosFacade;
}

namespace pos::adapters {

void run_console(application::PosFacade& facade, std::istream& input, std::ostream& output);

}
