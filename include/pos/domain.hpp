#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pos::domain {

using StudentId = std::uint64_t;
using CriterionId = std::uint64_t;

class Student {
public:
    Student(StudentId id, std::string name);

    [[nodiscard]] StudentId id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;

private:
    StudentId id_;
    std::string name_;
};

class Criterion {
public:
    Criterion(CriterionId id, std::string name, double max_score, double weight);

    [[nodiscard]] CriterionId id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] double max_score() const noexcept;
    [[nodiscard]] double weight() const noexcept;

private:
    CriterionId id_;
    std::string name_;
    double max_score_;
    double weight_;
};

class Rubric {
public:
    void add(Criterion criterion);

    [[nodiscard]] const std::vector<Criterion>& criteria() const noexcept;
    [[nodiscard]] const Criterion* find(CriterionId id) const noexcept;

private:
    std::vector<Criterion> criteria_;
};

class Score {
public:
    [[nodiscard]] double value() const noexcept;

private:
    friend class EvaluationFactory;
    Score(double value, double max_score);

    double value_;
};

class Evaluation {
public:
    [[nodiscard]] StudentId student_id() const noexcept;
    [[nodiscard]] CriterionId criterion_id() const noexcept;
    [[nodiscard]] const Score& score() const noexcept;

private:
    friend class EvaluationFactory;
    Evaluation(StudentId student_id, CriterionId criterion_id, Score score);

    StudentId student_id_;
    CriterionId criterion_id_;
    Score score_;
};

class EvaluationFactory {
public:
    [[nodiscard]] Evaluation create(
        StudentId student_id, CriterionId criterion_id, double score, const Rubric& rubric) const;
};

enum class ResultStatus {
    complete,
    empty_rubric,
    missing_evaluations
};

struct FinalResult {
    ResultStatus status;
    std::optional<double> percent;
};

class FinalGradeService {
public:
    [[nodiscard]] FinalResult calculate(
        StudentId student_id, const Rubric& rubric, std::span<const Evaluation> evaluations) const;
};

}  // namespace pos::domain
