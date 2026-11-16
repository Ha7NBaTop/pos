#pragma once

#include "pos/domain.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pos::application {

class StudentRepository {
public:
    virtual ~StudentRepository() = default;
    virtual domain::StudentId next_id() const = 0;
    virtual void add(domain::Student student) = 0;
    virtual bool contains(domain::StudentId id) const = 0;
    virtual std::vector<domain::Student> list() const = 0;
};

class RubricRepository {
public:
    virtual ~RubricRepository() = default;
    virtual domain::Rubric load() const = 0;
    virtual void save(domain::Rubric rubric) = 0;
};

class EvaluationRepository {
public:
    virtual ~EvaluationRepository() = default;
    virtual std::vector<domain::Evaluation> list_for(domain::StudentId student_id) const = 0;
    virtual bool insert(domain::Evaluation evaluation) = 0;
    virtual bool replace(domain::Evaluation evaluation) = 0;
    virtual bool erase(domain::StudentId student_id, domain::CriterionId criterion_id) = 0;
};

struct ReportRow {
    domain::StudentId student_id;
    std::string student_name;
    domain::FinalResult result;
};

struct CourseSummary {
    std::size_t total_students;
    std::size_t complete_students;
    std::optional<double> average_percent;
};

class ResultsExporter {
public:
    virtual ~ResultsExporter() = default;
    virtual void write(const std::filesystem::path& path, std::span<const ReportRow> rows) const = 0;
};

class PosFacade {
public:
    PosFacade(StudentRepository& students, RubricRepository& rubrics,
              EvaluationRepository& evaluations, const ResultsExporter& exporter);

    domain::StudentId add_student(std::string name);
    std::vector<domain::Student> list_students() const;
    domain::CriterionId add_criterion(std::string name, double max_score, double weight);
    std::vector<domain::Criterion> list_criteria() const;

    void assign_evaluation(domain::StudentId student_id, domain::CriterionId criterion_id, double score);
    void correct_evaluation(domain::StudentId student_id, domain::CriterionId criterion_id, double score);
    void cancel_evaluation(domain::StudentId student_id, domain::CriterionId criterion_id);
    std::vector<domain::Evaluation> list_evaluations(domain::StudentId student_id) const;

    domain::FinalResult final_result(domain::StudentId student_id) const;
    CourseSummary course_summary() const;
    void export_results(const std::filesystem::path& path) const;

private:
    void require_student(domain::StudentId student_id) const;
    domain::Evaluation make_evaluation(domain::StudentId student_id, domain::CriterionId criterion_id,
                                       double score) const;
    domain::FinalResult calculate_result(domain::StudentId student_id, const domain::Rubric& rubric) const;
    std::vector<ReportRow> report_rows() const;

    StudentRepository& students_;
    RubricRepository& rubrics_;
    EvaluationRepository& evaluations_;
    const ResultsExporter& exporter_;
    domain::EvaluationFactory evaluation_factory_;
    domain::FinalGradeService final_grade_service_;
};

} // namespace pos::application
