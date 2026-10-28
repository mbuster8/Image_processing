#include "main_window.h"

#include "appearance.h"
#include "controls.h"
#include "image_file.h"
#include "outline_icons.h"
#include "qimage_conversion.h"
#include "shades.h"
#include "ui_main_window.h"

#include "viewer_core/display_converter.h"
#include "visual_debug/endpoint.h"

#include <QAction>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <string_view>
#include <utility>
#include <variant>

namespace {

const QString kEndpointKey = QStringLiteral("client/endpoint");
// As around the viewer sidebar's search row.
constexpr int kCommandBarMargin = 6;
constexpr int kCommandBarSpacing = 4;

QString fromView(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// VISUAL_DEBUG_ENDPOINT when it is set, otherwise the endpoint last used, otherwise the default.
// defaultEndpoint() is the one place the environment is read, so a variable set to the default itself
// counts as unset.
QString startingEndpoint() {
    const std::string fromEnvironment = visual_debug::defaultEndpoint();
    if (fromEnvironment != visual_debug::kDefaultEndpoint) {
        return QString::fromStdString(fromEnvironment);
    }
    return QSettings().value(kEndpointKey, fromView(visual_debug::kDefaultEndpoint)).toString();
}

QToolButton* makeButton(const QString& objectName, QAction* action) {
    auto* button = new QToolButton;
    button->setObjectName(objectName);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    qt_common::frameButton(button);
    return button;
}

} // namespace

MainWindow::MainWindow(SenderFactory makeSender, QWidget* parent)
    : QMainWindow(parent), m_ui(std::make_unique<Ui::MainWindow>()), m_endpointField(new QLineEdit),
      m_makeSender(std::move(makeSender)) {
    m_ui->setupUi(this);
    m_ui->actionOpen->setIcon(qt_common::makeOutlineIcon(qt_common::OutlineIcon::Folder));
    m_ui->actionSend->setIcon(qt_common::makeOutlineIcon(qt_common::OutlineIcon::Send));

    m_endpointField->setObjectName(QStringLiteral("endpointField"));
    m_endpointField->setText(startingEndpoint());
    m_endpointField->setToolTip(tr("Where images go: the endpoint the viewer listens on"));
    // The mark is a property, so the outline is styled rather than hard-wired into the field.
    m_endpointField->setStyleSheet(QStringLiteral("QLineEdit[invalid=\"true\"] { border: 2px solid #d93025; "
                                                  "border-radius: 4px; }"));
    auto* endpointLabel = new QLabel(tr("Send to"));
    endpointLabel->setBuddy(m_endpointField);
    qt_common::sizeControl(m_endpointField);
    // A plain row rather than a QToolBar: macOS draws a tool bar's buttons without their frames.
    auto* commandBar = new QWidget;
    commandBar->setObjectName(QStringLiteral("commandBar"));
    commandBar->setFont(qt_common::regularFont());
    auto* commandLayout = new QHBoxLayout(commandBar);
    commandLayout->setContentsMargins(kCommandBarMargin, kCommandBarMargin, kCommandBarMargin,
                                      kCommandBarMargin);
    commandLayout->setSpacing(kCommandBarSpacing);
    commandLayout->addWidget(makeButton(QStringLiteral("openButton"), m_ui->actionOpen));
    commandLayout->addWidget(makeButton(QStringLiteral("sendButton"), m_ui->actionSend));
    commandLayout->addSpacing(kCommandBarSpacing);
    commandLayout->addWidget(endpointLabel);
    commandLayout->addWidget(m_endpointField, 1);
    qt_common::usePanelGrey(commandBar);
    // The divider marks the toolbar's bottom edge.
    auto* central = new QWidget;
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    auto* toolBarDivider = new qt_common::DividerLine;
    toolBarDivider->setObjectName(QStringLiteral("toolBarDivider"));
    centralLayout->addWidget(commandBar);
    centralLayout->addWidget(toolBarDivider);
    centralLayout->addWidget(takeCentralWidget(), 1);
    setCentralWidget(central);

    connect(m_ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(m_ui->actionSend, &QAction::triggered, this, &MainWindow::sendImage);
    connect(m_endpointField, &QLineEdit::textChanged, this, [this] { markEndpointInvalid(false); });
    // Appearance is all the View menu holds, and Qt before 6.8 cannot offer it.
    if (qt_common::addAppearanceMenu(m_ui->menuView) == nullptr) {
        m_ui->menuView->menuAction()->setVisible(false);
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    showPreview();
}

void MainWindow::openFile() {
    const QString path = qt_common::chooseImageFile(this);
    if (!path.isEmpty()) {
        openImage(path);
    }
}

void MainWindow::openImage(const QString& path) {
    const QString fileName = QFileInfo(path).fileName();
    auto result = qt_common::readImageFile(path);
    if (const auto* error = std::get_if<QString>(&result)) {
        statusBar()->showMessage(tr("Cannot open [%1]: %2").arg(fileName, *error));
        return;
    }
    m_image = std::get<cv::Mat>(std::move(result));
    m_fileName = fileName;
    m_preview = QPixmap::fromImage(qt_common::toQImage(viewer_core::toDisplayImage(m_image)));
    m_ui->actionSend->setEnabled(true);
    showPreview();
    statusBar()->showMessage(tr("Opened [%1]").arg(fileName));
}

// Sends the image as read, not the preview, so the viewer shows its original depth and values. A new
// endpoint gets a new sender; the same one keeps its sender, and with it the retry back-off.
void MainWindow::sendImage() {
    const std::string endpoint = m_endpointField->text().trimmed().toStdString();
    if (m_sender == nullptr || endpoint != m_senderEndpoint) {
        m_sender = m_makeSender(endpoint);
        m_senderEndpoint = endpoint;
    }
    const std::string name = m_counter.nameFor(m_fileName.toStdString());
    // Waits for the connection and the reply: about a second at most for a small image, longer for a large
    // one, because the reply deadline grows with the image's size.
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const visual_debug::SendResult result = m_sender->send(m_image, name);
    QGuiApplication::restoreOverrideCursor();
    m_counter.record(result);
    if (result == visual_debug::SendResult::InvalidEndpoint) {
        markEndpointInvalid(true);
    } else {
        QSettings().setValue(kEndpointKey, QString::fromStdString(endpoint));
    }
    statusBar()->showMessage(
        QStringLiteral("[%1]: %2")
            .arg(QString::fromStdString(name), fromView(visual_debug::toString(result))));
}

void MainWindow::showPreview() {
    if (!m_preview.isNull()) {
        m_ui->previewLabel->setPixmap(
            m_preview.scaled(m_ui->previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

// A property change does not restyle by itself: the style sheet is applied again.
void MainWindow::markEndpointInvalid(bool invalid) {
    if (m_endpointField->property("invalid").toBool() == invalid) {
        return;
    }
    m_endpointField->setProperty("invalid", invalid);
    m_endpointField->style()->unpolish(m_endpointField);
    m_endpointField->style()->polish(m_endpointField);
}
