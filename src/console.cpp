#include "pos/console.hpp"

#include "pos/application.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace pos::adapters {
namespace {

using application::PosFacade;
using domain::CriterionId;
using domain::StudentId;

struct EndOfInput {};

std::string_view trim(std::string_view value) {
    constexpr std::string_view whitespace = " \t\r\n\v\f";
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return value.substr(value.size());
    }
    const auto last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::string format_number(double value) {
    std::array<char, 32> buffer;
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

std::string format_percent(double value) {
    std::array<char, 16> buffer;
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                      std::chars_format::fixed, 2);
    return std::string(buffer.data(), result.ptr) + '%';
}

class ConsoleInput {
public:
    ConsoleInput(std::istream& input, std::ostream& output) : input_(input), output_(output) {}

    std::string text(std::string_view prompt) {
        auto value = line(prompt);
        if (trim(value).empty()) {
            throw std::invalid_argument("Заполните это поле.");
        }
        return value;
    }

    std::uint64_t integer(std::string_view prompt) {
        const auto raw = line(prompt);
        const auto value = trim(raw);
        std::uint64_t number{};
        const auto end = value.data() + value.size();
        const auto parsed = std::from_chars(value.data(), end, number);
        if (parsed.ec != std::errc{} || parsed.ptr != end) {
            throw std::invalid_argument("Введите целое неотрицательное число.");
        }
        return number;
    }

    double number(std::string_view prompt) {
        const auto raw = line(prompt);
        const auto value = trim(raw);
        double number{};
        const auto end = value.data() + value.size();
        const auto parsed = std::from_chars(value.data(), end, number);
        if (parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite(number)) {
            throw std::invalid_argument("Введите конечное число с точкой в качестве десятичного разделителя.");
        }
        return number;
    }

private:
    std::string line(std::string_view prompt) {
        output_ << prompt << std::flush;
        std::string value;
        if (!std::getline(input_, value)) {
            throw EndOfInput{};
        }
        if (!value.empty() && value.back() == '\r') {
            value.pop_back();
        }
        return value;
    }

    std::istream& input_;
    std::ostream& output_;
};

class ConsoleActions {
public:
    ConsoleActions(PosFacade& facade, ConsoleInput& input, std::ostream& output)
        : facade_(facade), input_(input), output_(output) {}

    void add_student() {
        const auto id = facade_.add_student(input_.text("Имя студента > "));
        output_ << "Студент добавлен, ID " << id << ".\n";
    }

    void list_students() {
        const auto students = facade_.list_students();
        for (const auto& student : students) {
            output_ << student.id() << " — " << student.name() << '\n';
        }
        if (students.empty()) {
            output_ << "Список студентов пуст.\n";
        }
    }

    void add_criterion() {
        const auto name = input_.text("Название критерия > ");
        const auto max_score = input_.number("Максимальный балл > ");
        const auto weight = input_.number("Относительный вес > ");
        const auto id = facade_.add_criterion(name, max_score, weight);
        output_ << "Критерий добавлен, ID " << id << ".\n";
    }

    void list_criteria() {
        const auto criteria = facade_.list_criteria();
        for (const auto& criterion : criteria) {
            output_ << criterion.id() << " — " << criterion.name()
                    << " (максимум " << format_number(criterion.max_score())
                    << ", вес " << format_number(criterion.weight()) << ")\n";
        }
        if (criteria.empty()) {
            output_ << "Рубрикатор пуст.\n";
        }
    }

    void assign_evaluation() {
        write_evaluation(&PosFacade::assign_evaluation, "Оценка выставлена.");
    }

    void list_evaluations() {
        const auto evaluations = facade_.list_evaluations(read_student_id());
        if (evaluations.empty()) {
            output_ << "У студента пока нет оценок.\n";
            return;
        }

        const auto criteria = facade_.list_criteria();
        for (const auto& evaluation : evaluations) {
            const auto criterion = std::ranges::find(criteria, evaluation.criterion_id(), &domain::Criterion::id);
            output_ << "Критерий " << evaluation.criterion_id();
            if (criterion != criteria.end()) {
                output_ << " («" << criterion->name() << "»)";
            }
            output_ << " — " << format_number(evaluation.score().value()) << '\n';
        }
    }

    void correct_evaluation() {
        write_evaluation(&PosFacade::correct_evaluation, "Оценка исправлена.");
    }

    void cancel_evaluation() {
        const auto student = read_student_id();
        const auto criterion = read_criterion_id();
        facade_.cancel_evaluation(student, criterion);
        output_ << "Оценка отменена.\n";
    }

    void final_result() {
        const auto result = facade_.final_result(read_student_id());
        switch (result.status) {
        case domain::ResultStatus::complete:
            output_ << "Итог — " << format_percent(result.percent.value()) << '\n';
            break;
        case domain::ResultStatus::empty_rubric:
            output_ << "Рубрикатор пуст. Добавьте критерии для расчёта итога.\n";
            break;
        case domain::ResultStatus::missing_evaluations:
            output_ << "Ожидаются оценки по всем критериям.\n";
            break;
        }
    }

    void course_summary() {
        const auto summary = facade_.course_summary();
        output_ << "Студентов с полным набором оценок — " << summary.complete_students
                << " из " << summary.total_students << ".\n";
        if (summary.average_percent) {
            output_ << "Средний итог — " << format_percent(*summary.average_percent) << '\n';
        } else {
            output_ << "Средний итог появится после заполнения всех оценок хотя бы у одного студента.\n";
        }
    }

    void export_results() {
        const auto path = input_.text("Путь к CSV-файлу > ");
        facade_.export_results(std::filesystem::path(path));
        output_ << "Ведомость сохранена в " << path << '\n';
    }

private:
    using EvaluationWrite = void (PosFacade::*)(StudentId, CriterionId, double);

    StudentId read_student_id() {
        return input_.integer("ID студента > ");
    }

    CriterionId read_criterion_id() {
        return input_.integer("ID критерия > ");
    }

    void write_evaluation(EvaluationWrite action, std::string_view confirmation) {
        const auto student = read_student_id();
        const auto criterion = read_criterion_id();
        const auto score = input_.number("Балл > ");
        (facade_.*action)(student, criterion, score);
        output_ << confirmation << '\n';
    }

    PosFacade& facade_;
    ConsoleInput& input_;
    std::ostream& output_;
};

struct Command {
    std::string_view label;
    std::function<void()> execute;
};

}

void run_console(PosFacade& facade, std::istream& input, std::ostream& output) {
    ConsoleInput reader(input, output);
    ConsoleActions actions(facade, reader, output);
    bool running = true;
    const std::array<Command, 12> commands{{
        {"Выход", [&running] { running = false; }},
        {"Добавить студента", [&actions] { actions.add_student(); }},
        {"Список студентов", [&actions] { actions.list_students(); }},
        {"Добавить критерий", [&actions] { actions.add_criterion(); }},
        {"Рубрикатор", [&actions] { actions.list_criteria(); }},
        {"Выставить оценку", [&actions] { actions.assign_evaluation(); }},
        {"Оценки студента", [&actions] { actions.list_evaluations(); }},
        {"Исправить оценку", [&actions] { actions.correct_evaluation(); }},
        {"Отменить оценку", [&actions] { actions.cancel_evaluation(); }},
        {"Итог студента", [&actions] { actions.final_result(); }},
        {"Сводка по курсу", [&actions] { actions.course_summary(); }},
        {"Экспорт итогов в CSV", [&actions] { actions.export_results(); }},
    }};

    output << "ПОС — преподавательская оценка студентов\n";
    while (running) {
        output << '\n';
        for (std::size_t index = 0; index < commands.size(); ++index) {
            output << index << ". " << commands[index].label << '\n';
        }

        try {
            const auto choice = reader.integer("Действие > ");
            if (choice >= commands.size()) {
                throw std::invalid_argument("Выберите номер из меню.");
            }
            commands[static_cast<std::size_t>(choice)].execute();
        } catch (const EndOfInput&) {
            output << '\n';
            break;
        } catch (const std::exception& error) {
            output << "Ошибка — " << error.what() << '\n';
        }
    }
    output << "Работа завершена.\n";
}

}
