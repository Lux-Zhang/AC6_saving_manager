#pragma once

#include "ac6dm/contracts/emblem_workflow.hpp"

#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

#include <array>
#include <optional>
#include <vector>

namespace ac6dm::app {

class HomeLibraryView final : public QWidget {
    Q_OBJECT

public:
    explicit HomeLibraryView(QWidget* parent = nullptr);

    void setCatalog(const std::vector<contracts::CatalogItemDto>& items);
    void setAcCatalog(const std::vector<contracts::CatalogItemDto>& items);
    void setSessionStatus(const contracts::SessionStatusDto& status);
    void setLastActionSummary(const QString& summary, bool isError = false);
    void setInlineStatus(const QString& summary, bool isError, bool canShowDetails);
    void clearInlineStatus();
    std::optional<contracts::CatalogItemDto> selectedItem() const;
    contracts::AssetKind currentLibraryAssetKind() const;
    void setCurrentLibrary(contracts::AssetKind assetKind);
    bool selectVisibleRow(contracts::AssetKind assetKind, int row);

signals:
    void openSaveRequested();
    void primaryActionRequested();
    void importFileRequested();
    void exportSelectedRequested();
    void inlineDetailsRequested();
    void createBuildLinkRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void handleSelectionChanged();
    void handleFilterChanged(const QString& text);
    void handleCurrentTabChanged(int index);
    void handleSourceFilterChanged(int index);

private:
    enum class LibraryKind {
        Emblem,
        Ac,
    };

    struct LibraryWidgets final {
        struct PreviewRowWidgets final {
            QWidget* container{nullptr};
            QLabel* iconLabel{nullptr};
            QLabel* slotLabel{nullptr};
            QLabel* partLabel{nullptr};
            QLabel* manufacturerLogoLabel{nullptr};
        };

        QLineEdit* filterEdit{nullptr};
        QComboBox* sourceFilterCombo{nullptr};
        QTableWidget* table{nullptr};
        QLabel* emptyStateLabel{nullptr};
        QLabel* inspectorTitleLabel{nullptr};
        QLabel* inspectorPlaceholderLabel{nullptr};
        QGroupBox* identityGroup{nullptr};
        QLabel* identityField1Label{nullptr};
        QLabel* identityField1Value{nullptr};
        QLabel* identityField2Label{nullptr};
        QLabel* identityField2Value{nullptr};
        QLabel* identityField3Label{nullptr};
        QLabel* identityField3Value{nullptr};
        QGroupBox* locationGroup{nullptr};
        QLabel* locationOriginValue{nullptr};
        QLabel* locationSlotValue{nullptr};
        QGroupBox* statusGroup{nullptr};
        QLabel* statusBadgeLabel{nullptr};
        QLabel* statusNoteLabel{nullptr};
        QLabel* previewValueLabel{nullptr};
        QWidget* previewPanel{nullptr};
        QLabel* previewEmptyStateLabel{nullptr};
        QLabel* previewNoteLabel{nullptr};
        QPushButton* createBuildLinkButton{nullptr};
        QScrollArea* previewScrollArea{nullptr};
        QWidget* previewScrollContent{nullptr};
        std::array<QGroupBox*, 4> previewGroups{};
        std::array<PreviewRowWidgets, 12> previewRows{};
        QPushButton* primaryActionButton{nullptr};
        QPushButton* importFileButton{nullptr};
        QPushButton* exportButton{nullptr};
        QLabel* helperTextLabel{nullptr};
        QString emptySelectionText;
        QString selectionPromptText;
        QString helperText;
        std::vector<contracts::CatalogItemDto> allItems{};
    };

    LibraryWidgets& widgetsFor(LibraryKind kind);
    const LibraryWidgets& widgetsFor(LibraryKind kind) const;
    LibraryKind currentLibraryKind() const;
    void initializeTab(LibraryKind kind, QWidget* page, const QString& actionText, const QString& fileImportText,
        const QString& exportText, const QString& emptySelectionText, const QString& selectionPromptText,
        const QString& helperText, const QString& tableObjectName, const QString& filterObjectName,
        const QString& filterComboObjectName, const QString& primaryButtonObjectName,
        const QString& importButtonObjectName, const QString& exportButtonObjectName,
        const QString& helperTextObjectName);
    void configureInspector(LibraryKind kind, QWidget* parent, LibraryWidgets& widgets);
    void configureAcPreviewPanel(QWidget* parent, LibraryWidgets& widgets);
    void setLibraryCatalog(LibraryKind kind, const std::vector<contracts::CatalogItemDto>& items);
    void refreshVisibleRows(LibraryKind kind);
    void showItemDetails(LibraryKind kind, const std::optional<contracts::CatalogItemDto>& item);
    void showAcPreview(const std::optional<contracts::CatalogItemDto>& item);
    std::optional<contracts::CatalogItemDto> selectedItem(LibraryKind kind) const;
    void updateActionState(LibraryKind kind, const std::optional<contracts::CatalogItemDto>& item);
    bool itemMatchesSourceFilter(LibraryKind kind, const contracts::CatalogItemDto& item) const;
    QString sourceBucketLabel(const contracts::CatalogItemDto& item) const;
    QString writeCapabilityLabel(const contracts::CatalogItemDto& item) const;
    QString emblemNameCodeText(const contracts::CatalogItemDto& item) const;
    QString archiveNameText(const contracts::CatalogItemDto& item) const;
    QString machineNameText(const contracts::CatalogItemDto& item) const;
    QString shareCodeText(const contracts::CatalogItemDto& item) const;
    QString slotText(const contracts::CatalogItemDto& item) const;
    QString statusNoteText(const contracts::CatalogItemDto& item) const;
    QString detailTitleFor(LibraryKind kind, const contracts::CatalogItemDto& item) const;
    QString previewPartText(const contracts::AcAssemblySlotDto& slot) const;
    QString previewManufacturerResource(const std::string& manufacturer) const;
    QString sessionStateText() const;
    void updateSessionCard();
    void updateStatusBadge(QLabel* label, const QString& text, const QString& state);
    void updateInlineStatusVisibility();

    QFrame* sessionCard_{nullptr};
    QLabel* sessionStateValueLabel_{nullptr};
    QLabel* currentSaveValueLabel_{nullptr};
    QLabel* lastActionValueLabel_{nullptr};
    QPushButton* openSaveButton_{nullptr};
    QTabWidget* libraryTabs_{nullptr};
    QFrame* inlineStatusFrame_{nullptr};
    QLabel* inlineStatusLabel_{nullptr};
    QPushButton* inlineStatusDetailsButton_{nullptr};
    QPushButton* inlineStatusDismissButton_{nullptr};
    bool inlineStatusIsError_{false};
    bool inlineStatusCanShowDetails_{false};
    bool sessionHasRealSave_{false};
    std::optional<std::filesystem::path> sourceSavePath_{};
    QString lastActionSummary_{QStringLiteral("No recent action.")};
    LibraryWidgets emblemWidgets_{};
    LibraryWidgets acWidgets_{};
};

}  // namespace ac6dm::app
