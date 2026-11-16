#include "pos/adapters.hpp"

#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unistd.h>

namespace pos::adapters {
namespace {

std::string_view csv_status(domain::ResultStatus status) {
    switch (status) {
        case domain::ResultStatus::complete:
            return "complete";
        case domain::ResultStatus::empty_rubric:
            return "empty_rubric";
        case domain::ResultStatus::missing_evaluations:
            return "missing_evaluations";
    }
    throw std::logic_error("Неизвестный статус итога.");
}

void write_csv_field(std::ostream& output, std::string_view field) {
    if (field.find_first_of(",\"\r\n") == std::string_view::npos) {
        output << field;
        return;
    }

    output << '"';
    for (const char character : field) {
        if (character == '"') {
            output << '"';
        }
        output << character;
    }
    output << '"';
}

void write_csv(std::ostream& output, std::span<const application::ReportRow> rows) {
    output.imbue(std::locale::classic());
    output << "student_id,student_name,status,final_percent\r\n";
    output << std::fixed << std::setprecision(2);
    for (const auto& row : rows) {
        output << row.student_id << ',';
        write_csv_field(output, row.student_name);
        output << ',' << csv_status(row.result.status) << ',';
        if (row.result.percent) {
            output << *row.result.percent;
        }
        output << "\r\n";
    }
}

}  // namespace

domain::StudentId InMemoryStudentRepository::next_id() const {
    if (students_.empty()) {
        return 1;
    }
    const auto last_id = students_.rbegin()->first;
    if (last_id == std::numeric_limits<domain::StudentId>::max()) {
        throw std::overflow_error("Достигнут предел идентификаторов студентов.");
    }
    return last_id + 1;
}

void InMemoryStudentRepository::add(domain::Student student) {
    const auto id = student.id();
    if (!students_.try_emplace(id, std::move(student)).second) {
        throw std::invalid_argument("Студент с таким идентификатором уже существует.");
    }
}

bool InMemoryStudentRepository::contains(domain::StudentId id) const {
    return students_.contains(id);
}

std::vector<domain::Student> InMemoryStudentRepository::list() const {
    std::vector<domain::Student> students;
    students.reserve(students_.size());
    for (const auto& [id, student] : students_) {
        students.push_back(student);
    }
    return students;
}

domain::Rubric InMemoryRubricRepository::load() const {
    return rubric_;
}

void InMemoryRubricRepository::save(domain::Rubric rubric) {
    rubric_ = std::move(rubric);
}

std::vector<domain::Evaluation> InMemoryEvaluationRepository::list_for(
    domain::StudentId student_id) const {
    std::vector<domain::Evaluation> evaluations;
    auto entry = evaluations_.lower_bound({student_id, 0});
    while (entry != evaluations_.end() && entry->first.first == student_id) {
        evaluations.push_back(entry->second);
        ++entry;
    }
    return evaluations;
}

bool InMemoryEvaluationRepository::insert(domain::Evaluation evaluation) {
    const Key key{evaluation.student_id(), evaluation.criterion_id()};
    return evaluations_.try_emplace(key, std::move(evaluation)).second;
}

bool InMemoryEvaluationRepository::replace(domain::Evaluation evaluation) {
    const Key key{evaluation.student_id(), evaluation.criterion_id()};
    const auto entry = evaluations_.find(key);
    if (entry == evaluations_.end()) {
        return false;
    }
    entry->second = std::move(evaluation);
    return true;
}

bool InMemoryEvaluationRepository::erase(domain::StudentId student_id,
                                         domain::CriterionId criterion_id) {
    return evaluations_.erase({student_id, criterion_id}) != 0;
}

void CsvResultsExporter::write(const std::filesystem::path& path,
                               std::span<const application::ReportRow> rows) const {
    if (path.empty() || path.native().find('\0') != std::string::npos) {
        throw std::invalid_argument("Укажи путь к файлу CSV.");
    }
    const auto target = std::filesystem::symlink_status(path);
    if (std::filesystem::exists(target) && !std::filesystem::is_regular_file(target)) {
        throw std::runtime_error("Путь для экспорта должен вести к обычному файлу.");
    }

    std::ostringstream csv;
    write_csv(csv, rows);
    if (!csv) {
        throw std::runtime_error("Не удалось сформировать ведомость CSV.");
    }
    const auto contents = std::move(csv).str();

    // Переименование в той же папке публикует CSV только после записи целиком.
    auto temporary = (path.parent_path() / ".pos-export-XXXXXX").string();
    const int descriptor = ::mkstemp(temporary.data());
    if (descriptor == -1) {
        throw std::runtime_error("Не удалось открыть файл для экспорта CSV: " + path.string());
    }
    auto* output = ::fdopen(descriptor, "wb");
    if (output == nullptr) {
        ::close(descriptor);
        std::remove(temporary.c_str());
        throw std::runtime_error("Не удалось открыть файл для экспорта CSV: " + path.string());
    }

    const auto bytes_written = std::fwrite(contents.data(), 1, contents.size(), output);
    const int close_status = std::fclose(output);
    if (bytes_written != contents.size() || close_status != 0 ||
        std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        throw std::runtime_error("Не удалось записать ведомость CSV: " + path.string());
    }
}

}  // namespace pos::adapters
