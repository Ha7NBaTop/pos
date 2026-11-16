#include "pos/application.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pos::application {

PosFacade::PosFacade(StudentRepository& students, RubricRepository& rubrics,
                     EvaluationRepository& evaluations, const ResultsExporter& exporter)
    : students_(students), rubrics_(rubrics), evaluations_(evaluations), exporter_(exporter)
{
}

domain::StudentId PosFacade::add_student(std::string name)
{
    const auto id = students_.next_id();
    students_.add(domain::Student{id, std::move(name)});
    return id;
}

std::vector<domain::Student> PosFacade::list_students() const
{
    return students_.list();
}

domain::CriterionId PosFacade::add_criterion(std::string name, double max_score, double weight)
{
    auto rubric = rubrics_.load();
    domain::CriterionId last_id = 0;
    for (const auto& criterion : rubric.criteria()) {
        last_id = std::max(last_id, criterion.id());
    }
    if (last_id == std::numeric_limits<domain::CriterionId>::max()) {
        throw std::overflow_error("Исчерпаны идентификаторы критериев.");
    }

    const auto id = last_id + 1;
    rubric.add(domain::Criterion{id, std::move(name), max_score, weight});
    rubrics_.save(std::move(rubric));
    return id;
}

std::vector<domain::Criterion> PosFacade::list_criteria() const
{
    const auto rubric = rubrics_.load();
    return rubric.criteria();
}

void PosFacade::assign_evaluation(domain::StudentId student_id, domain::CriterionId criterion_id, double score)
{
    if (!evaluations_.insert(make_evaluation(student_id, criterion_id, score))) {
        throw std::invalid_argument("Оценка уже выставлена. Используйте команду «Исправить оценку».");
    }
}

void PosFacade::correct_evaluation(domain::StudentId student_id, domain::CriterionId criterion_id, double score)
{
    if (!evaluations_.replace(make_evaluation(student_id, criterion_id, score))) {
        throw std::invalid_argument("Оценка отсутствует. Используйте команду «Выставить оценку».");
    }
}

void PosFacade::cancel_evaluation(domain::StudentId student_id, domain::CriterionId criterion_id)
{
    require_student(student_id);
    if (!evaluations_.erase(student_id, criterion_id)) {
        throw std::invalid_argument("Оценка для этого студента и критерия отсутствует.");
    }
}

std::vector<domain::Evaluation> PosFacade::list_evaluations(domain::StudentId student_id) const
{
    require_student(student_id);
    return evaluations_.list_for(student_id);
}

domain::FinalResult PosFacade::final_result(domain::StudentId student_id) const
{
    require_student(student_id);
    return calculate_result(student_id, rubrics_.load());
}

CourseSummary PosFacade::course_summary() const
{
    const auto rows = report_rows();
    CourseSummary summary{rows.size(), 0, std::nullopt};
    long double sum = 0.0L;

    for (const auto& row : rows) {
        if (row.result.percent.has_value()) {
            ++summary.complete_students;
            sum += *row.result.percent;
        }
    }
    if (summary.complete_students != 0) {
        summary.average_percent = static_cast<double>(sum / static_cast<long double>(summary.complete_students));
    }
    return summary;
}

void PosFacade::export_results(const std::filesystem::path& path) const
{
    const auto rows = report_rows();
    exporter_.write(path, rows);
}

void PosFacade::require_student(domain::StudentId student_id) const
{
    if (!students_.contains(student_id)) {
        throw std::invalid_argument("Студент с указанным идентификатором не найден.");
    }
}

domain::Evaluation PosFacade::make_evaluation(domain::StudentId student_id, domain::CriterionId criterion_id,
                                             double score) const
{
    require_student(student_id);
    return evaluation_factory_.create(student_id, criterion_id, score, rubrics_.load());
}

domain::FinalResult PosFacade::calculate_result(domain::StudentId student_id, const domain::Rubric& rubric) const
{
    const auto evaluations = evaluations_.list_for(student_id);
    return final_grade_service_.calculate(student_id, rubric, evaluations);
}

std::vector<ReportRow> PosFacade::report_rows() const
{
    const auto students = students_.list();
    const auto rubric = rubrics_.load();
    std::vector<ReportRow> rows;
    rows.reserve(students.size());

    for (const auto& student : students) {
        rows.push_back({student.id(), student.name(), calculate_result(student.id(), rubric)});
    }
    return rows;
}

} // namespace pos::application
