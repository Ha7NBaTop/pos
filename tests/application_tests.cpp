#include "pos/adapters.hpp"
#include "pos/application.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace application = pos::application;
namespace adapters = pos::adapters;
namespace domain = pos::domain;

class CapturingExporter final : public application::ResultsExporter {
public:
    void write(const std::filesystem::path& path, std::span<const application::ReportRow> rows) const override
    {
        destination = path;
        report.assign(rows.begin(), rows.end());
    }

    mutable std::filesystem::path destination;
    mutable std::vector<application::ReportRow> report;
};

struct Fixture {
    adapters::InMemoryStudentRepository students;
    adapters::InMemoryRubricRepository rubric;
    adapters::InMemoryEvaluationRepository evaluations;
    CapturingExporter exporter;
    application::PosFacade facade{students, rubric, evaluations, exporter};
};

void check(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void check_percent(std::optional<double> actual, double expected, const std::string& message)
{
    check(actual.has_value() && std::abs(*actual - expected) < 1e-10, message);
}

template <typename Operation>
void check_rejected(Operation operation, const std::string& message)
{
    try {
        operation();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message);
}

void check_evaluation_lifecycle()
{
    Fixture fixture;
    auto& facade = fixture.facade;
    const auto student = facade.add_student("Анна");
    const auto assignment = facade.add_criterion("Практика", 10, 2);
    const auto exam = facade.add_criterion("Экзамен", 20, 1);

    check_rejected([&] { facade.assign_evaluation(student + 1, assignment, 6); }, "Unknown student accepted");
    check_rejected([&] { facade.assign_evaluation(student, exam + 1, 6); }, "Unknown criterion accepted");
    check_rejected([&] { facade.correct_evaluation(student, assignment, 6); }, "Correction created an evaluation");
    check(facade.list_evaluations(student).empty(), "Invalid actions changed evaluations");

    facade.assign_evaluation(student, assignment, 8);
    check_rejected([&] { facade.assign_evaluation(student, assignment, 5); }, "Duplicate evaluation accepted");
    check_rejected([&] { facade.correct_evaluation(student, assignment, 11); }, "Invalid correction accepted");
    const auto evaluations = facade.list_evaluations(student);
    check(evaluations.size() == 1 && evaluations.front().score().value() == 8, "Rejected action changed score");

    facade.assign_evaluation(student, exam, 10);
    check_percent(facade.final_result(student).percent, 70, "Unexpected weighted result");
    facade.correct_evaluation(student, assignment, 10);
    check_percent(facade.final_result(student).percent, 100.0 * 2.5 / 3.0, "Result ignored corrected score");

    facade.cancel_evaluation(student, assignment);
    auto result = facade.final_result(student);
    check(result.status == domain::ResultStatus::missing_evaluations && !result.percent,
          "Canceled score still counted");
    check_rejected([&] { facade.cancel_evaluation(student, assignment); }, "Repeated cancellation succeeded");

    facade.assign_evaluation(student, assignment, 8);
    check_percent(facade.final_result(student).percent, 70, "Failed to assign again after cancellation");

    facade.add_criterion("Проект", 5, 1);
    result = facade.final_result(student);
    check(result.status == domain::ResultStatus::missing_evaluations && !result.percent,
          "New criterion did not affect completeness");
}

void check_summary_and_export()
{
    Fixture fixture;
    auto& facade = fixture.facade;
    const auto anna = facade.add_student("Анна");
    const auto alex = facade.add_student("Алексей");
    const auto vera = facade.add_student("Вера");
    check(facade.list_students().size() == 3, "Student list is incomplete");
    const auto empty_summary = facade.course_summary();
    check(empty_summary.total_students == 3 && empty_summary.complete_students == 0
              && !empty_summary.average_percent,
          "Empty rubric produced a course average");

    const auto criterion = facade.add_criterion("Проект", 20, 1);
    check(facade.list_criteria().size() == 1, "Criterion list is incomplete");
    facade.assign_evaluation(anna, criterion, 14);
    facade.assign_evaluation(alex, criterion, 20);

    const auto summary = facade.course_summary();
    check(summary.total_students == 3 && summary.complete_students == 2, "Unexpected course counts");
    check_percent(summary.average_percent, 85, "Unexpected course average");

    const std::filesystem::path path = "results.csv";
    facade.export_results(path);
    check(fixture.exporter.destination == path, "Export destination was lost");
    const auto& report = fixture.exporter.report;
    check(report.size() == 3, "Export did not include every student");

    const auto completed = std::ranges::find(report, anna, &application::ReportRow::student_id);
    check(completed != report.end() && completed->student_name == "Анна"
              && completed->result.status == domain::ResultStatus::complete,
          "Export is missing a completed result");
    check_percent(completed->result.percent, 70, "Unexpected exported result");

    const auto incomplete = std::ranges::find(report, vera, &application::ReportRow::student_id);
    check(incomplete != report.end() && incomplete->result.status == domain::ResultStatus::missing_evaluations
              && !incomplete->result.percent,
          "Export did not preserve an incomplete result");
}

} // namespace

int main()
{
    try {
        check_evaluation_lifecycle();
        check_summary_and_export();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
