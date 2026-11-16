#include "pos/adapters.hpp"
#include "pos/console.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace {

class DiContainer {
public:
    template <class Port>
    void add(std::shared_ptr<Port> implementation) {
        if (!services_.try_emplace(typeid(Port), std::move(implementation)).second) {
            throw std::logic_error("Реализация этого порта уже зарегистрирована.");
        }
    }

    template <class Port>
    Port& get() const {
        return *std::static_pointer_cast<Port>(services_.at(typeid(Port)));
    }

private:
    std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
};

}  // namespace

int main() {
    using namespace pos;
    try {
        DiContainer dependencies;
        dependencies.add<application::StudentRepository>(
            std::make_shared<adapters::InMemoryStudentRepository>());
        dependencies.add<application::RubricRepository>(
            std::make_shared<adapters::InMemoryRubricRepository>());
        dependencies.add<application::EvaluationRepository>(
            std::make_shared<adapters::InMemoryEvaluationRepository>());
        dependencies.add<application::ResultsExporter>(
            std::make_shared<adapters::CsvResultsExporter>());

        application::PosFacade facade(dependencies.get<application::StudentRepository>(),
                                      dependencies.get<application::RubricRepository>(),
                                      dependencies.get<application::EvaluationRepository>(),
                                      dependencies.get<application::ResultsExporter>());
        adapters::run_console(facade, std::cin, std::cout);
    } catch (const std::exception& error) {
        std::cerr << "Ошибка приложения: " << error.what() << '\n';
        return 1;
    }
}
