#include "home_library_view.hpp"

#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QStyle>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ac6dm::app {
namespace {

constexpr auto kAllSourcesFilter = "all";

bool isShareOrigin(const contracts::CatalogItemDto& item) {
    return item.origin == contracts::AssetOrigin::Share;
}

QString placeholderText(const QString& value) {
    return value.trimmed().isEmpty() ? QObject::tr("-") : value;
}

QString twoDigitNumber(const int zeroBasedIndex) {
    return QStringLiteral("%1").arg(zeroBasedIndex + 1, 2, 10, QChar('0'));
}

QString sourceLabelForBucket(const contracts::SourceBucket bucket) {
    switch (bucket) {
    case contracts::SourceBucket::User1:
        return QObject::tr("User 1");
    case contracts::SourceBucket::User2:
        return QObject::tr("User 2");
    case contracts::SourceBucket::User3:
        return QObject::tr("User 3");
    case contracts::SourceBucket::User4:
        return QObject::tr("User 4");
    case contracts::SourceBucket::Share:
        return QObject::tr("Share");
    }
    return QObject::tr("Share");
}

QString statusLabelForCapability(const contracts::WriteCapability capability) {
    switch (capability) {
    case contracts::WriteCapability::Editable:
        return QObject::tr("Editable");
    case contracts::WriteCapability::ReadOnly:
        return QObject::tr("Read-only");
    case contracts::WriteCapability::LockedPendingGate:
        return QObject::tr("Blocked");
    }
    return QObject::tr("Read-only");
}

QString badgeStateForCapability(const contracts::WriteCapability capability) {
    switch (capability) {
    case contracts::WriteCapability::Editable:
        return QStringLiteral("editable");
    case contracts::WriteCapability::ReadOnly:
        return QStringLiteral("readonly");
    case contracts::WriteCapability::LockedPendingGate:
        return QStringLiteral("blocked");
    }
    return QStringLiteral("readonly");
}

int recordIndexFromRecordRef(const std::string& recordRef) {
    const auto marker = recordRef.find("record/");
    if (marker == std::string::npos) {
        return -1;
    }
    const auto start = marker + 7U;
    std::size_t end = start;
    while (end < recordRef.size() && std::isdigit(static_cast<unsigned char>(recordRef[end])) != 0) {
        ++end;
    }
    if (end == start) {
        return -1;
    }
    return std::stoi(recordRef.substr(start, end - start));
}

QTableWidgetItem* makeTableItem(const QString& text, const int logicalIndex) {
    auto* item = new QTableWidgetItem(text);
    item->setData(Qt::UserRole, logicalIndex);
    return item;
}

QString genericEmptyStateText(const bool hasRealSave, const bool hasItems, const bool hasVisibleItems) {
    if (!hasRealSave) {
        return QObject::tr("No save opened. Open a real .sl2 save to browse and manage assets.");
    }
    if (!hasItems) {
        return QObject::tr("No assets were found in this library.");
    }
    if (!hasVisibleItems) {
        return QObject::tr("No items match the current search or source filter.");
    }
    return {};
}

}  // namespace

HomeLibraryView::HomeLibraryView(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    sessionCard_ = new QFrame(this);
    sessionCard_->setObjectName("sessionStatusCard");
    auto* sessionLayout = new QHBoxLayout(sessionCard_);
    sessionLayout->setContentsMargins(14, 12, 14, 12);
    sessionLayout->setSpacing(16);

    auto* sessionGrid = new QGridLayout();
    sessionGrid->setHorizontalSpacing(12);
    sessionGrid->setVerticalSpacing(6);
    sessionGrid->addWidget(new QLabel(tr("Status"), sessionCard_), 0, 0);
    sessionStateValueLabel_ = new QLabel(tr("No save opened"), sessionCard_);
    sessionStateValueLabel_->setObjectName("sessionStateValueLabel");
    sessionGrid->addWidget(sessionStateValueLabel_, 0, 1);
    sessionGrid->addWidget(new QLabel(tr("Opened save"), sessionCard_), 1, 0);
    currentSaveValueLabel_ = new QLabel(tr("None"), sessionCard_);
    currentSaveValueLabel_->setObjectName("currentSaveValueLabel");
    sessionGrid->addWidget(currentSaveValueLabel_, 1, 1);
    sessionGrid->addWidget(new QLabel(tr("Last action"), sessionCard_), 2, 0);
    lastActionValueLabel_ = new QLabel(lastActionSummary_, sessionCard_);
    lastActionValueLabel_->setObjectName("lastActionValueLabel");
    lastActionValueLabel_->setWordWrap(true);
    sessionGrid->addWidget(lastActionValueLabel_, 2, 1);
    sessionLayout->addLayout(sessionGrid, 1);

    openSaveButton_ = new QPushButton(tr("Open Save"), sessionCard_);
    openSaveButton_->setObjectName("openSaveButton");
    sessionLayout->addWidget(openSaveButton_, 0, Qt::AlignTop);
    layout->addWidget(sessionCard_);

    libraryTabs_ = new QTabWidget(this);
    libraryTabs_->setObjectName("libraryTabs");
    auto* emblemPage = new QWidget(libraryTabs_);
    auto* acPage = new QWidget(libraryTabs_);
    initializeTab(
        LibraryKind::Emblem,
        emblemPage,
        tr("Import selected to user slot"),
        tr("Import emblem"),
        tr("Export selected"),
        tr("Select an emblem to inspect."),
        tr("Select an emblem to inspect."),
        tr("Import from Share into a user slot, or export any selected emblem as .ac6emblemdata."),
        QStringLiteral("emblemTable"),
        QStringLiteral("emblemFilterEdit"),
        QStringLiteral("emblemSourceFilterCombo"),
        QStringLiteral("emblemImportSelectedButton"),
        QStringLiteral("emblemImportFileButton"),
        QStringLiteral("emblemExportButton"),
        QStringLiteral("emblemHelperTextLabel"));
    initializeTab(
        LibraryKind::Ac,
        acPage,
        tr("Import selected to user slot"),
        tr("Import AC"),
        tr("Export selected"),
        tr("Select an AC to inspect."),
        tr("Select an AC to inspect."),
        tr("Import from Share into a user slot, or export any selected AC as .ac6acdata."),
        QStringLiteral("acTable"),
        QStringLiteral("acFilterEdit"),
        QStringLiteral("acSourceFilterCombo"),
        QStringLiteral("acImportSelectedButton"),
        QStringLiteral("acImportFileButton"),
        QStringLiteral("acExportButton"),
        QStringLiteral("acHelperTextLabel"));
    libraryTabs_->addTab(emblemPage, tr("Emblems"));
    libraryTabs_->addTab(acPage, tr("ACs"));
    layout->addWidget(libraryTabs_, 1);

    inlineStatusFrame_ = new QFrame(this);
    inlineStatusFrame_->setObjectName("inlineStatusFrame");
    auto* inlineLayout = new QHBoxLayout(inlineStatusFrame_);
    inlineLayout->setContentsMargins(12, 8, 12, 8);
    inlineLayout->setSpacing(10);
    inlineStatusLabel_ = new QLabel(inlineStatusFrame_);
    inlineStatusLabel_->setObjectName("inlineStatusLabel");
    inlineStatusLabel_->setWordWrap(true);
    inlineLayout->addWidget(inlineStatusLabel_, 1);
    inlineStatusDetailsButton_ = new QPushButton(tr("View details"), inlineStatusFrame_);
    inlineStatusDetailsButton_->setObjectName("inlineStatusDetailsButton");
    inlineLayout->addWidget(inlineStatusDetailsButton_);
    inlineStatusDismissButton_ = new QPushButton(tr("Dismiss"), inlineStatusFrame_);
    inlineStatusDismissButton_->setObjectName("inlineStatusDismissButton");
    inlineLayout->addWidget(inlineStatusDismissButton_);
    layout->addWidget(inlineStatusFrame_);

    connect(openSaveButton_, &QPushButton::clicked, this, &HomeLibraryView::openSaveRequested);
    connect(libraryTabs_, &QTabWidget::currentChanged, this, &HomeLibraryView::handleCurrentTabChanged);
    connect(inlineStatusDetailsButton_, &QPushButton::clicked, this, &HomeLibraryView::inlineDetailsRequested);
    connect(inlineStatusDismissButton_, &QPushButton::clicked, this, &HomeLibraryView::clearInlineStatus);

    updateSessionCard();
    clearInlineStatus();
}

void HomeLibraryView::initializeTab(LibraryKind kind, QWidget* page, const QString& actionText,
    const QString& fileImportText, const QString& exportText, const QString& emptySelectionText,
    const QString& selectionPromptText, const QString& helperText, const QString& tableObjectName,
    const QString& filterObjectName, const QString& filterComboObjectName,
    const QString& primaryButtonObjectName, const QString& importButtonObjectName,
    const QString& exportButtonObjectName, const QString& helperTextObjectName) {
    auto& widgets = widgetsFor(kind);
    widgets.emptySelectionText = emptySelectionText;
    widgets.selectionPromptText = selectionPromptText;
    widgets.helperText = helperText;

    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(10, 10, 10, 10);
    pageLayout->setSpacing(10);

    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);
    widgets.filterEdit = new QLineEdit(page);
    widgets.filterEdit->setObjectName(filterObjectName);
    widgets.filterEdit->setPlaceholderText(tr("Search name, share code, slot or origin"));
    toolbarLayout->addWidget(widgets.filterEdit, 1);

    widgets.sourceFilterCombo = new QComboBox(page);
    widgets.sourceFilterCombo->setObjectName(filterComboObjectName);
    widgets.sourceFilterCombo->addItem(tr("All sources"), QString::fromUtf8(kAllSourcesFilter));
    widgets.sourceFilterCombo->addItem(tr("User 1"), QString::fromStdString(contracts::toString(contracts::SourceBucket::User1)));
    widgets.sourceFilterCombo->addItem(tr("User 2"), QString::fromStdString(contracts::toString(contracts::SourceBucket::User2)));
    widgets.sourceFilterCombo->addItem(tr("User 3"), QString::fromStdString(contracts::toString(contracts::SourceBucket::User3)));
    widgets.sourceFilterCombo->addItem(tr("User 4"), QString::fromStdString(contracts::toString(contracts::SourceBucket::User4)));
    widgets.sourceFilterCombo->addItem(tr("Share"), QString::fromStdString(contracts::toString(contracts::SourceBucket::Share)));
    toolbarLayout->addWidget(widgets.sourceFilterCombo);

    widgets.primaryActionButton = new QPushButton(actionText, page);
    widgets.primaryActionButton->setObjectName(primaryButtonObjectName);
    widgets.importFileButton = new QPushButton(fileImportText, page);
    widgets.importFileButton->setObjectName(importButtonObjectName);
    widgets.exportButton = new QPushButton(exportText, page);
    widgets.exportButton->setObjectName(exportButtonObjectName);
    toolbarLayout->addWidget(widgets.primaryActionButton);
    toolbarLayout->addWidget(widgets.importFileButton);
    toolbarLayout->addWidget(widgets.exportButton);
    pageLayout->addLayout(toolbarLayout);

    auto* contentSplitter = new QSplitter(Qt::Horizontal, page);
    contentSplitter->setChildrenCollapsible(false);

    auto* leftPanel = new QWidget(contentSplitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);
    widgets.emptyStateLabel = new QLabel(leftPanel);
    widgets.emptyStateLabel->setObjectName(kind == LibraryKind::Emblem
            ? QStringLiteral("emblemEmptyStateLabel")
            : QStringLiteral("acEmptyStateLabel"));
    widgets.emptyStateLabel->setProperty("emptyState", true);
    widgets.emptyStateLabel->setWordWrap(true);
    leftLayout->addWidget(widgets.emptyStateLabel);

    widgets.table = new QTableWidget(leftPanel);
    widgets.table->setObjectName(tableObjectName);
    if (kind == LibraryKind::Emblem) {
        widgets.table->setColumnCount(4);
        widgets.table->setHorizontalHeaderLabels(QStringList{tr("Name / Share Code"), tr("Origin"), tr("Slot"), tr("Status")});
    } else {
        widgets.table->setColumnCount(6);
        widgets.table->setHorizontalHeaderLabels(QStringList{tr("Archive"), tr("AC"), tr("Share Code"), tr("Origin"), tr("Slot"), tr("Status")});
    }
    widgets.table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    widgets.table->setSelectionBehavior(QAbstractItemView::SelectRows);
    widgets.table->setSelectionMode(QAbstractItemView::SingleSelection);
    widgets.table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    widgets.table->setWordWrap(false);
    widgets.table->setSortingEnabled(true);
    widgets.table->installEventFilter(this);
    leftLayout->addWidget(widgets.table, 1);

    widgets.helperTextLabel = new QLabel(helperText, leftPanel);
    widgets.helperTextLabel->setObjectName(helperTextObjectName);
    widgets.helperTextLabel->setProperty("secondaryText", true);
    widgets.helperTextLabel->setWordWrap(true);
    leftLayout->addWidget(widgets.helperTextLabel);

    auto* rightPanel = new QWidget(contentSplitter);
    configureInspector(kind, rightPanel, widgets);

    contentSplitter->addWidget(leftPanel);
    contentSplitter->addWidget(rightPanel);
    contentSplitter->setStretchFactor(0, 3);
    contentSplitter->setStretchFactor(1, 2);
    pageLayout->addWidget(contentSplitter, 1);

    connect(widgets.filterEdit, &QLineEdit::textChanged, this, &HomeLibraryView::handleFilterChanged);
    connect(widgets.sourceFilterCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
        &HomeLibraryView::handleSourceFilterChanged);
    connect(widgets.table, &QTableWidget::itemSelectionChanged, this, &HomeLibraryView::handleSelectionChanged);
    connect(widgets.table, &QTableWidget::cellDoubleClicked, this, [this, kind](int, int) {
        const auto& currentWidgets = widgetsFor(kind);
        if (currentWidgets.primaryActionButton->isEnabled()) {
            emit primaryActionRequested();
        }
    });
    connect(widgets.primaryActionButton, &QPushButton::clicked, this, &HomeLibraryView::primaryActionRequested);
    connect(widgets.importFileButton, &QPushButton::clicked, this, &HomeLibraryView::importFileRequested);
    connect(widgets.exportButton, &QPushButton::clicked, this, &HomeLibraryView::exportSelectedRequested);
}

void HomeLibraryView::configureInspector(LibraryKind kind, QWidget* parent, LibraryWidgets& widgets) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    widgets.inspectorTitleLabel = new QLabel(widgets.selectionPromptText, parent);
    widgets.inspectorTitleLabel->setObjectName(kind == LibraryKind::Emblem
            ? QStringLiteral("emblemInspectorTitleLabel")
            : QStringLiteral("acInspectorTitleLabel"));
    widgets.inspectorTitleLabel->setWordWrap(true);
    layout->addWidget(widgets.inspectorTitleLabel);

    auto* scrollArea = new QScrollArea(parent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto* scrollContent = new QWidget(scrollArea);
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(10);

    widgets.inspectorPlaceholderLabel = new QLabel(widgets.selectionPromptText, scrollContent);
    widgets.inspectorPlaceholderLabel->setObjectName(kind == LibraryKind::Emblem
            ? QStringLiteral("emblemInspectorPlaceholderLabel")
            : QStringLiteral("acInspectorPlaceholderLabel"));
    widgets.inspectorPlaceholderLabel->setProperty("emptyState", true);
    widgets.inspectorPlaceholderLabel->setWordWrap(true);
    scrollLayout->addWidget(widgets.inspectorPlaceholderLabel);

    widgets.identityGroup = new QGroupBox(tr("Identity"), scrollContent);
    auto* identityLayout = new QFormLayout(widgets.identityGroup);
    widgets.identityField1Label = new QLabel(kind == LibraryKind::Emblem ? tr("Name / Share code") : tr("Archive"), widgets.identityGroup);
    widgets.identityField1Value = new QLabel(tr("-"), widgets.identityGroup);
    widgets.identityField1Value->setWordWrap(true);
    identityLayout->addRow(widgets.identityField1Label, widgets.identityField1Value);
    widgets.identityField2Label = new QLabel(kind == LibraryKind::Emblem ? tr("Origin") : tr("AC"), widgets.identityGroup);
    widgets.identityField2Value = new QLabel(tr("-"), widgets.identityGroup);
    widgets.identityField2Value->setWordWrap(true);
    identityLayout->addRow(widgets.identityField2Label, widgets.identityField2Value);
    widgets.identityField3Label = new QLabel(kind == LibraryKind::Emblem ? tr("Slot") : tr("Share code"), widgets.identityGroup);
    widgets.identityField3Value = new QLabel(tr("-"), widgets.identityGroup);
    widgets.identityField3Value->setWordWrap(true);
    identityLayout->addRow(widgets.identityField3Label, widgets.identityField3Value);
    scrollLayout->addWidget(widgets.identityGroup);

    if (kind == LibraryKind::Ac) {
        widgets.locationGroup = new QGroupBox(tr("Location"), scrollContent);
        auto* locationLayout = new QFormLayout(widgets.locationGroup);
        widgets.locationOriginValue = new QLabel(tr("-"), widgets.locationGroup);
        widgets.locationOriginValue->setWordWrap(true);
        widgets.locationSlotValue = new QLabel(tr("-"), widgets.locationGroup);
        widgets.locationSlotValue->setWordWrap(true);
        locationLayout->addRow(tr("Origin"), widgets.locationOriginValue);
        locationLayout->addRow(tr("Slot"), widgets.locationSlotValue);
        scrollLayout->addWidget(widgets.locationGroup);
    }

    widgets.statusGroup = new QGroupBox(tr("Write status"), scrollContent);
    auto* statusLayout = new QFormLayout(widgets.statusGroup);
    widgets.statusBadgeLabel = new QLabel(tr("Read-only"), widgets.statusGroup);
    widgets.statusBadgeLabel->setObjectName(kind == LibraryKind::Emblem
            ? QStringLiteral("emblemStatusBadgeLabel")
            : QStringLiteral("acStatusBadgeLabel"));
    statusLayout->addRow(tr("Status"), widgets.statusBadgeLabel);
    widgets.statusNoteLabel = new QLabel(tr("-"), widgets.statusGroup);
    widgets.statusNoteLabel->setProperty("secondaryText", true);
    widgets.statusNoteLabel->setWordWrap(true);
    statusLayout->addRow(tr("Explanation"), widgets.statusNoteLabel);
    widgets.previewValueLabel = new QLabel(tr("Hidden in this build"), widgets.statusGroup);
    widgets.previewValueLabel->setProperty("secondaryText", true);
    widgets.previewValueLabel->setWordWrap(true);
    statusLayout->addRow(tr("Preview"), widgets.previewValueLabel);
    scrollLayout->addWidget(widgets.statusGroup);

    widgets.technicalToggleButton = new QToolButton(scrollContent);
    widgets.technicalToggleButton->setCheckable(true);
    widgets.technicalToggleButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    widgets.technicalToggleButton->setText(tr("Show technical details"));
    scrollLayout->addWidget(widgets.technicalToggleButton);

    widgets.technicalText = new QPlainTextEdit(scrollContent);
    widgets.technicalText->setReadOnly(true);
    widgets.technicalText->setObjectName(kind == LibraryKind::Emblem
            ? QStringLiteral("emblemTechnicalDetailsText")
            : QStringLiteral("acTechnicalDetailsText"));
    scrollLayout->addWidget(widgets.technicalText);
    scrollLayout->addStretch(1);

    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea, 1);

    connect(widgets.technicalToggleButton, &QToolButton::toggled, this, [this, &widgets](const bool checked) {
        setTechnicalDetailsExpanded(widgets, checked);
    });
    setTechnicalDetailsExpanded(widgets, false);
}

void HomeLibraryView::setCatalog(const std::vector<contracts::CatalogItemDto>& items) {
    setLibraryCatalog(LibraryKind::Emblem, items);
}

void HomeLibraryView::setAcCatalog(const std::vector<contracts::CatalogItemDto>& items) {
    setLibraryCatalog(LibraryKind::Ac, items);
}

void HomeLibraryView::setLibraryCatalog(LibraryKind kind, const std::vector<contracts::CatalogItemDto>& items) {
    auto& widgets = widgetsFor(kind);
    widgets.allItems = items;
    refreshVisibleRows(kind);
}

void HomeLibraryView::setSessionStatus(const contracts::SessionStatusDto& status) {
    sessionHasRealSave_ = status.hasRealSave;
    sourceSavePath_ = status.sourceSavePath;
    if (status.lastActionSummary.has_value()) {
        lastActionSummary_ = QString::fromStdString(*status.lastActionSummary);
    }
    updateSessionCard();
    updateActionState(LibraryKind::Emblem, selectedItem(LibraryKind::Emblem));
    updateActionState(LibraryKind::Ac, selectedItem(LibraryKind::Ac));
    refreshVisibleRows(LibraryKind::Emblem);
    refreshVisibleRows(LibraryKind::Ac);
}

void HomeLibraryView::setLastActionSummary(const QString& summary, const bool isError) {
    lastActionSummary_ = summary.trimmed().isEmpty() ? tr("No recent action.") : summary;
    lastActionValueLabel_->setProperty("statusTone", isError ? "error" : "normal");
    updateSessionCard();
}

void HomeLibraryView::setInlineStatus(const QString& summary, const bool isError, const bool canShowDetails) {
    inlineStatusLabel_->setText(summary.trimmed());
    inlineStatusIsError_ = isError;
    inlineStatusCanShowDetails_ = canShowDetails;
    updateInlineStatusVisibility();
}

void HomeLibraryView::clearInlineStatus() {
    inlineStatusLabel_->clear();
    inlineStatusIsError_ = false;
    inlineStatusCanShowDetails_ = false;
    updateInlineStatusVisibility();
}

std::optional<contracts::CatalogItemDto> HomeLibraryView::selectedItem() const {
    return selectedItem(currentLibraryKind());
}

contracts::AssetKind HomeLibraryView::currentLibraryAssetKind() const {
    return currentLibraryKind() == LibraryKind::Ac ? contracts::AssetKind::Ac : contracts::AssetKind::Emblem;
}

std::optional<contracts::CatalogItemDto> HomeLibraryView::selectedItem(LibraryKind kind) const {
    const auto& widgets = widgetsFor(kind);
    const auto selectedItems = widgets.table->selectedItems();
    if (selectedItems.isEmpty()) {
        return std::nullopt;
    }

    const int logicalIndex = selectedItems.front()->data(Qt::UserRole).toInt();
    if (logicalIndex < 0 || logicalIndex >= static_cast<int>(widgets.allItems.size())) {
        return std::nullopt;
    }
    return widgets.allItems[logicalIndex];
}

void HomeLibraryView::handleSelectionChanged() {
    const auto kind = currentLibraryKind();
    const auto item = selectedItem(kind);
    updateActionState(kind, item);
    showItemDetails(kind, item);
}

void HomeLibraryView::handleFilterChanged(const QString&) {
    refreshVisibleRows(currentLibraryKind());
}

void HomeLibraryView::handleCurrentTabChanged(int) {
    refreshVisibleRows(currentLibraryKind());
}

void HomeLibraryView::handleSourceFilterChanged(int) {
    refreshVisibleRows(currentLibraryKind());
}

void HomeLibraryView::refreshVisibleRows(LibraryKind kind) {
    auto& widgets = widgetsFor(kind);
    widgets.table->setSortingEnabled(false);
    widgets.table->setRowCount(0);

    const QString filter = widgets.filterEdit->text().trimmed();
    const auto containsFilter = [&](const contracts::CatalogItemDto& item) {
        if (filter.isEmpty()) {
            return true;
        }
        const QString haystack = QString::fromStdString(
            item.displayName + "\n" + item.archiveName + "\n" + item.machineName + "\n" + item.shareCode + "\n"
            + item.slotLabel + "\n" + item.sourceLabel + "\n" + item.description);
        return haystack.contains(filter, Qt::CaseInsensitive);
    };

    for (int index = 0; index < static_cast<int>(widgets.allItems.size()); ++index) {
        const auto& item = widgets.allItems[index];
        if (!containsFilter(item) || !itemMatchesSourceFilter(kind, item)) {
            continue;
        }
        const int row = widgets.table->rowCount();
        widgets.table->insertRow(row);

        if (kind == LibraryKind::Emblem) {
            widgets.table->setItem(row, 0, makeTableItem(emblemNameCodeText(item), index));
            widgets.table->setItem(row, 1, makeTableItem(sourceBucketLabel(item), index));
            widgets.table->setItem(row, 2, makeTableItem(slotText(item), index));
            widgets.table->setItem(row, 3, makeTableItem(writeCapabilityLabel(item), index));
        } else {
            widgets.table->setItem(row, 0, makeTableItem(archiveNameText(item), index));
            widgets.table->setItem(row, 1, makeTableItem(machineNameText(item), index));
            widgets.table->setItem(row, 2, makeTableItem(shareCodeText(item), index));
            widgets.table->setItem(row, 3, makeTableItem(sourceBucketLabel(item), index));
            widgets.table->setItem(row, 4, makeTableItem(slotText(item), index));
            widgets.table->setItem(row, 5, makeTableItem(writeCapabilityLabel(item), index));
        }
    }

    widgets.table->setSortingEnabled(true);
    widgets.table->resizeRowsToContents();
    widgets.table->clearSelection();

    const auto emptyState = genericEmptyStateText(
        sessionHasRealSave_,
        !widgets.allItems.empty(),
        widgets.table->rowCount() > 0);
    widgets.emptyStateLabel->setText(emptyState);
    widgets.emptyStateLabel->setVisible(!emptyState.isEmpty());

    updateActionState(kind, std::nullopt);
    showItemDetails(kind, std::nullopt);
}

void HomeLibraryView::showItemDetails(LibraryKind kind, const std::optional<contracts::CatalogItemDto>& item) {
    auto& widgets = widgetsFor(kind);
    if (!item.has_value()) {
        widgets.inspectorTitleLabel->setText(widgets.selectionPromptText);
        widgets.inspectorPlaceholderLabel->setText(widgets.emptySelectionText);
        widgets.inspectorPlaceholderLabel->setVisible(true);
        widgets.identityGroup->setVisible(false);
        if (widgets.locationGroup != nullptr) {
            widgets.locationGroup->setVisible(false);
        }
        widgets.statusGroup->setVisible(false);
        widgets.technicalToggleButton->setVisible(false);
        widgets.technicalText->setVisible(false);
        widgets.technicalText->clear();
        setTechnicalDetailsExpanded(widgets, false);
        return;
    }

    widgets.inspectorTitleLabel->setText(detailTitleFor(kind, *item));
    widgets.inspectorPlaceholderLabel->setVisible(false);
    widgets.identityGroup->setVisible(true);
    widgets.statusGroup->setVisible(true);
    widgets.technicalToggleButton->setVisible(true);

    if (kind == LibraryKind::Emblem) {
        widgets.identityField1Label->setText(tr("Name / Share code"));
        widgets.identityField1Value->setText(emblemNameCodeText(*item));
        widgets.identityField2Label->setText(tr("Origin"));
        widgets.identityField2Value->setText(sourceBucketLabel(*item));
        widgets.identityField3Label->setText(tr("Slot"));
        widgets.identityField3Value->setText(slotText(*item));
        if (widgets.locationGroup != nullptr) {
            widgets.locationGroup->setVisible(false);
        }
    } else {
        widgets.identityField1Label->setText(tr("Archive"));
        widgets.identityField1Value->setText(archiveNameText(*item));
        widgets.identityField2Label->setText(tr("AC"));
        widgets.identityField2Value->setText(machineNameText(*item));
        widgets.identityField3Label->setText(tr("Share code"));
        widgets.identityField3Value->setText(shareCodeText(*item));
        if (widgets.locationGroup != nullptr) {
            widgets.locationOriginValue->setText(sourceBucketLabel(*item));
            widgets.locationSlotValue->setText(slotText(*item));
            widgets.locationGroup->setVisible(true);
        }
    }

    updateStatusBadge(widgets.statusBadgeLabel, writeCapabilityLabel(*item), badgeStateForCapability(item->writeCapability));
    widgets.statusNoteLabel->setText(statusNoteText(*item));
    widgets.previewValueLabel->setText(tr("Hidden in this build"));

    QStringList lines;
    lines << tr("Description: %1").arg(QString::fromStdString(item->description));
    if (!item->tags.empty()) {
        QStringList tags;
        for (const auto& tag : item->tags) {
            tags << QString::fromStdString(tag);
        }
        lines << tr("Tags: %1").arg(tags.join(", "));
    }
    for (const auto& field : item->detailFields) {
        lines << QString::fromStdString(field.label + ": " + field.value);
    }
    widgets.technicalText->setPlainText(lines.join("\n"));
    if (!widgets.technicalToggleButton->isChecked()) {
        widgets.technicalText->setVisible(false);
    }
}

void HomeLibraryView::updateActionState(LibraryKind kind, const std::optional<contracts::CatalogItemDto>& item) {
    auto& widgets = widgetsFor(kind);
    const bool hasItem = item.has_value();
    const bool canImportEmblem = kind == LibraryKind::Emblem && hasItem && isShareOrigin(*item) && sessionHasRealSave_;
    const bool canExportEmblem = kind == LibraryKind::Emblem && hasItem;
    const bool canImportEmblemFile = kind == LibraryKind::Emblem && sessionHasRealSave_;
    const bool canImportAc = kind == LibraryKind::Ac && hasItem && isShareOrigin(*item) && sessionHasRealSave_;
    const bool canImportAcFile = kind == LibraryKind::Ac && sessionHasRealSave_;
    const bool canExportAc = kind == LibraryKind::Ac && hasItem;

    widgets.primaryActionButton->setEnabled(kind == LibraryKind::Emblem ? canImportEmblem : canImportAc);
    widgets.importFileButton->setEnabled(kind == LibraryKind::Emblem ? canImportEmblemFile : canImportAcFile);
    widgets.exportButton->setEnabled(kind == LibraryKind::Emblem ? canExportEmblem : canExportAc);

    if (kind == LibraryKind::Emblem) {
        widgets.primaryActionButton->setToolTip(
            canImportEmblem ? QString() : tr("Select a Share emblem and open a real save before importing."));
        widgets.importFileButton->setToolTip(
            canImportEmblemFile ? QString() : tr("Open a real save before importing an .ac6emblemdata file."));
        widgets.exportButton->setToolTip(
            canExportEmblem ? QString() : tr("Select an emblem before exporting."));
    } else {
        widgets.primaryActionButton->setToolTip(
            canImportAc ? QString() : tr("Select a Share AC and open a real save before importing."));
        widgets.importFileButton->setToolTip(
            canImportAcFile ? QString() : tr("Open a real save before importing an .ac6acdata file."));
        widgets.exportButton->setToolTip(
            canExportAc ? QString() : tr("Select an AC before exporting."));
    }

    widgets.helperTextLabel->setText(sessionHasRealSave_
            ? widgets.helperText
            : tr("Open a real .sl2 save to browse and manage assets."));
}

QString HomeLibraryView::sourceBucketLabel(const contracts::CatalogItemDto& item) const {
    return sourceLabelForBucket(item.sourceBucket);
}

QString HomeLibraryView::writeCapabilityLabel(const contracts::CatalogItemDto& item) const {
    return statusLabelForCapability(item.writeCapability);
}

QString HomeLibraryView::emblemNameCodeText(const contracts::CatalogItemDto& item) const {
    return item.shareCode.empty()
        ? tr("No in-game name / no share code")
        : QString::fromStdString(item.shareCode);
}

QString HomeLibraryView::archiveNameText(const contracts::CatalogItemDto& item) const {
    return placeholderText(QString::fromStdString(item.archiveName));
}

QString HomeLibraryView::machineNameText(const contracts::CatalogItemDto& item) const {
    return placeholderText(QString::fromStdString(item.machineName));
}

QString HomeLibraryView::shareCodeText(const contracts::CatalogItemDto& item) const {
    return placeholderText(QString::fromStdString(item.shareCode));
}

QString HomeLibraryView::slotText(const contracts::CatalogItemDto& item) const {
    if (item.assetKind == contracts::AssetKind::Emblem) {
        const auto slotLabel = QString::fromStdString(item.slotLabel);
        if (slotLabel == QStringLiteral("SHARE")) {
            return tr("Share");
        }
        if (slotLabel.startsWith(QStringLiteral("USER_EMBLEM_"))) {
            return tr("User emblem %1").arg(slotLabel.mid(QStringLiteral("USER_EMBLEM_").size()));
        }
        return placeholderText(slotLabel);
    }

    const int recordIndex = recordIndexFromRecordRef(item.recordRef);
    if (recordIndex >= 0) {
        return tr("%1 / AC %2").arg(sourceBucketLabel(item), twoDigitNumber(recordIndex));
    }
    return placeholderText(QString::fromStdString(item.slotLabel));
}

QString HomeLibraryView::statusNoteText(const contracts::CatalogItemDto& item) const {
    switch (item.writeCapability) {
    case contracts::WriteCapability::Editable:
        return tr("This asset is available for local editing in its current bucket.");
    case contracts::WriteCapability::ReadOnly:
        if (item.origin == contracts::AssetOrigin::Share) {
            return item.assetKind == contracts::AssetKind::Ac
                ? tr("Share AC. Import it into a user slot to create a local editable record.")
                : tr("Share emblem. Import it into a user slot before editing.");
        }
        return tr("This asset is read-only in its current location.");
    case contracts::WriteCapability::LockedPendingGate:
        return tr("This action is blocked until the write path is fully qualified.");
    }
    return tr("This asset is read-only in its current location.");
}

QString HomeLibraryView::detailTitleFor(LibraryKind kind, const contracts::CatalogItemDto& item) const {
    if (kind == LibraryKind::Emblem) {
        return item.shareCode.empty() ? tr("Emblem inspector") : QString::fromStdString(item.shareCode);
    }
    if (!item.archiveName.empty()) {
        return QString::fromStdString(item.archiveName);
    }
    if (!item.machineName.empty()) {
        return QString::fromStdString(item.machineName);
    }
    return QString::fromStdString(item.displayName);
}

bool HomeLibraryView::itemMatchesSourceFilter(LibraryKind kind, const contracts::CatalogItemDto& item) const {
    const auto& widgets = widgetsFor(kind);
    const auto filterValue = widgets.sourceFilterCombo->currentData().toString();
    if (filterValue == QString::fromUtf8(kAllSourcesFilter)) {
        return true;
    }
    return filterValue == QString::fromStdString(contracts::toString(item.sourceBucket));
}

QString HomeLibraryView::sessionStateText() const {
    return sessionHasRealSave_ ? tr("Ready") : tr("No save opened");
}

void HomeLibraryView::updateSessionCard() {
    sessionStateValueLabel_->setText(sessionStateText());
    currentSaveValueLabel_->setText(sourceSavePath_.has_value()
            ? QString::fromStdWString(sourceSavePath_->filename().wstring())
            : tr("None"));
    lastActionValueLabel_->setText(lastActionSummary_);
}

void HomeLibraryView::updateStatusBadge(QLabel* label, const QString& text, const QString& state) {
    label->setText(text);
    label->setProperty("badgeState", state);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

void HomeLibraryView::updateInlineStatusVisibility() {
    const bool visible = !inlineStatusLabel_->text().trimmed().isEmpty();
    inlineStatusFrame_->setVisible(visible);
    inlineStatusFrame_->setProperty("bannerTone", inlineStatusIsError_ ? "error" : "success");
    inlineStatusFrame_->style()->unpolish(inlineStatusFrame_);
    inlineStatusFrame_->style()->polish(inlineStatusFrame_);
    inlineStatusDetailsButton_->setVisible(visible && inlineStatusCanShowDetails_);
    inlineStatusDismissButton_->setVisible(visible);
}

void HomeLibraryView::setTechnicalDetailsExpanded(LibraryWidgets& widgets, const bool expanded) {
    widgets.technicalToggleButton->setChecked(expanded);
    widgets.technicalToggleButton->setText(expanded ? tr("Hide technical details") : tr("Show technical details"));
    widgets.technicalText->setVisible(expanded);
}

bool HomeLibraryView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (watched == emblemWidgets_.table && emblemWidgets_.primaryActionButton->isEnabled()) {
                emit primaryActionRequested();
                return true;
            }
            if (watched == acWidgets_.table && acWidgets_.primaryActionButton->isEnabled()) {
                emit primaryActionRequested();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

HomeLibraryView::LibraryWidgets& HomeLibraryView::widgetsFor(LibraryKind kind) {
    return kind == LibraryKind::Emblem ? emblemWidgets_ : acWidgets_;
}

const HomeLibraryView::LibraryWidgets& HomeLibraryView::widgetsFor(LibraryKind kind) const {
    return kind == LibraryKind::Emblem ? emblemWidgets_ : acWidgets_;
}

HomeLibraryView::LibraryKind HomeLibraryView::currentLibraryKind() const {
    return libraryTabs_->currentIndex() == 1 ? LibraryKind::Ac : LibraryKind::Emblem;
}

}  // namespace ac6dm::app
