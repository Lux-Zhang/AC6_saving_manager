#pragma once

#include <QString>

class QWidget;

namespace ac6dm::app {

QString chooseOpenFileWithHiddenItems(
    QWidget* parent,
    const QString& title,
    const QString& filter);

QString chooseSaveFileWithHiddenItems(
    QWidget* parent,
    const QString& title,
    const QString& suggestedName,
    const QString& filter);

}  // namespace ac6dm::app
