#include "ProcessRunner.hpp"

#include <QProcess>
#include <QProcessEnvironment>

namespace ac6dm::platform {

ProcessReport ProcessRunner::run(
    const std::string& program,
    const std::vector<std::string>& arguments,
    const std::optional<std::filesystem::path>& workingDirectory,
    const std::map<std::string, std::string>& environment,
    int timeoutMs) const {
    QProcess process;
    if (workingDirectory.has_value()) {
        process.setWorkingDirectory(QString::fromStdWString(workingDirectory->wstring()));
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const auto& [key, value] : environment) {
        env.insert(QString::fromStdString(key), QString::fromStdString(value));
    }
    process.setProcessEnvironment(env);

    QStringList argList;
    ProcessReport report;
    report.command.push_back(program);
    for (const auto& argument : arguments) {
        argList.push_back(QString::fromStdString(argument));
        report.command.push_back(argument);
    }

    process.start(QString::fromStdString(program), argList);
    if (!process.waitForStarted()) {
        report.stderrText = "process failed to start";
        return report;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished();
        report.stderrText = "process timed out";
        return report;
    }

    report.returnCode = process.exitCode();
    report.stdoutText = QString::fromUtf8(process.readAllStandardOutput()).toStdString();
    report.stderrText = QString::fromUtf8(process.readAllStandardError()).toStdString();
    return report;
}

}  // namespace ac6dm::platform
