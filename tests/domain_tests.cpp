#include "pos/domain.hpp"

#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace pos::domain;

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        throw std::runtime_error(std::string{message});
    }
}

template <typename Action>
void expect_invalid(Action action, std::string_view message)
{
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(std::string{message});
}

void expect_percent(const FinalResult& result, double expected, std::string_view message)
{
    expect(result.status == ResultStatus::complete &&
               result.percent.has_value() &&
               std::abs(*result.percent - expected) < 1e-10,
           message);
}

void test_rubric_and_student_invariants()
{
    expect_invalid([] { Student{0, "Анна"}; }, "Student ID zero must be rejected");
    expect_invalid([] { Student{1, " \t"}; }, "Blank student names must be rejected");
    expect_invalid([] { Criterion{0, "Практика", 10, 1}; }, "Criterion ID zero must be rejected");
    expect_invalid([] { Criterion{1, "", 10, 1}; }, "Blank criterion names must be rejected");

    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()}) {
        expect_invalid([invalid] { Criterion{1, "Практика", invalid, 1}; },
                       "Criterion maximum must be finite and positive");
        expect_invalid([invalid] { Criterion{1, "Практика", 10, invalid}; },
                       "Criterion weight must be finite and positive");
    }

    Rubric rubric;
    rubric.add(Criterion{1, "Практика", 10, 1});
    expect_invalid([&] { rubric.add(Criterion{1, "Проект", 20, 2}); },
                   "A rubric must reject a duplicate criterion ID");
    expect(rubric.criteria().size() == 1 && rubric.find(1)->name() == "Практика",
           "Rejecting a duplicate must preserve the original criterion");
}

void test_evaluation_boundaries_and_membership()
{
    Rubric rubric;
    rubric.add(Criterion{1, "Практика", 10, 1});
    const EvaluationFactory factory;

    const auto zero = factory.create(7, 1, 0, rubric);
    const auto maximum = factory.create(7, 1, 10, rubric);
    expect(zero.score().value() == 0 && maximum.score().value() == 10,
           "Score should accept both boundaries");
    expect(maximum.student_id() == 7 && maximum.criterion_id() == 1,
           "Evaluation should carry the requested student and criterion IDs");

    for (double invalid : {-0.1, 10.1, std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()}) {
        expect_invalid([&] { (void)factory.create(7, 1, invalid, rubric); },
                       "Score must be finite and within the criterion limits");
    }

    expect_invalid([&] { (void)factory.create(7, 2, 0, rubric); },
                   "Evaluation for a criterion missing from the rubric must be rejected");
    expect_invalid([&] { (void)factory.create(0, 1, 0, rubric); },
                   "Evaluation with student ID zero must be rejected");
}

void test_calculation_uses_current_evaluations()
{
    const FinalGradeService service;
    const EvaluationFactory factory;
    Rubric rubric;
    const auto empty = service.calculate(7, rubric, {});
    expect(empty.status == ResultStatus::empty_rubric && !empty.percent,
           "An empty rubric should have its own status and no percent");

    rubric.add(Criterion{1, "Практика", 10, 2});
    rubric.add(Criterion{2, "Проект", 20, 1});
    std::vector evaluations{
        factory.create(7, 1, 8, rubric),
        factory.create(8, 2, 20, rubric)
    };
    const auto incomplete = service.calculate(7, rubric, evaluations);
    expect(incomplete.status == ResultStatus::missing_evaluations && !incomplete.percent,
           "An evaluation for another student must not fill a missing score");

    evaluations.push_back(factory.create(7, 2, 10, rubric));
    expect_percent(service.calculate(7, rubric, evaluations), 70,
                   "Different maxima must be normalized before applying weights");
    evaluations.back() = factory.create(7, 2, 16, rubric);
    expect_percent(service.calculate(7, rubric, evaluations), 80,
                   "Correcting an evaluation must update the final result");
    evaluations.pop_back();
    const auto cancelled = service.calculate(7, rubric, evaluations);
    expect(cancelled.status == ResultStatus::missing_evaluations && !cancelled.percent,
           "Cancelling an evaluation must return the result to incomplete");
}

void test_large_finite_weights_and_scores()
{
    const auto maximum = std::numeric_limits<double>::max();
    Rubric rubric;
    rubric.add(Criterion{1, "Практика", maximum, maximum});
    rubric.add(Criterion{2, "Проект", maximum, maximum});

    const EvaluationFactory factory;
    const std::vector evaluations{
        factory.create(1, 1, maximum, rubric),
        factory.create(1, 2, maximum / 2, rubric)
    };
    expect_percent(FinalGradeService{}.calculate(1, rubric, evaluations), 75,
                   "The result must stay finite for large finite scores and weights");
}

}  // namespace

int main()
{
    try {
        test_rubric_and_student_invariants();
        test_evaluation_boundaries_and_membership();
        test_calculation_uses_current_evaluations();
        test_large_finite_weights_and_scores();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
