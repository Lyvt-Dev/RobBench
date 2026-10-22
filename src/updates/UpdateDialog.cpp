#include "updates/UpdateDialog.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>
namespace rb {
namespace {
bool allowed(const QUrl &url) {
    return url.scheme() == "https" && url.userInfo().isEmpty() &&
           QStringList{"api.github.com", "github.com", "release-assets.githubusercontent.com",
                       "objects.githubusercontent.com"}
               .contains(url.host().toLower());
}
} // namespace
UpdateDialog::UpdateDialog(QWidget *parent, std::function<bool()> guard)
    : QDialog(parent), beforeInstall(std::move(guard)) {
    setWindowTitle("RobBench updates");
    resize(620, 470);
    auto *layout = new QVBoxLayout(this);
    status = new QLabel("Check GitHub for a newer Windows release.");
    status->setWordWrap(true);
    layout->addWidget(status);
    token = new QLineEdit;
    token->setEchoMode(QLineEdit::Password);
    token->setPlaceholderText("Optional GitHub read token for a private repository (not saved)");
    layout->addWidget(token);
    notes = new QPlainTextEdit;
    notes->setReadOnly(true);
    layout->addWidget(notes);
    progress = new QProgressBar;
    progress->setRange(0, 100);
    layout->addWidget(progress);
    auto *row = new QHBoxLayout;
    layout->addLayout(row);
    check = new QPushButton("Check");
    download = new QPushButton("Download & verify");
    install = new QPushButton("Install & close RobBench");
    row->addWidget(check);
    row->addWidget(download);
    row->addWidget(install);
    download->setEnabled(false);
    install->setEnabled(false);
    auto *website = new QPushButton("Open releases in browser");
    layout->addWidget(website);
    layout->addWidget(
        new QLabel("SHA256 checks file integrity. It is not an Authenticode publisher signature."));
    connect(check, &QPushButton::clicked, this, [this] { checkNow(); });
    connect(download, &QPushButton::clicked, this, [this] { downloadNow(); });
    connect(website, &QPushButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl("https://github.com/Lyvt-Dev/RobBench/releases/latest")); });
    connect(install, &QPushButton::clicked, this, [this] {
        QFile verified(installerPath);
        QCryptographicHash finalHash(QCryptographicHash::Sha256);
        if (!verified.open(QIODevice::ReadOnly) || !finalHash.addData(&verified) ||
            QString::fromLatin1(finalHash.result().toHex()) != expectedHash) {
            fail("Installer changed after verification. Download it again.");
            return;
        }
        verified.close();
        if (beforeInstall && !beforeInstall())
            return;
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(installerPath)))
            QApplication::quit();
        else
            fail("Could not launch the verified installer.");
    });
}
UpdateDialog::~UpdateDialog() {
    for (auto *reply : network.findChildren<QNetworkReply *>()) {
        reply->disconnect(this);
        reply->abort();
    }
    if (file)
        file->cancelWriting();
    token->clear();
}
QNetworkReply *UpdateDialog::get(const QUrl &url, bool json) {
    if (!allowed(url))
        throw std::runtime_error("Update URL is not an approved HTTPS GitHub host");
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "RobBench-Updater");
    request.setRawHeader("Accept", json ? "application/vnd.github+json" : "application/octet-stream");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (url.host() == "api.github.com" && !token->text().trimmed().isEmpty())
        request.setRawHeader("Authorization", "Bearer " + token->text().trimmed().toUtf8());
    active = network.get(request);
    return active;
}
void UpdateDialog::fail(const QString &message) {
    if (file) {
        file->cancelWriting();
        file.reset();
    }
    status->setText(message);
    check->setEnabled(true);
    token->setEnabled(true);
    download->setEnabled(false);
    install->setEnabled(false);
    progress->setRange(0, 100);
    progress->setValue(0);
}
void UpdateDialog::fetch(const QUrl &url, qint64 limit, bool json, std::function<void(QByteArray)> done,
                         int redirects) {
    if (redirects > 5) {
        fail("Too many download redirects");
        return;
    }
    QNetworkReply *reply = nullptr;
    try {
        reply = get(url, json);
    } catch (const std::exception &e) {
        fail(QString::fromUtf8(e.what()));
        return;
    }
    auto data = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, this, [reply, data, limit] {
        data->append(reply->readAll());
        if (data->size() > limit)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        auto redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto error = reply->error();
        data->append(reply->readAll());
        reply->deleteLater();
        if (code >= 300 && code < 400 && !redirect.isEmpty()) {
            fetch(url.resolved(redirect), limit, json, done, redirects + 1);
            return;
        }
        if (error != QNetworkReply::NoError || code != 200 || data->size() > limit) {
            fail(code == 404 ? "Release not accessible. This repository may be private: supply a read token "
                               "or open the browser release page."
                             : QString("GitHub request failed (HTTP %1).").arg(code));
            return;
        }
        try {
            done(*data);
        } catch (const std::exception &e) {
            fail(QString::fromUtf8(e.what()));
        }
    });
}
void UpdateDialog::checkNow() {
    check->setEnabled(false);
    token->setEnabled(false);
    download->setEnabled(false);
    install->setEnabled(false);
    status->setText("Checking GitHub…");
    fetch(QUrl("https://api.github.com/repos/Lyvt-Dev/RobBench/releases/latest"), 1024 * 1024, true,
          [this](QByteArray data) {
              auto json = nlohmann::json::parse(data.constData(), data.constData() + data.size());
              auto remote = parseVersion(json.at("tag_name").get<std::string>());
              check->setEnabled(true);
              token->setEnabled(true);
              if (remote <= parseVersion(QApplication::applicationVersion().toStdString())) {
                  status->setText("You are using the latest version.");
                  return;
              }
              info = parseUpdate(json);
              notes->setPlainText(QString::fromStdString(info.notes));
              status->setText("Available: " + QString::fromStdString(info.version));
#ifdef Q_OS_WIN
              download->setEnabled(true);
#else
        status->setText(status->text()+" — automatic installer updates are currently Windows-only. Use the browser for other platforms.");
#endif
          });
}
void UpdateDialog::downloadNow() {
    check->setEnabled(false);
    token->setEnabled(false);
    download->setEnabled(false);
    status->setText("Fetching SHA256SUMS…");
    const bool authenticated = !token->text().trimmed().isEmpty();
    fetch(QUrl(QString::fromStdString(authenticated ? info.sumsApiUrl : info.sumsBrowserUrl)), 1024 * 1024,
          false, [this, authenticated](QByteArray data) {
              expectedHash = QString::fromStdString(checksumFor(data.toStdString(), info.filename));
              auto directory =
                  QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/updates";
              if (!QDir().mkpath(directory))
                  throw std::runtime_error("Cannot create update download directory");
              installerPath = directory + "/" + QString::fromStdString(info.filename);
              file = std::make_unique<QSaveFile>(installerPath);
              if (!file->open(QIODevice::WriteOnly))
                  throw std::runtime_error(file->errorString().toStdString());
              received = 0;
              hash.reset();
              status->setText("Downloading installer…");
              downloadBinary(QUrl(QString::fromStdString(authenticated ? info.apiUrl : info.browserUrl)));
          });
}
void UpdateDialog::downloadBinary(const QUrl &url, int redirects) {
    if (redirects > 5) {
        fail("Too many installer redirects");
        return;
    }
    QNetworkReply *reply = nullptr;
    try {
        reply = get(url, false);
    } catch (const std::exception &e) {
        fail(QString::fromUtf8(e.what()));
        return;
    }
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        auto data = reply->readAll();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
            return;
        received += data.size();
        if (!file || received > static_cast<qint64>(info.size) || file->write(data) != data.size()) {
            reply->abort();
            return;
        }
        hash.addData(data);
        progress->setValue(static_cast<int>(received * 100 / static_cast<qint64>(info.size)));
    });
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        auto redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto error = reply->error();
        reply->deleteLater();
        if (code >= 300 && code < 400 && !redirect.isEmpty()) {
            downloadBinary(url.resolved(redirect), redirects + 1);
            return;
        }
        if (error != QNetworkReply::NoError || code != 200 || received != static_cast<qint64>(info.size) ||
            QString::fromLatin1(hash.result().toHex()) != expectedHash) {
            fail("Installer download failed size/SHA256 validation. Nothing will be installed.");
            return;
        }
        if (!file || !file->commit()) {
            fail("Cannot save verified installer");
            return;
        }
        file.reset();
        progress->setValue(100);
        status->setText("SHA256 verified. Ready to install.");
        install->setEnabled(true);
        check->setEnabled(true);
        token->setEnabled(true);
    });
}
} // namespace rb
