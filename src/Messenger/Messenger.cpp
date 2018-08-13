#include "Messenger.h"

#include "check.h"
#include "SlotWrapper.h"
#include "Log.h"
#include "makeJsFunc.h"

#include "MessengerMessages.h"
#include "MessengerJavascript.h"

#include <functional>
using namespace std::placeholders;

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>

#include <QCryptographicHash>

#include "dbstorage.h"

static QString createHashMessage(const QString &message) {
    return QString(QCryptographicHash::hash(message.toUtf8(), QCryptographicHash::Sha512).toHex());
}

std::vector<QString> Messenger::stringsForSign() {
    return {makeTextForGetMyMessagesRequest(), makeTextForGetChannelRequest(), makeTextForGetChannelsRequest(), makeTextForMsgAppendKeyOnlineRequest()};
}

QString Messenger::makeTextForSignRegisterRequest(const QString &address, const QString &rsaPubkeyHex, uint64_t fee) {
    return ::makeTextForSignRegisterRequest(address, rsaPubkeyHex, fee);
}

QString Messenger::makeTextForGetPubkeyRequest(const QString &address) {
    return ::makeTextForGetPubkeyRequest(address);
}

QString Messenger::makeTextForSendMessageRequest(const QString &address, const QString &dataHex, uint64_t fee) {
    return ::makeTextForSendMessageRequest(address, dataHex, fee);
}

Messenger::Messenger(MessengerJavascript &javascriptWrapper, QObject *parent)
    : db(*DBStorage::instance())
    , TimerClass(1s, parent)
    , javascriptWrapper(javascriptWrapper)
    , wssClient("wss.wss.com")
{
    CHECK(connect(this, SIGNAL(timerEvent()), this, SLOT(onTimerEvent())), "not connect onTimerEvent");
    CHECK(connect(&wssClient, &WebSocketClient::messageReceived, this, &Messenger::onWssMessageReceived), "not connect wssClient");
    CHECK(connect(this, SIGNAL(startedEvent()), this, SLOT(onRun())), "not connect run");

    CHECK(connect(this, &Messenger::registerAddress, this, &Messenger::onRegisterAddress), "not connect onRegisterAddress");
    CHECK(connect(this, &Messenger::savePubkeyAddress, this, &Messenger::onSavePubkeyAddress), "not connect onGetPubkeyAddress");
    CHECK(connect(this, &Messenger::sendMessage, this, &Messenger::onSendMessage), "not connect onSendMessage");
    CHECK(connect(this, &Messenger::signedStrings, this, &Messenger::onSignedStrings), "not connect onSignedStrings");
    CHECK(connect(this, &Messenger::getLastMessage, this, &Messenger::onGetLastMessage), "not connect onSignedStrings");
    CHECK(connect(this, &Messenger::getSavedPos, this, &Messenger::onGetSavedPos), "not connect onGetSavedPos");
    CHECK(connect(this, &Messenger::savePos, this, &Messenger::onSavePos), "not connect onGetSavedPos");
    CHECK(connect(this, &Messenger::getHistoryAddress, this, &Messenger::onGetHistoryAddress), "not connect onGetHistoryAddress");
    CHECK(connect(this, &Messenger::getHistoryAddressAddress, this, &Messenger::onGetHistoryAddressAddress), "not connect onGetHistoryAddressAddress");
    CHECK(connect(this, &Messenger::getHistoryAddressAddressCount, this, &Messenger::onGetHistoryAddressAddressCount), "not connect onGetHistoryAddressAddressCount");

    wssClient.start();
}

void Messenger::invokeCallback(size_t requestId, const TypedException &exception) {
    auto found = callbacks.find(requestId);
    CHECK(found != callbacks.end(), "Not found callback for request " + std::to_string(requestId));
    const ResponseCallbacks callback = found->second; // копируем
    callbacks.erase(found);
    callback(exception);
}

std::vector<QString> Messenger::getMonitoredAddresses() const {
    const QStringList res = db.getUsersList();
    std::vector<QString> result;
    for (const QString r: res) {
        result.emplace_back(r);
    }
    return result;
}

void Messenger::onSignedStrings(const std::vector<QString> &signedHexs, const SignedStringsCallback &callback) {
BEGIN_SLOT_WRAPPER
    const TypedException exception = apiVrapper2([&, this] {
        const std::vector<QString> keys = stringsForSign();
        CHECK(keys.size() == signedHexs.size(), "Incorrect signed strings");

        QJsonArray arrJson;
        for (size_t i = 0; i < keys.size(); i++) {
            const QString &key = keys[i];
            const QString &value = signedHexs[i];

            QJsonObject obj;
            obj.insert("key", key);
            obj.insert("value", value);
            arrJson.push_back(obj);
        }

        const QString arr = QJsonDocument(arrJson).toJson(QJsonDocument::Compact);
        // Сохранить arr в бд
    });

    emit javascriptWrapper.callbackCall(std::bind(callback, exception));
END_SLOT_WRAPPER
}

void Messenger::onGetLastMessage(const QString &address, const GetSavedPosCallback &callback) {
BEGIN_SLOT_WRAPPER
    Message::Counter lastCounter;
    const TypedException exception = apiVrapper2([&, this] {
        lastCounter = db.getMessageMaxCounter(address);
    });
    emit javascriptWrapper.callbackCall(std::bind(callback, lastCounter, exception));
END_SLOT_WRAPPER
}

void Messenger::onGetSavedPos(const QString &address, const GetSavedPosCallback &callback) {
BEGIN_SLOT_WRAPPER
    // Получить counter
    Message::Counter lastCounter = 0;
    const TypedException exception = apiVrapper2([&, this] {

    });
    emit javascriptWrapper.callbackCall(std::bind(callback, lastCounter, exception));
END_SLOT_WRAPPER
}

void Messenger::onSavePos(const QString &address, Message::Counter pos, const SavePosCallback &callback) {
BEGIN_SLOT_WRAPPER
    // Сохранить позицию
    const TypedException exception = apiVrapper2([&, this] {

    });
    emit javascriptWrapper.callbackCall(std::bind(callback, exception));
END_SLOT_WRAPPER
}

QString Messenger::getSignFromMethod(const QString &address, const QString &method) const {
    // Взять json из бд
    const QString jsonString = "";
    const QJsonDocument json = QJsonDocument::fromJson(jsonString.toUtf8());
    CHECK(json.isArray(), "Incorrect json");
    const QJsonArray &array = json.array();
    for (const QJsonValue &val: array) {
        CHECK(val.isObject(), "Incorrect json");
        const QJsonObject v = val.toObject();
        CHECK(v.contains("key") && v.value("key").isString(), "Incorrect json: key field not found");
        const QString key = v.value("key").toString();
        CHECK(v.contains("value") && v.value("value").isString(), "Incorrect json: value field not found");
        const QString value = v.value("value").toString();

        if (key == method) {
            return value;
        }
    }
    throwErr(("Not found signed method " + method + " in address " + address).toStdString());
}

void Messenger::onRun() {
BEGIN_SLOT_WRAPPER
    const std::vector<QString> monitoredAddresses = getMonitoredAddresses();
    clearAddressesToMonitored();
    for (const QString &address: monitoredAddresses) {
        addAddressToMonitored(address);
    }
END_SLOT_WRAPPER
}

void Messenger::onTimerEvent() {
BEGIN_SLOT_WRAPPER
    for (auto &pairDeferred: deferredMessages) {
        const QString &address = pairDeferred.first;
        DeferredMessage &deferred = pairDeferred.second;
        if (deferred.check()) {
            deferred.resetDeferred();
            const Message::Counter lastCnt = db.getMessageMaxCounter(address);
            emit javascriptWrapper.newMessegesSig(address, lastCnt);
        }
    }
END_SLOT_WRAPPER
}

void Messenger::processMessages(const QString &address, const std::vector<NewMessageResponse> &messages) {
    CHECK(!messages.empty(), "Empty messages");
    const Message::Counter currConfirmedCounter = db.getMessageMaxConfirmedCounter(address);
    const Message::Counter minCounterInServer = messages.front().counter;
    const Message::Counter maxCounterInServer = messages.back().counter;

    for (const NewMessageResponse &m: messages) {
        const QString hashMessage = createHashMessage(m.data);
        if (m.isInput) {
            db.addMessage(address, m.collocutor, m.data, m.timestamp, m.counter, m.isInput, true, true, hashMessage);
        } else {
            // Вычислить хэш сообщения, найти сообщение в bd минимальное по номеру, которое не подтвержденное, заменить у него counter. Если сообщение не нашлось, поискать просто по хэшу. Если и оно не нашлось, то вставить
            // Потом запросить сообщение по предыдущему counter output-а, если он изменился и такого номера еще нет, и установить deferrer
        }
    }

    if (minCounterInServer > currConfirmedCounter + 1) {
        deferredMessages[address].setDeferred(2s);
        getMessagesFromAddressFromWss(address, currConfirmedCounter + 1, minCounterInServer);
    } else {
        if (!deferredMessages[address].isDeferred()) {
            emit javascriptWrapper.newMessegesSig(address, maxCounterInServer);
        }
    }
}

void Messenger::onWssMessageReceived(QString message) {
BEGIN_SLOT_WRAPPER
    const QJsonDocument messageJson = QJsonDocument::fromJson(message.toUtf8());
    const ResponseType responseType = getMethodAndAddressResponse(messageJson);

    if (responseType.isError) {
        LOG << "Messenger response error " << responseType.method << " " << responseType.address << " " << responseType.error;
        if (responseType.method != METHOD::NOT_SET && !responseType.address.isEmpty() && !responseType.address.isNull()) {
            invokeCallback(responseType.id, TypedException(TypeErrors::MESSENGER_ERROR, responseType.error.toStdString()));
        }
        return;
    }

    if (responseType.method == METHOD::APPEND_KEY_TO_ADDR) {
        invokeCallback(responseType.id, TypedException());
    } else if (responseType.method == METHOD::COUNT_MESSAGES) {
        const Message::Counter currCounter = db.getMessageMaxConfirmedCounter(responseType.address);
        const Message::Counter messagesInServer = parseCountMessagesResponse(messageJson);
        if (currCounter < messagesInServer) {
            getMessagesFromAddressFromWss(responseType.address, currCounter + 1, messagesInServer); // TODO уточнить, to - это включительно или нет
        }
    } else if (responseType.method == METHOD::GET_KEY_BY_ADDR) {
        const auto publicKeyPair = parseKeyMessageResponse(messageJson);
        const QString &address = publicKeyPair.first;
        const QString &pkey = publicKeyPair.second;
        db.setUserPublicKey(address, pkey);
        invokeCallback(responseType.id, TypedException());
    } else if (responseType.method == METHOD::NEW_MSG) {
        const NewMessageResponse messages = parseNewMessageResponse(messageJson);
        processMessages(responseType.address, {messages});
    } else if (responseType.method == METHOD::NEW_MSGS) {
        const std::vector<NewMessageResponse> messages = parseNewMessagesResponse(messageJson);
        processMessages(responseType.address, messages);
    } else if (responseType.method == METHOD::SEND_TO_ADDR) {
        invokeCallback(responseType.id, TypedException());
    } else {
        throwErr("Incorrect response type");
    }
END_SLOT_WRAPPER
}

void Messenger::onRegisterAddress(bool isForcibly, const QString &address, const QString &rsaPubkeyHex, const QString &pubkeyAddressHex, const QString &signHex, uint64_t fee, const RegisterAddressCallback &callback) {
BEGIN_SLOT_WRAPPER
    // Проверить в базе, если пользователь уже зарегистрирован, то больше не регестрировать
    const size_t idRequest = id.get();
    const QString message = makeRegisterRequest(rsaPubkeyHex, pubkeyAddressHex, signHex, fee, idRequest);
    const bool isNew = true;
    callbacks[idRequest] = std::bind(callback, isNew, _1);
    emit wssClient.sendMessage(message);
END_SLOT_WRAPPER
}

void Messenger::onSavePubkeyAddress(bool isForcibly, const QString &address, const QString &pubkeyHex, const QString &signHex, const SavePubkeyCallback &callback) {
BEGIN_SLOT_WRAPPER
    // Проверить, есть ли нужных ключ в базе
    const size_t idRequest = id.get();
    const QString message = makeGetPubkeyRequest(address, pubkeyHex, signHex, idRequest);
    callbacks[idRequest] = std::bind(callback, true, _1);
    emit wssClient.sendMessage(message);
END_SLOT_WRAPPER
}

void Messenger::onGetPubkeyAddress(const QString &address, const GetPubkeyAddress &callback) {
    // Взять публичный ключ из базы
    const QString pubkey = "";
    const TypedException exception = apiVrapper2([&, this] {

    });
    emit javascriptWrapper.callbackCall(std::bind(callback, pubkey, exception));
}

void Messenger::onSendMessage(const QString &thisAddress, const QString &toAddress, const QString &dataHex, const QString &pubkeyHex, const QString &signHex, uint64_t fee, uint64_t timestamp, const QString &encryptedDataHex, const SendMessageCallback &callback) {
BEGIN_SLOT_WRAPPER
    const QString hashMessage = createHashMessage(dataHex);
    const Message::Counter lastCnt = db.getMessageMaxCounter(thisAddress);
    db.addMessage(thisAddress, toAddress, encryptedDataHex, timestamp, lastCnt + 1, false, true, false, hashMessage);
    const size_t idRequest = id.get();
    const QString message = makeSendMessageRequest(toAddress, dataHex, pubkeyHex, signHex, fee, timestamp, idRequest);
    callbacks[idRequest] = callback;
    emit wssClient.sendMessage(message);
END_SLOT_WRAPPER
}

void Messenger::getMessagesFromAddressFromWss(const QString &fromAddress, Message::Counter from, Message::Counter to) {
    // Получаем sign и pubkey для данного типа сообщений из базы
    const QString pubkeyHex = "";
    const QString signHex = getSignFromMethod(fromAddress, makeTextForGetMyMessagesRequest());
    const QString message = makeGetMyMessagesRequest(pubkeyHex, signHex, from, to, id.get());
    emit wssClient.sendMessage(message);
}

void Messenger::clearAddressesToMonitored() {
    emit wssClient.setHelloString(std::vector<QString>{});
}

void Messenger::addAddressToMonitored(const QString &address) {
    // Получаем sign для данного типа сообщений из базы
    const QString pubkeyHex = "";
    const QString signHex = getSignFromMethod(address, makeTextForMsgAppendKeyOnlineRequest());
    const QString message = makeAppendKeyOnlineRequest(pubkeyHex, signHex, id.get());
    emit wssClient.addHelloString(message);
    emit wssClient.sendMessage(message);
}

void Messenger::onGetHistoryAddress(QString address, Message::Counter from, Message::Counter to, const GetMessagesCallback &callback) {
BEGIN_SLOT_WRAPPER
    std::vector<Message> messages;
    const TypedException exception = apiVrapper2([&, this] {
        const std::list<Message> result = db.getMessagesForUser(address, from, to);
        std::copy(result.begin(), result.end(), std::back_inserter(messages));
    });
    emit javascriptWrapper.callbackCall(std::bind(callback, messages, exception));
    // Обработать ошибки
END_SLOT_WRAPPER
}

void Messenger::onGetHistoryAddressAddress(QString address, QString collocutor, Message::Counter from, Message::Counter to, const GetMessagesCallback &callback) {
BEGIN_SLOT_WRAPPER
    std::vector<Message> messages;
    const TypedException exception = apiVrapper2([&, this] {
        const std::list<Message> result = db.getMessagesForUserAndDest(address, collocutor, from, to);
        std::copy(result.begin(), result.end(), std::back_inserter(messages));
    });
    emit javascriptWrapper.callbackCall(std::bind(callback, messages, exception));
END_SLOT_WRAPPER
}

void Messenger::onGetHistoryAddressAddressCount(QString address, QString collocutor, Message::Counter count, Message::Counter to, const GetMessagesCallback &callback) {
BEGIN_SLOT_WRAPPER
       // TODO from -> to
    std::vector<Message> messages;
    const TypedException exception = apiVrapper2([&, this] {
        const std::list<Message> result = db.getMessagesForUserAndDestNum(address, collocutor, count, to);
        std::copy(result.begin(), result.end(), std::back_inserter(messages));
    });
    emit javascriptWrapper.callbackCall(std::bind(callback, messages, exception));
END_SLOT_WRAPPER
}
