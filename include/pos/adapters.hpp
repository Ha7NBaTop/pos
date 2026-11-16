#pragma once

#include "pos/application.hpp"

#include <map>
#include <utility>

namespace pos::adapters {

class InMemoryStudentRepository final : public application::StudentRepository {
public:
    domain::StudentId next_id() const override;
    void add(domain::Student student) override;
    bool contains(domain::StudentId id) const override;
    std::vector<domain::Student> list() const override;

private:
    std::map<domain::StudentId, domain::Student> students_;
};

class InMemoryRubricRepository final : public application::RubricRepository {
public:
    domain::Rubric load() const override;
    void save(domain::Rubric rubric) override;

private:
    domain::Rubric rubric_;
};

class InMemoryEvaluationRepository final : public application::EvaluationRepository {
public:
    std::vector<domain::Evaluation> list_for(domain::StudentId student_id) const override;
    bool insert(domain::Evaluation evaluation) override;
    bool replace(domain::Evaluation evaluation) override;
    bool erase(domain::StudentId student_id, domain::CriterionId criterion_id) override;

private:
    using Key = std::pair<domain::StudentId, domain::CriterionId>;
    std::map<Key, domain::Evaluation> evaluations_;
};

class CsvResultsExporter final : public application::ResultsExporter {
public:
    void write(const std::filesystem::path& path,
               std::span<const application::ReportRow> rows) const override;
};

}  // namespace pos::adapters
