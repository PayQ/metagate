#ifndef CRYPTOGRAPHICMANAGER_H
#define CRYPTOGRAPHICMANAGER_H

#include <QObject>

#include "TimerClass.h"
#include "CallbackWrapper.h"

#include "Message.h"

#include "Wallet.h"
#include "WalletRsa.h"

class TypedException;

namespace messenger {

class CryptographicManager : public TimerClass {
    Q_OBJECT
public:

    explicit CryptographicManager(QObject *parent = nullptr);

    bool isSaveDecrypted() const {
        return isSaveDecrypted_;
    }

public:

    using DecryptMessagesCallback = CallbackWrapper<std::function<void(const std::vector<Message> &messages)>>;

    using SignMessageCallback = CallbackWrapper<std::function<void(const QString &pubkey, const QString &sign)>>;

    using SignMessagesCallback = CallbackWrapper<std::function<void(const QString &pubkey, const std::vector<QString> &sign)>>;

    using GetPubkeyRsaCallback = CallbackWrapper<std::function<void(const QString &pubkeyRsa)>>;

    using EncryptMessageCallback = CallbackWrapper<std::function<void(const QString &encryptedData)>>;

    using UnlockWalletCallback = CallbackWrapper<std::function<void()>>;

    using LockWalletCallback = CallbackWrapper<std::function<void()>>;

signals:

    void decryptMessages(const std::vector<Message> &messages, const QString &address, const DecryptMessagesCallback &callback);

    void tryDecryptMessages(const std::vector<Message> &messages, const QString &address, const DecryptMessagesCallback &callback);

    void signMessage(const QString &address, const QString &message, const SignMessageCallback &callback);

    void signMessages(const QString &address, const std::vector<QString> &messages, const SignMessagesCallback &callback);

    void getPubkeyRsa(const QString &address, const GetPubkeyRsaCallback &callback);

    void encryptDataRsa(const QString &dataHex, const QString &pubkeyDest, const EncryptMessageCallback &callback);

    void encryptDataPrivateKey(const QString &dataHex, const QString &address, const EncryptMessageCallback &callback);

    void unlockWallet(const QString &folder, const QString &address, const QString &password, const QString &passwordRsa, const seconds &time_, const UnlockWalletCallback &callbackWrapper);

    void lockWallet(const LockWalletCallback &callback);

private slots:

    void onDecryptMessages(const std::vector<Message> &messages, const QString &address, const DecryptMessagesCallback &callback);

    void onTryDecryptMessages(const std::vector<Message> &messages, const QString &address, const DecryptMessagesCallback &callback);

    void onSignMessage(const QString &address, const QString &message, const SignMessageCallback &callback);

    void onSignMessages(const QString &address, const std::vector<QString> &messages, const SignMessagesCallback &callback);

    void onGetPubkeyRsa(const QString &address, const GetPubkeyRsaCallback &callback);

    void onEncryptDataRsa(const QString &dataHex, const QString &pubkeyDest, const EncryptMessageCallback &callback);

    void onEncryptDataPrivateKey(const QString &dataHex, const QString &address, const EncryptMessageCallback &callback);

    void onUnlockWallet(const QString &folder, const QString &address, const QString &password, const QString &passwordRsa, const seconds &time_, const UnlockWalletCallback &callbackWrapper);

    void onLockWallet(const LockWalletCallback &callback);

private slots:

    void onResetWallets();

private:

    Wallet& getWallet(const std::string &address) const;

    WalletRsa& getWalletRsa(const std::string &address) const;

    WalletRsa* getWalletRsaWithoutCheck(const std::string &address) const;

    void unlockWalletImpl(const QString &folder, const std::string &address, const std::string &password, const std::string &passwordRsa, const seconds &time_);

    void lockWalletImpl();

private:

    const bool isSaveDecrypted_;

    std::unique_ptr<Wallet> wallet;
    std::unique_ptr<WalletRsa> walletRsa;

    seconds time;
    time_point startTime;

};

}

#endif // CRYPTOGRAPHICMANAGER_H