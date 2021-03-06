#include "consolemodel.h"
#include <qredisclient/redisclient.h>

using namespace Console;

Model::Model(QSharedPointer<RedisClient::Connection> connection, int dbIndex)
    : TabModel(connection, dbIndex), m_current_db(dbIndex) {
  QObject::connect(this, &TabModel::initialized, [this]() {
    if (m_connection->mode() == RedisClient::Connection::Mode::Cluster) {
      emit addOutput(QObject::tr("Connected to cluster.\n"), "complete");
    } else {
      emit addOutput(QObject::tr("Connected.\n"), "complete");
    }

    updatePrompt(true);
  });

  QObject::connect(this, &TabModel::error, [this](const QString& msg) {
    emit addOutput(msg, "error");
  });
}

QString Model::getName() const { return m_connection->getConfig().name(); }

void Model::executeCommand(const QString& cmd) {
  if (cmd == "segfault") {  // crash
    delete reinterpret_cast<QString*>(0xFEE1DEAD);
    return;
  }

  using namespace RedisClient;

  Command command(Command::splitCommandString(cmd), m_current_db);

  if (command.isSubscriptionCommand()) {
    emit addOutput(
        "Switch to Pub/Sub mode. Close console tab to stop listen for "
        "messages.",
        "part");

    command.setCallBack(this, [this](Response result, QString err) {
      if (!err.isEmpty()) {
        emit addOutput(QObject::tr("Subscribe error: %1").arg(err), "error");
        return;
      }

      QVariant value = result.getValue();
      emit addOutput(RedisClient::Response::valueToHumanReadString(value),
                     "part");
    });

    m_connection->command(command);
  } else {
    Response result;

    try {
      result = m_connection->commandSync(command);
    } catch (Connection::Exception& e) {
      emit addOutput(
          QString(QObject::tr("Connection error:")) + QString(e.what()),
          "error");
      return;
    }

    if (command.isSelectCommand() ||
        m_connection->mode() == RedisClient::Connection::Mode::Cluster) {
      m_current_db = m_connection->dbIndex();
      updatePrompt(false);
    }
    QVariant value = result.getValue();
    emit addOutput(RedisClient::Response::valueToHumanReadString(value),
                   "complete");
  }
}

void Model::updatePrompt(bool showPrompt) {
  if (m_connection->mode() == RedisClient::Connection::Mode::Cluster) {
    emit changePrompt(QString("%1(%2:%3)>")
                          .arg(m_connection->getConfig().name())
                          .arg(m_connection->getConfig().host())
                          .arg(m_connection->getConfig().port()),
                      showPrompt);
  } else {
    emit changePrompt(QString("%1:%2>")
                          .arg(m_connection->getConfig().name())
                          .arg(m_current_db),
                      showPrompt);
  }
}
