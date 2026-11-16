#include "pos/domain.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace pos::domain {
namespace {

bool is_blank(std::string_view text)
{
    return std::all_of(text.begin(), text.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

}  // namespace

Student::Student(StudentId id, std::string name)
    : id_{id}, name_{std::move(name)}
{
    if (id_ == 0) {
        throw std::invalid_argument("Идентификатор студента должен быть положительным.");
    }
    if (is_blank(name_)) {
        throw std::invalid_argument("Введите имя студента.");
    }
}

StudentId Student::id() const noexcept
{
    return id_;
}

const std::string& Student::name() const noexcept
{
    return name_;
}

Criterion::Criterion(CriterionId id, std::string name, double max_score, double weight)
    : id_{id}, name_{std::move(name)}, max_score_{max_score}, weight_{weight}
{
    if (id_ == 0) {
        throw std::invalid_argument("Идентификатор критерия должен быть положительным.");
    }
    if (is_blank(name_)) {
        throw std::invalid_argument("Введите название критерия.");
    }
    if (!std::isfinite(max_score_) || max_score_ <= 0) {
        throw std::invalid_argument("Максимальный балл должен быть конечным положительным числом.");
    }
    if (!std::isfinite(weight_) || weight_ <= 0) {
        throw std::invalid_argument("Вес критерия должен быть конечным положительным числом.");
    }
}

CriterionId Criterion::id() const noexcept
{
    return id_;
}

const std::string& Criterion::name() const noexcept
{
    return name_;
}

double Criterion::max_score() const noexcept
{
    return max_score_;
}

double Criterion::weight() const noexcept
{
    return weight_;
}

void Rubric::add(Criterion criterion)
{
    if (find(criterion.id()) != nullptr) {
        throw std::invalid_argument("Критерий с таким идентификатором уже есть в рубрикаторе.");
    }
    criteria_.push_back(std::move(criterion));
}

const std::vector<Criterion>& Rubric::criteria() const noexcept
{
    return criteria_;
}

const Criterion* Rubric::find(CriterionId id) const noexcept
{
    const auto criterion = std::find_if(criteria_.begin(), criteria_.end(), [id](const Criterion& item) {
        return item.id() == id;
    });
    return criterion == criteria_.end() ? nullptr : &*criterion;
}

Score::Score(double value, double max_score)
    : value_{value}
{
    if (!std::isfinite(value_) || value_ < 0 || value_ > max_score) {
        throw std::invalid_argument("Введите конечный балл от нуля до максимума критерия включительно.");
    }
}

double Score::value() const noexcept
{
    return value_;
}

Evaluation::Evaluation(StudentId student_id, CriterionId criterion_id, Score score)
    : student_id_{student_id}, criterion_id_{criterion_id}, score_{score}
{
}

StudentId Evaluation::student_id() const noexcept
{
    return student_id_;
}

CriterionId Evaluation::criterion_id() const noexcept
{
    return criterion_id_;
}

const Score& Evaluation::score() const noexcept
{
    return score_;
}

Evaluation EvaluationFactory::create(
    StudentId student_id, CriterionId criterion_id, double score, const Rubric& rubric) const
{
    if (student_id == 0) {
        throw std::invalid_argument("Идентификатор студента должен быть положительным.");
    }
    const auto* criterion = rubric.find(criterion_id);
    if (criterion == nullptr) {
        throw std::invalid_argument("Критерий отсутствует в текущем рубрикаторе.");
    }
    return Evaluation{student_id, criterion_id, Score{score, criterion->max_score()}};
}

FinalResult FinalGradeService::calculate(
    StudentId student_id, const Rubric& rubric, std::span<const Evaluation> evaluations) const
{
    const auto& criteria = rubric.criteria();
    if (criteria.empty()) {
        return {ResultStatus::empty_rubric, std::nullopt};
    }

    std::unordered_map<CriterionId, double> scores;
    for (const auto& evaluation : evaluations) {
        if (evaluation.student_id() == student_id) {
            scores.emplace(evaluation.criterion_id(), evaluation.score().value());
        }
    }

    const auto largest_weight = std::max_element(
        criteria.begin(), criteria.end(), [](const Criterion& left, const Criterion& right) {
            return left.weight() < right.weight();
        })->weight();

    long double weighted_sum = 0;
    long double weight_sum = 0;
    for (const auto& criterion : criteria) {
        const auto score = scores.find(criterion.id());
        if (score == scores.end()) {
            return {ResultStatus::missing_evaluations, std::nullopt};
        }

        // Scaling the weights keeps the sum finite even when several weights are near DBL_MAX.
        const auto weight = static_cast<long double>(criterion.weight()) / largest_weight;
        const auto fraction = static_cast<long double>(score->second) / criterion.max_score();
        weighted_sum += weight * fraction;
        weight_sum += weight;
    }

    const auto percent = static_cast<double>(100 * (weighted_sum / weight_sum));
    return {ResultStatus::complete, percent};
}

}  // namespace pos::domain
