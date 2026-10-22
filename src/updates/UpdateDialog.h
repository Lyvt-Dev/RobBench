#pragma once
#include "updates/UpdateManifest.h"
#include <QCryptographicHash>
#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QSaveFile>
#include <functional>
#include <memory>
class QLabel;
class QPushButton;
class QLineEdit;
class QProgressBar;
class QPlainTextEdit;
namespace rb {
class UpdateDialog final : public QDialog {
  public:
    explicit UpdateDialog(QWidget *parent, std::function<bool()> beforeInstall);
    ~UpdateDialog() override;

  private:
    QNetworkAccessManager network;
    QPointer<QNetworkReply> active;
    QLabel *status{};
    QLineEdit *token{};
    QProgressBar *progress{};
    QPlainTextEdit *notes{};
    QPushButton *check{}, *download{}, *install{};
    UpdateInfo info;
    QString installerPath, expectedHash;
    std::function<bool()> beforeInstall;
    std::unique_ptr<QSaveFile> file;
    QCryptographicHash hash{QCryptographicHash::Sha256};
    qint64 received{};
    QNetworkReply *get(const QUrl &, bool json);
    void fetch(const QUrl &, qint64 limit, bool json, std::function<void(QByteArray)>, int redirects = 0);
    void checkNow();
    void downloadNow();
    void downloadBinary(const QUrl &, int redirects = 0);
    void fail(const QString &);
};
} // namespace rb
